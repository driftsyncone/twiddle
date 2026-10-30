#include "battery.h"

#include <Arduino.h>
#include <Wire.h>

#include "board.h"

namespace battery {

static const uint8_t ADDR = 0x36;
static const uint8_t REG_VCELL = 0x02, REG_SOC = 0x04, REG_VERSION = 0x08, REG_CRATE = 0x16;

static TwoWire *bus = nullptr;  // where the gauge answered, or null
static bool p4Started, p4Probed;
static Info cur;
static uint32_t lastRead, lastProbe;

// ---------- voltage divider on an ADC pin ----------
#if BOARD_BATTERY_ADC_PIN >= 0
static bool adcReady;
static float filtered;  // smoothed cell voltage, 0 = nothing yet

// Open-circuit LiPo voltage -> %; the panel's draw (~0.5 A from the cell) sags
// this by roughly 0.05 V, so the table sits a little low to compensate.
static float percentFromVolts(float v) {
  static const float V[] = {3.30f, 3.50f, 3.60f, 3.68f, 3.74f, 3.79f, 3.85f, 3.92f, 4.00f, 4.10f, 4.18f};
  static const float P[] = {0, 5, 12, 22, 33, 44, 55, 66, 78, 90, 100};
  const int n = sizeof V / sizeof V[0];
  if (v <= V[0]) return 0;
  if (v >= V[n - 1]) return 100;
  int i = 1;
  while (v > V[i]) i++;
  return P[i - 1] + (P[i] - P[i - 1]) * (v - V[i - 1]) / (V[i] - V[i - 1]);
}

static void adcBegin() {
  if (p4Started) {
    Wire1.end();  // no gauge on P4: the pin goes to the ADC
    p4Started = false;
  }
  pinMode(BOARD_BATTERY_ADC_PIN, INPUT);
  analogSetPinAttenuation(BOARD_BATTERY_ADC_PIN, ADC_11db);  // 12 dB: up to ~3.1 V (4.2 V cell -> 3.16 V)
  adcReady = true;
}

// ADC2 is shared with Wi-Fi on the ESP32-S3; a read can come back 0 while the
// radio holds it. Take several, keep the median of the ones that worked.
static bool adcReadVolts(float &volts) {
  uint32_t mv[9];
  int n = 0;
  for (int i = 0; i < 9; i++) {
    uint32_t m = analogReadMilliVolts(BOARD_BATTERY_ADC_PIN);
    if (m > 0) mv[n++] = m;
    delayMicroseconds(200);
  }
  if (n < 3) return false;
  for (int i = 1; i < n; i++)  // insertion sort, n <= 9
    for (int j = i; j > 0 && mv[j - 1] > mv[j]; j--) {
      uint32_t t = mv[j]; mv[j] = mv[j - 1]; mv[j - 1] = t;
    }
  volts = mv[n / 2] / 1000.0f / BOARD_BATTERY_DIVIDER;
  return true;
}

static void adcRead() {
  float v;
  if (!adcReadVolts(v)) return;  // keep the last value
  if (v < 2.8f) {  // no cell connected (USB only): the divider floats near 0
    cur = Info();
    filtered = 0;
    return;
  }
  filtered = filtered == 0 ? v : filtered + (v - filtered) * 0.2f;
  cur.present = true;
  cur.volts = filtered;
  cur.percent = percentFromVolts(filtered);
  cur.charging = false;  // nothing on the board says so; the % just sits at 100 while charging
  cur.chip = "divider";
}
#endif

static bool present(TwoWire &w) {
  w.beginTransmission(ADDR);
  return w.endTransmission() == 0;
}

static bool read16(uint16_t &out, uint8_t reg) {
  bus->beginTransmission(ADDR);
  bus->write(reg);
  if (bus->endTransmission(false) != 0 || bus->requestFrom(ADDR, (uint8_t)2) != 2) return false;
  out = (bus->read() << 8) | bus->read();
  return true;
}

static void probe() {
  lastProbe = millis();
#if !BOARD_FUEL_GAUGE
  return;  // see board.h: no usable bus/address for a gauge on this board
#endif
  // Touch bus first: already running (board.cpp), shared with the GT911 at 0x5D.
  if (present(Wire)) {
    bus = &Wire;
  } else if (BOARD_HAS_P4_I2C && !p4Probed) {
    p4Probed = true;  // boot only: GPIO17 is the battery ADC afterwards
    if (!p4Started) p4Started = Wire1.begin(17, 18, 100000);
    if (p4Started && present(Wire1)) bus = &Wire1;
  }
  if (!bus) {
#if BOARD_BATTERY_ADC_PIN >= 0
    if (!adcReady) adcBegin();
#endif
    return;
  }
  uint16_t ver = 0;
  // MAX17048/49 report 0x001x; the older MAX17043/44 report 0x000x.
  if (!read16(ver, REG_VERSION)) cur.chip = "MAX1704x";
  else cur.chip = (ver & 0xFFF0) == 0x0010 ? "MAX17048" : "MAX17043";
  Serial.printf("battery: %s (version 0x%04x) on %s\n", cur.chip, ver, bus == &Wire ? "touch bus" : "P4");
}

static void read() {
  uint16_t v, soc;
  if (!read16(v, REG_VCELL) || !read16(soc, REG_SOC)) {
    // Unplugged or bus error: forget it and probe again later.
    bus = nullptr;
    cur = Info();
    return;
  }
  cur.present = true;
  cur.volts = v * 78.125e-6f;  // both chips: 78.125 uV per LSB
  cur.percent = constrain(soc / 256.0f, 0.0f, 100.0f);
  cur.charging = false;
  uint16_t crate;
  if (strcmp(cur.chip, "MAX17048") == 0 && read16(crate, REG_CRATE))
    cur.charging = (int16_t)crate * 0.208f > 0.5f;  // %/hour; above +0.5 %/h = charging
}

void begin() { probe(); }

void loop() {
#ifdef BATTERY_FAKE_PERCENT  // UI test builds only: -DBATTERY_FAKE_PERCENT=57
  cur.present = true;
  cur.percent = BATTERY_FAKE_PERCENT;
  cur.volts = 3.5f + BATTERY_FAKE_PERCENT * 0.007f;
  cur.chip = "fake";
  return;
#endif
  uint32_t now = millis();
  if (!bus) {
    if (now - lastProbe > 30000) probe();
#if BOARD_BATTERY_ADC_PIN >= 0
    if (adcReady && (now - lastRead > 2000 || !cur.present)) {
      lastRead = now;
      adcRead();
    }
#endif
    return;
  }
  if (now - lastRead > 5000 || !cur.present) {
    lastRead = now;
    read();
  }
}

Info info() { return cur; }

bool usesP4() { return bus == &Wire1; }

}  // namespace battery
