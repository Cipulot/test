#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef enum { HID_USAGE_NONE, HID_USAGE_VIA, HID_USAGE_QMK_CONSOLE } hid_usage_kind_t;
typedef struct {
    hid_usage_kind_t kind;
    uint16_t usage_page, usage;
    uint8_t report_id;
    uint16_t input_bytes, output_bytes, feature_bytes;
} hid_usage_profile_t;
/* Parses HID short items and finds top-level application collections used by VIA.
 * Long items are skipped. Bit sizes are rounded up to bytes per report. */
size_t hid_usage_scan_report_descriptor(const uint8_t *descriptor, size_t length,
                                        hid_usage_profile_t *profiles,
                                        size_t capacity);
