/*
 * Minimal copy of ZMK's input processor driver API, for building an input
 * processor OUTSIDE the zmk app target. ZMK's own header
 * (app/include/drivers/input_processor.h) is target_include_directories(app
 * PRIVATE ...), so external modules cannot include it.
 *
 * Copied verbatim (minus the syscall plumbing, which only the caller side —
 * ZMK's input listener — needs) from:
 *   zmk @ 83bc82dbfdcda4ef392461202534a7cc337ca055
 *   app/include/drivers/input_processor.h  (MIT, (c) 2024 The ZMK Contributors)
 *
 * The struct layouts here MUST match that revision: ZMK invokes our device
 * through dev->api, so this is an ABI contract. Re-diff this file against the
 * ZMK header whenever the zmk pin in west.yml is bumped.
 */

#pragma once

#include <zephyr/types.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>

#define ZMK_INPUT_PROC_CONTINUE 0
#define ZMK_INPUT_PROC_STOP 1

struct zmk_input_processor_state {
    uint8_t input_device_index;
    int16_t *remainder;
};

typedef int (*zmk_input_processor_handle_event_callback_t)(const struct device *dev,
                                                           struct input_event *event,
                                                           uint32_t param1, uint32_t param2,
                                                           struct zmk_input_processor_state *state);

struct zmk_input_processor_driver_api {
    zmk_input_processor_handle_event_callback_t handle_event;
};
