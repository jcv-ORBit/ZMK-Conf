/*
 * Public hooks into the ORBit glow engine (modules/orbit/src/glow.c).
 */

#pragma once

#include <zephyr/types.h>

/* Run the breathe scene for duration_ms, then fall back to the current
 * activity scene. Item 7 calls this on proximity wake to mask the 1-2 s BLE
 * reconnect (spec §4.7). Safe from any thread; no-op if glow is asleep. */
void orbit_glow_breathe(uint32_t duration_ms);
