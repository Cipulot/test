#include "hid_usage.h"
#include "hid_console.h"
#include "via_protocol.h"
#include <string.h>
typedef struct { uint32_t page, usage, report_size, report_count; uint8_t report_id; } state_t;
static uint32_t value_le(const uint8_t *p, size_t n) {
    uint32_t v = 0; for (size_t i = 0; i < n; i++) v |= (uint32_t)p[i] << (8 * i); return v;
}
static hid_usage_kind_t kind(uint32_t page, uint32_t usage) {
    if (page == VIA_USAGE_PAGE && usage == VIA_USAGE) return HID_USAGE_VIA;
    if (page == QMK_CONSOLE_USAGE_PAGE && usage == QMK_CONSOLE_USAGE) return HID_USAGE_QMK_CONSOLE;
    return HID_USAGE_NONE;
}
size_t hid_usage_scan_report_descriptor(const uint8_t *d, size_t n,
                                        hid_usage_profile_t *out, size_t cap) {
    if (!d || (!out && cap)) return 0;
    state_t s = {0}; hid_usage_profile_t current = {0}; unsigned depth = 0;
    size_t found = 0;
    for (size_t i = 0; i < n;) {
        uint8_t prefix = d[i++];
        if (prefix == 0xfe) { if (i + 2 > n) break; size_t z = d[i]; i += 2 + z; if (i > n) break; continue; }
        size_t size = prefix & 3u; if (size == 3) size = 4;
        uint8_t type = (prefix >> 2) & 3u, tag = prefix >> 4;
        if (i + size > n) break;
        uint32_t v = value_le(&d[i], size);
        i += size;
        if (type == 1) {
            if (tag == 0) s.page = v; else if (tag == 7) s.report_size = v;
            else if (tag == 8) s.report_id = (uint8_t)v; else if (tag == 9) s.report_count = v;
        } else if (type == 2 && tag == 0) s.usage = v;
        else if (type == 0 && tag == 10) {
            depth++;
            if (depth == 1) {
                memset(&current, 0, sizeof(current)); current.kind = kind(s.page, s.usage);
                current.usage_page = (uint16_t)s.page; current.usage = (uint16_t)s.usage;
            }
        } else if (type == 0 && tag == 12) {
            if (depth == 1 && current.kind != HID_USAGE_NONE) {
                if (found < cap) out[found] = current;
                found++;
            } if (depth) depth--;
        } else if (type == 0 && depth && current.kind != HID_USAGE_NONE &&
                   (tag == 8 || tag == 9 || tag == 11)) {
            uint16_t bytes = (uint16_t)((s.report_size * s.report_count + 7u) / 8u);
            current.report_id = s.report_id;
            if (tag == 8) current.input_bytes = bytes;
            else if (tag == 9) current.output_bytes = bytes;
            else current.feature_bytes = bytes;
        }
    }
    return found;
}
