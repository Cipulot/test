# VIA parity tracker

Reference snapshot: `the-via/app` at commit
`6463e399a4f33d932068c326da5d3ca22cd24193` (2026-07-16).

| Area | VIA reference | TabVIA phase | Status |
| --- | --- | --- | --- |
| Device authorization/discovery | WebHID chooser | 1 | HID descriptor discovery and ESP adapter implemented; hardware test pending |
| Automatic definition lookup | VIA definitions API | 1 | Source policy specified |
| Manual definition import | Design tab JSON import | 1 | SD catalog and validator implemented; LVGL pending |
| Keymap read/write | Configure / Keymap | 1 | Packet codec implemented |
| Layers | Layer selector | 1 | Packet codec implemented |
| Keycode categories/search | Keycode pane | 2 | Pending |
| Layout options | Configure / Layouts | 2 | Packet constants ready |
| Save/load keymap | Configure / Save + Load | 2 | Pending |
| Macros v10/v11 | Configure / Macros | 3 | Pending |
| Lighting | Configure / Lighting | 3 | Packet constants ready |
| Custom menus | Definition menus | 4 | Pending |
| Key tester | Key Tester | 4 | Pending |
| Design/debug tools | Design | 5 | Manual SD import arrives in phase 1 |
| QMK HID Console | HID Console | 2 | Filtering, ESP adapter, touch screen, ring buffer and SD export implemented; hardware test pending |
| Settings/theme/language | Settings | 5 | Pending |

“Parity” means the same keyboard operation and compatible definition semantics.
Touch layout may differ where desktop controls would be too small.

## Phase 1 acceptance

- A keyboard connected to USB-A is detected without stealing its boot interface.
- TabVIA reads protocol version, layer count and full keymap.
- A matching definition can be loaded from microSD with Wi-Fi disabled.
- The rendered keyboard supports selecting a key and assigning a basic keycode.
- Reconnecting the keyboard shows the persisted change.
