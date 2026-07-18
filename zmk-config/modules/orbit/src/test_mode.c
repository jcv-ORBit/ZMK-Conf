/*
 * ORBit manufacturing test mode (spec §6): a hidden build target that
 * exercises every subsystem so the HARDWARE_VALIDATION.md checklist can be
 * run without a host-side harness. Enabled only by building with
 * vessel_testmode.conf (CONFIG_ORBIT_TEST_MODE) — never in the normal
 * firmware.
 *
 * What it does, all reported over the USB CDC console (ZMK_USB_LOGGING):
 *   - LED walk: each chain LED in turn, 1 s apart, via the glow engine's
 *     index API — proves chain order, color and every LED.
 *   - Haptic sweep: steps through the DRV2605 ROM library one effect per
 *     LED-walk lap — proves the LRA and lets a human pick effect IDs.
 *   - Sensor dumps: every touch/trackball input event is logged (trackball
 *     deltas summarized once per tick so the console stays readable), and
 *     the IMU is polled for raw accel each tick.
 *
 * Tone (I2S speaker) is deliberately absent: audio is Phase-2 scope
 * (spec §5) and no amp path exists in the v1 firmware to exercise.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/input/input.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/logging/log.h>

#ifdef CONFIG_ORBIT_GLOW
#include <orbit_glow.h>
#endif
#ifdef CONFIG_ORBIT_HAPTIC_FEEDBACK
#include <orbit_haptic.h>
#endif

LOG_MODULE_REGISTER(orbit_test, LOG_LEVEL_INF);

#define TICK_MS 1000
#define CHAIN_LEN 8
#define EFFECT_MAX 123 /* TI ROM library size */

/* Raw accel poll via the IMU address the wake module owns (same chip). */
#define IMU_NODE DT_NODELABEL(wake)
#define IMU_REG_CTRL1_XL 0x10
#define IMU_REG_OUTX_L_XL 0x28

static struct k_work_delayable tick_work;
static atomic_t rel_x_accum;
static atomic_t rel_y_accum;
static unsigned int step;

#if DT_NODE_EXISTS(IMU_NODE)
static const struct i2c_dt_spec imu = I2C_DT_SPEC_GET(IMU_NODE);
#endif

static void test_input_cb(struct input_event *evt, void *user_data) {
    ARG_UNUSED(user_data);

    if (evt->type == INPUT_EV_KEY) {
        LOG_INF("TOUCH code=%u %s", evt->code, evt->value ? "press" : "release");
    } else if (evt->type == INPUT_EV_REL) {
        /* Trackball floods; accumulate and report from the tick. */
        if (evt->code == INPUT_REL_X) {
            atomic_add(&rel_x_accum, evt->value);
        } else if (evt->code == INPUT_REL_Y) {
            atomic_add(&rel_y_accum, evt->value);
        }
    }
}

/* NULL device = every input device (touch chip and trackball). */
INPUT_CALLBACK_DEFINE(NULL, test_input_cb, NULL);

static void test_tick(struct k_work *work) {
    ARG_UNUSED(work);

#ifdef CONFIG_ORBIT_GLOW
    LOG_INF("LED walk: index %u", step % CHAIN_LEN);
    orbit_glow_show_index(step % CHAIN_LEN, TICK_MS - 100);
#endif

#ifdef CONFIG_ORBIT_HAPTIC_FEEDBACK
    if (step % CHAIN_LEN == 0) {
        unsigned int effect = 1 + (step / CHAIN_LEN) % EFFECT_MAX;

        LOG_INF("HAPTIC effect %u", effect);
        orbit_haptic_play(effect);
    }
#endif

    int32_t dx = atomic_set(&rel_x_accum, 0);
    int32_t dy = atomic_set(&rel_y_accum, 0);

    if (dx != 0 || dy != 0) {
        LOG_INF("TRACKBALL dx=%d dy=%d (last %d ms)", dx, dy, TICK_MS);
    }

#if DT_NODE_EXISTS(IMU_NODE)
    uint8_t raw[6];

    if (i2c_burst_read_dt(&imu, IMU_REG_OUTX_L_XL, raw, sizeof(raw)) == 0) {
        int16_t ax = (int16_t)((raw[1] << 8) | raw[0]);
        int16_t ay = (int16_t)((raw[3] << 8) | raw[2]);
        int16_t az = (int16_t)((raw[5] << 8) | raw[4]);

        LOG_INF("IMU accel raw x=%d y=%d z=%d", ax, ay, az);
    } else {
        LOG_WRN("IMU read failed");
    }
#endif

    step++;
    k_work_schedule(&tick_work, K_MSEC(TICK_MS));
}

static int test_mode_init(void) {
    LOG_INF("=== ORBit MANUFACTURING TEST MODE (spec §6) ===");
    LOG_INF("LED walk 1 Hz; haptic sweep 1 effect/lap; touch/trackball/IMU dumps");

#if DT_NODE_EXISTS(IMU_NODE)
    /* Wake the accelerometer for the raw polls (26 Hz low-power, 2 g). */
    i2c_reg_write_byte_dt(&imu, IMU_REG_CTRL1_XL, 0x20);
#endif

    k_work_init_delayable(&tick_work, test_tick);
    k_work_schedule(&tick_work, K_SECONDS(3)); /* let USB CDC enumerate */
    return 0;
}

SYS_INIT(test_mode_init, APPLICATION, 99);
