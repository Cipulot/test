#pragma once
#include <stddef.h>
#include <stdint.h>
#define QMK_CONSOLE_USAGE_PAGE 0xFF31u
#define QMK_CONSOLE_USAGE 0x0074u
typedef struct { char *storage; size_t capacity, start, length, dropped; } hid_console_t;
void hid_console_init(hid_console_t *console, char *storage, size_t capacity);
void hid_console_clear(hid_console_t *console);
void hid_console_append_report(hid_console_t *console, const uint8_t *data, size_t length);
size_t hid_console_read(const hid_console_t *console, char *out, size_t out_size);
int hid_console_export(const hid_console_t *console, const char *path,
                       const char *product_name, uint16_t vendor_id, uint16_t product_id);
