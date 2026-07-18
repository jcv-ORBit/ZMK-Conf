/*
 * ORBit haptic feedback glue (spec §4 item 5).
 *
 * Playback goes through the stock Zephyr ti,drv2605 haptics driver: on each
 * mapped EV_KEY press, load the ROM effect into the waveform sequencer and
 * set GO. That path is a handful of 400 kHz I2C register writes on the input
 * dispatch thread — well inside the spec's <10 ms touch-to-haptic budget.
 *
 * Auto-calibration is ours: the driver's public API rejects
 * DRV2605_MODE_AUTO_CAL as a config trigger (drv2605_haptic_config_rom
 * returns -EINVAL), so calibration talks to the same chip directly through
 * its own i2c_dt_spec. Boot flow, per the spec ("auto-cal at boot, persist"):
 *
 *   - stored result in settings ("orbit/lracal")? restore the three cal
 *     registers, switch to closed loop, done — no motor buzz.
 *   - nothing stored? run the chip's auto-cal routine (MODE=0x07 + GO, poll
 *     GO from the system workqueue in 100 ms slices), then persist the
 *     compensation/back-EMF/gain results.
 *
 * The Zephyr driver deliberately forces LRA OPEN loop (CONTROL3 bit 0) so an
 * uncalibrated chip still works. Open loop needs no cal but drives the LRA
 * at a fixed period; closed loop tracks resonance and is what makes an LRA
 * feel crisp. So: cal success/restore => clear the open-loop bit; cal
 * failure/timeout => keep the driver's open-loop fallback and still play
 * effects. Either way `ready` flips on and touches produce feedback.
 */

#define DT_DRV_COMPAT orbit_haptic_feedback

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/input/input.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/haptics.h>
#include <zephyr/drivers/haptics/drv2605.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(orbit_haptics, CONFIG_HAPTICS_LOG_LEVEL);

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1,
             "orbit,haptic-feedback supports exactly one instance");

/* DRV2605L registers touched directly for calibration (not exposed by the
 * Zephyr driver). Names per the TI datasheet register map. */
#define HF_REG_STATUS        0x00
#define HF_STATUS_DIAG_FAIL  BIT(3) /* 0 = last diag/cal passed */
#define HF_REG_MODE          0x01
#define HF_MODE_AUTO_CAL     0x07   /* also leaves STANDBY clear */
#define HF_MODE_INT_TRIG     0x00
#define HF_REG_GO            0x0C
#define HF_GO                BIT(0)
#define HF_REG_ACAL_COMP     0x18
#define HF_REG_ACAL_BEMF     0x19
#define HF_REG_FB_CTRL       0x1A
#define HF_FB_BEMF_GAIN      GENMASK(1, 0) /* cal result lands here */
#define HF_REG_CONTROL1      0x1B
#define HF_CTRL1_DRIVE_TIME  GENMASK(4, 0)
#define HF_REG_CONTROL3      0x1D
#define HF_CTRL3_LRA_OPEN_LOOP BIT(0)

#define HF_SETTINGS_KEY      "orbit/lracal"
#define HF_CAL_POLL_MS       100
#define HF_CAL_POLL_MAX      20 /* 2 s >> the ~1 s worst-case LRA cal */

#define HF_LEN DT_INST_PROP_LEN(0, codes)

enum hf_phase {
    HF_PHASE_START,
    HF_PHASE_POLL,
};

static const uint16_t hf_codes[HF_LEN] = DT_INST_PROP(0, codes);
static const uint16_t hf_effects[HF_LEN] = DT_INST_PROP(0, effects);
BUILD_ASSERT(DT_INST_PROP_LEN(0, codes) == DT_INST_PROP_LEN(0, effects),
             "codes and effects must be the same length");

static const struct device *const hf_haptics = DEVICE_DT_GET(DT_INST_PHANDLE(0, haptics));
static const struct i2c_dt_spec hf_chip = I2C_DT_SPEC_GET(DT_INST_PHANDLE(0, haptics));

static struct {
    struct k_work_delayable cal_work;
    int64_t last_fire[HF_LEN];
    enum hf_phase phase;
    int polls;
    bool have_saved;
    uint8_t saved[3]; /* ACAL_COMP, ACAL_BEMF, FB_CTRL(BEMF_GAIN bits) */
    atomic_t ready;
} hf_data;

/* --- settings: restore persisted cal results ---------------------------- */

static int hf_settings_set(const char *name, size_t len, settings_read_cb read_cb,
                           void *cb_arg) {
    const char *next;

    if (settings_name_steq(name, "lracal", &next) && !next) {
        if (len != sizeof(hf_data.saved)) {
            return -EINVAL;
        }
        if (read_cb(cb_arg, hf_data.saved, sizeof(hf_data.saved)) < 0) {
            return -EIO;
        }
        hf_data.have_saved = true;
        return 0;
    }

    return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(orbit, "orbit", NULL, hf_settings_set, NULL, NULL);

/* --- boot-time calibration state machine (system workqueue) ------------- */

static void hf_cal_apply(const uint8_t cal[3]) {
    i2c_reg_write_byte_dt(&hf_chip, HF_REG_ACAL_COMP, cal[0]);
    i2c_reg_write_byte_dt(&hf_chip, HF_REG_ACAL_BEMF, cal[1]);
    i2c_reg_update_byte_dt(&hf_chip, HF_REG_FB_CTRL, HF_FB_BEMF_GAIN,
                           cal[2] & HF_FB_BEMF_GAIN);
    /* Calibrated: closed loop tracks LRA resonance from here on. */
    i2c_reg_update_byte_dt(&hf_chip, HF_REG_CONTROL3, HF_CTRL3_LRA_OPEN_LOOP, 0);
}

static void hf_cal_work_fn(struct k_work *work) {
    switch (hf_data.phase) {
    case HF_PHASE_START:
        /* ZMK loads settings itself, but do a targeted load so this works
         * regardless of ordering; both calls are idempotent. */
        settings_subsys_init();
        settings_load_subtree("orbit");

        if (hf_data.have_saved) {
            hf_cal_apply(hf_data.saved);
            atomic_set(&hf_data.ready, 1);
            LOG_INF("LRA cal restored (comp=%02x bemf=%02x fb=%02x)",
                    hf_data.saved[0], hf_data.saved[1], hf_data.saved[2]);
            return;
        }

        if (DT_INST_PROP(0, lra_drive_time) <= 31) {
            i2c_reg_update_byte_dt(&hf_chip, HF_REG_CONTROL1, HF_CTRL1_DRIVE_TIME,
                                   DT_INST_PROP(0, lra_drive_time) & HF_CTRL1_DRIVE_TIME);
        }

        /* First boot: run the chip's auto-cal (buzzes the LRA up to ~1 s).
         * The driver API refuses MODE_AUTO_CAL, hence raw register access. */
        i2c_reg_write_byte_dt(&hf_chip, HF_REG_MODE, HF_MODE_AUTO_CAL);
        i2c_reg_write_byte_dt(&hf_chip, HF_REG_GO, HF_GO);
        hf_data.phase = HF_PHASE_POLL;
        hf_data.polls = 0;
        k_work_schedule(&hf_data.cal_work, K_MSEC(HF_CAL_POLL_MS));
        return;

    case HF_PHASE_POLL: {
        uint8_t go = HF_GO;
        int ret = i2c_reg_read_byte_dt(&hf_chip, HF_REG_GO, &go);

        if (ret == 0 && (go & HF_GO) == 0) {
            uint8_t status = 0xff;

            i2c_reg_read_byte_dt(&hf_chip, HF_REG_STATUS, &status);
            i2c_reg_write_byte_dt(&hf_chip, HF_REG_MODE, HF_MODE_INT_TRIG);

            if ((status & HF_STATUS_DIAG_FAIL) == 0) {
                uint8_t cal[3];

                i2c_reg_read_byte_dt(&hf_chip, HF_REG_ACAL_COMP, &cal[0]);
                i2c_reg_read_byte_dt(&hf_chip, HF_REG_ACAL_BEMF, &cal[1]);
                i2c_reg_read_byte_dt(&hf_chip, HF_REG_FB_CTRL, &cal[2]);
                settings_save_one(HF_SETTINGS_KEY, cal, sizeof(cal));
                hf_cal_apply(cal);
                LOG_INF("LRA auto-cal OK (comp=%02x bemf=%02x fb=%02x), persisted",
                        cal[0], cal[1], cal[2]);
            } else {
                LOG_WRN("LRA auto-cal FAILED (status=%02x): check LRA wiring; "
                        "staying in open-loop fallback", status);
            }
            atomic_set(&hf_data.ready, 1);
            return;
        }

        if (++hf_data.polls > HF_CAL_POLL_MAX) {
            i2c_reg_write_byte_dt(&hf_chip, HF_REG_MODE, HF_MODE_INT_TRIG);
            LOG_WRN("LRA auto-cal timed out; staying in open-loop fallback");
            atomic_set(&hf_data.ready, 1);
            return;
        }
        k_work_schedule(&hf_data.cal_work, K_MSEC(HF_CAL_POLL_MS));
        return;
    }
    }
}

/* --- per-press effect playback (input dispatch context) ----------------- */

static void hf_input_cb(struct input_event *evt, void *user_data) {
    ARG_UNUSED(user_data);

    if (evt->type != INPUT_EV_KEY || evt->value != 1 || !atomic_get(&hf_data.ready)) {
        return;
    }

    for (size_t i = 0; i < HF_LEN; i++) {
        if (hf_codes[i] != evt->code) {
            continue;
        }

        /* One buzz per touch: the chord processor re-injects held-back
         * CS7/CS8 presses, so the same code can legitimately appear twice
         * within its window. */
        int64_t now = k_uptime_get();

        if (now - hf_data.last_fire[i] < DT_INST_PROP(0, dedup_ms)) {
            return;
        }
        hf_data.last_fire[i] = now;

        struct drv2605_rom_data rom = {
            .trigger = DRV2605_MODE_INTERNAL_TRIGGER,
            .library = DRV2605_LIBRARY_LRA,
            .seq_regs = {(uint8_t)hf_effects[i], 0},
        };
        const union drv2605_config_data cfg = {.rom_data = &rom};

        if (drv2605_haptic_config(hf_haptics, DRV2605_HAPTICS_SOURCE_ROM, &cfg) == 0) {
            haptics_start_output(hf_haptics);
        }
        return;
    }
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_INST_PHANDLE(0, input_dev)), hf_input_cb, NULL);

static int hf_init(void) {
    k_work_init_delayable(&hf_data.cal_work, hf_cal_work_fn);
    k_work_schedule(&hf_data.cal_work, K_MSEC(DT_INST_PROP(0, boot_cal_delay_ms)));
    return 0;
}

SYS_INIT(hf_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
