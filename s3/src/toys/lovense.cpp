// Lovense toys over Bluetooth LE: text commands ("Vibrate:10;") on a UART-like
// TX characteristic, replies as notifications on RX.
//
// Protocol details from the Buttplug project, BSD-3-Clause, Copyright (c)
// 2016-2023 Nonpolynomial Labs, LLC (see THIRD_PARTY_NOTICES.md):
//   crates/buttplug_server_device_config/device-config/protocols/lovense.yml
//   crates/buttplug_server/src/device/protocol_impl/lovense/mod.rs,
//   lovense_max.rs, lovense_rotate_vibrator.rs, lovense_stroker.rs
// and the STPIHKAL Lovense notes (AutoSwith, Air:Level).
#include <Arduino.h>

#include "../log.h"
#include "toys.h"

namespace toys {

enum : uint8_t {
  LV_AIR = 1,          // Max: air pump, "Air:Level:N;" 0..3
  LV_ROT = 2,          // rotating head: "Rotate:N;" 0..20 as well
  LV_SOLACE = 4,       // Solace / Solace Pro: oscillation via "Mply:N:20;"
  LV_SOLACE_ZERO = 8,  // Solace (H): stop with the range zeroed too, "Mply:0:0;"
  LV_STROKE = 16,      // a thrusting machine rather than a vibrator (for caps)
};

// DeviceType code -> model, from lovense.yml (the "Lovense " prefix dropped).
// Codes not listed get the yml's defaults: one vibrator, 0..20.
struct Model {
  const char *code, *name;
  uint8_t flags;
};
static const Model MODELS[] = {
    {"B", "Max", LV_AIR},
    {"P", "Edge", 0},
    {"A", "Nora", LV_ROT},
    {"C", "Nora", LV_ROT},
    {"CB", "Velvo", LV_ROT},
    {"L", "Ambi", 0},
    {"S", "Lush", 0},
    {"AN", "Lush Anal", 0},
    {"Z", "Hush", 0},
    {"ZA", "Synth", 0},
    {"W", "Domi", 0},
    {"O", "Osci", 0},
    {"V", "Mission", 0},
    {"CA", "Mission 2", 0},
    {"X", "Ferri", 0},
    {"R", "Diamo", 0},
    {"ToyS", "Loveai Dolp", 0},
    {"QB", "Loveai Fizz", 0},
    {"F", "Sex Machine", LV_STROKE},
    {"FS", "Mini Sex Machine", LV_STROKE},
    {"J", "Dolce", 0},
    {"OC", "Osci 3", 0},
    {"ED", "Gush", 0},
    {"EZ", "Gush 2", 0},
    {"EB", "Hyphy", 0},
    {"T", "Calor", 0},
    {"EI", "Flexer", 0},  // firmware < 3: Buttplug wants an update for separate motors
    {"EI-FW3", "Flexer", 0},
    {"N", "Gemini", 0},
    {"EA", "Gravity", 0},
    {"Q", "Tenera", 0},
    {"EL", "Ridge", LV_ROT},
    {"U", "Lapis", 0},
    {"SD", "Vulse", 0},
    {"H", "Solace", LV_SOLACE | LV_SOLACE_ZERO | LV_STROKE},
    {"BA", "Solace Pro", LV_SOLACE | LV_STROKE},
    {"WD", "Spinel", 0},
};

// Lovense's own service family: XXXXXXXX-0023/0024-4bd4-bbd5-a6920e4c5653.
static bool familyUuid(const std::string &u) {
  return u.size() == 36 && u.compare(8, 4, "-002") == 0 && (u[12] == '3' || u[12] == '4') &&
         u.compare(13, 23, "-4bd4-bbd5-a6920e4c5653") == 0;
}

static const char *NUS = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";

static bool matches(const NimBLEAdvertisedDevice &adv) {
  std::string n = adv.getName();
  if (n.compare(0, 4, "LVS-") == 0 || n.compare(0, 5, "LOVE-") == 0) return true;
  // Company 0x026C followed by FF 21, as lovense.yml has it.
  for (uint8_t i = 0; i < adv.getManufacturerDataCount(); i++) {
    std::string m = adv.getManufacturerData(i);
    if (m.size() >= 4 && (uint8_t)m[0] == 0x6c && (uint8_t)m[1] == 0x02 && (uint8_t)m[2] == 0xff &&
        (uint8_t)m[3] == 0x21)
      return true;
  }
  for (uint8_t i = 0; i < adv.getServiceUUIDCount(); i++) {
    std::string u = adv.getServiceUUID(i).toString();
    if (familyUuid(u)) return true;
    // The Nordic UART service is used by plenty of non-Lovense gadgets, so on
    // its own it only counts for a device that advertises no name.
    if (u == NUS && n.empty()) return true;
  }
  return false;
}

// TX/RX on whichever of the three service layouts the toy has.
static bool findChars(Ctx &c) {
  NimBLERemoteService *svc = nullptr;
  NimBLEUUID txU, rxU;
  for (NimBLERemoteService *s : c.client->getServices(true)) {
    NimBLEUUID u = s->getUUID();
    std::string str = u.toString();
    if (u == NimBLEUUID((uint16_t)0xfff0)) {
      svc = s, txU = NimBLEUUID((uint16_t)0xfff2), rxU = NimBLEUUID((uint16_t)0xfff1);
    } else if (str == NUS) {
      svc = s, txU = NimBLEUUID("6e400002-b5a3-f393-e0a9-e50e24dcca9e"),
      rxU = NimBLEUUID("6e400003-b5a3-f393-e0a9-e50e24dcca9e");
    } else if (familyUuid(str)) {
      // TX/RX = service + 1 / + 2 in the first group (0001 -> 0002/0003, and
      // the odd 50300011 -> 50300012/50300013).
      unsigned long v = strtoul(str.substr(0, 8).c_str(), nullptr, 16);
      char b[40];
      snprintf(b, sizeof b, "%08lx%s", v + 1, str.c_str() + 8);
      txU = NimBLEUUID(std::string(b));
      snprintf(b, sizeof b, "%08lx%s", v + 2, str.c_str() + 8);
      rxU = NimBLEUUID(std::string(b));
      svc = s;
    } else {
      continue;
    }
    plog("ble: %s service %s\n", c.advName, str.c_str());
    break;
  }
  if (!svc) return false;
  c.tx = svc->getCharacteristic(txU);
  c.rx = svc->getCharacteristic(rxU);
  if (!c.tx || !c.rx) return false;
  c.txNoResponse = c.tx->canWriteNoResponse();
  return true;
}

static const Model *model(const char *code) {
  for (const Model &m : MODELS)
    if (!strcmp(m.code, code)) return &m;
  return nullptr;
}

// "Z:11:0082059AD3BD;" -> code "Z", firmware 11. false if it isn't one.
static bool parseType(const char *r, char *code, size_t n, int &fw) {
  size_t i = 0;
  while (r[i] && r[i] != ':' && i + 1 < n) {
    if (!isalnum((unsigned char)r[i])) return false;
    code[i] = r[i];
    i++;
  }
  code[i] = 0;
  if (!i || r[i] != ':') return false;
  fw = atoi(r + i + 1);
  return true;
}

// Fallback when the toy won't say: Buttplug's name regex LVS-([A-Z]+)\d+.
static bool codeFromName(const char *name, char *code, size_t n) {
  const char *p = strstr(name, "LVS-");
  if (!p) return false;
  p += 4;
  size_t i = 0;
  while (p[i] >= 'A' && p[i] <= 'Z' && i + 1 < n) code[i] = p[i], i++;
  code[i] = 0;
  return i && isdigit((unsigned char)p[i]);
}

static bool init(Ctx &c) {
  if (!findChars(c)) {
    plog("ble: %s has no Lovense TX/RX characteristics\n", c.advName);
    return false;
  }
  if (!subscribe(c, c.rx)) plog("ble: %s: subscribing to RX failed\n", c.advName);

  LovenseState &s = c.lv;
  s = LovenseState();
  s.sentVib = s.sentAir = s.sentRot = -1;

  // DeviceType; -> "code:firmware:mac;". 500 ms per try, as Buttplug does.
  char r[32], code[8] = "";
  int fw = 0;
  bool got = false;
  for (int i = 0; i < 6 && !got; i++) {
    if (!request(c, "DeviceType;", r, sizeof r, 500)) continue;
    plog("ble: %s DeviceType reply \"%s\"\n", c.advName, r);
    got = parseType(r, code, sizeof code, fw);
  }
  if (!got) {
    plog("ble: %s no DeviceType reply, going by the name\n", c.advName);
    if (!codeFromName(c.advName, code, sizeof code)) code[0] = 0;
  }
  if (!strcmp(code, "EI") && fw >= 3) strcpy(code, "EI-FW3");
  strlcpy(s.code, code, sizeof s.code);

  const Model *m = model(code);
  s.flags = m ? m->flags : 0;
  if (m) strlcpy(c.name, m->name, sizeof c.name);
  else if (code[0]) snprintf(c.name, sizeof c.name, "Lovense %s", code);
  else strcpy(c.name, "Lovense");
  c.caps = devices::CAP_SPEED | (s.flags & LV_STROKE ? devices::CAP_STROKE : devices::CAP_VIBRATE) |
           (s.flags & LV_AIR ? devices::CAP_AIR : 0);
  plog("ble: %s is a %s (code \"%s\", fw %d)\n", c.advName, c.name, code, fw);

  // Switch itself off if the link drops by accident (STPIHKAL: beta, seen on
  // Lush 2 / Hush / Domi). Not every toy knows it; the reply is only logged.
  if (request(c, "AutoSwith:On:Off;", r, sizeof r, 500))
    plog("ble: %s AutoSwith reply \"%s\"\n", c.advName, r);
  else
    plog("ble: %s no AutoSwith reply\n", c.advName);
  return true;
}

static void level(Ctx &c, int level, uint32_t) {
  LovenseState &s = c.lv;
  s.vib = level ? max(1, (level * 20 + 50) / 100) : 0;
  // Air 0..3 as Buttplug has it (STPIHKAL says 0..5); untested on a real Max.
  s.air = s.flags & LV_AIR ? (level * 3 + 50) / 100 : 0;
}

// Each records the value as sent only if the write went out; otherwise the
// sent value is left (or set to -1 by stop()) so tick() sends it again.
static bool sendVib(Ctx &c, int v) {
  char cmd[24];
  if (c.lv.flags & LV_SOLACE)
    snprintf(cmd, sizeof cmd, "Mply:%d:%d;", v, v == 0 && (c.lv.flags & LV_SOLACE_ZERO) ? 0 : 20);
  else
    snprintf(cmd, sizeof cmd, "Vibrate:%d;", v);  // all motors of a multi-motor toy
  if (!writeStr(c, cmd)) return false;
  c.lv.sentVib = v;
  return true;
}

static bool sendRot(Ctx &c, int v) {
  char cmd[16];
  snprintf(cmd, sizeof cmd, "Rotate:%d;", v);
  if (!writeStr(c, cmd)) return false;
  c.lv.sentRot = v;
  return true;
}

static bool sendAir(Ctx &c, int v) {
  char cmd[20];
  snprintf(cmd, sizeof cmd, "Air:Level:%d;", v);
  if (!writeStr(c, cmd)) return false;
  c.lv.sentAir = v;
  return true;
}

// One command per 100 ms at most; whatever is wanted by then goes out.
static void tick(Ctx &c, uint32_t now) {
  LovenseState &s = c.lv;
  if (now - s.lastSend < 100) return;
  if (s.sentVib != s.vib) sendVib(c, s.vib);
  else if ((s.flags & LV_ROT) && s.sentRot != s.vib) sendRot(c, s.vib);
  else if ((s.flags & LV_AIR) && s.sentAir != s.air) sendAir(c, s.air);
  else return;
  s.lastSend = now;  // a failed write is retried 100 ms later
}

static void stop(Ctx &c) {
  LovenseState &s = c.lv;
  s.vib = s.air = 0;
  if (!sendVib(c, 0)) s.sentVib = -1;
  if ((s.flags & LV_ROT) && !sendRot(c, 0)) s.sentRot = -1;
  if ((s.flags & LV_AIR) && !sendAir(c, 0)) s.sentAir = -1;
  s.lastSend = millis();
}

// "Battery;" -> "85;" (some answer "s85;"). Other replies in between (a late
// "OK;") are skipped; a timeout leaves the last reading.
static void battery(Ctx &c) {
  uint32_t seq = c.rxSeq;
  if (!writeStr(c, "Battery;")) return;
  c.lv.lastSend = millis();
  char r[32];
  uint32_t t0 = millis();
  while (millis() - t0 < 500) {
    idle(10);
    if (!reply(c, seq, r, sizeof r)) continue;
    const char *p = r[0] == 's' ? r + 1 : r;
    char *end;
    long v = strtol(p, &end, 10);
    if (end != p && *end == ';' && v >= 0 && v <= 100) {
      // A Lush just switched on answers "0;" at first (seen at 55 %): the
      // first reading after a connect only counts if it isn't 0.
      if (v == 0 && c.battery < 0) {
        plog("ble: %s battery \"0;\" right after connecting, asking again later\n", c.name);
        return;
      }
      plog("ble: %s battery \"%s\" -> %ld%%\n", c.name, r, v);
      c.battery = (int)v;
      return;
    }
    plog("ble: %s battery: ignoring \"%s\"\n", c.name, r);
  }
  plog("ble: %s battery: no reply\n", c.name);
}

const Driver lovense = {'L', devices::Kind::Lovense, "Lovense", matches, init, level, tick, stop, battery};

}  // namespace toys
