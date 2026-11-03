// Heart-rate-driven edging: a pure state machine, no ESP, LVGL or Arduino includes, so it
// unit-tests on the host (test/host/edge_test.cpp).
//
// Inputs: bpm samples (sample()) and the clock (tick()). Output: a phase and a target speed.
//
//   Climb    the speed rises in steps toward a cap (as the plain Autopilot climbs). EDGE when the bpm
//            reaches edgeBpm, or has risen by riseBpm or more within 10 s.
//   HoldOff  the edge: straight down to the level floor for a moment. Counts as an edge.
//   Rest     at the floor until the bpm is down to edgeBpm - recoverDrop AND the minimum rest time has
//            passed (or the maximum rest time, so a monitor that never recovers cannot stall it).
//            A finished cycle raises edgeBpm by edgeStep and the climb starts again.
//   Lost     no valid sample for lostMs: floor, then Done after lostFloorMs (back to Rest if it returns).
//   Done     session over (maxEdges reached, lengthMs passed, or signal lost). Speed 0.
//
// The speed is always within [floor, ceiling] of the intensity level (0 only when Done).
#pragma once
#include <stdint.h>

namespace edge {

enum class Phase : uint8_t { Idle, Climb, HoldOff, Rest, Lost, Done };
enum class End : uint8_t { None, Edges, Length, SignalLost };

static const int BPM_MIN = 40, BPM_MAX = 220;  // samples outside this are artefacts

struct Config {
  int floor = 20, ceiling = 70;  // speed range of the intensity level
  int restingBpm = 70;           // resting heart rate (lower bound for the targets)
  int edgeBpm = 120;             // trigger
  int recoverDrop = 15;          // rest until bpm <= edgeBpm - recoverDrop (never below restingBpm + 3)
  int riseBpm = 20;              // also an edge: rose this much within riseWindowMs (0 = off)
  int edgeStep = 2;              // edgeBpm goes up by this after every finished cycle
  int maxEdges = 5;              // session ends after this many edges (0 = no limit)
  uint32_t lengthMs = 0;         // session ends after this long (0 = no limit)
  int maxEdgeBpm = 200;          // edgeBpm never goes above this
  uint32_t stepMs = 2500;        // climb: one step this often
  uint32_t holdOffMs = 3000;     // at the floor right after an edge
  uint32_t minRestMs = 20000, maxRestMs = 120000;
  uint32_t riseWindowMs = 10000;
  uint32_t lostMs = 5000;        // no sample for this long = signal lost
  uint32_t lostFloorMs = 5000;   // ... then this long at the floor before Done
  int approachBpm = 8;           // climb holds its speed when this close to the edge
};

struct Output {
  Phase phase = Phase::Idle;
  End end = End::None;
  int speed = 0;       // target speed, 0 when Done / Idle
  int edges = 0;       // edges so far
  int edgeBpm = 0;     // the current trigger (it climbs with every cycle)
  int bpm = 0;         // last accepted sample, 0 none yet
};

// Reject nonsense in a config (swapped range, thresholds out of order).
Config sanitize(Config c);

class Controller {
 public:
  void start(const Config &c, uint32_t now);
  // New settings mid-session: keeps the place; edgeBpm moves by the change in cfg.edgeBpm.
  void reconfigure(const Config &c);
  // A bpm reading. Out-of-range values and sudden jumps that are not confirmed by a
  // second reading are dropped. Call tick() as well; time only advances there.
  void sample(uint32_t now, int bpm);
  void tick(uint32_t now);
  // "Back off": rest now (an edge that is not counted); while resting, rest 5 s longer.
  void backOff(uint32_t now);
  // Use this bpm as the edge trigger.
  void learnEdge(int bpm);
  Output out() const;
  bool running() const { return phase != Phase::Idle && phase != Phase::Done; }
  int rejected() const { return nRejected; }

 private:
  struct Pt { uint32_t t; int bpm; };
  void enter(Phase p, uint32_t now);
  void beginEdge(uint32_t now);
  void finish(End e);
  bool riseTrigger(uint32_t now) const;
  int capFor(int cycle) const;
  int recoverTarget() const;

  Config cfg;
  Phase phase = Phase::Idle;
  End endReason = End::None;
  uint32_t started = 0, phaseStart = 0, lastSample = 0, lastAcceptedT = 0, stepAt = 0, capSince = 0;
  uint32_t restExtra = 0;
  int speed = 0, cap = 0, cycle = 0, edges = 0, edgeBpm = 0, last = 0, pending = 0, nRejected = 0;
  static const int HIST = 16;
  Pt hist[HIST];
  int nHist = 0;
};

}  // namespace edge
