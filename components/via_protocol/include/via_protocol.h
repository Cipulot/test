#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VIA_REPORT_SIZE 32u
#define VIA_USAGE_PAGE  0xFF60u
#define VIA_USAGE       0x0061u

typedef enum {
    VIA_CMD_GET_PROTOCOL_VERSION = 0x01,
    VIA_CMD_GET_KEYBOARD_VALUE   = 0x02,
    VIA_CMD_SET_KEYBOARD_VALUE   = 0x03,
    VIA_CMD_DYNAMIC_KEYMAP_GET_KEYCODE = 0x04,
    VIA_CMD_DYNAMIC_KEYMAP_SET_KEYCODE = 0x05,
    VIA_CMD_CUSTOM_SET_VALUE     = 0x07,
    VIA_CMD_CUSTOM_GET_VALUE     = 0x08,
    VIA_CMD_CUSTOM_SAVE          = 0x09,
    VIA_CMD_EEPROM_RESET         = 0x0A,
    VIA_CMD_BOOTLOADER_JUMP      = 0x0B,
    VIA_CMD_DYNAMIC_KEYMAP_MACRO_GET_COUNT = 0x0C,
    VIA_CMD_DYNAMIC_KEYMAP_MACRO_GET_BUFFER_SIZE = 0x0D,
    VIA_CMD_DYNAMIC_KEYMAP_MACRO_GET_BUFFER = 0x0E,
    VIA_CMD_DYNAMIC_KEYMAP_MACRO_SET_BUFFER = 0x0F,
    VIA_CMD_DYNAMIC_KEYMAP_MACRO_RESET = 0x10,
    VIA_CMD_DYNAMIC_KEYMAP_GET_LAYER_COUNT = 0x11,
    VIA_CMD_DYNAMIC_KEYMAP_GET_BUFFER = 0x12,
    VIA_CMD_DYNAMIC_KEYMAP_SET_BUFFER = 0x13,
    VIA_CMD_DYNAMIC_KEYMAP_GET_ENCODER = 0x14,
    VIA_CMD_DYNAMIC_KEYMAP_SET_ENCODER = 0x15,
    VIA_CMD_UNHANDLED = 0xFF
} via_command_t;

typedef struct { uint8_t bytes[VIA_REPORT_SIZE]; } via_report_t;

void via_report_clear(via_report_t *report);
void via_make_get_protocol_version(via_report_t *report);
void via_make_get_layer_count(via_report_t *report);
bool via_make_get_keycode(via_report_t *report, uint8_t layer,
                          uint8_t row, uint8_t column);
bool via_make_set_keycode(via_report_t *report, uint8_t layer,
                          uint8_t row, uint8_t column, uint16_t keycode);
bool via_make_get_keymap_buffer(via_report_t *report, uint16_t offset,
                                uint8_t length);
bool via_make_set_keymap_buffer(via_report_t *report, uint16_t offset,
                                const uint8_t *data, uint8_t length);
bool via_response_protocol_version(const via_report_t *report, uint16_t *version);
bool via_response_keycode(const via_report_t *report, uint16_t *keycode);
