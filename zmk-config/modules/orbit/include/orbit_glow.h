/*
 * Public hooks into the ORBit glow engine (modules/orbit/src/glow.c).
 */

#pragma once

#include <zephyr/types.h>

/* Run the breathe scene for duration_ms, then fall back to the current
 * activity scene. Item 7 calls this on proximity wake to mask the 1-2 s BLE
 * reconnect (spec §4.7). Safe from any thread; no-op if glow is asleep. */
void orbit_glow_breathe(uint32_t duration_ms);

/* Light chain LED `index` alone at flash brightness for duration_ms — the
 * item-8 profile indicator ("LED0-4 blink index"). Safe from any thread;
 * no-op if glow is asleep or index is off-chain. */
void orbit_glow_show_index(uint8_t index, uint32_t duration_ms);
