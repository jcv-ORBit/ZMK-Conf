/*
 * ORBit glow engine (spec §4 item 6).
 *
 * Scene model — deliberately minimal ("no animations" beyond what the spec
 * names):
 *   - ACTIVE: every LED at idle-brightness amber ("idle low duty").
 *   - zone flash: a touched zone's LED jumps to flash-brightness for
 *     flash-ms, then falls back (input callback on the touch device).
 *   - breathe: triangle envelope over the whole chain for a bounded time;
 *     exported as orbit_glow_breathe() for item 7's proximity-wake BLE mask.
 *     Not triggered anywhere yet.
 *   - ZMK IDLE or SLEEP: all off. IDLE keeps the "quiet dark desk object"
 *     identity (and the power budget); SLEEP blanks the strip synchronously
 *     in the event listener — WS2812s latch their last frame, so anything
 *     still lit at poweroff would stay lit off a powered rail.
 *
 * Rendering runs on the system workqueue (one 8-pixel SPI frame ~ hundreds
 * of µs); the input callback and event listener only poke state and submit
 * work, except the SLEEP blank which renders in place before ZMK cuts over
 * to soft-off.
 *
 * ZMK event access: this module includes <zmk/...> via the application
 * include dir (${APPLICATION_SOURCE_DIR}/include in CMake) — ZMK is the
 * application, not a Zephyr module, so that is the supported spelling.
 */

#define DT_DRV_COMPAT orbit_glow

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/input/input.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/activity.h>

#include <orbit_glow.h>

LOG_MODULE_REGISTER(orbit_glow, CONFIG_LED_STRIP_LOG_LEVEL);

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1,
             "orbit,glow supports exactly one instance");

#define GLOW_LEN     DT_INST_PROP_LEN(0, codes)
#define GLOW_CHAIN   DT_PROP(DT_INST_PHANDLE(0, led_strip), chain_length)
#define GLOW_COLOR   DT_INST_PROP(0, color_hex)
#define GLOW_IDLE    DT_INST_PROP(0, idle_brightness)
#define GLOW_FLASH   DT_INST_PROP(0, flash_brightness)
#define GLOW_FLASH_MS DT_INST_PROP(0, flash_ms)
#define GLOW_BREATHE_MS DT_INST_PROP(0, breathe_period_ms)

/* Render tick while a flash or breathe is in flight. */
#define GLOW_FRAME_MS 40

static const uint16_t glow_codes[GLOW_LEN] = DT_INST_PROP(0, codes);
static const uint16_t glow_leds[GLOW_LEN] = DT_INST_PROP(0, zone_leds);
BUILD_ASSERT(DT_INST_PROP_LEN(0, codes) == DT_INST_PROP_LEN(0, zone_leds),
             "codes and zone-leds must be the same length");

static const struct device *const glow_strip = DEVICE_DT_GET(DT_INST_PHANDLE(0, led_strip));

static struct {
    struct k_work_delayable render_work;
    int64_t flash_until[GLOW_LEN];
    int64_t breathe_until;
    atomic_t scene; /* enum zmk_activity_state */
} glow;

static void glow_pixel(struct led_rgb *px, uint32_t pct) {
    /* Amber family only (spec §7): one base color, scaled by duty. */
    px->r = (uint8_t)(((GLOW_COLOR >> 16) & 0xff) * pct / 100);
    px->g = (uint8_t)(((GLOW_COLOR >> 8) & 0xff) * pct / 100);
    px->b = (uint8_t)((GLOW_COLOR & 0xff) * pct / 100);
}

static void glow_render(void) {
    struct led_rgb px[GLOW_CHAIN];
    int64_t now = k_uptime_get();
    bool busy = false;
    uint32_t base = 0;

    if (atomic_get(&glow.scene) == ZMK_ACTIVITY_ACTIVE) {
        base = GLOW_IDLE;

        if (now < glow.breathe_until) {
            /* Triangle envelope between 0 and flash-brightness. */
            uint32_t phase = (uint32_t)(now % GLOW_BREATHE_MS);
            uint32_t half = GLOW_BREATHE_MS / 2;
            uint32_t ramp = (phase < half) ? phase : (GLOW_BREATHE_MS - phase);

            base = GLOW_FLASH * ramp / half;
            busy = true;
        }
    }

    for (int i = 0; i < GLOW_CHAIN; i++) {
        glow_pixel(&px[i], base);
    }

    if (atomic_get(&glow.scene) == ZMK_ACTIVITY_ACTIVE) {
        for (int i = 0; i < GLOW_LEN; i++) {
            if (now < glow.flash_until[i]) {
                glow_pixel(&px[glow_leds[i]], GLOW_FLASH);
                busy = true;
            }
        }
    }

    int ret = led_strip_update_rgb(glow_strip, px, GLOW_CHAIN);

    if (ret < 0) {
        LOG_WRN("strip update failed (%d)", ret);
    }

    if (busy) {
        k_work_schedule(&glow.render_work, K_MSEC(GLOW_FRAME_MS));
    }
}

static void glow_render_work_fn(struct k_work *work) {
    ARG_UNUSED(work);
    glow_render();
}

void orbit_glow_breathe(uint32_t duration_ms) {
    if (atomic_get(&glow.scene) != ZMK_ACTIVITY_ACTIVE) {
        return;
    }
    glow.breathe_until = k_uptime_get() + duration_ms;
    k_work_schedule(&glow.render_work, K_NO_WAIT);
}

static void glow_input_cb(struct input_event *evt, void *user_data) {
    ARG_UNUSED(user_data);

    if (evt->type != INPUT_EV_KEY || evt->value != 1 ||
        atomic_get(&glow.scene) != ZMK_ACTIVITY_ACTIVE) {
        return;
    }

    for (int i = 0; i < GLOW_LEN; i++) {
        if (glow_codes[i] == evt->code) {
            glow.flash_until[i] = k_uptime_get() + GLOW_FLASH_MS;
            k_work_schedule(&glow.render_work, K_NO_WAIT);
            return;
        }
    }
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_INST_PHANDLE(0, input_dev)), glow_input_cb, NULL);

static int glow_activity_listener(const zmk_event_t *eh) {
    const struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);

    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    atomic_set(&glow.scene, ev->state);

    if (ev->state == ZMK_ACTIVITY_SLEEP) {
        /* Blank NOW, in the listener: after this event ZMK powers the SoC
         * off, and a latched WS2812 frame would glow forever. */
        glow_render();
    } else {
        k_work_schedule(&glow.render_work, K_NO_WAIT);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(orbit_glow, glow_activity_listener);
ZMK_SUBSCRIPTION(orbit_glow, zmk_activity_state_changed);

static int glow_init(void) {
    k_work_init_delayable(&glow.render_work, glow_render_work_fn);
    atomic_set(&glow.scene, ZMK_ACTIVITY_ACTIVE);
    /* First frame shortly after boot, once the strip driver is up. */
    k_work_schedule(&glow.render_work, K_MSEC(50));
    return 0;
}

SYS_INIT(glow_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
