/*
 * ORBit BLE-profile feedback (spec §4 item 8): on every active-profile
 * change — however it was triggered — fire a haptic micro-tick and blink
 * the profile's index on the zone-dot LEDs ("haptic tick per step, LED0-4
 * blink index").
 *
 * The gesture itself (slider hold -> profile layer, crown spin -> BT_NXT/
 * BT_PRV, lone-pad long-hold -> BT_CLR) lives entirely in the keymap plus
 * the chord processor's long-hold injection; this file is only the
 * feedback path, listening to ZMK's own event so it also reflects
 * profile changes made any other way.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>

#ifdef CONFIG_ORBIT_GLOW
#include <orbit_glow.h>
#endif
#ifdef CONFIG_ORBIT_HAPTIC_FEEDBACK
#include <orbit_haptic.h>
#endif

LOG_MODULE_REGISTER(orbit_profile, CONFIG_ZMK_LOG_LEVEL);

/* TI ROM library effect 24 = Sharp Tick 1 (the spec's "micro-tick per
 * profile step"). Matches the slider-tap feel from item 5. */
#define PROFILE_TICK_EFFECT 24
#define PROFILE_SHOW_MS 900

static int profile_listener(const zmk_event_t *eh) {
    const struct zmk_ble_active_profile_changed *ev =
        as_zmk_ble_active_profile_changed(eh);

    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    LOG_DBG("profile -> %u", ev->index);
#ifdef CONFIG_ORBIT_HAPTIC_FEEDBACK
    orbit_haptic_play(PROFILE_TICK_EFFECT);
#endif
#ifdef CONFIG_ORBIT_GLOW
    orbit_glow_show_index(ev->index, PROFILE_SHOW_MS);
#endif

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(orbit_profile, profile_listener);
ZMK_SUBSCRIPTION(orbit_profile, zmk_ble_active_profile_changed);
