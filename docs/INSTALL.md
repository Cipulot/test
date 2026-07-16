# Installing TabVIA

`tabvia.bin` is an ESP-IDF ESP32-P4 application image intended for bmorcelli
Launcher 2.7.2. Import it as an app image, not as a full-flash backup. Its
SHA-256 is supplied alongside each release artifact. The image header declares
the Tab5's 16 MB flash size.

The build detects the Tab5 display controller at startup. ST7123 is supported
by the vendored M5Stack board code; no alternate display binary is required.

Do not write the application image directly at offset zero. For a conventional
ESP-IDF partition table it belongs at the selected app partition offset (the
standalone development table uses `0x10000`). A launcher controls this offset,
so follow its normal import/install workflow.

For Launcher 2.7.2, copy the file to a FAT32/MBR microSD card, open `SD`, select
the binary, and choose `Install`. Launcher validates the ESP32-P4 image and
selects or creates an app partition. The same binary may instead be uploaded
through Launcher's WebUI.

Physical-device validation remains necessary before treating this milestone as
production firmware. In particular, verify USB-A host power, keyboard attach,
touch coordinates, SD mounting, and VIA request/response traffic.
