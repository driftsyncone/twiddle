#include "control.h"

#include <Arduino.h>
#include <lvgl.h>

#include "autoblow.h"
#include "devices.h"
#include "edge.h"
#include "encscale.h"
#include "hrm.h"
#include "settings.h"

namespace control {

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

// ---------- HR edging (edge.h) ----------
// Off by default; when off none of this runs and the Autopilot above is exactly what it was.
static const int HR_POINTS = 96;                     // = the Autopilot graph's resolution
static const uint32_t HR_NOMINAL_MS = 30 * 60000;    // graph width when Length is Auto
static bool apHr;
static edge::Controller hrCtl;
static uint32_t hrSeq;
static uint8_t hrSpd[HR_POINTS], hrBpmHist[HR_POINTS];  // what happened, per graph column
static int hrEnd;                                    // HrStatus::end

// ---------- local scripts ----------
static int sSlot = -1;
static bool sPlaying;

static uint32_t lostSeen;

// ---------- physical encoder (encoder.h) ----------
// Turning it changes the ACTIVE speed: Remote's manual speed, or Autopilot's live scale (only ever downwards from what the
// chosen Intensity plans, so it can never exceed what the screen allows). A short press pauses / resumes; a resume ramps back
// up to the paused speed over RAMP_MS and never jumps.
static const int ENC_SCALE_MIN = 20;
static const uint32_t RAMP_MS = 1500;
static int encScale = 100;             // Autopilot / HR edging: percent of the planned speed
static Active encPaused = Active::None;  // mode the encoder paused, to resume it
static bool ramping;
static uint32_t rampStart;

static int scaled(int s) { return s > 5 ? max(5, s * encScale / 100) : s; }

// During a resume ramp the speed climbs linearly from the 5 % minimum to its value.
static int shaped(int s) {
  if (!ramping) return s;
  uint32_t el = millis() - rampStart;
  if (el >= RAMP_MS) {
    ramping = false;
    return s;
  }
  return s > 5 ? 5 + (int)((int64_t)(s - 5) * el / RAMP_MS) : s;
}

// ---------- desired state ----------

static void push() {
  autoblow::Desired d;
  switch (act) {
    case Active::Remote:
      d.want = Want::Speed;
      d.speed = finishing ? 100 : shaped(rSpeed);
      break;
    case Active::Autopilot:
      d.want = Want::Speed;
      d.speed = shaped(scaled(apSpeed));
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
static bool apResumable;  // the last thing that happened was an Autopilot pause: Start continues that session

static void leave() {
  apResumable = false;
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

static uint32_t hrSessionMs() { return apLength ? LENGTH_MS[apLength] : HR_NOMINAL_MS; }

static edge::Config hrConfig() {
  edge::Config c;
  c.floor = LEVELS[apIntensity].floor;
  c.ceiling = LEVELS[apIntensity].ceiling;
  c.restingBpm = settings.hrResting;
  c.edgeBpm = settings.hrEdge;
  c.recoverDrop = settings.hrDrop;
  c.maxEdges = settings.hrEdges;
  c.edgeStep = settings.hrStep;
  c.riseBpm = settings.hrRise;
  c.lengthMs = LENGTH_MS[apLength];
  return c;
}

static void hrBegin() {
  uint32_t now = millis();
  memset(hrSpd, 0, sizeof hrSpd);
  memset(hrBpmHist, 0, sizeof hrBpmHist);
  hrEnd = 0;
  apElapsed = 0;
  hrCtl.start(hrConfig(), now);
  hrSeq = hrm::seq();
  if (hrm::ageMs() < 2000) hrCtl.sample(now, hrm::bpm());  // a reading that is already there
  apSpeed = hrCtl.out().speed;
}

// false once the session has ended (everything has been stopped).
static bool hrTick(uint32_t now) {
  if (hrm::seq() != hrSeq) {
    hrSeq = hrm::seq();
    hrCtl.sample(now, hrm::bpm());
  }
  hrCtl.tick(now);
  edge::Output o = hrCtl.out();
  int idx = min<uint32_t>(HR_POINTS - 1, (uint64_t)apElapsed * HR_POINTS / hrSessionMs());
  hrSpd[idx] = o.speed;
  hrBpmHist[idx] = hrm::ageMs() < 5000 ? o.bpm : 0;
  if (o.phase == edge::Phase::Done) {
    hrEnd = (int)o.end;
    stopAll();
    return false;
  }
  apSpeed = o.speed;
  return true;
}

static void autopilotTick() {
  uint32_t now = millis();
  uint32_t dt = now - apLastTick;  // real time, so a stalled loop doesn't stretch the session
  apLastTick = now;
  apPhaseMs += dt;
  apElapsed += dt;
  if (apHr) {
    if (!hrTick(now)) return;
  } else {
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
  }

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

  if (act != Active::None) encPaused = Active::None;  // something else started: nothing left to resume
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
  apHr = settings.hrEdging;
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
  bool resumed = apResumable;  // read before leave() clears it
  leave();
  encScale = encscale::onStart(encScale, resumed);  // a lowered scale never carries into a new session or another mode's return
  if (!apPlanned) restartSession();
  act = Active::Autopilot;
  apPlaying = true;
  apLastTick = millis();  // resume where it paused, without the pause counting
  if (apHr) hrBegin();
  else apSpeed = apCurrentSpeed();
  push();
}

void autopilotPause() {
  if (act == Active::Autopilot) {
    leave();
    apResumable = true;
  }
  push();
}

static void hrApply() { hrCtl.reconfigure(hrConfig()); }

// Changing a setting re-plans the waves but keeps the place in the session.
static void replan() {
  uint32_t done = apElapsed;
  plan();
  apElapsed = min(done, sessionMs);
  if (apHr) hrApply();
  if (act == Active::Autopilot && !apHr) {
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
int autopilotSpeed() { return act == Active::Autopilot ? scaled(apSpeed) : 0; }
int autopilotProgress() {
  if (!apPlanned || !sessionMs) return 0;
  return min<uint32_t>(100, (uint64_t)apElapsed * 100 / sessionMs);
}

// ---------- session shape, steering, graph ----------

void autopilotSetLength(int l) { apLength = constrain(l, 0, 4); replan(); }
void autopilotSetSurprise(int l) { apSurprise = constrain(l, 0, 2); replan(); }
int autopilotLength() { return apLength; }
int autopilotSurprise() { return apSurprise; }

static void applyNow() {
  if (apHr) return;  // the edge controller owns the speed
  apWobbleSlot = -1;
  if (act == Active::Autopilot) {
    apSpeed = apCurrentSpeed();
    push();
  }
}

void autopilotMore() {
  if (apHr) return;
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
  if (apHr) {
    hrCtl.backOff(millis());
    if (act == Active::Autopilot) {
      apSpeed = hrCtl.out().speed;
      push();
    }
    return;
  }
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
  if (apHr) return t <= apElapsed ? hrSpd[min<uint32_t>(HR_POINTS - 1, (uint64_t)t * HR_POINTS / hrSessionMs())] : 0;
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

uint32_t autopilotSessionMs() { return apHr ? hrSessionMs() : apPlanned ? sessionMs : 0; }
uint32_t autopilotElapsedMs() { return apPlanned ? apElapsed : 0; }

uint32_t autopilotNextPeakMs() {
  if (apHr || !apPlanned) return 0;
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
  if (apHr) return;
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
// ---------- HR edging ----------

void autopilotSetHrEdging(bool on) {
  if (on == apHr) return;
  if (act == Active::Autopilot) leave();  // never switch the generator under a running session
  apHr = on;
  settings.hrEdging = on;
  settingsSaveHr();
  push();
}
bool autopilotHrEdging() { return apHr; }
void autopilotHrApply() { hrApply(); }

void autopilotHrLearnEdge() {
  if (!hrm::connected() || hrm::ageMs() > 5000 || !hrm::bpm()) return;
  settings.hrEdge = constrain(hrm::bpm(), settings.hrResting + 10, 200);
  settingsSaveHr();
  hrApply();
}

HrStatus autopilotHrStatus() {
  edge::Output o = hrCtl.out();
  HrStatus h = {};
  h.phase = o.phase == edge::Phase::Idle ? -1 : (int)o.phase - 1;
  h.edges = o.edges;
  h.maxEdges = settings.hrEdges;
  h.edgeBpm = o.phase == edge::Phase::Idle ? settings.hrEdge : o.edgeBpm;
  h.bpm = hrm::connected() && hrm::ageMs() < 5000 ? hrm::bpm() : 0;
  h.end = hrEnd;
  return h;
}

int autopilotHrBpmAt(uint32_t t) {
  if (!apHr || t > apElapsed) return 0;
  return hrBpmHist[min<uint32_t>(HR_POINTS - 1, (uint64_t)t * HR_POINTS / hrSessionMs())];
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
int autopilotWaves() { return WAVES; }

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

// ---------- encoder ----------

bool encoderAdjust(int pct) {
  if (pct == 0) return false;
  if (act == Active::Remote && !finishing) {
    rSpeed = constrain(rSpeed + pct, 5, 100);
    push();
    return true;
  }
  if (act == Active::Autopilot) {
    encScale = constrain(encScale + pct, ENC_SCALE_MIN, 100);
    push();
    return true;
  }
  return false;  // Scripts run on the device: no speed to scale. Paused / stopped: nothing to change.
}

bool encoderPaused() { return encPaused != Active::None; }

bool encoderTogglePause() {
  if (act == Active::Script) {
    scriptToggle();
    return true;
  }
  if (act == Active::Remote && !finishing) {
    remotePause();
    encPaused = Active::Remote;
    return true;
  }
  if (act == Active::Autopilot) {
    autopilotPause();
    encPaused = Active::Autopilot;
    return true;
  }
  if (act == Active::None && encPaused != Active::None) {
    Active m = encPaused;
    encPaused = Active::None;
    rampStart = millis();
    ramping = true;
    if (m == Active::Remote) remoteStart();
    else autopilotStart();
    return true;
  }
  return false;
}

// ---------- stop ----------

void stopAll() {
  leave();
  encPaused = Active::None;  // a hard stop is final: the encoder button can't resume it
  encScale = 100;
  ramping = false;
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
  if ((ms > 800 && !moving) || ms > 5000) ESP.restart();
}

void stopAndRestart() {
  stopAll();
  restartStart = millis();
  lv_timer_create(restartPoll, 100, nullptr);
}

}  // namespace control
