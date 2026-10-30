// Bluetooth LE toys (Lovense, Kiiroo Keon, OSSM), driven straight from the panel.
//
// Like autoblow::, the caller only sets what it wants (a level per toy); a BLE
// task on core 0 owns the radio, connects, sends whatever changed and reads
// battery levels. Nothing here blocks the caller.
//
// Protocols follow the Buttplug project's device definitions (BSD-3-Clause,
// see ../THIRD_PARTY_NOTICES.md); the per-brand code lives in toys/.
#pragma once
#include <stdint.h>

#include "devices.h"

namespace ble {

struct Toy {
  devices::Kind kind;
  char name[24];    // model name, e.g. "Lush 3"; the advertised name until known
  char addr[18];    // "aa:bb:cc:dd:ee:ff"
  uint8_t caps;     // devices::Cap bits
  bool known;       // remembered in NVS: connected again whenever it is in range
  bool connected;
  bool connecting;
  int battery;      // 0..100, -1 unknown
  int rssi;         // last seen, 0 unknown
  int level;        // 0..100 last sent to the toy
  uint32_t drops;   // how often an established link dropped
  bool held;        // connected but held at 0 until the panel's level has been 0 once
  char status[48];  // "Ready", "Connecting...", "Out of range", "Not a supported toy", ...
};

void begin();  // starts the stack and the tasks (if wanted, see enabled()); reconnects remembered toys
// Bluetooth runs this boot: Settings has it on, or (auto) a toy is remembered.
// Off: Wi-Fi runs without modem sleep, and scan / connect do nothing.
bool enabled();

int count();       // remembered toys first, then new ones from the last scan
Toy toy(int i);

void scan(uint32_t ms);
bool scanning();

void connect(int i);  // connect and remember
void forget(int i);   // disconnect (stopping it first) and forget

// 0..100; 0 = stop. The task sends the latest value only; a toy that can't
// keep up (Keon) just skips steps. Ignored while not connected.
void setLevel(int i, int level);
// Stop every toy now (STOP button, loss): sent ahead of anything else.
void stopAll();

}  // namespace ble
