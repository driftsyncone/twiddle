// OSSM (Open Source Sex Machine, Research and Desire / KinkyMakers) over
// Bluetooth LE. The panel only drives the speed; stroke, depth, sensation and
// pattern stay whatever the user sets on the machine. The machine's speed knob
// stays an upper limit for what we send (its default, which we leave alone).
//
// Our own client, written from the OSSM's documented BLE API (docs.ossm, BLE
// Protocol page) and checked against the firmware's behaviour; no firmware code
// is copied. The UUIDs and the name match Buttplug's device config, BSD-3-Clause,
// Copyright (c) 2016-2023 Nonpolynomial Labs, LLC (see THIRD_PARTY_NOTICES.md):
//   crates/buttplug_server_device_config/device-config/protocols/ossm.yml
//
// What the machine does, as far as this driver cares:
// - Text commands on 1000: "set:speed:N" (0..100), "go:strokeEngine",
//   "go:menu". A command it doesn't understand turns the characteristic's value
//   into "fail:<cmd>"; a good one is queued and the value stays the command as
//   written. Nothing ever says "ok:" (despite the docs) and nothing is notified
//   on 1000, so a reply is read back from it (init only; tick() must not wait).
// - State JSON notifications on 2000: {"timestamp":..,"state":"menu.idle",
//   "speed":0,...}. Older firmware sends one every second, current firmware
//   only on a change.
// - "go:strokeEngine" acts like a button press with Stroke Engine picked in
//   the menu, so it is only sent from the menu. Entering stroke engine resets
//   the BLE speed, after which the machine runs at the knob position, so the
//   speed is sent again every time it enters a play mode. If the knob isn't at
//   zero it waits in "strokeEngine.preflight" until the user turns it down.
// - When the last client disconnects it goes back to the menu (stopping), and
//   if it was moving it ramps its speed to 0 over about 2 s first.
// - No pairing: the firmware sets no security (no bonding, no MITM, no Secure
//   Connections) and no characteristic needs encryption.
#include <Arduino.h>
#include <ArduinoJson.h>

#include "../log.h"
#include "toys.h"

namespace toys {

static const char *SVC = "522b443a-4f53-534d-0001-420badbabe69";
static const char *CMD = "522b443a-4f53-534d-1000-420badbabe69";
static const char *STATE = "522b443a-4f53-534d-2000-420badbabe69";

enum : uint8_t {
  OSSM_ARMING,   // connected; getting it into stroke engine
  OSSM_RUNNING,  // it reached a play mode once; never switched again
};

static const uint32_t SEND_MS = 100;     // one speed command per 100 ms at most
static const uint32_t MODE_MS = 1500;    // between mode-switch attempts
static const uint8_t MAX_TRIES = 10;
static const uint32_t INIT_MS = 4000;    // how long init() waits for stroke engine
static const uint32_t SILENT_MS = 5000;
static const uint8_t LOG_SPEEDS = 5;     // speed commands logged after connecting

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

static bool matches(const NimBLEAdvertisedDevice &adv) {
  // The name can be changed on the machine; the service UUID can't.
  return adv.getName() == "OSSM" || adv.isAdvertisingService(NimBLEUUID(SVC));
}

// The value of "key" in flat JSON text, without a parser (the notification may
// be cut short at the MTU). String values without the quotes.
static bool field(const char *json, const char *key, char *out, size_t n) {
  char k[16];
  snprintf(k, sizeof k, "\"%s\"", key);
  const char *p = strstr(json, k);
  if (!p) return false;
  p += strlen(k);
  while (*p == ' ') p++;
  if (*p++ != ':') return false;
  while (*p == ' ') p++;
  bool quoted = *p == '"';
  if (quoted) p++;
  size_t i = 0;
  while (*p && i + 1 < n && (quoted ? *p != '"' : (*p != ',' && *p != '}'))) out[i++] = *p++;
  out[i] = 0;
  return quoted ? *p == '"' : i > 0;
}

// NimBLE host task: keep the state name and speed, nothing else.
static void onState(Ctx *c, const uint8_t *d, size_t n) {
  char buf[260], st[32] = "", sp[8] = "";
  size_t m = n < sizeof buf - 1 ? n : sizeof buf - 1;
  memcpy(buf, d, m);
  buf[m] = 0;
  bool hasState = field(buf, "state", st, sizeof st);
  bool hasSpeed = field(buf, "speed", sp, sizeof sp);
  portENTER_CRITICAL(&mux);
  if (hasState) strlcpy(c->os.state, st, sizeof c->os.state);
  if (hasSpeed) c->os.speed = (int8_t)constrain(atoi(sp), 0, 100);
  c->os.seq = c->os.seq + 1;
  c->os.lastNotify = millis();
  portEXIT_CRITICAL(&mux);
}

static bool startsWith(const char *s, const char *p) { return !strncmp(s, p, strlen(p)); }

// A mode where "set:speed" moves it.
static bool playing(const char *s) {
  return (startsWith(s, "strokeEngine") || startsWith(s, "simplePenetration")) &&
         !strstr(s, "preflight") && strchr(s, '.');
}

// Log a long text in pieces (plog lines are short).
static void logLong(const char *what, const char *s) {
  size_t n = strlen(s);
  for (size_t i = 0; i < n || i == 0; i += 140)
    plog("ble: ossm %s%s%.140s\n", what, i ? " (cont) " : " ", s + i);
}

// A command with response, then what the characteristic says about it.
static void command(Ctx &c, const char *cmd) {
  bool ok = c.tx->writeValue((const uint8_t *)cmd, strlen(cmd), true);
  if (!ok) {
    plog("ble: ossm %s: write FAILED\n", cmd);
    return;
  }
  NimBLEAttValue v = c.tx->readValue();
  std::string r((const char *)v.data(), v.size());
  plog("ble: ossm %s -> reply \"%s\"%s\n", cmd, r.c_str(),
       startsWith(r.c_str(), "fail:") ? " (REJECTED)" : "");
}

static void mode(Ctx &c, const char *cmd, bool wait, uint32_t now) {
  OssmState &o = c.os;
  o.tries++;
  o.lastMode = now;
  plog("ble: ossm in \"%s\": sending %s (try %u)\n", o.seen, cmd, o.tries);
  if (wait) command(c, cmd);
  else writeStr(c, cmd);
}

// Follow the machine's state: log changes, send the speed again on entering a
// play mode, and while arming get it from the menu into stroke engine.
// `wait`: from init(), where writes may wait for their reply.
static void follow(Ctx &c, uint32_t now, bool wait) {
  OssmState &o = c.os;
  char st[32];
  int speed;
  uint32_t seq, last;
  portENTER_CRITICAL(&mux);
  strlcpy(st, o.state, sizeof st);
  speed = o.speed;
  seq = o.seq;
  last = o.lastNotify;
  portEXIT_CRITICAL(&mux);

  if (seq != o.seenSeq) {
    o.seenSeq = seq;
    if (strcmp(st, o.seen)) {
      plog("ble: ossm state \"%s\" -> \"%s\" (speed %d)\n", o.seen, st, speed);
      bool was = playing(o.seen);
      strlcpy(o.seen, st, sizeof o.seen);
      if (playing(st) && !was) {
        o.sent = -1;  // entering resets its BLE speed: send ours again
        if (o.phase == OSSM_ARMING) {
          o.phase = OSSM_RUNNING;
          plog("ble: ossm ready in \"%s\"\n", st);
        }
      } else if (was && !playing(st)) {
        plog("ble: ossm left play mode; not switching back (pick Stroke Engine on the machine)\n");
      }
      if (strstr(st, "preflight"))
        plog("ble: ossm waits for its speed knob at 0 (preflight)\n");
      else if (startsWith(st, "error"))
        plog("ble: ossm reports an error state\n");
    }
    if (speed >= 0 && speed != o.lastSpeed && now - o.lastSpeedLog >= 1000) {
      plog("ble: ossm reports speed %d (we sent %d)\n", speed, o.sent);
      o.lastSpeed = speed;
      o.lastSpeedLog = now;
    }
  }

  if (!o.silent && (int32_t)(now - last) > (int32_t)SILENT_MS) {  // last may be newer than now
    o.silent = true;  // once per connection: current firmware is quiet while nothing changes
    plog("ble: ossm no state notification for %u s (state \"%s\")\n", (unsigned)(SILENT_MS / 1000),
         o.seen);
  }

  if (o.phase != OSSM_ARMING || o.tries >= MAX_TRIES || now - o.lastMode < MODE_MS) return;
  if (startsWith(o.seen, "menu")) mode(c, "go:strokeEngine", wait, now);
  else if (startsWith(o.seen, "streaming")) mode(c, "go:menu", wait, now);
  else return;  // homing, preflight, update, error...: wait for it
  if (o.tries >= MAX_TRIES)
    plog("ble: ossm not in stroke engine after %u tries; pick it on the machine\n", o.tries);
}

static bool init(Ctx &c) {
  NimBLERemoteService *s = c.client->getService(NimBLEUUID(SVC));
  if (!s) {
    plog("ble: ossm %s has no OSSM service\n", c.advName);
    return false;
  }
  c.tx = s->getCharacteristic(NimBLEUUID(CMD));
  c.rx = s->getCharacteristic(NimBLEUUID(STATE));
  if (!c.tx || !c.rx) {
    plog("ble: ossm %s: command/state characteristic missing\n", c.advName);
    return false;
  }
  c.txNoResponse = c.tx->canWriteNoResponse();

  OssmState &o = c.os;
  portENTER_CRITICAL(&mux);
  o = OssmState();
  o.speed = -1;
  o.lastNotify = millis();
  portEXIT_CRITICAL(&mux);
  o.want = 0;
  o.sent = -1;  // the first tick sends 0
  o.lastSpeed = -1;
  o.phase = OSSM_ARMING;
  o.lastMode = millis() - MODE_MS;
  strcpy(c.name, "OSSM");
  c.caps = devices::CAP_SPEED | devices::CAP_STROKE;

  NimBLEConnInfo ci = c.client->getConnInfo();
  plog("ble: ossm %s: MTU %u, encrypted %s, bonded %s, write-no-response %s\n", c.advName,
       c.client->getMTU(), ci.isEncrypted() ? "yes" : "no", ci.isBonded() ? "yes" : "no",
       c.txNoResponse ? "yes" : "no");

  Ctx *pc = &c;
  bool sub = c.rx->canNotify() &&
             c.rx->subscribe(true, [pc](NimBLERemoteCharacteristic *, uint8_t *d, size_t n, bool) {
               onState(pc, d, n);
             });
  plog("ble: ossm subscribe to state %s\n", sub ? "ok" : "FAILED");
  // Without state notifications arming would run blind and press the mode
  // button (go:strokeEngine) again and again: not supported this time; a
  // remembered OSSM is tried again later.
  if (!sub) return false;

  // The whole state once (a read is not cut at the MTU), for the log.
  NimBLEAttValue v = c.rx->readValue();
  std::string raw((const char *)v.data(), v.size());
  logLong("state", raw.empty() ? "(empty)" : raw.c_str());
  JsonDocument doc;
  if (deserializeJson(doc, raw) == DeserializationError::Ok) {
    const char *st = doc["state"] | "";
    int speed = doc["speed"] | -1;
    plog("ble: ossm is in \"%s\", speed %d, stroke %d, depth %d, sensation %d, pattern %d\n", st,
         speed, (int)(doc["stroke"] | -1), (int)(doc["depth"] | -1),
         (int)(doc["sensation"] | -1), (int)(doc["pattern"] | -1));
    portENTER_CRITICAL(&mux);
    if (!o.state[0]) {
      strlcpy(o.state, st, sizeof o.state);
      o.speed = (int8_t)constrain(speed, -1, 100);
      o.seq = o.seq + 1;
    }
    portEXIT_CRITICAL(&mux);
  } else {
    plog("ble: ossm state is not JSON\n");
  }

  // Never resume: speed 0 before anything else.
  command(c, "set:speed:0");

  // Into stroke engine, waiting a little; tick() carries on if it takes longer
  // (homing after power-up, or preflight until the knob is at 0).
  uint32_t t0 = millis();
  while (millis() - t0 < INIT_MS && o.phase == OSSM_ARMING) {
    follow(c, millis(), true);
    if (o.phase != OSSM_ARMING) break;
    idle(10);
  }
  if (o.phase == OSSM_ARMING)
    plog("ble: ossm not in a play mode yet (\"%s\"); carrying on\n", o.seen);
  return true;
}

static void level(Ctx &c, int level, uint32_t) { c.os.want = (int8_t)constrain(level, 0, 100); }

// `quiet` (STOP): log only a failure. A write refused because the previous
// one (with response) is still in flight isn't logged either; it's retried.
static void sendSpeed(Ctx &c, int v, uint32_t now, bool quiet = false) {
  OssmState &o = c.os;
  char cmd[20];
  snprintf(cmd, sizeof cmd, "set:speed:%d", v);
  bool ok = writeStr(c, cmd);
  if (ok ? !quiet && o.logged < LOG_SPEEDS : !c.wrRefused) {
    o.logged++;
    plog("ble: ossm %s%s\n", cmd, ok ? "" : " write FAILED");
  }
  // Only a write that went out counts: a failed set:speed:0 (STOP) is sent
  // again on the next tick.
  o.sent = ok ? (int8_t)v : (int8_t)-1;
  o.lastSend = now;
}

static void tick(Ctx &c, uint32_t now) {
  follow(c, now, false);
  OssmState &o = c.os;
  if (o.sent != o.want && now - o.lastSend >= SEND_MS) sendSpeed(c, o.want, now);
}

static void stop(Ctx &c) {
  c.os.want = 0;
  sendSpeed(c, 0, millis(), true);
}

static void battery(Ctx &) {}  // mains powered

const Driver ossm = {'O', devices::Kind::Ossm, "OSSM", matches, init, level, tick, stop, battery};

}  // namespace toys
