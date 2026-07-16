#include "via_protocol.h"
#include <string.h>

static uint16_t read_be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

void via_report_clear(via_report_t *report) { memset(report, 0, sizeof(*report)); }

void via_make_get_protocol_version(via_report_t *report) {
    via_report_clear(report);
    report->bytes[0] = VIA_CMD_GET_PROTOCOL_VERSION;
}

void via_make_get_layer_count(via_report_t *report) {
    via_report_clear(report);
    report->bytes[0] = VIA_CMD_DYNAMIC_KEYMAP_GET_LAYER_COUNT;
}

bool via_make_get_keycode(via_report_t *report, uint8_t layer,
                          uint8_t row, uint8_t column) {
    via_report_clear(report);
    report->bytes[0] = VIA_CMD_DYNAMIC_KEYMAP_GET_KEYCODE;
    report->bytes[1] = layer; report->bytes[2] = row; report->bytes[3] = column;
    return true;
}

bool via_make_set_keycode(via_report_t *report, uint8_t layer,
                          uint8_t row, uint8_t column, uint16_t keycode) {
    via_report_clear(report);
    report->bytes[0] = VIA_CMD_DYNAMIC_KEYMAP_SET_KEYCODE;
    report->bytes[1] = layer; report->bytes[2] = row; report->bytes[3] = column;
    report->bytes[4] = (uint8_t)(keycode >> 8);
    report->bytes[5] = (uint8_t)keycode;
    return true;
}

bool via_make_get_keymap_buffer(via_report_t *report, uint16_t offset,
                                uint8_t length) {
    if (length > VIA_REPORT_SIZE - 4u) return false;
    via_report_clear(report);
    report->bytes[0] = VIA_CMD_DYNAMIC_KEYMAP_GET_BUFFER;
    report->bytes[1] = (uint8_t)(offset >> 8);
    report->bytes[2] = (uint8_t)offset;
    report->bytes[3] = length;
    return true;
}

bool via_make_set_keymap_buffer(via_report_t *report, uint16_t offset,
                                const uint8_t *data, uint8_t length) {
    if ((length && !data) || length > VIA_REPORT_SIZE - 4u) return false;
    via_report_clear(report);
    report->bytes[0] = VIA_CMD_DYNAMIC_KEYMAP_SET_BUFFER;
    report->bytes[1] = (uint8_t)(offset >> 8);
    report->bytes[2] = (uint8_t)offset;
    report->bytes[3] = length;
    if (length) memcpy(&report->bytes[4], data, length);
    return true;
}

bool via_response_protocol_version(const via_report_t *report, uint16_t *version) {
    if (!report || !version || report->bytes[0] != VIA_CMD_GET_PROTOCOL_VERSION)
        return false;
    *version = read_be16(&report->bytes[1]);
    return true;
}

bool via_response_keycode(const via_report_t *report, uint16_t *keycode) {
    if (!report || !keycode ||
        report->bytes[0] != VIA_CMD_DYNAMIC_KEYMAP_GET_KEYCODE) return false;
    *keycode = read_be16(&report->bytes[4]);
    return true;
}
