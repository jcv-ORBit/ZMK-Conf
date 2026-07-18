/*
 * ORBit wake sources (spec §4 item 7).
 *
 * ZMK deep sleep is nRF System OFF: everything stops, wake happens only via
 * GPIO SENSE, and waking is a RESET (full reboot). So this module has two
 * halves:
 *
 *  - Going down (ZMK activity SLEEP event, which fires synchronously before
 *    ZMK calls poweroff): program the IMU's wake-on-motion engine and the
 *    CAP1188's cycled standby-proximity mode over I2C, then arm both
 *    interrupt pins as level wakes. The nRF GPIO driver maps level
 *    interrupts onto SENSE, which System OFF honors. Pins are armed LAST —
 *    a level interrupt with no handler would storm if motion happened in
 *    the few ms before poweroff.
 *
 *  - Coming up (any boot, since wake IS a boot): trigger the glow breathe
 *    scene to mask the 1-2 s BLE reconnect (spec: "glow breathe masks BLE
 *    reconnect").
 *
 * The IMU is handled with raw register writes (ST AN4987 wake-on-motion
 * recipe) — no sensor driver, no data path, no runtime cost while awake:
 * the chip sits in its power-down reset state until the way down. Its
 * 3 V3 rail is a GPIO-controlled regulator (P1.08, replicated from the
 * xiao_ble sense variant DTS); System OFF retains GPIO outputs, so the IMU
 * stays powered while watching for motion.
 */

#define DT_DRV_COMPAT orbit_wake

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/activity.h>

#ifdef CONFIG_ORBIT_GLOW
#include <orbit_glow.h>
#endif

LOG_MODULE_REGISTER(orbit_wake, CONFIG_ZMK_LOG_LEVEL);

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1,
             "orbit,wake supports exactly one instance");

/* LSM6DS3TR-C registers (ST AN4987 §5.1 wake-on-motion) */
#define IMU_REG_CTRL1_XL   0x10
#define IMU_CTRL1_26HZ_2G  0x20 /* ODR 26 Hz low-power, FS 2 g */
#define IMU_REG_WAKE_THS   0x5B
#define IMU_REG_WAKE_DUR   0x5C
#define IMU_REG_TAP_CFG    0x58
#define IMU_TAP_CFG_INT_EN 0x80 /* INTERRUPTS_ENABLE */
#define IMU_REG_MD1_CFG    0x5E
#define IMU_MD1_INT1_WU    0x20 /* route wake-up to INT1 */

/* CAP1188 standby registers (datasheet §6.9-6.13) */
#define CAP_REG_MAIN       0x00
#define CAP_MAIN_STBY      BIT(5)
#define CAP_REG_STBY_CHAN  0x40
#define CAP_STBY_CS7_CS8   0xC0 /* gang the slider pads as proximity antenna */
#define CAP_REG_STBY_SENS  0x42
#define CAP_REG_STBY_THR   0x43

static const struct i2c_dt_spec wake_imu = I2C_DT_SPEC_INST_GET(0);
static const struct i2c_dt_spec wake_cap = I2C_DT_SPEC_GET(DT_INST_PHANDLE(0, touch));
static const struct gpio_dt_spec wake_imu_int =
    GPIO_DT_SPEC_INST_GET(0, imu_int_gpios);
static const struct gpio_dt_spec wake_cap_alert =
    GPIO_DT_SPEC_GET(DT_INST_PHANDLE(0, touch), int_gpios);

static void wake_arm(void) {
    int ret;

    /* IMU wake-on-motion. Each write logged only on failure — this path
     * runs once, right before poweroff. */
    ret = i2c_reg_write_byte_dt(&wake_imu, IMU_REG_CTRL1_XL, IMU_CTRL1_26HZ_2G);
    ret |= i2c_reg_write_byte_dt(&wake_imu, IMU_REG_WAKE_DUR, 0x00);
    ret |= i2c_reg_write_byte_dt(&wake_imu, IMU_REG_WAKE_THS,
                                 DT_INST_PROP(0, wake_threshold));
    ret |= i2c_reg_write_byte_dt(&wake_imu, IMU_REG_TAP_CFG, IMU_TAP_CFG_INT_EN);
    ret |= i2c_reg_write_byte_dt(&wake_imu, IMU_REG_MD1_CFG, IMU_MD1_INT1_WU);
    if (ret != 0) {
        LOG_WRN("IMU wake-on-motion config failed — motion wake unavailable");
    }

    /* CAP1188 standby proximity on the ganged slider pads. */
    ret = i2c_reg_write_byte_dt(&wake_cap, CAP_REG_STBY_CHAN, CAP_STBY_CS7_CS8);
    ret |= i2c_reg_write_byte_dt(&wake_cap, CAP_REG_STBY_SENS,
                                 DT_INST_PROP(0, proximity_sensitivity));
    ret |= i2c_reg_write_byte_dt(&wake_cap, CAP_REG_STBY_THR,
                                 DT_INST_PROP(0, proximity_threshold));
    ret |= i2c_reg_update_byte_dt(&wake_cap, CAP_REG_MAIN, CAP_MAIN_STBY,
                                  CAP_MAIN_STBY);
    if (ret != 0) {
        LOG_WRN("CAP1188 standby config failed — proximity wake unavailable");
    }

    /* Arm the wake pins LAST (see header comment). Level interrupts become
     * nRF SENSE, which is what System OFF wakes on. */
    gpio_pin_configure_dt(&wake_imu_int, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&wake_imu_int, GPIO_INT_LEVEL_ACTIVE);
    gpio_pin_interrupt_configure_dt(&wake_cap_alert, GPIO_INT_LEVEL_ACTIVE);
}

static int wake_activity_listener(const zmk_event_t *eh) {
    const struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);

    if (ev != NULL && ev->state == ZMK_ACTIVITY_SLEEP) {
        wake_arm();
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(orbit_wake, wake_activity_listener);
ZMK_SUBSCRIPTION(orbit_wake, zmk_activity_state_changed);

#ifdef CONFIG_ORBIT_GLOW
static void wake_breathe_work_fn(struct k_work *work) {
    ARG_UNUSED(work);
    orbit_glow_breathe(DT_INST_PROP(0, breathe_ms));
}

static K_WORK_DELAYABLE_DEFINE(wake_breathe_work, wake_breathe_work_fn);
#endif

static int wake_init(void) {
#ifdef CONFIG_ORBIT_GLOW
    /* Every boot is potentially a proximity/motion wake (System OFF wake =
     * reset); breathe while BLE reconnects. */
    k_work_schedule(&wake_breathe_work, K_MSEC(300));
#endif
    return 0;
}

SYS_INIT(wake_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
