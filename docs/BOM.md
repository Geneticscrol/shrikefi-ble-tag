# Bill of Materials

## Required

| Qty | Item | Notes | Approx. cost |
|---:|---|---|---|
| 1 | **Vicharak ShrikeFi** (base version) | ESP32-S3 + Renesas SLG47910 ForgeFPGA, BLE 5 + Wi-Fi, dual USB-C. [Store](https://store.vicharak.in/?product=shrikefi&post_type=product&name=shrikefi) · [Docs](https://vicharak-in.github.io/shrike/) | see Vicharak store |
| 1 | **USB-C data cable** | must support data (charge-only cables won't flash). USB-A→C or C→C depending on your computer | — |

That's it. No soldering, no breadboard, no extra parts.

## Optional

| Qty | Item | Why |
|---:|---|---|
| 1 | USB power bank (any 5 V) | walk around with the tag for range tests / demo video |
| 1 | 3.3 V **active** buzzer module + 2 jumper wires | audible "Play sound" alert (set `BUZZER_PIN` in `config.h`) |
| 1 | Small enclosure / zip-lock bag + keyring | make it look like a tag |
| — | LiPo cell + BMS parts | the ShrikeFi base board has a TP4056/DW01A charging circuit footprint marked **DNP**; parts list in Vicharak's [hardware overview](https://vicharak-in.github.io/shrike/hardware_overview.html). Not needed for v0.1 |

## Software (all free)

| Tool | Used for |
|---|---|
| Arduino IDE 2.x **or** PlatformIO **or** esptool | building / flashing firmware |
| Arduino-ESP32 core (Espressif) 3.x | ESP32-S3 support + BLE library |
| nRF Connect for Mobile / LightBlue | phone-side scanning and alerts (Android + iOS) |
| Python 3 + `bleak` (optional) | laptop finder script `tools/find_tag.py` |
