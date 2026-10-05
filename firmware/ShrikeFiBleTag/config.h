// =============================================================================
//  ShrikeFi BLE Tag - user configuration
//  Edit the values in this file, then re-flash. Nothing else needs to change.
// =============================================================================
#pragma once

// ---------------------------------------------------------------------------
// Identity
// ---------------------------------------------------------------------------

// Name shown in nRF Connect / LightBlue. Keep it short (<= 12 chars) so it
// fits in the 31-byte advertising packet together with the service UUID.
#define TAG_NAME "ShrikeFi-Tag"

// iBeacon Proximity UUID. GENERATE YOUR OWN so your tag is unique:
//   Linux/macOS:  uuidgen
//   Windows (PS): [guid]::NewGuid()
//   Python:       python -c "import uuid; print(uuid.uuid4())"
// Format: 8-4-4-4-12 hex digits, with dashes.
#define IBEACON_UUID "5F1C0DE0-5B1E-4A6F-9E2A-5A4B5341D001"

// iBeacon Major / Minor (0..65535). Use them to tell several tags apart,
// e.g. Major = owner/group, Minor = item (1 = keys, 2 = bag, ...).
#define IBEACON_MAJOR 1
#define IBEACON_MINOR 1

// "Measured power": expected RSSI (dBm) at 1 metre from the tag. Phones use it
// to estimate distance. -59 is a common default; calibrate it for your board
// (see docs/HOWTO.md -> "Calibrating distance").
#define IBEACON_MEASURED_POWER (-59)

// ---------------------------------------------------------------------------
// Advertising behaviour
// ---------------------------------------------------------------------------

// The tag alternates between two advertising "frames":
//   1. iBeacon frame   - non-connectable, for beacon scanner apps.
//   2. Find Me frame   - connectable, name + Immediate Alert Service (0x1802),
//                        so a phone can connect and make the tag blink/beep.
// iOS hides iBeacon packets from normal Bluetooth apps (CoreBluetooth), so
// the Find Me frame is what LightBlue / nRF Connect on iPhone will show.
#define ENABLE_IBEACON_FRAME 1
#define ENABLE_FINDME_FRAME 1

// How long each frame is advertised before switching (ms).
#define FRAME_SWITCH_MS 1000

// Advertising interval (ms). Lower = faster discovery + smoother RSSI,
// higher = less power. 100-1000 ms is reasonable. (BLE allows 20..10240.)
#define ADV_INTERVAL_MS 200

// ---------------------------------------------------------------------------
// Hardware
// ---------------------------------------------------------------------------

// ShrikeFi MCU user LED (ESP32-S3 GPIO21, active high).
#define LED_PIN 21
#define LED_ACTIVE_HIGH 1

// Optional ACTIVE buzzer module (beeps when its pin is HIGH). -1 = none.
// Example: wire buzzer + to GPIO4 and - to GND, then set BUZZER_PIN 4.
#define BUZZER_PIN -1

// How long an alert (written from the phone) lasts before auto-stopping (ms).
#define ALERT_DURATION_MS 15000

// Serial monitor baud rate.
#define SERIAL_BAUD 115200
