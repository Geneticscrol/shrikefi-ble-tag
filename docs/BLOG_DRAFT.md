# Blog draft — "Building an Open AirTag-Style BLE Tag on ShrikeFi"

> Draft for a guest feature on [blog.vicharak.in](https://blog.vicharak.in/). Outline + starter text; replace every **[TODO]** with real measurements, photos and screenshots from your own build before submitting. Target length: 1,200–1,800 words + 6–10 images + a 60–90 s demo video.

**Working title options**
- "Can you make an AirTag with ShrikeFi? Building an open BLE finder tag"
- "ShrikeFi Finder: an AirTag-style Bluetooth tag in one Arduino sketch"

**Author:** Saksham Sud (Vicharak Campus Fellow 2026, Navrachana University) · GitHub: [Geneticscrol/shrikefi-ble-tag](https://github.com/Geneticscrol/shrikefi-ble-tag)

**Tags:** ShrikeFi, ESP32-S3, Bluetooth LE, iBeacon, Arduino, Fellowship

---

## 1. Intro (≈150 words)

- Hook: everyone has lost keys/a bag. AirTags solve it — can a ₹-budget dev board do something similar?
- The prompt from the Vicharak Fellowship group: *"Guys, can we make an AirTag using ShrikeFi?"*
- What I built: a ShrikeFi that advertises over Bluetooth LE, shows up on **both Android and iPhone**, gives a hot/cold signal-strength readout and **blinks (beeps) on command** from the phone.
- One-line honesty box: *This is an AirTag-style tag, not an Apple AirTag — it doesn't use Apple's Find My network (explained in section 6).*
- **[TODO] hero image:** ShrikeFi on a keyring/bag next to a phone showing nRF Connect.

## 2. What an AirTag actually does (≈200 words)

Break the AirTag into features and mark which ones this project replicates:

| AirTag feature | This project |
|---|---|
| BLE advertising so nearby phones notice it | ✅ iBeacon + Find Me frames |
| "Play sound" to locate it nearby | ✅ Immediate Alert Service → LED / buzzer |
| Proximity ("hot/cold", Precision Finding uses UWB) | ✅ RSSI-based (no UWB) |
| Crowd-sourced location via millions of Apple devices | ❌ needs Apple's Find My network (MFi) |
| Rotating keys + anti-stalking | ❌ (roadmap / discussed) |
| Coin-cell battery for a year | ❌ USB / power bank in v0.1 |

## 3. Why ShrikeFi? (≈200 words)

- ESP32-S3 with on-chip **Bluetooth LE 5** + Wi-Fi → the radio side is "free".
- Dual USB-C (CH9102 + native USB) → flashing is plug-and-play for beginners.
- On-board MCU LED on **GPIO21** → instant status indicator, no wiring.
- The **ForgeFPGA** sitting next to the MCU opens doors no plain ESP32 board has: hardware LED patterns, ultra-low-power wake logic, custom signal processing (see Future work).
- Optional BMS footprint → path to a truly portable tag.
- Link: Vicharak's own explainer, [ESP32 Meets FPGA: Understanding ShrikeFi](https://blog.vicharak.in/esp32-meets-fpga-understanding-shrikefi/).

## 4. How it works (≈450 words + 2 diagrams)

### 4.1 Architecture
- **[TODO] diagram:** reuse the Mermaid flowchart from the README (export as PNG).
- Firmware = one Arduino sketch + `config.h`.

### 4.2 Bluetooth LE advertising in 60 seconds
- BLE devices shout small packets (≤ 31 bytes) on 3 advertising channels; phones listen.
- **Frame 1 – iBeacon:** company ID 0x004C, type 0x02 0x15, 16-byte UUID, Major, Minor, measured power. Show the byte table from HOWTO §6.
- **Frame 2 – Find Me:** name + service UUID 0x1802, connectable.
- **The iPhone gotcha** (good "lesson learned" paragraph): iOS hides iBeacon packets from normal Bluetooth apps → solution: alternate two frames every second. **[TODO] screenshot:** Android showing both vs iPhone showing name only.

### 4.3 Finding by signal strength
- RSSI explained; formula `d ≈ 10^((P₁ₘ − RSSI)/(10·n))`.
- Why it's "hot/cold", not centimetre-accurate (walls, bodies, antenna orientation).
- **[TODO] chart:** your RSSI-vs-distance measurements from `TEST_LOG.md`, both phones.

### 4.4 "Play sound": Immediate Alert Service
- Standard Bluetooth SIG service, so off-the-shelf apps (nRF Connect, LightBlue) can trigger it.
- Write `01` / `02` → blink patterns; `00` stops; 15 s timeout.
- Short code snippet: the `AlertLevelCallbacks::onWrite` + LED pattern function (≈15 lines).

### 4.5 One sketch, two BLE stacks
- Arduino-ESP32 3.x uses NimBLE on the S3, 2.x used Bluedroid → small `#if` for advertising types; payload type trick with `decltype`. (Nice detail for developers.)

## 5. Build it yourself — demo steps (≈300 words)

Condensed from `docs/HOWTO.md` — keep it to numbered steps with screenshots:

1. What you need: ShrikeFi + USB-C data cable (+ phone). **[TODO] photo of kit**
2. Arduino IDE → add Espressif board URL → install esp32 core.
3. Open sketch, generate your UUID, set board **ESP32S3 Dev Module**, Flash 4MB, USB CDC On Boot per port.
4. Upload (BOOT button tip) → Serial Monitor banner. **[TODO] screenshot**
5. Android: nRF Connect → scan → iBeacon details + RSSI graph. **[TODO] screenshots (OnePlus 11)**
6. iPhone: LightBlue → ShrikeFi-Tag → write `02` to Alert Level → LED blinks. **[TODO] screenshots (iPhone)**
7. Hide-and-seek demo: hide the tag, find it using the RSSI graph, trigger the alert. **[TODO] embed 60–90 s video (YouTube/Instagram)**

## 6. Limitations — honest section (≈200 words)

- Not Apple Find My / Google Find My Device: those require certified accessories, rotating crypto keys and a network of phones. Community reverse-engineering (OpenHaystack etc.) exists but is unofficial, brittle and not first-class on ESP32-S3 — deliberately out of scope.
- Range = your phone's Bluetooth range. **[TODO] your measured max range.**
- Fixed address/UUID → trackable by anyone; real AirTags rotate IDs and alert against stalking. Use only on your own stuff.
- Power: base board has BMS unpopulated; v0.1 doesn't deep-sleep.
- Trademark note: AirTag, iBeacon, Find My are Apple trademarks; no affiliation.

## 7. Future work (≈150 words)

- **FPGA status LED:** offload the blink pattern to the ForgeFPGA (FPGA GPIO16) via the MCU–FPGA link — the MCU could sleep while the FPGA keeps signalling.
- **Low power:** longer advertising intervals + light/deep sleep, measured current, LiPo via the BMS footprint.
- **Link-loss alert** ("you left your bag behind") using Link Loss Service 0x1803.
- **Companion app** (Flutter): UUID filter, hot/cold gauge, last-seen time + GPS pin.
- **Rotating identifiers** for privacy; Eddystone-UID option.
- **[TODO]** ask readers: what would you add?

## 8. Conclusion + links (≈100 words)

- Recap: ShrikeFi + one sketch + free phone apps = a working AirTag-*style* finder on Android and iPhone in ~20 minutes.
- Call to action: repo link, Vicharak Discord, star/fork, share your builds.
- Links: [Repo](https://github.com/Geneticscrol/shrikefi-ble-tag) · [ShrikeFi docs](https://vicharak-in.github.io/shrike/) · [Vicharak Discord](https://discord.com/invite/EhQy97CQ9G) · [Shrike projects showcase](https://github.com/vicharak-in/shrike_projects)

---

## Asset checklist before submitting

- [ ] Hero photo (board on keys/bag + phone)
- [ ] Close-up of ShrikeFi top side, LED on
- [ ] Arduino IDE Tools-menu screenshot + Serial Monitor banner
- [ ] nRF Connect (OnePlus 11): iBeacon details + RSSI graph
- [ ] LightBlue (iPhone): device list + Alert Level write
- [ ] RSSI vs distance chart (from TEST_LOG.md)
- [ ] Architecture diagram PNG
- [ ] 60–90 s demo video (hide → search hot/cold → trigger blink → found)
- [ ] All [TODO]s replaced, no claim of Find My compatibility
- [ ] Ask the Vicharak team (Fellowship group / Discord) how they want the feature submitted (guest post doc, Medium draft, or PR to `shrike_projects`)
