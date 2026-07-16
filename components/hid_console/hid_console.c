#include "hid_console.h"
#include <stdio.h>
void hid_console_init(hid_console_t *c, char *storage, size_t capacity) {
    *c = (hid_console_t){.storage = storage, .capacity = capacity};
}
void hid_console_clear(hid_console_t *c) { c->start = c->length = c->dropped = 0; }
static void push(hid_console_t *c, char value) {
    if (!c->capacity) return;
    if (c->length == c->capacity) { c->start = (c->start + 1) % c->capacity; c->dropped++; }
    else c->length++;
    c->storage[(c->start + c->length - 1) % c->capacity] = value;
}
void hid_console_append_report(hid_console_t *c, const uint8_t *data, size_t n) {
    if (!c || (!data && n)) return;
    for (size_t i = 0; i < n; i++) if (data[i]) push(c, (char)data[i]);
}
size_t hid_console_read(const hid_console_t *c, char *out, size_t out_size) {
    if (!c || !out || !out_size) return 0;
    size_t n = c->length < out_size - 1 ? c->length : out_size - 1;
    size_t skip = c->length - n;
    for (size_t i = 0; i < n; i++) out[i] = c->storage[(c->start + skip + i) % c->capacity];
    out[n] = 0; return n;
}
int hid_console_export(const hid_console_t *c, const char *path,
                       const char *name, uint16_t vid, uint16_t pid) {
    FILE *f = fopen(path, "wb"); if (!f) return -1;
    fprintf(f, "HID Console Log\nBoard: %s\nVendor ID: 0x%04X\nProduct ID: 0x%04X\nDevice ID: 0x%04X:0x%04X\n\n--- Console Output ---\n\n",
            name ? name : "Unnamed device", vid, pid, vid, pid);
    for (size_t i = 0; i < c->length; i++) fputc(c->storage[(c->start + i) % c->capacity], f);
    return fclose(f) ? -2 : 0;
}
