// Session logic: Remote, the Autopilot wave generator, local scripts, valves, vacuum pulses, STOP (see control.h).
#include "control.h"

#include <Arduino.h>
#include <esp_random.h>
#include <esp_system.h>
#include <lvgl.h>
#include <math.h>

#include <algorithm>

#include "autoblow.h"
#include "devices.h"
#include "settings.h"

namespace control {

using std::max;
using std::min;

using autoblow::Want;

static Active act = Active::None;
static bool plusHeld, minusHeld;  // stroke-plus (Tighter), stroke-minus (Looser)

// Vacuum maintenance pulse: briefly opens the Tighter (stroke-plus) valve. The network task
// times it (see autoblow::Desired); here it is just a counted request.
static uint32_t pulseSeq;
static uint16_t pulseMs;

// ---------- remote ----------
// Vacuum maintenance: open the Tighter (stroke-plus) valve for pulseMs every intervalMs.
struct Vac { uint16_t pulseMs; uint16_t intervalMs; };  // also used by Autopilot below
static const Vac REMOTE_VAC[] = {{0, 0}, {300, 12000}, {400, 9000}, {500, 6000}};  // off low medium high
static int rSpeed = 20, rVac = 0;
static bool finishing;  // "Finish Me" runs on the remote path without vacuum pulses
static uint32_t rLastPulse;

// ---------- autopilot ----------
// Our own edging generator (not the Autoblow web app's patterns): a session is
// WAVES waves. Each climbs from the level's floor to a peak, rides the peak,
// then drops back to the floor and rests. Peaks rise as the session goes on.
static const int WAVES = 8;
struct Level { int floor, ceiling; };
static const Level LEVELS[] = {{8, 25}, {10, 40}, {20, 70}, {35, 100}};  // warmup low medium high
struct EdgeStyle {
  uint16_t peakMinMs, peakMaxMs;  // time spent at the peak
  uint16_t restMinMs, restMaxMs;  // time at the floor after the drop
  uint8_t wobble;                 // random dip below the peak while holding it
};
static const EdgeStyle EDGE_STYLES[] = {
    {5000, 9000, 15000, 25000, 12},  // gentle: short peaks, long rests
    {10000, 16000, 10000, 15000, 8},  // moderate
    {15000, 25000, 5000, 9000, 4},  // intense: long peaks, short rests
};
// Vacuum maintenance while Autopilot runs: pulse length and interval (off low high).
static const Vac AP_VAC[] = {{0, 0}, {250, 4000}, {400, 2500}};

// A session is planned as WAVES waves up front (so progress is known), then
// played as a state machine driven by the clock: climb in steps to the peak,
// hold it (with a little wobble), drop to the floor and rest. Changing a
// setting re-plans the waves but keeps the position, so it blends in.
enum class Phase : uint8_t { Climb, Hold, Rest };
struct Wave { int peak; uint8_t steps; uint32_t climbMs, holdMs, restMs; };
static Wave waves[WAVES];
static uint32_t sessionMs;  // planned length of the session
static bool apPlaying, apPlanned;
static int apIntensity = 2, apEdge = 1, apVac = 0;
static int apWave;
static Phase apPhase;
static uint32_t apPhaseMs, apElapsed, apLastTick, apLastSuction;
static int apWobble, apWobbleSlot = -1;
static int apSpeed;
static int apLength, apSurprise;        // see control.h
static int customPeak[WAVES];           // 0 = planner's choice
static const uint32_t LENGTH_MS[] = {0, 5 * 60000, 10 * 60000, 20 * 60000, 30 * 60000};

// ---------- local scripts ----------
static int sSlot = -1;
static bool sPlaying;

static uint32_t lostSeen;

// ---------- desired state ----------

static void push() {
  autoblow::Desired d;
  switch (act) {
    case Active::Remote:
      d.want = Want::Speed;
      d.speed = finishing ? 100 : rSpeed;
      break;
    case Active::Autopilot:
      d.want = Want::Speed;
      d.speed = apSpeed;
      break;
    case Active::Script:
      d.want = Want::Script;
      d.script = sSlot;
      d.scriptPlaying = sPlaying;
      break;
    case Active::None:
      break;
  }
  d.valvePlus = plusHeld;
  d.valveMinus = minusHeld;
  d.pulseSeq = pulseSeq;
  d.pulseMs = pulseMs;
  devices::apply(d);
}

static void startPulse(uint32_t ms) {
  pulseSeq++;
  pulseMs = constrain(ms, 50, 2000);
}

// Leave whatever mode is running.
static void leave() {
  apPlaying = false;
  finishing = false;
  act = Active::None;
}

// ---------- autopilot engine ----------

static int rnd(int lo, int hi) { return lo + esp_random() % (hi - lo + 1); }

static void plan() {
  const Level &lv = LEVELS[apIntensity];
  const EdgeStyle &es = EDGE_STYLES[apEdge];
  int peaks[WAVES];
  for (int w = 0; w < WAVES; w++) {
    float build = 0.6f + 0.4f * w / (WAVES - 1);  // first peak at 60 % of the range, last at 100 %
    peaks[w] = lv.floor + lroundf((lv.ceiling - lv.floor) * build);
    if (apSurprise == 2) peaks[w] = rnd(lv.floor + (lv.ceiling - lv.floor) * 3 / 10, lv.ceiling);
  }
  if (apSurprise == 1)  // same peaks, any order
    for (int w = WAVES - 1; w > 0; w--) {
      int j = rnd(0, w);
      int t = peaks[w]; peaks[w] = peaks[j]; peaks[j] = t;
    }
  sessionMs = 0;
  for (int w = 0; w < WAVES; w++) {
    Wave &wv = waves[w];
    wv.peak = customPeak[w] ? customPeak[w] : peaks[w];
    wv.steps = rnd(4, 7);
    wv.climbMs = wv.steps * rnd(2000, 3500);
    wv.holdMs = rnd(es.peakMinMs, es.peakMaxMs);
    wv.restMs = rnd(es.restMinMs, es.restMaxMs);  // the edge: drop and rest
    if (apSurprise == 2) {  // wild: durations anywhere from half to double
      wv.holdMs = wv.holdMs * rnd(50, 200) / 100;
      wv.restMs = wv.restMs * rnd(50, 200) / 100;
    }
    sessionMs += wv.climbMs + wv.holdMs + wv.restMs;
  }
  if (apLength && sessionMs) {  // fit the waves into the chosen length
    float k = (float)LENGTH_MS[apLength] / sessionMs;
    sessionMs = 0;
    for (Wave &wv : waves) {
      wv.climbMs = max<uint32_t>(1000, wv.climbMs * k);
      wv.holdMs = max<uint32_t>(1000, wv.holdMs * k);
      wv.restMs = max<uint32_t>(1000, wv.restMs * k);
      sessionMs += wv.climbMs + wv.holdMs + wv.restMs;
    }
  }
  apPlanned = true;
}

// Where t ms into the session lands: wave, phase and time into that phase.
static void locate(uint32_t t, int &wave, Phase &phase, uint32_t &phaseMs) {
  for (wave = 0; wave < WAVES; wave++) {
    const Wave &w = waves[wave];
    if (t < w.climbMs) { phase = Phase::Climb; phaseMs = t; return; }
    t -= w.climbMs;
    if (t < w.holdMs) { phase = Phase::Hold; phaseMs = t; return; }
    t -= w.holdMs;
    if (t < w.restMs) { phase = Phase::Rest; phaseMs = t; return; }
    t -= w.restMs;
  }
  wave = WAVES - 1;
  phase = Phase::Rest;
  phaseMs = waves[wave].restMs ? waves[wave].restMs - 1 : 0;
}

static void restartSession() {
  plan();
  apWave = 0;
  apPhase = Phase::Climb;
  apPhaseMs = apElapsed = 0;
  apWobbleSlot = -1;
}

static uint32_t phaseLength() {
  const Wave &w = waves[apWave];
  return apPhase == Phase::Climb ? w.climbMs : apPhase == Phase::Hold ? w.holdMs : w.restMs;
}

// Speed for the current position in the session.
static int apCurrentSpeed() {
  const Level &lv = LEVELS[apIntensity];
  const Wave &w = waves[apWave];
  switch (apPhase) {
    case Phase::Climb: {
      int step = min<int>(w.steps, 1 + apPhaseMs * w.steps / max<uint32_t>(w.climbMs, 1));
      return constrain(lv.floor + (w.peak - lv.floor) * step / w.steps, 5, 100);
    }
    case Phase::Hold: {
      int slot = apPhaseMs / 2500;  // a fresh random dip every 2.5 s
      if (slot != apWobbleSlot) {
        apWobbleSlot = slot;
        apWobble = rnd(0, EDGE_STYLES[apEdge].wobble);
      }
      return constrain(w.peak - apWobble, 5, 100);
    }
    default: return constrain(lv.floor, 5, 100);
  }
}

static void autopilotTick() {
  uint32_t now = millis();
  uint32_t dt = now - apLastTick;  // real time, so a stalled loop doesn't stretch the session
  apLastTick = now;
  apPhaseMs += dt;
  apElapsed += dt;
  while (apPhaseMs >= phaseLength()) {
    apPhaseMs -= phaseLength();
    apWobbleSlot = -1;
    if (apPhase == Phase::Climb) {
      apPhase = Phase::Hold;
    } else if (apPhase == Phase::Hold) {
      apPhase = Phase::Rest;
    } else if (++apWave >= WAVES) {
      restartSession();  // session over: start a fresh one
      break;
    } else {
      apPhase = Phase::Climb;
    }
  }
  apSpeed = apCurrentSpeed();

  const Vac &v = AP_VAC[apVac];
  if (v.pulseMs && apElapsed - apLastSuction >= v.intervalMs) {
    // A little shorter at high speed, where the stroke itself pulls more air.
    startPulse(lroundf(v.pulseMs * (1.2f - apSpeed / 250.0f)));
    apLastSuction = apElapsed;
  }
}

// ---------- 100 ms timer ----------

static void tick(lv_timer_t *) {
  uint32_t now = millis();

  if (act == Active::Autopilot && apPlaying) autopilotTick();

  if (act == Active::Remote && !finishing) {
    const Vac &v = REMOTE_VAC[rVac];
    if (v.pulseMs && !plusHeld && !minusHeld && now - rLastPulse >= v.intervalMs) {
      startPulse(v.pulseMs);
      rLastPulse = now;
    }
  }

  // Lost a device that was following (cloud gone past the grace period, or a
  // Bluetooth link dropped mid-run): stop everything rather than resume later.
  // The Autoblow task sends nothing but STOP until this ack.
  devices::loop();
  if (devices::lost() != lostSeen) {
    lostSeen = devices::lost();
    if (act != Active::None) stopAll();
    devices::ackLost();
  }

  push();
}

void begin() {
  restartSession();  // so the graph shows the coming session before Start
  lv_timer_create(tick, 100, nullptr);
}

Active active() { return act; }

// ---------- remote ----------

void remoteStart() {
  leave();
  act = Active::Remote;
  rLastPulse = millis();
  push();
}

void remotePause() {
  if (act == Active::Remote) leave();
  push();
}

void remoteSetSpeed(int s) {
  rSpeed = constrain(s, 5, 100);
  if (act != Active::Remote || finishing) remoteStart();
  push();
}

int remoteSpeed() { return rSpeed; }

void remoteSetVacuum(int level) {
  rVac = constrain(level, 0, 3);
  rLastPulse = millis();
  push();
}

int remoteVacuum() { return rVac; }

// ---------- valves ----------

// No vacuum pulse while either valve is held, and a full interval after a
// manual Tighter before the next one.
void tighter(bool held) {
  plusHeld = held;
  if (!held) rLastPulse = millis();
  push();
}

void looser(bool held) {
  minusHeld = held;
  if (!held) rLastPulse = millis();
  push();
}

// ---------- autopilot ----------

void autopilotStart() {
  leave();
  if (!apPlanned) restartSession();
  act = Active::Autopilot;
  apPlaying = true;
  apLastTick = millis();  // resume where it paused, without the pause counting
  apSpeed = apCurrentSpeed();
  push();
}

void autopilotPause() {
  if (act == Active::Autopilot) leave();
  push();
}

// Changing a setting re-plans the waves but keeps the place in the session.
static void replan() {
  uint32_t done = apElapsed;
  plan();
  apElapsed = min(done, sessionMs);
  if (act == Active::Autopilot) {
    apSpeed = apCurrentSpeed();
    push();
  }
}
void autopilotSetIntensity(int l) { apIntensity = constrain(l, 0, 3); replan(); }
void autopilotSetEdge(int l) { apEdge = constrain(l, 0, 2); replan(); }
void autopilotSetVacuum(int l) {
  apVac = constrain(l, 0, 2);
  apLastSuction = 0;
  push();
}
int autopilotIntensity() { return apIntensity; }
int autopilotEdge() { return apEdge; }
int autopilotVacuum() { return apVac; }
int autopilotSpeed() { return act == Active::Autopilot ? apSpeed : 0; }

// ---------- session shape, steering, graph ----------

void autopilotSetLength(int l) { apLength = constrain(l, 0, 4); replan(); }
void autopilotSetSurprise(int l) { apSurprise = constrain(l, 0, 2); replan(); }
int autopilotLength() { return apLength; }
int autopilotSurprise() { return apSurprise; }

static void applyNow() {
  apWobbleSlot = -1;
  if (act == Active::Autopilot) {
    apSpeed = apCurrentSpeed();
    push();
  }
}

void autopilotMore() {
  if (!apPlanned) restartSession();
  Wave &w = waves[apWave];
  if (apPhase == Phase::Climb) {  // to the peak now
    apElapsed += w.climbMs - apPhaseMs;
    apPhase = Phase::Hold;
    apPhaseMs = 0;
  } else if (apPhase == Phase::Hold) {  // hold it longer, a bit higher
    w.holdMs += 5000;
    sessionMs += 5000;
    w.peak = min(100, w.peak + 5);
  } else if (apWave + 1 < WAVES) {  // cut the rest short: next wave now
    apElapsed += w.restMs - apPhaseMs;
    apWave++;
    apPhase = Phase::Climb;
    apPhaseMs = 0;
  }
  applyNow();
}

void autopilotBackOff() {
  if (!apPlanned) restartSession();
  Wave &w = waves[apWave];
  if (apPhase == Phase::Rest) {  // rest longer
    w.restMs += 5000;
    sessionMs += 5000;
  } else {  // drop to rest now
    apElapsed += (apPhase == Phase::Climb ? w.climbMs + w.holdMs : w.holdMs) - apPhaseMs;
    apPhase = Phase::Rest;
    apPhaseMs = 0;
  }
  applyNow();
}

int autopilotSpeedAt(uint32_t t) {
  if (!apPlanned) return 0;
  const Level &lv = LEVELS[apIntensity];
  int wave;
  Phase phase;
  uint32_t ms;
  locate(t, wave, phase, ms);
  const Wave &w = waves[wave];
  if (phase == Phase::Climb) {
    int step = min<int>(w.steps, 1 + ms * w.steps / max<uint32_t>(w.climbMs, 1));
    return constrain(lv.floor + (w.peak - lv.floor) * step / w.steps, 5, 100);
  }
  return constrain(phase == Phase::Hold ? w.peak : lv.floor, 5, 100);
}

uint32_t autopilotSessionMs() { return apPlanned ? sessionMs : 0; }
uint32_t autopilotElapsedMs() { return apPlanned ? apElapsed : 0; }

uint32_t autopilotNextPeakMs() {
  if (!apPlanned) return 0;
  const Wave &w = waves[apWave];
  if (apPhase == Phase::Climb) return w.climbMs - apPhaseMs;
  if (apPhase == Phase::Hold) return 0;
  if (apWave + 1 >= WAVES) return 0;
  return w.restMs - apPhaseMs + waves[apWave + 1].climbMs;
}

int autopilotWaveAt(uint32_t t) {
  int wave;
  Phase phase;
  uint32_t ms;
  if (!apPlanned) return 0;
  locate(t, wave, phase, ms);
  return wave;
}

void autopilotSeek(uint32_t t) {
  if (!apPlanned) restartSession();
  t = min(t, sessionMs ? sessionMs - 1 : 0);
  locate(t, apWave, apPhase, apPhaseMs);
  apElapsed = t;
  applyNow();
}

void autopilotSetPeak(int wave, int peak) {
  if (wave < 0 || wave >= WAVES) return;
  customPeak[wave] = peak ? constrain(peak, 5, 100) : 0;
  if (!apPlanned) restartSession();
  else if (customPeak[wave]) waves[wave].peak = customPeak[wave];
  else replan();
  applyNow();
}
int autopilotPeak(int wave) { return apPlanned && wave >= 0 && wave < WAVES ? waves[wave].peak : 0; }
void autopilotClearPeaks() {
  for (int &p : customPeak) p = 0;
  replan();
}
void autopilotSaveFavourite(int slot) {
  if (slot < 0 || slot >= Settings::FAVOURITES) return;
  if (!apPlanned) restartSession();
  for (int w = 0; w < WAVES; w++) settings.favPeaks[slot][w] = waves[w].peak;
  settingsSaveFavourite(slot);
}
bool autopilotLoadFavourite(int slot) {
  if (slot < 0 || slot >= Settings::FAVOURITES || !settings.favPeaks[slot][0]) return false;
  for (int w = 0; w < WAVES; w++) customPeak[w] = settings.favPeaks[slot][w];
  replan();
  applyNow();
  return true;
}

void finishMe() {
  leave();
  plusHeld = minusHeld = false;
  act = Active::Remote;
  finishing = true;
  push();
}

// ---------- local scripts ----------

void scriptPlay(int slot) {
  leave();
  act = Active::Script;
  sSlot = constrain(slot, 0, 29);
  sPlaying = true;
  push();
}

void scriptToggle() {
  if (act != Active::Script) return;
  sPlaying = !sPlaying;
  push();
}

int scriptSlot() { return act == Active::Script ? sSlot : -1; }
bool scriptPlaying() { return act == Active::Script && sPlaying; }

// ---------- stop ----------

void stopAll() {
  leave();
  plusHeld = minusHeld = false;
  push();
  devices::stopNow();
}

// ---------- restart ----------

static uint32_t restartStart;

static void restartPoll(lv_timer_t *) {
  bool moving = devices::anyMoving();
  uint32_t ms = millis() - restartStart;
  // Give the STOP a moment to reach the device, but don't hang on a dead network.
  if ((ms > 800 && !moving) || ms > 5000) esp_restart();
}

void stopAndRestart() {
  stopAll();
  restartStart = millis();
  lv_timer_create(restartPoll, 100, nullptr);
}

}  // namespace control
