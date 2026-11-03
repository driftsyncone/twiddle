#include "encoder.h"

#include "board.h"

#if !BOARD_HAS_P4_I2C
// Waveshare ESP32-S3-Touch-LCD-5: GPIO17/18 drive the display (no free I2C connector), so there is nothing to talk to.
namespace encoder {
void begin() {}
void loop() {}
bool present() { return false; }
bool ownsP4() { return false; }
uint8_t address() { return 0; }
bool isEncoderAt(uint8_t) { return false; }
}  // namespace encoder
#else

#include <Arduino.h>
#include <Wire.h>

#include "battery.h"
#include "control.h"
#include "log.h"
#include "p4bus.h"
#include "seesaw.h"
#include "settings.h"

namespace encoder {

using namespace seesaw;

static const uint32_t POLL_MS = 40, REPROBE_MS = 2000, HOLD_LIGHT_MS = 800;
static const int MAX_FAILS = 3;

enum class State : uint8_t { Disabled, Searching, Active };
static State state = State::Disabled;
static bool reserved;       // see ownsP4()
static uint8_t addr;        // 0 = none
static uint8_t lastAddr;    // where it was last seen (reprobe there first)
static int fails;
static uint32_t lastPoll, lastProbe, holdLightUntil;
static Clicks clicks;
static Button button;
static int shown = -1;      // last light written (Light value), -1 = unknown

// ---------- I2C transport (Wire1 on GPIO17/18; every transfer is bounded by Wire1.setTimeOut, see p4bus.h) ----------

static bool wr(uint8_t a, const uint8_t *buf, size_t n) {
  Wire1.beginTransmission(a);
  Wire1.write(buf, n);
  return Wire1.endTransmission() == 0;
}

static bool rd(uint8_t a, uint8_t base, uint8_t reg, uint8_t *out, size_t n) {
  uint8_t f[2];
  size_t fl = readPrefix(base, reg, f);
  if (!wr(a, f, fl)) return false;
  delayMicroseconds(250);  // the seesaw wants ~125 us between the register write and the read
  if (Wire1.requestFrom(a, (uint8_t)n) != n) return false;
  for (size_t i = 0; i < n; i++) out[i] = Wire1.read();
  return true;
}

// ACK and the seesaw HW ID and product id, not just an ACK: a MAX17048 gauge also answers at 0x36.
static bool probe(uint8_t a) {
  Wire1.beginTransmission(a);
  if (Wire1.endTransmission() != 0) return false;
  uint8_t id = 0, ver[4] = {0, 0, 0, 0};
  if (!rd(a, STATUS_BASE, STATUS_HW_ID, &id, 1) || !validHwId(id)) return false;
  if (!rd(a, STATUS_BASE, STATUS_VERSION, ver, 4)) return false;
  if (!isEncoderBoard(id, ver)) {
    plog("encoder: seesaw (hw id 0x%02x, product %u) at 0x%02x is not a known rotary encoder board, ignored\n", id,
         (unsigned)productFromVersion(ver), a);
    return false;
  }
  return true;
}

static void showLight(Light l) {
  if ((int)l == shown) return;
  Rgb c = lightColor(l);
  uint8_t f[8];
  size_t n = npColor(c.r, c.g, c.b, f);
  bool ok = wr(addr, f, n);
  n = npShow(f);
  ok = wr(addr, f, n) && ok;
  if (ok) shown = (int)l;
}

static bool connect(uint8_t a) {
  uint8_t f[8], b[3][6];
  addr = a;
  buttonSetupFrames(b);
  for (int i = 0; i < 3; i++)
    if (!wr(a, b[i], 6)) return false;
  if (!wr(a, f, npSpeed(f)) || !wr(a, f, npLength(f)) || !wr(a, f, npPin(f))) return false;
  clicks.reset();  // the first reading after connect is only a reference
  button.reset();
  shown = -1;
  fails = 0;
  lastAddr = a;
  state = State::Active;
  showLight(Light::Off);
  plog("encoder: seesaw rotary encoder found at 0x%02x\n", a);
  return true;
}

static bool search() {
  lastProbe = millis();
  if (lastAddr && probe(lastAddr) && connect(lastAddr)) return true;
  for (uint8_t a = ADDR_DEFAULT; a <= ADDR_LAST; a++)
    if (a != lastAddr && probe(a) && connect(a)) return true;
  addr = 0;
  return false;
}

static void lost() {
  plog("encoder: lost (unplugged?), looking again every %lu s\n", (unsigned long)(REPROBE_MS / 1000));
  state = State::Searching;
  addr = 0;
  lastProbe = millis();
}

static bool batteryEmpty() {
  battery::Info b = battery::info();
  return b.present && !b.charging && b.percent <= 5;  // same condition as the low-battery stop in ui.cpp
}

static Light wantedLight(uint32_t now) {
  if ((int32_t)(holdLightUntil - now) > 0) return Light::Hold;
  if (control::encoderPaused()) return Light::Paused;
  switch (control::active()) {
    case control::Active::Remote:
    case control::Active::Autopilot: return Light::Running;
    case control::Active::Script: return control::scriptPlaying() ? Light::Running : Light::Paused;
    default: return Light::Off;
  }
}

void begin() {
  if (settings.encoder == 0) return;  // off: touch nothing, GPIO17 stays free for the battery divider
  if (!p4bus::begin()) return;
  bool found = search();
  reserved = settings.encoder == 1 || found;
  if (!found && settings.encoder == 1) state = State::Searching;  // On: keep looking
  if (!found && !reserved) p4bus::end();                           // Auto, nothing there: the bus goes back to the battery code
  if (!found) plog("encoder: none found at boot (%s)\n", settings.encoder == 1 ? "mode On, will keep looking" : "mode Auto, off");
}

void loop() {
  if (state == State::Disabled) return;
  uint32_t now = millis();
  if (state == State::Searching) {
    if (now - lastProbe >= REPROBE_MS) search();
    return;
  }
  // Active
  Light want = wantedLight(now);
  if (now - lastPoll < POLL_MS) {
    showLight(want);  // writes only on change
    return;
  }
  lastPoll = now;
  uint8_t pos[4], gp[4];
  if (!rd(addr, ENCODER_BASE, ENCODER_POSITION, pos, 4) || !rd(addr, GPIO_BASE, GPIO_BULK, gp, 4)) {
    if (++fails >= MAX_FAILS) {
      lost();
      // A button that was held when it vanished must not leave anything half-done: nothing is, STOP only fires on a completed hold.
    }
    return;
  }
  fails = 0;
  int n = clicks.feed(i32be(pos));
  Press p = button.update(buttonPressed(gp), now);
  bool empty = batteryEmpty();  // low battery: no starting / speeding up from the encoder; STOP still works
  if (p == Press::Hold) {
    control::stopAll();
    holdLightUntil = now + HOLD_LIGHT_MS;
    want = Light::Hold;
  } else if (!empty) {
    if (n != 0 && !button.held()) control::encoderAdjust(n * settings.encStep * (settings.encInvert ? -1 : 1));
    if (p == Press::Short) control::encoderTogglePause();
    want = wantedLight(now);
  }
  showLight(want);
}

bool present() { return state == State::Active; }
bool ownsP4() { return reserved; }
uint8_t address() { return state == State::Active ? addr : 0; }
bool isEncoderAt(uint8_t a) {
  return p4bus::started() && probe(a);
}

}  // namespace encoder
#endif
