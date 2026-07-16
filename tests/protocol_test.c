#include "via_protocol.h"
#include "definition_source.h"
#include "hid_console.h"
#include "hid_usage.h"
#include "via_transport.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { uint32_t now; int sends, callbacks; bool success; via_report_t sent; } fake_io_t;
static bool fake_send(void *ctx, uint8_t id, const uint8_t *data, size_t length) {
    fake_io_t *io = ctx; assert(id == 0 && length == VIA_REPORT_SIZE);
    memcpy(io->sent.bytes, data, length); io->sends++; return true;
}
static uint32_t fake_clock(void *ctx) { return ((fake_io_t *)ctx)->now; }
static void fake_done(void *ctx, bool success, const via_report_t *response) {
    fake_io_t *io = ctx; io->callbacks++; io->success = success;
    if (success) assert(response && response->bytes[0] == io->sent.bytes[0]);
}

int main(void) {
    via_report_t r;
    via_make_set_keycode(&r, 2, 3, 4, 0x1234);
    assert(r.bytes[0] == VIA_CMD_DYNAMIC_KEYMAP_SET_KEYCODE);
    assert(r.bytes[1] == 2 && r.bytes[2] == 3 && r.bytes[3] == 4);
    assert(r.bytes[4] == 0x12 && r.bytes[5] == 0x34);

    assert(!via_make_get_keymap_buffer(&r, 0, 29));
    assert(via_make_get_keymap_buffer(&r, 0x3456, 28));
    assert(r.bytes[1] == 0x34 && r.bytes[2] == 0x56 && r.bytes[3] == 28);

    memset(&r, 0, sizeof(r));
    r.bytes[0] = VIA_CMD_GET_PROTOCOL_VERSION;
    r.bytes[1] = 0; r.bytes[2] = 12;
    uint16_t version = 0;
    assert(via_response_protocol_version(&r, &version) && version == 12);

    char path[96];
    assert(definition_exact_sd_path(path, sizeof(path), "/sdcard",
                                    0xfeed, 0x6060) > 0);
    assert(strcmp(path, "/sdcard/tabvia/definitions/feed-6060.json") == 0);

    char ring[5], output[8];
    hid_console_t console;
    hid_console_init(&console, ring, sizeof(ring));
    const uint8_t report[] = {'a', 'b', 0, 'c', 'd', 'e', 'f'};
    hid_console_append_report(&console, report, sizeof(report));
    assert(hid_console_read(&console, output, sizeof(output)) == 5);
    assert(strcmp(output, "bcdef") == 0 && console.dropped == 1);

    /* Two top-level vendor collections: VIA has 32-byte in/out reports and
       QMK Console has a 32-byte input report. */
    const uint8_t descriptor[] = {
        0x06,0x60,0xff, 0x09,0x61, 0xa1,0x01,
        0x75,0x08, 0x95,0x20, 0x81,0x02, 0x91,0x02, 0xc0,
        0x06,0x31,0xff, 0x09,0x74, 0xa1,0x01,
        0x75,0x08, 0x95,0x20, 0x81,0x02, 0xc0
    };
    hid_usage_profile_t profiles[2];
    assert(hid_usage_scan_report_descriptor(descriptor, sizeof(descriptor), profiles, 2) == 2);
    assert(profiles[0].kind == HID_USAGE_VIA && profiles[0].input_bytes == 32 && profiles[0].output_bytes == 32);
    assert(profiles[1].kind == HID_USAGE_QMK_CONSOLE && profiles[1].input_bytes == 32);

    fake_io_t io = {0}; via_transport_t transport;
    via_transport_init(&transport, fake_send, fake_clock, &io, 0, 100);
    via_make_get_protocol_version(&r);
    assert(via_transport_request(&transport, &r, fake_done, &io));
    assert(!via_transport_request(&transport, &r, fake_done, &io));
    uint8_t unrelated[32] = {VIA_CMD_DYNAMIC_KEYMAP_GET_LAYER_COUNT};
    assert(!via_transport_on_input(&transport, 0, unrelated, sizeof(unrelated)));
    uint8_t reply[32] = {VIA_CMD_GET_PROTOCOL_VERSION, 0, 12};
    assert(via_transport_on_input(&transport, 0, reply, sizeof(reply)));
    assert(io.callbacks == 1 && io.success);
    assert(via_transport_request(&transport, &r, fake_done, &io));
    io.now = 100; via_transport_poll(&transport);
    assert(io.callbacks == 2 && !io.success && !transport.busy);
    puts("TabVIA protocol tests passed");
    return 0;
}
