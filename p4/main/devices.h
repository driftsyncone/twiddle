// Every toy the panel drives. control:: works out one output (the same
// autoblow::Desired it always has); devices::apply() fans it out: the Autoblow
// device gets it as is, each Bluetooth toy gets the speed as a 0..100 level,
// scaled to its own range. Only devices that are switched on follow.
//
// Device 0 is always the Autoblow device (cloud, needs a token). Devices 1..
// are Bluetooth toys: the ones remembered from earlier plus whatever the last
// scan found. Indexes are stable until the next scan or forget().
//
// Everything here runs on the loop task (LVGL thread), like control::.
#pragma once
#include <stdint.h>

#include "autoblow.h"

namespace devices {

enum class Kind : uint8_t { Autoblow, Lovense, Kiiroo, Ossm, Hrm };  // Hrm: heart rate monitor, an input, not a toy

// What a device can do, for the UI to show the right controls.
enum Cap : uint8_t {
  CAP_SPEED = 1,    // speed/strength 0..100 (all of them)
  CAP_VALVES = 2,   // VacuGlide: Tighter/Looser valves, vacuum, local scripts
  CAP_VIBRATE = 4,  // vibration motor(s)
  CAP_STROKE = 8,   // linear stroker (Keon): level = stroke speed
  CAP_AIR = 16,     // Lovense Max: air pump (follows the level)
  CAP_HR = 32,      // heart rate monitor: reports bpm, takes no output (no CAP_SPEED)
};

struct Info {
  Kind kind;
  char name[24];     // "VacuGlide", "Lush 3", "Keon"
  char addr[18];     // Bluetooth address, "" for the cloud device
  uint8_t caps;      // Cap bits
  bool known;        // remembered (connects by itself at boot / when in range)
  bool connected;    // link up (cloud: reachable; Bluetooth: connected)
  bool connecting;
  bool enabled;      // follows the panel's output
  int scale;         // 10..100: % of the output this device gets
  int battery;       // 0..100, -1 unknown
  int level;         // 0..100 it is running at now (0 = stopped)
  int rssi;          // Bluetooth signal, 0 unknown
  int bpm;           // CAP_HR devices: last heart rate, 0 unknown or stale
  char status[48];   // one line for the UI: "Ready", "Connecting...", "Out of range", errors
};

void begin();  // after autoblow::begin() and settingsLoad()
void loop();   // from control:: 100 ms tick (loop task)

int count();
Info info(int i);

void setEnabled(int i, bool on);  // saved
void setScale(int i, int pct);    // saved
// Bluetooth: connect and remember (connects again at boot), or disconnect and forget.
void connect(int i);
void forget(int i);

// Look for Bluetooth toys for `ms`; found ones appear in count()/info().
void scan(uint32_t ms = 8000);
bool scanning();

// control:: calls this every 100 ms tick and on every change.
void apply(const autoblow::Desired &d);

// STOP: resend the stop to everything, ahead of anything else (after apply()
// has been given an idle state).
void stopNow();

// Bumps when a switched-on device drops out while running; control:: stops
// everything when it changes, as it always has for the VacuGlide.
uint32_t lost();
// Which device was lost last, for the banner ("Lush 3"); "" if none.
const char *lostName();
void ackLost();

// True if any switched-on device is moving (for the screen-sleep rule and the
// lost banner). VacuGlide: from the cloud state; Bluetooth: level > 0.
bool anyMoving();

}  // namespace devices
