// Heart-rate edge controller (see edge.h). Pure C++: no hardware, no clock of its own.
#include "edge.h"

#include <stdlib.h>

namespace edge {

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static const int JUMP_BPM = 30;      // a jump this big between samples needs a second reading to be believed
static const int CONFIRM_BPM = 10;   // ... one within this of the first
static const uint32_t JUMP_WINDOW_MS = 5000;
static const uint32_t CAP_BUMP_MS = 20000;  // at the cap this long without an edge: allow a bit more

Config sanitize(Config c) {
  c.floor = clampi(c.floor, 5, 100);
  c.ceiling = clampi(c.ceiling, c.floor, 100);
  c.restingBpm = clampi(c.restingBpm, BPM_MIN, 150);
  c.maxEdgeBpm = clampi(c.maxEdgeBpm, c.restingBpm + 10, BPM_MAX);
  c.edgeBpm = clampi(c.edgeBpm, c.restingBpm + 10, c.maxEdgeBpm);
  c.recoverDrop = clampi(c.recoverDrop, 3, 60);
  c.riseBpm = c.riseBpm < 8 ? 0 : clampi(c.riseBpm, 8, 60);  // below 8 bpm is noise: off
  c.edgeStep = clampi(c.edgeStep, 0, 10);
  c.maxEdges = c.maxEdges < 0 ? 0 : c.maxEdges;
  if (c.stepMs < 500) c.stepMs = 500;
  if (c.maxRestMs < c.minRestMs) c.maxRestMs = c.minRestMs;
  if (c.riseWindowMs < 2000) c.riseWindowMs = 2000;
  if (c.lostMs < 2000) c.lostMs = 2000;
  return c;
}

int Controller::capFor(int cy) const {
  // Like the plain Autopilot's waves: the first peak at 60 % of the range, rising to 100 % by the 8th cycle.
  int range = cfg.ceiling - cfg.floor;
  int pct = 60 + 40 * clampi(cy, 0, 7) / 7;
  return clampi(cfg.floor + range * pct / 100, cfg.floor, cfg.ceiling);
}

int Controller::recoverTarget() const {
  int t = edgeBpm - cfg.recoverDrop;
  return t < cfg.restingBpm + 3 ? cfg.restingBpm + 3 : t;
}

void Controller::enter(Phase p, uint32_t now) {
  phase = p;
  phaseStart = now;
  if (p == Phase::Climb) {
    speed = cfg.floor;
    stepAt = capSince = now;
    nHist = 0;  // the rise is measured within this climb only
    restExtra = 0;
  } else if (p != Phase::Done) {
    speed = cfg.floor;
  }
}

void Controller::start(const Config &c, uint32_t now) {
  cfg = sanitize(c);
  endReason = End::None;
  started = lastSample = lastAcceptedT = now;
  edges = cycle = nRejected = 0;
  edgeBpm = cfg.edgeBpm;
  last = pending = 0;
  nHist = 0;
  cap = capFor(0);
  enter(Phase::Climb, now);
}

void Controller::reconfigure(const Config &c) {
  if (phase == Phase::Idle || phase == Phase::Done) return;
  Config n = sanitize(c);
  edgeBpm = clampi(edgeBpm + (n.edgeBpm - cfg.edgeBpm), n.restingBpm + 10, n.maxEdgeBpm);
  cfg = n;
  cap = capFor(cycle);
  if (speed > cfg.ceiling) speed = cfg.ceiling;
  if (speed < cfg.floor) speed = cfg.floor;
  // Lost / Rest / HoldOff always sit at the floor: follow it when it was lowered.
  if (phase != Phase::Climb) speed = cfg.floor;
}

void Controller::sample(uint32_t now, int bpm) {
  if (phase == Phase::Idle || phase == Phase::Done) return;
  if (bpm < BPM_MIN || bpm > BPM_MAX) { nRejected++; return; }
  // A jump from the last good reading within a few seconds is believed only when the next
  // reading agrees with it (a loose strap or a dropout gives single wild values).
  if (last && now - lastAcceptedT <= JUMP_WINDOW_MS && abs(bpm - last) > JUMP_BPM) {
    if (pending && abs(bpm - pending) <= CONFIRM_BPM) {
      pending = 0;  // confirmed: fall through and accept
    } else {
      pending = bpm;
      nRejected++;
      return;
    }
  }
  pending = 0;
  last = bpm;
  lastAcceptedT = lastSample = now;
  if (nHist == HIST) {
    for (int i = 1; i < HIST; i++) hist[i - 1] = hist[i];
    nHist--;
  }
  hist[nHist++] = {now, bpm};
}

bool Controller::riseTrigger(uint32_t now) const {
  if (!cfg.riseBpm || !last) return false;
  int lo = last;
  for (int i = 0; i < nHist; i++)
    if (now - hist[i].t <= cfg.riseWindowMs && hist[i].bpm < lo) lo = hist[i].bpm;
  return last - lo >= cfg.riseBpm;
}

void Controller::beginEdge(uint32_t now) {
  edges++;
  enter(Phase::HoldOff, now);
}

void Controller::finish(End e) {
  phase = Phase::Done;
  endReason = e;
  speed = 0;
}

void Controller::backOff(uint32_t now) {
  if (phase == Phase::Climb) enter(Phase::Rest, now);
  else if (phase == Phase::Rest) restExtra += 5000;
}

void Controller::learnEdge(int bpm) {
  edgeBpm = clampi(bpm, cfg.restingBpm + 10, cfg.maxEdgeBpm);
}

void Controller::tick(uint32_t now) {
  if (phase == Phase::Idle || phase == Phase::Done) return;
  if (cfg.lengthMs && now - started >= cfg.lengthMs) return finish(End::Length);
  // The Edges setting can be lowered mid-session (reconfigure): never allow an edge past the target.
  if (cfg.maxEdges && edges >= cfg.maxEdges && phase != Phase::HoldOff) return finish(End::Edges);

  uint32_t age = now - lastSample;
  if (phase != Phase::Lost && age > cfg.lostMs) enter(Phase::Lost, now);

  switch (phase) {
    case Phase::Lost:
      if (age <= cfg.lostMs) enter(Phase::Rest, now);  // it is back: rest before climbing again
      else if (now - phaseStart >= cfg.lostFloorMs) return finish(End::SignalLost);
      break;

    case Phase::Climb:
      if (last >= edgeBpm || riseTrigger(now)) {
        beginEdge(now);
        break;
      }
      if (!last) break;  // no reading yet: wait at the floor
      if (now - stepAt >= cfg.stepMs) {
        stepAt = now;
        bool near = edgeBpm - last <= cfg.approachBpm;
        if (speed < cap) {
          if (!near) {
            int step = (cap - cfg.floor) / 6;
            speed += step < 1 ? 1 : step;
            if (speed > cap) speed = cap;
          }
          capSince = now;
        } else if (now - capSince >= CAP_BUMP_MS && cap < cfg.ceiling) {
          int bump = (cfg.ceiling - cfg.floor) / 20;
          cap += bump < 2 ? 2 : bump;
          if (cap > cfg.ceiling) cap = cfg.ceiling;
          capSince = now;
        }
      }
      break;

    case Phase::HoldOff:
      if (now - phaseStart >= cfg.holdOffMs) {
        if (cfg.maxEdges && edges >= cfg.maxEdges) return finish(End::Edges);
        enter(Phase::Rest, now);
      }
      break;

    case Phase::Rest: {
      uint32_t t = now - phaseStart;
      bool recovered = last && last <= recoverTarget();
      if ((t >= cfg.minRestMs + restExtra && recovered) || t >= cfg.maxRestMs + restExtra) {
        cycle++;
        edgeBpm = clampi(edgeBpm + cfg.edgeStep, cfg.restingBpm + 10, cfg.maxEdgeBpm);
        cap = capFor(cycle);
        enter(Phase::Climb, now);
      }
      break;
    }
    default: break;
  }
  // The ceiling is a hard limit, whatever happened above.
  if (phase != Phase::Done) {
    if (speed > cfg.ceiling) speed = cfg.ceiling;
    if (speed < cfg.floor) speed = cfg.floor;
  }
}

Output Controller::out() const {
  Output o;
  o.phase = phase;
  o.end = endReason;
  o.speed = (phase == Phase::Idle || phase == Phase::Done) ? 0 : speed;
  o.edges = edges;
  o.edgeBpm = edgeBpm;
  o.bpm = last;
  return o;
}

}  // namespace edge
