# Test log

Fill this in on real hardware — measured numbers make the blog post credible. Add photos/screenshots to `docs/images/` and link them here.

**Firmware:** v0.1.0 · commit `________` · Arduino-ESP32 core `____` · `ADV_INTERVAL_MS = 200`, `FRAME_SWITCH_MS = 1000`
**Board:** ShrikeFi, flash ___ MB, powered from: laptop USB / power bank · on breadboard? yes / no
**Date / place:** ____________ (indoor / outdoor)

## Functional checklist

| # | Test | OnePlus 11 (Android) | iPhone 14/15 (iOS) | Notes |
|---|---|---|---|---|
| 1 | Flash succeeds, Serial banner prints | n/a | n/a | |
| 2 | LED heartbeat (1 blink/s) when idle | n/a | n/a | |
| 3 | Tag visible by name `ShrikeFi-Tag` | ☐ | ☐ | app used: |
| 4 | iBeacon frame decoded (UUID/Major/Minor) | ☐ | expected ✗ (iOS) | |
| 5 | Connect → LED solid | ☐ | ☐ | |
| 6 | Write Alert Level 02 → fast blink | ☐ | ☐ | |
| 7 | Write 01 → slow blink, 00 → stop | ☐ | ☐ | |
| 8 | Alert auto-stops after 15 s | ☐ | ☐ | |
| 9 | Disconnect → heartbeat resumes, reconnect works | ☐ | ☐ | |
| 10 | `tools/find_tag.py` sees tag / sends alert | laptop OS: | | |

## Calibration

| Phone | Avg RSSI at 1.0 m (30 s) | → `IBEACON_MEASURED_POWER` |
|---|---|---|
| OnePlus 11 | | |
| iPhone | | |

## Range (average RSSI, dBm)

| Distance | OnePlus 11 | iPhone | Notes |
|---|---|---|---|
| 0.5 m | | | |
| 1 m | | | |
| 2 m | | | |
| 5 m | | | |
| 10 m | | | |
| 20 m | | | |
| 1 wall | | | |
| Lost at | | | |

## Observations

- 
