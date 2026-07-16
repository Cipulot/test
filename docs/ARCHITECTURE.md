# Architecture

## Runtime flow

1. Initialize the Tab5 BSP, display, touch, SD card, Wi-Fi and USB Host.
2. Enumerate HID interfaces and select usage page `0xFF60`, usage `0x61`.
3. Read VIA protocol version and keyboard UID where supported.
4. Resolve a matching V3 definition from SD, cache, or web.
5. Render the physical layout from `layouts.keymap` and query the active layer.
6. Serialize UI operations through one 32-byte request queue.

## Components

- `via_protocol`: allocation-free encoding/decoding of VIA reports.
- `via_transport`: one request at a time over a USB HID interface, with timeout.
- `definitions`: V2/V3 parsing, validation, matching, cache and source policy.
- `platform`: Tab5 BSP, USB, Wi-Fi and SD adapters.
- `ui`: LVGL screens and VIA-inspired theme.

The protocol layer must never depend on LVGL, USB Host, cJSON, or the BSP. This
keeps packet behavior host-testable and makes recorded-traffic tests possible.

## USB constraints

The keyboard may expose boot keyboard, console, mouse and raw-HID interfaces.
TabVIA must claim only the VIA interface. The USB Host client owns transfers;
callbacks copy completed reports into the protocol task queue and do not touch
LVGL. All LVGL mutation happens while holding the LVGL port lock.

QMK Console is a separate HID collection at usage page `0xFF31`, usage `0x74`.
Its NUL-padded reports feed a bounded PSRAM ring buffer and can be exported to
microSD. Console reports never enter the VIA request/reply queue.

`hid_usage` parses each interface's report descriptor instead of trusting the
USB interface subclass or protocol. Boot keyboard and mouse interfaces are
left unopened. `platform_tab5` targets `espressif/usb_host_hid` 1.2 and sends
VIA requests using HID `Set_Report`; incoming raw reports are routed by the
discovered application usage.

`via_transport` permits one outstanding request, matches replies by report ID
and command byte, ignores unrelated input, handles 32-bit clock wrap naturally,
and reports disconnects/timeouts through the same completion callback.

## Definition format

The parser retains the complete JSON file on SD and builds a compact runtime
model in PSRAM: identity, matrix, layout keys, labels, menus and lighting/custom
features. Unknown fields are preserved on disk but ignored at runtime. V2
definitions are normalized into the same model.

The Design screen has an explicit V2/V3 selector, matching VIA. It accepts both
source keyboard definitions (`vendorId` + `productId`) and already-transformed
VIA definitions (`vendorProductId`). Validation errors retain reader-style JSON
instance paths so they can be shown beside the selected file.

Remote indexes change independently of firmware. Fetch over TLS, write to a
temporary cache file, validate JSON and IDs, then atomically rename. A failed
refresh must never destroy the last valid cache.

## UI direction

Match VIA's information architecture and recognizable visual language: dark
canvas, purple accent, centered keyboard, left configure menu, layer strip and
bottom keycode palette. Adapt density and hit targets for a 1280x720 touch
screen rather than reproducing desktop CSS pixel-for-pixel.
