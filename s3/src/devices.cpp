#include "devices.h"

#include <Arduino.h>
#include <Preferences.h>

#include "ble.h"
#include "hrm.h"
#include "settings.h"

namespace devices {

// Per device: switched on + scale, in NVS keyed by Bluetooth address without
// colons ("ab" for the Autoblow device). Packed as on << 8 | scale.
static const char *NS = "devices";
static const int MAX_SEEN = 16;  // Bluetooth toys tracked for loss detection

struct Prefs { bool on; int scale; };

static Prefs abPrefs = {true, 100};
static uint32_t abLostSeen;

// Per Bluetooth toy, keyed by address since indexes move with every scan: its
// prefs (read from NVS once), and what it was last asked for, to tell a drop
// mid-run from an idle one.
struct Seen {
  char addr[18];
  bool loaded;
  Prefs prefs;
  bool up;  // connected when last looked at
  uint32_t drops;
  int asked;
};
static Seen seen[MAX_SEEN];

static uint32_t lostCount, lostAcked;
static char lostWho[24];
static autoblow::Desired lastD;

static void key(const char *addr, char *out) {
  if (!*addr) { strcpy(out, "ab"); return; }
  int n = 0;
  for (const char *p = addr; *p && n < 12; p++)
    if (*p != ':') out[n++] = *p;
  out[n] = 0;
}

static Prefs readPrefs(const char *addr, bool defOn) {
  char k[16];
  key(addr, k);
  Preferences p;
  p.begin(NS, true);
  int v = p.getUShort(k, (defOn ? 1 << 8 : 0) | 100);
  p.end();
  return {(v >> 8) != 0, constrain(v & 0xff, 10, 100)};
}

static Seen &seenFor(const char *addr);

// Bluetooth toys: from the cache, NVS only the first time. apply() and the UI
// ask for these several times a second.
static Prefs loadPrefs(const char *addr) {
  Seen &s = seenFor(addr);
  if (!s.loaded) {
    s.prefs = readPrefs(addr, false);
    s.loaded = true;
  }
  return s.prefs;
}

// Flash writes glitch the display: only when something changed.
static void savePrefs(const char *addr, const Prefs &pr) {
  if (*addr) {
    Seen &s = seenFor(addr);
    if (s.loaded && s.prefs.on == pr.on && s.prefs.scale == pr.scale) return;
    s.prefs = pr;
    s.loaded = true;
  }
  char k[16];
  key(addr, k);
  Preferences p;
  p.begin(NS, false);
  p.putUShort(k, (pr.on ? 1 << 8 : 0) | pr.scale);
  p.end();
}

static Seen &seenFor(const char *addr) {
  for (Seen &s : seen)
    if (!strcmp(s.addr, addr)) return s;
  for (Seen &s : seen)
    if (!s.addr[0]) {
      strlcpy(s.addr, addr, sizeof s.addr);
      return s;
    }
  static Seen spare;  // table full (prune() keeps it from happening): track nothing
  spare = Seen();
  return spare;
}

// ---------- the Autoblow device ----------

static const char *modeText(const autoblow::Status &st) {
  const char *m = st.mode;
  if (!strcmp(m, "LOCAL_SCRIPT_PLAYING")) return "Script playing";
  if (!strcmp(m, "LOCAL_SCRIPT_PAUSED")) return "Script paused";
  if (!strcmp(m, "FIRMWARE_UPDATING")) return "Updating firmware";
  if (!strcmp(m, "EMERGENCY_STOP")) return "Emergency stop on the device";
  return st.moving ? "Running" : "Ready";
}

static bool abMoving(const autoblow::Status &st) { return st.online && st.moving; }

// Unknown until the first discovery: assume the VacuGlide, as before.
static bool isVacuGlide(autoblow::Type t) {
  return t == autoblow::Type::VacuGlide || t == autoblow::Type::Unknown;
}

static Info abInfo() {
  autoblow::Status st = autoblow::status();
  Info in = {};
  in.kind = Kind::Autoblow;
  strlcpy(in.name, st.type == autoblow::Type::Unknown ? "VacuGlide" : autoblow::typeName(st.type),
          sizeof in.name);
  in.caps = CAP_SPEED | (isVacuGlide(st.type) ? CAP_VALVES : 0);
  in.known = true;
  in.connected = st.online;
  in.connecting = st.wifi && !st.online && settings.token.length();
  in.enabled = abPrefs.on;
  in.scale = abPrefs.scale;
  in.battery = -1;
  in.level = abMoving(st) ? st.targetSpeed : 0;
  if (!settings.token.length()) strcpy(in.status, "Set the token in Settings");
  else if (!st.wifi) strcpy(in.status, "No Wi-Fi");
  else if (!st.online) strlcpy(in.status, st.error[0] ? st.error : "Connecting...", sizeof in.status);
  else strlcpy(in.status, modeText(st), sizeof in.status);
  return in;
}

// ---------- API ----------

void begin() {
  // Opening read-write creates the namespace, so read-only opens later don't
  // log "nvs_open failed: NOT_FOUND" on every call.
  Preferences p;
  p.begin(NS, false);
  p.end();
  abPrefs = readPrefs("", true);
  abLostSeen = autoblow::status().lost;
  ble::begin();
}

int count() { return 1 + ble::count(); }

Info info(int i) {
  if (i <= 0) return abInfo();
  ble::Toy t = ble::toy(i - 1);
  Prefs pr = loadPrefs(t.addr);
  Info in = {};
  in.kind = t.kind;
  strlcpy(in.name, t.name, sizeof in.name);
  strlcpy(in.addr, t.addr, sizeof in.addr);
  in.caps = t.caps;
  in.known = t.known;
  in.connected = t.connected;
  in.connecting = t.connecting;
  in.enabled = pr.on;
  in.scale = pr.scale;
  in.battery = t.battery;
  in.level = t.level;
  in.rssi = t.rssi;
  strlcpy(in.status, t.status, sizeof in.status);
  if (t.kind == Kind::Hrm) {  // an input: nothing follows, nothing is scaled
    in.enabled = false;
    in.level = 0;
    in.bpm = t.connected && hrm::ageMs() < 10000 ? hrm::bpm() : 0;
  }
  return in;
}

void setEnabled(int i, bool on) {
  if (i > 0 && ble::toy(i - 1).kind == Kind::Hrm) return;  // an input, nothing to switch
  if (i <= 0) {
    abPrefs.on = on;
    savePrefs("", abPrefs);
  } else {
    ble::Toy t = ble::toy(i - 1);
    Prefs pr = loadPrefs(t.addr);
    pr.on = on;
    savePrefs(t.addr, pr);
  }
  apply(lastD);
}

void setScale(int i, int pct) {
  pct = constrain(pct, 10, 100);
  if (i > 0 && ble::toy(i - 1).kind == Kind::Hrm) return;
  if (i <= 0) {
    abPrefs.scale = pct;
    savePrefs("", abPrefs);
  } else {
    ble::Toy t = ble::toy(i - 1);
    Prefs pr = loadPrefs(t.addr);
    pr.scale = pct;
    savePrefs(t.addr, pr);
  }
  apply(lastD);
}

void connect(int i) {
  if (i <= 0) return;
  ble::Toy t = ble::toy(i - 1);
  // A toy connected on purpose follows the panel unless it was switched off before.
  char k[16];
  key(t.addr, k);
  Preferences p;
  p.begin(NS, true);
  bool had = p.isKey(k);
  p.end();
  if (!had) savePrefs(t.addr, {true, 100});
  ble::connect(i - 1);
}

void forget(int i) {
  if (i <= 0) return;
  // Gone from the list at once, so loop() never sees it disconnect: drop its
  // entry, or a re-paired toy (drops from 0 again) would read as a loss.
  ble::Toy t = ble::toy(i - 1);
  for (Seen &s : seen)
    if (!strcmp(s.addr, t.addr)) s = Seen();
  ble::forget(i - 1);
}

// Entries for toys that are no longer listed (scan results that went away,
// random-address gadgets): at most 8 are listed, so the table never fills.
static void prune() {
  for (Seen &s : seen) {
    if (!s.addr[0]) continue;
    bool listed = false;
    for (int i = 0; i < ble::count() && !listed; i++) listed = !strcmp(ble::toy(i).addr, s.addr);
    if (!listed) s = Seen();
  }
}

void scan(uint32_t ms) { ble::scan(ms); }
bool scanning() { return ble::scanning(); }

void apply(const autoblow::Desired &d) {
  lastD = d;

  // The Autoblow device: everything, as before, but only while switched on.
  autoblow::Desired a;
  if (abPrefs.on) {
    a = d;
    if (a.want == autoblow::Want::Speed) a.speed = max(5, a.speed * abPrefs.scale / 100);
  }
  autoblow::Desired cur = autoblow::desired();
  if (cur.want != a.want || cur.speed != a.speed || cur.script != a.script ||
      cur.scriptPlaying != a.scriptPlaying || cur.valvePlus != a.valvePlus ||
      cur.valveMinus != a.valveMinus || cur.pulseSeq != a.pulseSeq)
    autoblow::setDesired(a);

  // Bluetooth toys: the speed as a level. A VacuGlide script has no level to
  // share, so they rest while one plays.
  int level = d.want == autoblow::Want::Speed ? d.speed : 0;
  for (int i = 0; i < ble::count(); i++) {
    ble::Toy t = ble::toy(i);
    if (t.kind == Kind::Hrm) {  // a heart rate monitor never gets output
      ble::setLevel(i, 0);
      continue;
    }
    Prefs pr = loadPrefs(t.addr);
    int l = pr.on && level ? max(1, level * pr.scale / 100) : 0;
    // Only a toy that really runs counts as asked: one held at 0 after a
    // reconnect dropping out again isn't a loss (and mustn't stop the session).
    if (t.connected) seenFor(t.addr).asked = t.held ? 0 : l;
    ble::setLevel(i, l);
  }
}

void loop() {
  static uint32_t lastPrune;
  if (millis() - lastPrune > 1000) {  // a scan can swap toys without changing the count
    lastPrune = millis();
    prune();
  }
  // The Autoblow device dropped off: a loss only if it was switched on. Either
  // way its task waits for the ack before it sends anything but STOP.
  autoblow::Status st = autoblow::status();
  if (st.lost != abLostSeen) {
    abLostSeen = st.lost;
    if (abPrefs.on) {
      lostCount++;
      strlcpy(lostWho, abInfo().name, sizeof lostWho);
    } else {
      autoblow::ackLost();
    }
  }
  // A Bluetooth toy whose link dropped while it was asked to run.
  for (int i = 0; i < ble::count(); i++) {
    ble::Toy t = ble::toy(i);
    if (!t.known || t.kind == Kind::Hrm) continue;  // a monitor dropping out is no loss of output
    Seen &s = seenFor(t.addr);
    // (Re)connected: start counting from here. A forgotten and re-paired toy
    // starts its drops at 0 again, which mustn't read as a loss.
    if (t.connected && !s.up) {
      s.up = true;
      s.drops = t.drops;
      s.asked = 0;
      continue;
    }
    if (!t.connected) s.up = false;
    if (t.drops != s.drops) {
      s.drops = t.drops;
      if (s.asked > 0) {
        lostCount++;
        strlcpy(lostWho, t.name, sizeof lostWho);
      }
      s.asked = 0;
    }
  }
}

uint32_t lost() { return lostCount; }
const char *lostName() { return lostWho; }

void ackLost() {
  lostAcked = lostCount;
  autoblow::ackLost();
}

void stopNow() {
  ble::stopAll();
  autoblow::resync();
}

bool anyMoving() {
  if (abPrefs.on && abMoving(autoblow::status())) return true;
  for (int i = 0; i < ble::count(); i++) {
    ble::Toy t = ble::toy(i);
    if (t.kind != Kind::Hrm && t.connected && t.level > 0) return true;
  }
  return false;
}

bool vacuGlideEnabled() { return abPrefs.on && isVacuGlide(autoblow::status().type); }

}  // namespace devices
