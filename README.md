# TabVIA

TabVIA is a native, touch-first VIA configurator for the M5Stack Tab5. It talks
to QMK/VIA keyboards through the Tab5 USB-A host port; it does not embed a web
browser or require WebHID.

## Status

This repository is an initial architecture and compilable protocol test slice.
The VIA packet codec, device/definition matching model, source selection policy,
and parity plan are present. Hardware USB, networking, SD mounting, and LVGL
screens are intentionally represented by narrow interfaces ready for the Tab5
BSP integration.

## Definition sources

Definitions are resolved in this order:

1. `/sdcard/tabvia/definitions/<vendorId>-<productId>.json`
2. cached web definition under `/sdcard/tabvia/cache/`
3. the published `the-via/keyboards` compatible web index

The local filename uses four-digit lowercase hexadecimal IDs, for example
`feed-6060.json`. Users can also browse and import any JSON file from the SD
card. A definition is accepted only when its vendor/product IDs match the
connected device, unless the user explicitly confirms a manual import.

The Design workflow has a V2/V3 selector like VIA. Validation accepts source
keyboard definitions and transformed VIA definitions and reports JSON instance
paths. QMK HID Console (`0xFF31/0x74`) uses a bounded log buffer and can export
text logs to microSD.

## Target toolchain

- ESP-IDF 5.4.2
- `espressif/m5stack_tab5` BSP
- LVGL 9 through `esp_lvgl_port`
- USB Host Library with a vendor-defined HID transport
- `espressif/usb_host_hid` 1.2

The official M5Tab5 UserDemo is the board bring-up reference. Do not copy its
entire application into this project; keep board code behind `platform/`.

The vendored `m5stack_tab5` and `esp_lcd_st7121` components are from M5Stack
UserDemo commit `68b19d37fbf9cefd5f256992f5dca34794c62ab4`. This revision detects and
initializes ST7123/ST7121 panels as well as the original Tab5 panel.

## Build the host tests

```sh
cmake -S tests -B build/tests
cmake --build build/tests
ctest --test-dir build/tests --output-on-failure
```

## Firmware build

```sh
idf.py set-target esp32p4
idf.py build
idf.py flash monitor
```

The launcher-oriented artifact is `build/tabvia.bin`, an ESP-IDF application
image for ESP32-P4. It is not a merged full-flash image: a firmware launcher
should place it in one of its application/OTA slots. The current image fits in
a 1 MiB app slot. Launcher-specific manifests, thumbnails, and slot limits can
be added once the launcher name and version are known.

The display path uses the current M5Stack UserDemo detection logic and supports
the updated Tab5 fitted with an ST7123 panel and touch controller.

## Repository references

- UI/feature reference: <https://github.com/the-via/app>
- Published definitions: <https://github.com/the-via/keyboards>
- Tab5 reference firmware: <https://github.com/m5stack/M5Tab5-UserDemo>

See `docs/PARITY.md`, `docs/ARCHITECTURE.md`, and `docs/VALIDATION.md` before
implementing another feature.

## Current hardware milestone

The project includes a concrete Espressif HID-host adapter and first LVGL
Configure, Design, and HID Console shell. Descriptor parsing and transport are
covered by host tests; flashing on a physical updated Tab5 is still required to
confirm board USB power, BSP initialization, and report behavior with real VIA
keyboards.

### Cyan-screen correction

The first Launcher build allocated the 64-entry SD definition catalog on the
startup task stack. On hardware this could overflow before LVGL's first frame,
leaving the ST7123 panel's cyan initialization buffer visible. The catalog is
now allocated in PSRAM. The startup task has an explicit 8 KB stack and yields
to the LVGL task before SD/USB startup. PSRAM execute-in-place remains disabled
for compatibility with Launcher's retained boot environment. The ST7123 DSI
framebuffer uses M5Stack's 200 MHz PSRAM and 256 KB/128-byte L2 cache settings.

When installed as an application image by Launcher 2.7.2, Launcher's retained
bootloader initializes HEX PSRAM at 20 MHz. The Launcher build therefore uses a
15 MHz ST7123 DPI pixel clock (roughly 12 fps) to prevent external-framebuffer
underruns. A standalone full-flash build can retain M5Stack's 70 MHz timing when
paired with its 200 MHz PSRAM bootloader configuration.
