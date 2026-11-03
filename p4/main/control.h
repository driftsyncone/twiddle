// What the panel is doing: manual remote, Autopilot (our own edging generator),
// or a local script. Runs on the LVGL thread from a 100 ms timer and only ever
// sets autoblow::Desired.
#pragma once
#include <stdint.h>

namespace control {

enum class Active { None, Remote, Autopilot, Script };

void begin();
Active active();

// Remote control
void remoteStart();
void remotePause();
void remoteSetSpeed(int speed);  // also starts, like moving a speed slider would
int remoteSpeed();
void remoteSetVacuum(int level);  // 0 off, 1 low, 2 medium, 3 high
int remoteVacuum();

// Hold-to-open valves (both pages). As the real VacuGlide 2 behaves (tested
// 2026-10-22): Tighter = the API's stroke-plus valve (air out, more suction),
// Looser = stroke-minus (air in). The manual and Autoblow's web app read it the
// other way round; the device doesn't.
void tighter(bool held);  // stroke-plus
void looser(bool held);   // stroke-minus

// Autopilot
void autopilotStart();
void autopilotPause();
void autopilotSetIntensity(int level);  // 0 warmup, 1 low, 2 medium, 3 high
void autopilotSetEdge(int level);       // 0 gentle, 1 moderate, 2 intense
void autopilotSetVacuum(int level);     // 0 off, 1 low, 2 high
int autopilotIntensity();
int autopilotEdge();
int autopilotVacuum();
int autopilotSpeed();  // last speed sent, 0 when not running
void finishMe();

// Session shape
void autopilotSetLength(int level);    // 0 natural, 1..4 = 5, 10, 20, 30 min
void autopilotSetSurprise(int level);  // 0 rising peaks, 1 shuffled, 2 wild
int autopilotLength();
int autopilotSurprise();
// Steering while it runs: More = to the peak now / hold it longer and a bit
// higher; Back off = drop to rest now / rest longer.
void autopilotMore();
void autopilotBackOff();
// The planned curve, for a graph: speed at t ms into the session (no wobble),
// the session length, the time played, time until the next peak, the wave at t.
int autopilotSpeedAt(uint32_t t);
uint32_t autopilotSessionMs();
uint32_t autopilotElapsedMs();
uint32_t autopilotNextPeakMs();
int autopilotWaveAt(uint32_t t);
void autopilotSeek(uint32_t t);  // jump to t ms into the session
// Draw your own: a peak per wave (5..100); 0 clears it (back to the planner).
void autopilotSetPeak(int wave, int peak);
int autopilotPeak(int wave);
void autopilotClearPeaks();
// HR edging (edge.h): Autopilot driven by a BLE heart rate monitor. Off by default; off = the plain
// Autopilot, untouched. In this mode the speed is the edge controller's, Length applies, and
// More / graph seek do nothing; Back off drops to rest. A start always begins a fresh HR session.
void autopilotSetHrEdging(bool on);
bool autopilotHrEdging();
void autopilotHrApply();       // settings.hr* changed: take them over (also mid-session)
void autopilotHrLearnEdge();   // edge bpm := the current bpm (needs a fresh reading); saved
struct HrStatus {
  int phase;      // 0 climb, 1 hold-off (edge), 2 rest, 3 signal lost, 4 done, -1 not running
  int edges, maxEdges;
  int edgeBpm;    // the current trigger (it rises with every cycle)
  int bpm;        // 0 = no reading
  int end;        // why a finished session ended: 0 running/none, 1 edges, 2 length, 3 signal lost
};
HrStatus autopilotHrStatus();
int autopilotHrBpmAt(uint32_t t);  // graph: recorded bpm at t ms into the session (0 none)
void autopilotSaveFavourite(int slot);  // the current peaks
bool autopilotLoadFavourite(int slot);  // false if the slot is empty

// Local scripts
void scriptPlay(int slot);  // 0..29
void scriptToggle();        // pause / resume
int scriptSlot();           // -1 if none
bool scriptPlaying();

// Everything off, valves closed, and resend it all.
void stopAll();
// stopAll(), then restart the panel once the device has stopped (5 s at most).
void stopAndRestart();

}  // namespace control
