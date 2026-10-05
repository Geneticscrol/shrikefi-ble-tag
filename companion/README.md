# ShrikeFi last-seen map companion

A tiny **Progressive Web App** (no build step) that uses **Web Bluetooth** + the **phone’s GPS** to drop map pins when your ShrikeFi BLE Tag is nearby.

> This is **not** Apple Find My / Google Find My Device, and the ShrikeFi board has **no onboard GPS**. Pins are “where *my phone* was when it last saw the tag.”

## Features

- Connect to a device named `ShrikeFi-Tag` (name is editable in the UI).
- While connected: automatic GPS pins every few seconds; RSSI when `watchAdvertisements()` is available (Chromium).
- **Mark last seen** button for a manual pin when you are next to the tag.
- Leaflet + OpenStreetMap map with last-seen marker + recent trail (stored in `localStorage` on the phone).

## Requirements

| | |
|---|---|
| **Primary** | **Android Chrome** (tested target: OnePlus) |
| Permissions | Bluetooth + Location (GPS) |
| Secure context | **HTTPS** or `http://localhost` (plain `http://192.168.x.x` will block Web Bluetooth) |
| iPhone | Safari usually has **no Web Bluetooth** — use LightBlue / nRF Connect for find-me; this map is Android-first |

## Open on a OnePlus (Android Chrome)

### Option A — GitHub Pages (easiest once enabled)

If Pages is publishing this repo, open:

`https://geneticscrol.github.io/shrikefi-ble-tag/companion/`

### Option B — Laptop HTTP + `adb reverse` (secure `localhost` on the phone)

1. On the laptop, from this folder:
   ```bash
   cd companion
   python -m http.server 8080
   ```
2. Plug the OnePlus in with USB debugging on, then:
   ```bash
   adb reverse tcp:8080 tcp:8080
   ```
3. On the phone, open **Chrome** → `http://localhost:8080/`
4. Tap **Connect to tag** → pick **ShrikeFi-Tag** → allow Location when asked.
5. Walk with the phone; pins appear on the map. Use **Mark last seen** for an immediate pin.

### Option C — Desktop Chrome only

`python -m http.server 8080` then open `http://localhost:8080/` (needs a BLE adapter).

## Limitations

- Continuous BLE *scan* without connecting is limited in browsers; this app **connects** and then auto-pins / watches advertisements.
- RSSI is best-effort (`watchAdvertisements`); if missing, pins still record GPS.
- Map tiles need network the first time; the app shell is cached by the service worker.
- Privacy: pins live only in this browser’s `localStorage`. Clear with **Clear trail**.

See the main [README](../README.md) and [HOWTO § last-seen map](../docs/HOWTO.md#8-last-seen-map-companion-android-chrome).
