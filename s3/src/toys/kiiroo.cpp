// Kiiroo Keon strokers over Bluetooth LE. A level becomes continuous full
// strokes; the stroke speed follows the level.
//
// Protocol details from the Buttplug project, BSD-3-Clause, Copyright (c)
// 2016-2026 Nonpolynomial Labs, LLC (see THIRD_PARTY_NOTICES.md):
//   crates/buttplug_server_device_config/device-config/protocols/
//     kiiroo-v21-initialized.yml, kiiroo-v3.yml
//   crates/buttplug_server/src/device/protocol_impl/kiiroo_v21_initialized.rs,
//     kiiroo_v21.rs, kiiroo_v3.rs, fleshlight_launch_helper.rs (speed formula)
// Keon 2 / Keon WiFi details (UUIDs, movement mode, the 0x03 0x01 move, pause,
// battery) from the FeelRobotics keon-wifi-sdk, MIT, Copyright (c) 2025
// FeelRobotics: src/devices/keon2.ts, keonWifi.ts, keonProtocol.ts,
// src/managers/BleManager.ts.
#include <Arduino.h>
#include <math.h>

#include "../log.h"
#include "toys.h"

namespace toys {

enum : uint8_t { KEON_1, KEON_2, KEON_WIFI };

static const uint32_t KEEPALIVE_MS = 2000;  // Keon 1: Buttplug repeats the last packet every 2 s
static const uint32_t SLOW_MS = 1500, FAST_MS = 250;  // half stroke at level 1 / 100

// fleshlight_launch_helper.rs calculate_speed(), times 99 as kiiroo_v21*.rs do,
// clamped to the protocol's 0..99. distance 0..1, duration in ms.
static uint8_t speedFor(float distance, uint32_t duration) {
  if (distance <= 0) return 0;
  if (distance > 1) distance = 1;
  float scalar = powf((duration * 90.0f) / (distance * 100.0f), -1.05f);
  float s = 250.0f * scalar * 99.0f;
  if (!(s > 0)) return 0;
  return s >= 99 ? 99 : (uint8_t)s;
}

static uint32_t halfStrokeMs(int level) {
  level = constrain(level, 1, 100);
  return SLOW_MS - (SLOW_MS - FAST_MS) * (uint32_t)(level - 1) / 99;
}

// Only a packet that went out becomes the keepalive packet.
static bool send(Ctx &c, const uint8_t *p, uint8_t n, uint32_t now) {
  if (!write(c, p, n)) return false;
  KeonState &k = c.kn;
  memcpy(k.last, p, n);
  k.lastLen = n;
  k.lastWrite = now;
  return true;
}

static bool move(Ctx &c, uint8_t speed, uint8_t pos, uint32_t now) {
  // Keon 1: 03 00 speed pos (Buttplug). Keon 2 / WiFi: 03 01 speed pos as the
  // vendor SDK sends it (Buttplug uses 00 there too).
  uint8_t p[4] = {0x03, (uint8_t)(c.kn.variant == KEON_1 ? 0x00 : 0x01), speed, pos};
  return send(c, p, sizeof p, now);
}

static bool pause(Ctx &c, uint32_t now) {
  static const uint8_t p[1] = {0x00};
  return send(c, p, 1, now);
}

static void initState(Ctx &c, uint8_t variant) {
  KeonState &k = c.kn;
  k = KeonState();
  k.variant = variant;
  k.lo = 5;
  k.hi = variant == KEON_WIFI ? 90 : 95;  // Keon WiFi: max position 90 (SDK)
  k.target = k.from = k.lo;  // so the first stroke goes up
  c.caps = devices::CAP_SPEED | devices::CAP_STROKE;
}

// Where the Keon is now, assuming it keeps to the half stroke's timing.
static uint8_t estimate(const KeonState &k, uint32_t now) {
  if (!k.moving || (int32_t)(now - k.boundary) >= 0 || k.boundary == k.start) return k.target;
  float f = (float)(now - k.start) / (float)(k.boundary - k.start);
  f = constrain(f, 0.0f, 1.0f);
  return (uint8_t)lroundf(k.from + (k.target - k.from) * f);
}

static const uint8_t STOP_SPEED = 10;  // Keon 1 stop: a slow move to where it is

// The stop packet: Keon 2 / WiFi pause; Keon 1 has no documented stop or pause,
// so it gets a slow move to its estimated position, which also becomes the
// keepalive packet (so the keepalive doesn't drive it on to the old target).
// Keon 1: the position is estimated once, by stop(), into k.stopPos; retries
// reuse it.
static bool sendStop(Ctx &c, uint32_t now) {
  KeonState &k = c.kn;
  bool ok;
  if (k.variant == KEON_1) {
    ok = move(c, STOP_SPEED, k.stopPos, now);
    if (ok) k.target = k.from = k.stopPos;
  } else {
    ok = pause(c, now);
  }
  k.moving = false;
  k.stopPending = !ok;
  return ok;
}

// ---------- shared stroking ----------

static void level(Ctx &c, int level, uint32_t now) {
  KeonState &k = c.kn;
  k.level = level;
  if (level > 0) {
    k.stopPending = false;
    if (!k.moving) {
      k.moving = true;
      k.boundary = now;  // start right away
    }
  } else if (k.variant != KEON_1) {
    sendStop(c, now);
  }
  // Keon 1 at level 0 (not STOP): no new strokes from the next boundary on; it
  // finishes the half stroke it is on.
}

static void tick(Ctx &c, uint32_t now) {
  KeonState &k = c.kn;
  if (k.stopPending) {
    sendStop(c, now);
    return;
  }
  if (k.moving && (int32_t)(now - k.boundary) >= 0) {
    if (k.level <= 0) {
      k.moving = false;
    } else {
      uint32_t d = halfStrokeMs(k.level);
      uint8_t to = k.target == k.hi ? k.lo : k.hi;
      if (move(c, speedFor((k.hi - k.lo) / 100.0f, d), to, now)) {
        k.from = k.target;
        k.target = to;
        k.start = now;
        k.boundary = now + d;
      }  // else: try again on the next tick
    }
  }
  // Keon 1: repeat the last packet when idle, as Buttplug does. Once stopped it
  // is a move to where the Keon already is, so nothing moves.
  if (k.variant == KEON_1 && k.lastLen && now - k.lastWrite >= KEEPALIVE_MS)
    if (!send(c, k.last, k.lastLen, now)) k.lastWrite = now;  // no retry storm
}

static void stop(Ctx &c) {
  KeonState &k = c.kn;
  uint32_t now = millis();
  k.level = 0;
  // A STOP while a previous stop is still being retried keeps its position.
  if (!k.stopPending) k.stopPos = estimate(k, now);
  sendStop(c, now);
}

// ---------- Keon (1st generation) ----------

static bool keon1Matches(const NimBLEAdvertisedDevice &adv) {
  std::string n = adv.getName();
  return n == "KEON" || n == "Keon R2";
}

static bool keon1Init(Ctx &c) {
  NimBLERemoteService *s = c.client->getService(NimBLEUUID((uint16_t)0x1900));
  if (!s) return false;
  c.tx = s->getCharacteristic(NimBLEUUID((uint16_t)0x1902));
  c.rx = s->getCharacteristic(NimBLEUUID((uint16_t)0x1903));
  if (!c.tx) return false;
  c.txNoResponse = c.tx->canWriteNoResponse();
  initState(c, KEON_1);
  strcpy(c.name, "Keon");
  // Buttplug's init: two writes with response, 03 00 speed position. Buttplug
  // sends them at speed 100 (0x64), which jolts the Keon on every reconnect;
  // here at a slow 20. It ends up heading for position 0.
  static const uint8_t A[4] = {0x03, 0x00, 20, 0x19}, B[4] = {0x03, 0x00, 20, 0x00};
  bool ok = c.tx->writeValue(A, 4, true) && c.tx->writeValue(B, 4, true);
  plog("ble: %s init writes %s\n", c.advName, ok ? "ok" : "FAILED");
  c.kn.target = c.kn.from = 0;
  return ok;
}

static void keon1Battery(Ctx &) {}  // no battery reading known for the 1st generation

// ---------- Keon 2 / Keon WiFi ----------

static bool keon2Matches(const NimBLEAdvertisedDevice &adv) {
  std::string n = adv.getName();
  return n == "KEON2" || n == "KEON WIFI" || n == "Keon Wifi";
}

static bool keon2Init(Ctx &c) {
  bool wifi = strcmp(c.advName, "KEON2") != 0;
  NimBLERemoteService *s = c.client->getService(NimBLEUUID((uint16_t)(wifi ? 0x1900 : 0x1400)));
  if (!s) return false;
  c.tx = s->getCharacteristic(NimBLEUUID((uint16_t)(wifi ? 0x1800 : 0x1801)));
  if (!c.tx) return false;
  c.txNoResponse = c.tx->canWriteNoResponse();
  c.aux = s->getCharacteristic(NimBLEUUID((uint16_t)0x2a19));
  initState(c, wifi ? KEON_WIFI : KEON_2);
  strcpy(c.name, wifi ? "Keon WiFi" : "Keon 2");
  // Movement mode, then position notifications (unused here; the SDK subscribes
  // too and the Keon may expect it). The SDK carries on if either fails.
  static const uint8_t MODE[1] = {0x00};
  bool ok = c.tx->writeValue(MODE, 1, c.tx->canWrite());
  bool sub = c.tx->canNotify() && subscribe(c, c.tx);
  plog("ble: %s movement mode %s, notify %s, battery char %s\n", c.advName,
                ok ? "ok" : "FAILED", sub ? "ok" : "no", c.aux ? "yes" : "no");
  return true;
}

static void keon2Battery(Ctx &c) {
  if (!c.aux || !c.aux->canRead()) return;
  NimBLEAttValue v = c.aux->readValue();
  if (v.size() < 1) {
    plog("ble: %s battery: no reading\n", c.name);
    return;
  }
  plog("ble: %s battery %u%%\n", c.name, v[0]);
  if (v[0] <= 100) c.battery = v[0];
}

const Driver keon = {'K', devices::Kind::Kiiroo, "Kiiroo", keon1Matches, keon1Init, level, tick, stop, keon1Battery};
const Driver keon2 = {'2', devices::Kind::Kiiroo, "Kiiroo", keon2Matches, keon2Init, level, tick, stop, keon2Battery};

}  // namespace toys
