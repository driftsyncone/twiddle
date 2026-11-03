// Per-brand Bluetooth toy protocols, used by ble.cpp.
//
// A driver knows one protocol: which advertisements are its toys, how to set a
// toy up after connecting, and how to turn a 0..100 level into commands.
// Driver functions run on ble.cpp's two tasks, never on the UI thread:
// - level(), tick() and stop() on the "blesvc" task. They must not wait: write
//   only through toys::write()/writeStr(), which queue the write and return at
//   once, and never call blocking NimBLE functions (discovery, readValue(),
//   writeValue(..., true), subscribe()). STOP depends on this.
// - init() and battery() on the "ble" worker task. They may block and wait
//   (toys::idle(), toys::request()); the other toys keep running meanwhile.
//   stop() can run on the same toy while its battery() waits.
#pragma once
#include <NimBLEDevice.h>
#include <stdint.h>

#include "../devices.h"

namespace toys {

// Lovense per-toy state.
struct LovenseState {
  char code[8];      // DeviceType code ("S", "B", "EI-FW3"), "" unknown
  uint8_t flags;     // LV_* from lovense.cpp
  int8_t vib, air;   // wanted: Vibrate 0..20, Air 0..3
  int8_t sentVib, sentAir, sentRot;  // last sent, -1 unknown
  uint32_t lastSend; // millis() of the last command, for the 100 ms rate limit
};

// Kiiroo Keon per-toy state.
struct KeonState {
  uint8_t variant;   // KEON_* from kiiroo.cpp
  uint8_t lo, hi;    // stroke ends (positions 0..99)
  uint8_t target;    // where the current half stroke goes
  uint8_t from;      // where it started (for the position estimate on STOP)
  bool moving;       // strokes are being sent
  bool stopPending;  // the stop packet failed to go out: tick() retries it
  uint8_t stopPos;   // Keon 1: where STOP parks it (estimated once per STOP)
  int level;         // 0..100, read at each half-stroke boundary
  uint32_t start;    // millis() when the current half stroke began
  uint32_t boundary; // millis() when the current half stroke ends
  uint32_t lastWrite;
  uint8_t last[4];   // last packet, for the keepalive
  uint8_t lastLen;
};

// OSSM per-toy state. The first three fields are written by the NimBLE host
// task from state notifications (under ossm.cpp's spinlock).
struct OssmState {
  char state[32];        // latest "state" from the machine ("strokeEngine.idle"), "" none yet
  int8_t speed;          // latest "speed" it reports, -1 unknown
  uint32_t seq;          // bumps with every state notification
  uint32_t lastNotify;   // millis() of the last one
  // BLE task only
  char seen[32];         // the state tick() last acted on
  uint32_t seenSeq;
  int8_t want, sent;     // speed 0..100: wanted / last sent (-1: send again)
  int8_t lastSpeed;      // last reported speed that was logged
  uint8_t phase;         // OSSM_* from ossm.cpp
  uint8_t tries;         // mode-switch commands sent while arming
  uint8_t logged;        // speed commands logged so far (the first few only)
  bool silent;           // "no state notification" already logged
  uint32_t lastSend, lastMode, lastSpeedLog;
};

// One connected (or connecting) toy. ble.cpp owns it; drivers fill in the
// characteristics, name, caps and battery.
struct Ctx {
  NimBLEClient *client;
  char advName[32];  // the advertised name ("LVS-Z36D", "KEON2")
  NimBLERemoteCharacteristic *tx, *rx, *aux;  // aux: battery characteristic (Keon 2)
  bool txNoResponse;   // tx takes writes without response
  // Writes with response (tx without write-no-response): at most one in flight
  // per toy. Set by write(), cleared by the host task when the reply comes.
  volatile bool wrPending;
  bool wrRefused;      // the last write() was refused because one was in flight
  char name[24];       // model name for the UI ("Lush", "Keon 2")
  uint8_t caps;        // devices::Cap bits
  volatile int battery;  // 0..100, -1 unknown
  // Notifications on rx: the latest one, as a NUL-terminated string (Lovense
  // replies are text). Written by the NimBLE host task; read with reply().
  char rxBuf[32];
  volatile uint32_t rxSeq;
  union {
    LovenseState lv;
    KeonState kn;
    OssmState os;
  };
};

struct Driver {
  char id;              // stored in NVS: 'L' Lovense, 'K' Keon, '2' Keon 2 / Keon WiFi, 'O' OSSM, 'H' heart rate monitor
  devices::Kind kind;
  const char *brand;    // for logs
  // An advertisement from a scan: one of ours?
  bool (*matches)(const NimBLEAdvertisedDevice &adv);
  // After connecting (services not yet discovered): find the characteristics,
  // subscribe, identify the model, set name and caps. false = not supported.
  bool (*init)(Ctx &c);
  // A new level 0..100 (0 = stop). Takes effect from the next tick() on.
  void (*level)(Ctx &c, int level, uint32_t now);
  // Called every few ms while connected: sends what changed, strokes, keepalives.
  void (*tick)(Ctx &c, uint32_t now);
  // Stop now, ahead of anything queued.
  void (*stop)(Ctx &c);
  // Read the battery into c.battery (every 60 s). Leave it alone on a timeout.
  void (*battery)(Ctx &c);
};

extern const Driver lovense, keon, keon2, ossm, hrmDriver;  // hrmDriver: a heart rate monitor, not a toy

// The driver whose toys look like `adv`, or nullptr.
const Driver *match(const NimBLEAdvertisedDevice &adv);
const Driver *byId(char id);

// ---- helpers for drivers (implemented in ble.cpp) ----

// Write to c.tx (without response if it allows that, else with) without
// waiting: true if the write was queued. A write with response is refused
// (false, c.wrRefused) while the previous one is still waiting for its reply;
// drivers just try again on a later tick with whatever is wanted by then.
bool write(Ctx &c, const uint8_t *data, size_t len);
bool writeStr(Ctx &c, const char *s);
// Subscribe to notifications on `chr`; they land in c.rxBuf / c.rxSeq.
bool subscribe(Ctx &c, NimBLERemoteCharacteristic *chr);
// Copy the latest notification into `out` if it is newer than `seq`; updates seq.
bool reply(Ctx &c, uint32_t &seq, char *out, size_t n);
// Send a text command and wait up to `ms` for the next notification.
bool request(Ctx &c, const char *cmd, char *out, size_t n, uint32_t ms);
// Wait `ms` (worker task only: init() and battery()).
void idle(uint32_t ms);

}  // namespace toys
