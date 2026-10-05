// =============================================================================
//  ShrikeFi BLE Tag  -  an open, AirTag-*style* BLE finder tag for ShrikeFi
//  https://github.com/Geneticscrol/shrikefi-ble-tag
//
//  Board : Vicharak ShrikeFi (ESP32-S3 + Renesas ForgeFPGA)
//  Core  : Arduino-ESP32 (Espressif) 2.0.x or 3.x
//
//  What it does
//   * Advertises an iBeacon frame (your UUID / Major / Minor) so beacon apps
//     can see it and estimate distance from RSSI.
//   * Alternates with a connectable "Find Me" frame (name + Immediate Alert
//     Service 0x1802). Connect from a phone, write 1 (mild) or 2 (high) to the
//     Alert Level characteristic 0x2A06 and the tag blinks (and beeps if a
//     buzzer is fitted) so you can find it. Write 0 to stop.
//   * MCU LED (GPIO21): short blink every second = advertising,
//     solid = phone connected, fast blink = alert.
//
//  This is NOT an Apple AirTag and does NOT use Apple's Find My network.
//  It is a local, open BLE beacon: you can only find it within Bluetooth
//  range (~10-30 m indoors) of your own phone.
//
//  License: MIT (see LICENSE in the repository root)
// =============================================================================

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>

#include "config.h"

#define FW_VERSION "0.1.0"

// Bluetooth SIG assigned numbers (Find Me profile)
static const uint16_t UUID_IMMEDIATE_ALERT_SERVICE = 0x1802;
static const uint16_t UUID_ALERT_LEVEL_CHAR = 0x2A06;

// Advertising PDU types. Arduino-ESP32 3.x uses the NimBLE host on ESP32-S3,
// 2.x uses Bluedroid; the library takes a different constant for each.
#if defined(CONFIG_NIMBLE_ENABLED)
#define TAG_ADV_CONNECTABLE BLE_GAP_CONN_MODE_UND
#define TAG_ADV_NONCONNECTABLE BLE_GAP_CONN_MODE_NON
#else
#define TAG_ADV_CONNECTABLE ADV_TYPE_IND
#define TAG_ADV_NONCONNECTABLE ADV_TYPE_NONCONN_IND
#endif

// Alert levels defined by the Immediate Alert Service spec
enum AlertLevel : uint8_t { ALERT_NONE = 0, ALERT_MILD = 1, ALERT_HIGH = 2 };

// ---------------------------------------------------------------------------
// State shared between BLE callbacks (Bluetooth task) and loop()
// ---------------------------------------------------------------------------
static volatile bool g_connected = false;
static volatile bool g_restartAdvertising = false;
static volatile uint8_t g_alertLevel = ALERT_NONE;
static volatile uint32_t g_alertStartMs = 0;

static BLEServer* g_server = nullptr;
static BLECharacteristic* g_alertChar = nullptr;

enum Frame { FRAME_IBEACON, FRAME_FINDME };
static Frame g_currentFrame = FRAME_FINDME;
static uint32_t g_lastFrameSwitchMs = 0;

static uint8_t g_uuidBytes[16];

// The Arduino BLE library uses std::string on core 2.x and Arduino String on
// core 3.x for advertisement payloads. BLEAdvertisementData::getPayload()
// returns whichever type this core uses, so we build our payload in that type.
using PayloadString = decltype(BLEAdvertisementData().getPayload());

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void ledWrite(bool on) {
  digitalWrite(LED_PIN, (on == (LED_ACTIVE_HIGH != 0)) ? HIGH : LOW);
}

static void buzzerWrite(bool on) {
#if BUZZER_PIN >= 0
  digitalWrite(BUZZER_PIN, on ? HIGH : LOW);
#else
  (void)on;
#endif
}

static int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Parses "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX" into 16 big-endian bytes.
static bool parseUuid(const char* text, uint8_t out[16]) {
  int n = 0;
  int hi = -1;
  for (const char* p = text; *p; ++p) {
    if (*p == '-') continue;
    int v = hexNibble(*p);
    if (v < 0 || n >= 16) return false;
    if (hi < 0) {
      hi = v;
    } else {
      out[n++] = (uint8_t)((hi << 4) | v);
      hi = -1;
    }
  }
  return n == 16 && hi < 0;
}

// Apple iBeacon manufacturer-specific data (25 bytes):
//   4C 00        company ID (Apple, little endian)
//   02 15        iBeacon type, remaining length (21)
//   UUID[16]     proximity UUID (big endian)
//   MAJOR[2]     big endian
//   MINOR[2]     big endian
//   TX[1]        measured power at 1 m (signed dBm)
static PayloadString buildIBeaconManufacturerData() {
  uint8_t buf[25];
  buf[0] = 0x4C;
  buf[1] = 0x00;
  buf[2] = 0x02;
  buf[3] = 0x15;
  memcpy(&buf[4], g_uuidBytes, 16);
  buf[20] = (uint8_t)((IBEACON_MAJOR >> 8) & 0xFF);
  buf[21] = (uint8_t)(IBEACON_MAJOR & 0xFF);
  buf[22] = (uint8_t)((IBEACON_MINOR >> 8) & 0xFF);
  buf[23] = (uint8_t)(IBEACON_MINOR & 0xFF);
  buf[24] = (uint8_t)(int8_t)IBEACON_MEASURED_POWER;

  PayloadString s;
  for (size_t i = 0; i < sizeof(buf); ++i) s += (char)buf[i];
  return s;
}

static uint16_t msToAdvUnits(uint32_t ms) {
  uint32_t units = (ms * 1000UL) / 625UL;  // 0.625 ms units
  if (units < 0x20) units = 0x20;          // 20 ms minimum
  if (units > 0x4000) units = 0x4000;      // 10.24 s maximum
  return (uint16_t)units;
}

static void startFrame(Frame frame) {
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->stop();

  BLEAdvertisementData advData;

  if (frame == FRAME_IBEACON) {
    advData.setFlags(0x06);  // LE General Discoverable, BR/EDR not supported
    advData.setManufacturerData(buildIBeaconManufacturerData());
    adv->setAdvertisementType(TAG_ADV_NONCONNECTABLE);
    adv->setScanResponse(false);
  } else {
    advData.setFlags(0x06);
    advData.setCompleteServices(BLEUUID(UUID_IMMEDIATE_ALERT_SERVICE));
    advData.setName(TAG_NAME);
    // No iBeacon data in this frame on purpose: iOS hides any packet that
    // carries iBeacon data from CoreBluetooth apps such as LightBlue.
    adv->setAdvertisementType(TAG_ADV_CONNECTABLE);
    adv->setScanResponse(false);
  }

  adv->setAdvertisementData(advData);
  adv->setMinInterval(msToAdvUnits(ADV_INTERVAL_MS));
  adv->setMaxInterval(msToAdvUnits(ADV_INTERVAL_MS + 20));
  adv->start();

  g_currentFrame = frame;
  g_lastFrameSwitchMs = millis();
}

static Frame nextFrame() {
  const bool beacon = ENABLE_IBEACON_FRAME;
  const bool findMe = ENABLE_FINDME_FRAME && !g_connected;  // only one connection at a time
  if (beacon && findMe) return (g_currentFrame == FRAME_IBEACON) ? FRAME_FINDME : FRAME_IBEACON;
  if (findMe) return FRAME_FINDME;
  return FRAME_IBEACON;
}

static bool advertisingWanted() {
  return ENABLE_IBEACON_FRAME || (ENABLE_FINDME_FRAME && !g_connected);
}

static void setAlert(uint8_t level) {
  if (level > ALERT_HIGH) level = ALERT_HIGH;
  g_alertLevel = level;
  g_alertStartMs = millis();
}

// ---------------------------------------------------------------------------
// BLE callbacks (run in the Bluetooth task - keep them short)
// ---------------------------------------------------------------------------
class TagServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override {
    (void)server;
    g_connected = true;
    g_restartAdvertising = true;  // switch to iBeacon-only while connected
  }
  void onDisconnect(BLEServer* server) override {
    (void)server;
    g_connected = false;
    g_restartAdvertising = true;  // become connectable again
  }
};

class AlertLevelCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    auto value = characteristic->getValue();
    if (value.length() > 0) setAlert((uint8_t)value[0]);
  }
};

// ---------------------------------------------------------------------------
// LED / buzzer patterns (non-blocking)
// ---------------------------------------------------------------------------
static void updateIndicators(uint32_t now) {
  uint8_t level = g_alertLevel;

  if (level != ALERT_NONE && (now - g_alertStartMs) > ALERT_DURATION_MS) {
    level = ALERT_NONE;
    g_alertLevel = ALERT_NONE;
    if (g_alertChar) {
      uint8_t zero = ALERT_NONE;
      g_alertChar->setValue(&zero, 1);
    }
    Serial.println("[alert] timed out");
  }

  if (level == ALERT_HIGH) {
    bool on = ((now / 100) % 2) == 0;  // 5 Hz
    ledWrite(on);
    buzzerWrite(on);
  } else if (level == ALERT_MILD) {
    bool on = ((now / 250) % 2) == 0;  // 2 Hz
    ledWrite(on);
    buzzerWrite(((now / 1000) % 2) == 0 && on);
  } else if (g_connected) {
    ledWrite(true);  // solid while a phone is connected
    buzzerWrite(false);
  } else if (advertisingWanted()) {
    ledWrite((now % 1000) < 60);  // 60 ms heartbeat every second
    buzzerWrite(false);
  } else {
    ledWrite(false);
    buzzerWrite(false);
  }
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------
void setup() {
  pinMode(LED_PIN, OUTPUT);
  ledWrite(false);
#if BUZZER_PIN >= 0
  pinMode(BUZZER_PIN, OUTPUT);
  buzzerWrite(false);
#endif

  Serial.begin(SERIAL_BAUD);
  delay(300);
  Serial.println();
  Serial.println("=== ShrikeFi BLE Tag v" FW_VERSION " ===");
  Serial.println("Open BLE beacon - NOT an Apple AirTag / Find My device.");

  if (!parseUuid(IBEACON_UUID, g_uuidBytes)) {
    Serial.println("[error] IBEACON_UUID in config.h is not a valid UUID. Halting.");
    while (true) {  // fast double-blink forever = config error
      ledWrite(true);  delay(80);
      ledWrite(false); delay(80);
      ledWrite(true);  delay(80);
      ledWrite(false); delay(760);
    }
  }

  BLEDevice::init(TAG_NAME);

  g_server = BLEDevice::createServer();
  g_server->setCallbacks(new TagServerCallbacks());

  // Immediate Alert Service (Find Me profile, Bluetooth SIG 0x1802)
  BLEService* ias = g_server->createService(BLEUUID(UUID_IMMEDIATE_ALERT_SERVICE));
  g_alertChar = ias->createCharacteristic(
      BLEUUID(UUID_ALERT_LEVEL_CHAR),
      BLECharacteristic::PROPERTY_WRITE_NR | BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_READ);
  uint8_t initial = ALERT_NONE;
  g_alertChar->setValue(&initial, 1);
  g_alertChar->setCallbacks(new AlertLevelCallbacks());
  ias->start();

  Serial.printf("Name   : %s\n", TAG_NAME);
  Serial.printf("MAC    : %s\n", BLEDevice::getAddress().toString().c_str());
  Serial.printf("UUID   : %s\n", IBEACON_UUID);
  Serial.printf("Major  : %u  Minor: %u  Measured power: %d dBm\n",
                (unsigned)IBEACON_MAJOR, (unsigned)IBEACON_MINOR, (int)IBEACON_MEASURED_POWER);
  Serial.printf("Frames : iBeacon=%s FindMe=%s (switch every %u ms, interval %u ms)\n",
                ENABLE_IBEACON_FRAME ? "on" : "off", ENABLE_FINDME_FRAME ? "on" : "off",
                (unsigned)FRAME_SWITCH_MS, (unsigned)ADV_INTERVAL_MS);

  startFrame(ENABLE_FINDME_FRAME ? FRAME_FINDME : FRAME_IBEACON);
  Serial.println("Advertising... (LED blinks once per second)");
}

void loop() {
  const uint32_t now = millis();
  static uint8_t lastReportedAlert = ALERT_NONE;
  static bool lastConnected = false;

  if (g_restartAdvertising) {
    g_restartAdvertising = false;
    if (advertisingWanted()) {
      startFrame(nextFrame());
    } else {
      BLEDevice::getAdvertising()->stop();
    }
  } else if (advertisingWanted() && (now - g_lastFrameSwitchMs) >= FRAME_SWITCH_MS) {
    Frame next = nextFrame();
    if (next != g_currentFrame) startFrame(next);
    else g_lastFrameSwitchMs = now;
  }

  if (g_connected != lastConnected) {
    lastConnected = g_connected;
    Serial.println(lastConnected ? "[ble] phone connected" : "[ble] phone disconnected");
  }
  if (g_alertLevel != lastReportedAlert) {
    lastReportedAlert = g_alertLevel;
    const char* names[] = {"none", "mild", "HIGH"};
    Serial.printf("[alert] level = %s\n", names[lastReportedAlert > 2 ? 2 : lastReportedAlert]);
  }

  updateIndicators(now);
  delay(10);
}
