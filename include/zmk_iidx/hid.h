/*
 * Copyright (c) 2026 The zmk-iidx contributors
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/toolchain.h>

#ifdef __cplusplus
extern "C" {
#endif

enum zmk_iidx_axis {
    ZMK_IIDX_AXIS_X = 0,
    ZMK_IIDX_AXIS_Y = 1,
};

struct zmk_iidx_hid_report {
    int8_t x;
    int8_t y;
    uint8_t keys_1_7;
    uint8_t keys_e1_e4;
    uint8_t reserved;
} __packed;

/** Set or clear one of the usable HID button bit positions (0-6, 8-11). */
int zmk_iidx_hid_set_button(uint8_t bit, bool pressed);

/** Set one absolute axis. Values outside -127..127 are clamped. */
int zmk_iidx_hid_set_axis(enum zmk_iidx_axis axis, int16_t value);

/** Add a delta to one absolute axis, clamping the result to -127..127. */
int zmk_iidx_hid_adjust_axis(enum zmk_iidx_axis axis, int16_t delta);

/** Set both absolute axes. Values outside -127..127 are clamped. */
int zmk_iidx_hid_set_axes(int16_t x, int16_t y);

/** Send the current five-byte report without changing it. */
int zmk_iidx_hid_send(void);

/** Reset axes and buttons to zero, then send the neutral report. */
int zmk_iidx_hid_clear(void);

#ifdef __cplusplus
}
#endif
