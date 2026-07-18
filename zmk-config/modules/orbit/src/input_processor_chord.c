/*
 * ORBit chord input processor (spec §4 item 4: slider CS7+CS8 both = mute).
 *
 * ZMK combos only exist for kscan positions; the touch zones are input events,
 * so two-pad chording needs an input processor. This one sits FIRST in the
 * listener chain and rewrites the event stream; the stock
 * zmk,input-processor-behaviors after it maps the (possibly rewritten) codes
 * to behaviors as usual.
 *
 * Semantics:
 *  - Press of code A (or B) alone: held back. If nothing else happens within
 *    timeout-ms it is re-injected unchanged — a single tap/hold works, delayed
 *    by at most the window.
 *  - Press of the other code inside the window: the second press event is
 *    rewritten to chord-code (press). Neither original press is ever
 *    delivered. When the FIRST of the two fingers lifts, that release is
 *    rewritten to chord-code (release); the other finger's release is
 *    swallowed. New presses are ignored until both fingers are up.
 *  - Quick tap shorter than the window: press+release are re-injected
 *    back-to-back on release.
 *
 * Concurrency: events arrive on the input dispatch context; the timeout runs
 * on the system workqueue. All state lives under a spinlock; injected events
 * (input_report_key on the source device, so they re-enter this same
 * processor) are counted in pass_cnt[] and waved through. Injection happens
 * strictly OUTSIDE the lock: with CONFIG_INPUT_MODE_SYNCHRONOUS the report
 * recurses into this handler on the spot.
 *
 * The FIFO input queue plus inject-while-processing ordering means every
 * injected event is consumed before any later genuine event that could
 * change its meaning; the one exception (re-tap plus chord inside a single
 * window — humanly implausible) degrades to "chord missed, keys released",
 * never to a stuck key.
 */

#define DT_DRV_COMPAT orbit_input_processor_chord

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/spinlock.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/logging/log.h>

#include <orbit_input_processor_api.h>

LOG_MODULE_REGISTER(orbit_chord, CONFIG_INPUT_LOG_LEVEL);

enum chord_state {
    ST_IDLE,        /* no chord activity */
    ST_PENDING,     /* one code pressed, held back, window timer running */
    ST_CHORD,       /* chord-code press delivered, both fingers down */
    ST_CHORD_DRAIN, /* chord-code release delivered, one finger still down */
};

struct chord_config {
    uint16_t code_a;
    uint16_t code_b;
    uint16_t chord_code;
    uint32_t timeout_ms;
};

struct chord_data {
    const struct chord_config *cfg;
    const struct device *src; /* input device we shadow; learned from events */
    struct k_work_delayable timeout_work;
    struct k_spinlock lock;
    enum chord_state state;
    uint16_t pending_code;      /* the held-back code while ST_PENDING */
    uint8_t pass_cnt[2];        /* injected events in flight, per code index */
    bool deferred_release[2];   /* genuine release overtook an injected press */
    bool delivered_down[2];     /* a press was delivered; its release must pass */
};

static inline int code_idx(const struct chord_config *cfg, uint16_t code) {
    return (code == cfg->code_a) ? 0 : 1;
}

/* Inject outside the lock. On (pathological) queue-full failure, roll the
 * pass counter back so the bookkeeping can't leak a swallowed genuine event. */
static void chord_inject(struct chord_data *data, uint16_t code, int32_t value) {
    int ret = input_report_key(data->src, code, value, true, K_NO_WAIT);

    if (ret < 0) {
        k_spinlock_key_t key = k_spin_lock(&data->lock);
        data->pass_cnt[code_idx(data->cfg, code)]--;
        k_spin_unlock(&data->lock, key);
        LOG_ERR("dropped injected event %u/%d (%d)", code, value, ret);
    }
}

static void chord_timeout(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct chord_data *data = CONTAINER_OF(dwork, struct chord_data, timeout_work);
    uint16_t code = 0;
    bool inject = false;

    k_spinlock_key_t key = k_spin_lock(&data->lock);
    if (data->state == ST_PENDING) {
        /* Window elapsed with one finger down: deliver the held-back press. */
        data->state = ST_IDLE;
        code = data->pending_code;
        data->pass_cnt[code_idx(data->cfg, code)]++;
        inject = true;
    }
    k_spin_unlock(&data->lock, key);

    if (inject && data->src != NULL) {
        chord_inject(data, code, 1);
    }
}

static int chord_handle_event(const struct device *dev, struct input_event *event,
                              uint32_t param1, uint32_t param2,
                              struct zmk_input_processor_state *state) {
    const struct chord_config *cfg = dev->config;
    struct chord_data *data = dev->data;

    if (event->type != INPUT_EV_KEY ||
        (event->code != cfg->code_a && event->code != cfg->code_b)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    const int i = code_idx(cfg, event->code);
    const int other = 1 - i;
    int verdict;
    bool inject_press = false;
    bool inject_release = false;

    data->src = event->dev;

    k_spinlock_key_t key = k_spin_lock(&data->lock);

    if (event->value) { /* ---- press ---- */
        if (data->pass_cnt[i] > 0) {
            /* One of our own injected presses: wave it through. */
            data->pass_cnt[i]--;
            data->delivered_down[i] = true;
            if (data->deferred_release[i]) {
                /* Its genuine release already came and went; replay it. */
                data->deferred_release[i] = false;
                data->pass_cnt[i]++;
                inject_release = true;
            }
            verdict = ZMK_INPUT_PROC_CONTINUE;
        } else {
            switch (data->state) {
            case ST_IDLE:
                if (data->delivered_down[other]) {
                    /* Other pad already delivered as a single: too late to
                     * chord, treat this one independently too. */
                    data->delivered_down[i] = true;
                    verdict = ZMK_INPUT_PROC_CONTINUE;
                } else {
                    data->state = ST_PENDING;
                    data->pending_code = event->code;
                    k_work_schedule(&data->timeout_work, K_MSEC(cfg->timeout_ms));
                    verdict = ZMK_INPUT_PROC_STOP;
                }
                break;
            case ST_PENDING:
                if (event->code != data->pending_code) {
                    /* Chord: second pad inside the window. The held-back
                     * press is dropped; this event becomes the chord press.
                     * The cancel is best-effort — if the timer already fired
                     * it will see state != ST_PENDING and do nothing. */
                    k_work_cancel_delayable(&data->timeout_work);
                    data->state = ST_CHORD;
                    event->code = cfg->chord_code;
                    verdict = ZMK_INPUT_PROC_CONTINUE;
                } else {
                    verdict = ZMK_INPUT_PROC_STOP; /* duplicate press glitch */
                }
                break;
            default: /* ST_CHORD / ST_CHORD_DRAIN */
                /* Re-presses while the chord drains are ignored; a fresh
                 * touch counts once both fingers are up. */
                verdict = ZMK_INPUT_PROC_STOP;
                break;
            }
        }
    } else { /* ---- release ---- */
        if (data->state == ST_PENDING && event->code == data->pending_code) {
            /* Tap shorter than the window: replay press+release in order.
             * Timer cancel is safe under the lock — if its handler is
             * already queued/running it re-checks state and no-ops. */
            k_work_cancel_delayable(&data->timeout_work);
            data->state = ST_IDLE;
            data->pass_cnt[i] += 2;
            inject_press = true;
            inject_release = true;
            verdict = ZMK_INPUT_PROC_STOP;
        } else if (data->state == ST_CHORD) {
            /* First finger up ends the chord. */
            data->state = ST_CHORD_DRAIN;
            event->code = cfg->chord_code;
            verdict = ZMK_INPUT_PROC_CONTINUE;
        } else if (data->state == ST_CHORD_DRAIN) {
            /* Second finger up: chord release already went out. */
            data->state = ST_IDLE;
            verdict = ZMK_INPUT_PROC_STOP;
        } else if (data->delivered_down[i]) {
            /* Release matching a delivered press (genuine or injected). */
            data->delivered_down[i] = false;
            if (data->pass_cnt[i] > 0) {
                data->pass_cnt[i]--; /* it was one of our injected releases */
            }
            verdict = ZMK_INPUT_PROC_CONTINUE;
        } else if (data->pass_cnt[i] > 0) {
            /* Genuine release processed before our injected press reached
             * the front of the queue: hold it, replay after the press. */
            data->deferred_release[i] = true;
            verdict = ZMK_INPUT_PROC_STOP;
        } else {
            verdict = ZMK_INPUT_PROC_STOP; /* stray release */
        }
    }

    k_spin_unlock(&data->lock, key);

    if (inject_press) {
        chord_inject(data, event->code, 1);
    }
    if (inject_release) {
        chord_inject(data, event->code, 0);
    }

    return verdict;
}

static const struct zmk_input_processor_driver_api chord_driver_api = {
    .handle_event = chord_handle_event,
};

static int chord_init(const struct device *dev) {
    struct chord_data *data = dev->data;

    k_work_init_delayable(&data->timeout_work, chord_timeout);
    data->state = ST_IDLE;
    return 0;
}

#define CHORD_INST(n)                                                                              \
    BUILD_ASSERT(DT_INST_PROP_LEN(n, codes) == 2,                                                  \
                 "orbit,input-processor-chord needs exactly two codes");                           \
    static const struct chord_config chord_config_##n = {                                          \
        .code_a = DT_INST_PROP_BY_IDX(n, codes, 0),                                                \
        .code_b = DT_INST_PROP_BY_IDX(n, codes, 1),                                                \
        .chord_code = DT_INST_PROP(n, chord_code),                                                 \
        .timeout_ms = DT_INST_PROP(n, timeout_ms),                                                 \
    };                                                                                             \
    static struct chord_data chord_data_##n = {                                                    \
        .cfg = &chord_config_##n,                                                                  \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, chord_init, NULL, &chord_data_##n, &chord_config_##n, POST_KERNEL,    \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &chord_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CHORD_INST)
