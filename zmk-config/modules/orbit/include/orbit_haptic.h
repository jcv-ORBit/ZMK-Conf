/*
 * Public hook into the ORBit haptic glue (modules/orbit/src/haptic_feedback.c).
 */

#pragma once

#include <zephyr/types.h>

/* Play one DRV2605 ROM library effect (TI datasheet table 11.2). Used by the
 * item-8 profile indicator for the per-step tick. Safe from any thread;
 * no-op until the boot calibration path has finished. */
void orbit_haptic_play(uint16_t effect);
