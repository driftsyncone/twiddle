// Host test of the heart-rate edge controller (main/edge.cpp) and the 0x2A37 decoder (main/hrmparse.cpp)
// against simulated bpm traces. No hardware, no radio. Run: test/host/run.sh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <functional>
#include <memory>
#include <vector>

#include "edge.h"
#include "hrmparse.h"

static int fails, checks;
#define CHECK(cond)                                                \
  do {                                                             \
    checks++;                                                      \
    if (!(cond)) {                                                 \
      fails++;                                                     \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
    }                                                              \
  } while (0)

using edge::Phase;

// Drives a controller at 100 ms, one bpm sample per second from bpmAt(t_ms) (<=0: no sample).
struct Sim {
  edge::Controller c;
  edge::Config cfg;
  uint32_t t = 0;
  int maxSpeed = 0, minSpeedRunning = 1000;
  std::vector<Phase> seen;
  Sim() {
    cfg.floor = 20;
    cfg.ceiling = 70;
    cfg.restingBpm = 70;
    cfg.edgeBpm = 120;
    cfg.recoverDrop = 15;
    cfg.edgeStep = 2;
    cfg.maxEdges = 3;
    cfg.minRestMs = 10000;
    cfg.holdOffMs = 3000;
  }
  void begin() { c.start(cfg, t); }
  void run(uint32_t ms, const std::function<int(uint32_t)> &bpmAt) {
    uint32_t end = t + ms;
    for (; t < end; t += 100) {
      if (t % 1000 == 0) {
        int b = bpmAt(t);
        if (b > 0) c.sample(t, b);
      }
      c.tick(t);
      edge::Output o = c.out();
      if (o.speed > maxSpeed) maxSpeed = o.speed;
      if (o.phase != Phase::Done && o.phase != Phase::Idle && o.speed < minSpeedRunning) minSpeedRunning = o.speed;
      if (seen.empty() || seen.back() != o.phase) seen.push_back(o.phase);
    }
  }
  bool saw(Phase p) const {
    for (Phase q : seen) if (q == p) return true;
    return false;
  }
};

static void testHrmParse() {
  hrm::Reading r;
  uint8_t a[] = {0x00, 72};
  r = hrm::parse(a, 2);
  CHECK(r.ok && r.bpm == 72 && !r.contactSupported);
  uint8_t b[] = {0x01, 0x2C, 0x01};  // uint16 300
  r = hrm::parse(b, 3);
  CHECK(!r.ok && r.bpm == 300);  // decoded as 16-bit, but out of range
  uint8_t b2[] = {0x01, 0xB4, 0x00};  // uint16 180
  r = hrm::parse(b2, 3);
  CHECK(r.ok && r.bpm == 180);
  uint8_t c1[] = {0x06, 80};  // contact supported + detected
  r = hrm::parse(c1, 2);
  CHECK(r.ok && r.contactSupported && r.contact);
  uint8_t c2[] = {0x04, 80};  // supported, no contact
  r = hrm::parse(c2, 2);
  CHECK(!r.ok && r.contactSupported && !r.contact);
  uint8_t c3[] = {0x02, 80};  // contact bit set but not supported: ignore it
  r = hrm::parse(c3, 2);
  CHECK(r.ok);
  uint8_t lo[] = {0x00, 39}, hi[] = {0x00, 221}, e40[] = {0x00, 40}, e220[] = {0x00, 220};
  CHECK(!hrm::parse(lo, 2).ok && !hrm::parse(hi, 2).ok);
  CHECK(hrm::parse(e40, 2).ok && hrm::parse(e220, 2).ok);
  uint8_t shortWide[] = {0x01, 90};  // 16-bit flag but one byte of value
  CHECK(!hrm::parse(shortWide, 2).ok);
  uint8_t zero[] = {0x00, 0};
  CHECK(!hrm::parse(zero, 2).ok);
  CHECK(!hrm::parse(a, 1).ok && !hrm::parse(nullptr, 0).ok);
  uint8_t rr[] = {0x10, 65, 0x00, 0x04};  // RR interval flag (Polar H10): bpm still byte 1
  CHECK(hrm::parse(rr, 4).ok && hrm::parse(rr, 4).bpm == 65);
}

// Piecewise-linear bpm trace through (ms, bpm) points.
static std::function<int(uint32_t)> trace(std::vector<std::pair<uint32_t, int>> pts) {
  return [pts](uint32_t t) {
    if (t <= pts.front().first) return pts.front().second;
    for (size_t i = 1; i < pts.size(); i++)
      if (t <= pts[i].first) {
        double f = (double)(t - pts[i - 1].first) / (pts[i].first - pts[i - 1].first);
        return (int)(pts[i - 1].second + f * (pts[i].second - pts[i - 1].second));
      }
    return pts.back().second;
  };
}

static void testNormalCycle() {
  Sim s;
  s.cfg.maxEdges = 0;
  s.begin();
  // 70 -> 125 over 100 s (threshold 120 hit at ~91 s), then falls to 80 by 160 s, rests, climbs again.
  auto bpm = trace({{0, 70}, {100000, 125}, {105000, 125}, {150000, 80}, {400000, 80}});
  s.run(95000, bpm);
  edge::Output o = s.c.out();
  CHECK(o.edges == 1);
  CHECK(o.phase == Phase::HoldOff || o.phase == Phase::Rest);
  CHECK(s.saw(Phase::Climb) && s.saw(Phase::HoldOff));
  CHECK(o.speed == s.cfg.floor);  // dropped to the floor
  s.run(15000, bpm);              // 110 s: still resting, bpm 125 > 105
  CHECK(s.c.out().phase == Phase::Rest && s.c.out().speed == s.cfg.floor);
  s.run(30000, bpm);              // 140 s: bpm ~ 90, still above 120-15=105? falls 125->80 over 45 s: 105 at ~120 s
  CHECK(s.c.out().phase == Phase::Climb);
  CHECK(s.c.out().edgeBpm == 122);  // raised by edgeStep after the finished cycle
  CHECK(s.maxSpeed > s.cfg.floor);  // it did climb before the edge
  CHECK(s.maxSpeed <= s.cfg.ceiling);
  CHECK(s.minSpeedRunning >= s.cfg.floor);
}

static void testThresholdHold() {
  // bpm exactly at the threshold counts; below it never edges.
  Sim s;
  s.begin();
  s.run(120000, [](uint32_t) { return 119; });
  CHECK(s.c.out().edges == 0 && s.c.out().phase == Phase::Climb);
  CHECK(s.c.out().speed <= s.cfg.ceiling && s.c.out().speed >= s.cfg.floor);  // within 8 bpm of the edge it holds its speed
  Sim s2;
  s2.begin();
  s2.run(20000, [](uint32_t) { return 120; });
  CHECK(s2.c.out().edges == 1);
}

static void testFastRise() {
  Sim s;
  s.cfg.edgeBpm = 160;
  s.cfg.riseBpm = 20;
  s.begin();
  // 80 for 20 s, then +25 bpm in 8 s (below the 160 threshold): the rise trigger fires.
  s.run(40000, trace({{0, 80}, {20000, 80}, {28000, 105}, {60000, 105}}));
  CHECK(s.c.out().edges == 1);
  CHECK(s.c.out().bpm < 160);
  // A slow drift of the same size over 60 s does not.
  Sim t;
  t.cfg.edgeBpm = 160;
  t.cfg.riseBpm = 20;
  t.begin();
  t.run(90000, trace({{0, 80}, {80000, 105}}));
  CHECK(t.c.out().edges == 0);
  // riseBpm 0 switches it off.
  Sim u;
  u.cfg.edgeBpm = 160;
  u.cfg.riseBpm = 0;
  u.begin();
  u.run(40000, trace({{0, 80}, {20000, 80}, {28000, 105}, {60000, 105}}));
  CHECK(u.c.out().edges == 0);
}

static void testSignalLoss() {
  Sim s;
  s.begin();
  auto bpm = [](uint32_t t) { return t < 30000 ? 85 : 0; };  // samples stop at 30 s
  s.run(34000, bpm);
  CHECK(s.c.out().phase == Phase::Climb);  // 4 s without a sample: still fine
  s.run(3000, bpm);                        // > 5 s since the last sample
  CHECK(s.c.out().phase == Phase::Lost && s.c.out().speed == s.cfg.floor);
  s.run(10000, bpm);
  CHECK(s.c.out().phase == Phase::Done && s.c.out().end == edge::End::SignalLost && s.c.out().speed == 0);
  CHECK(s.minSpeedRunning >= s.cfg.floor);
  // Signal returns during the floor period: back to rest, not stopped.
  Sim r;
  r.begin();
  r.run(30000, [](uint32_t t) { return t < 20000 ? 85 : t < 26000 ? 0 : 85; });
  CHECK(r.c.out().phase != Phase::Done && r.saw(Phase::Lost));
  CHECK(r.c.out().speed == r.cfg.floor || r.c.out().phase == Phase::Climb);
  // Never any signal: no climbing, stops.
  Sim n;
  n.begin();
  n.run(20000, [](uint32_t) { return 0; });
  CHECK(n.c.out().phase == Phase::Done && n.maxSpeed == n.cfg.floor);
}

static void testArtefacts() {
  Sim s;
  s.begin();
  // Wild values: out of range, and a single spike (180) in a steady 80: none of them is an edge.
  auto bpm = [](uint32_t t) {
    if (t == 10000) return 255;
    if (t == 11000) return 30;
    if (t == 12000) return 0 + 190;  // spike above the 120 threshold, one reading only
    return 80;
  };
  s.run(40000, bpm);
  CHECK(s.c.out().edges == 0);
  CHECK(s.c.out().bpm == 80);
  CHECK(s.c.rejected() >= 3);
  // A real, confirmed jump (two agreeing readings) is accepted.
  Sim t;
  t.begin();
  t.run(20000, [](uint32_t tt) { return tt < 10000 ? 80 : 125; });
  CHECK(t.c.out().edges == 1);
  // The controller ignores samples after Done.
  t.c.sample(t.t, 100);
}

static void testSessionEnd() {
  Sim s;
  s.cfg.maxEdges = 3;
  s.cfg.minRestMs = 5000;
  s.begin();
  // A sawtooth: climbs to 130 and falls to 80 every 60 s.
  auto bpm = [](uint32_t t) {
    uint32_t p = t % 60000;
    return p < 30000 ? 80 + (int)(p / 600) : 130 - (int)((p - 30000) / 600);
  };
  s.run(400000, bpm);
  CHECK(s.c.out().phase == Phase::Done && s.c.out().end == edge::End::Edges);
  CHECK(s.c.out().edges == 3 && s.c.out().speed == 0);
  CHECK(s.maxSpeed <= s.cfg.ceiling);
  // Length setting.
  Sim l;
  l.cfg.maxEdges = 0;
  l.cfg.lengthMs = 60000;
  l.begin();
  l.run(100000, [](uint32_t) { return 90; });
  CHECK(l.c.out().phase == Phase::Done && l.c.out().end == edge::End::Length);
}

static void testLimits() {
  // The ceiling holds through everything, including a changed level mid-session and a huge cap bump.
  Sim s;
  s.cfg.edgeBpm = 200;
  s.cfg.maxEdges = 0;
  s.begin();
  s.run(300000, [](uint32_t) { return 100; });  // never reaches the edge: sits at the cap, then bumps it
  CHECK(s.c.out().speed <= s.cfg.ceiling && s.c.out().speed >= s.cfg.floor);
  CHECK(s.maxSpeed == s.cfg.ceiling);
  edge::Config low = s.cfg;
  low.floor = 8;
  low.ceiling = 25;
  s.c.reconfigure(low);
  s.run(1000, [](uint32_t) { return 100; });
  CHECK(s.c.out().speed <= 25);
  // sanitize
  edge::Config bad;
  bad.floor = 60;
  bad.ceiling = 30;
  bad.edgeBpm = 500;
  bad.restingBpm = 10;
  edge::Config ok = edge::sanitize(bad);
  CHECK(ok.ceiling >= ok.floor && ok.edgeBpm <= ok.maxEdgeBpm && ok.restingBpm >= 40);
  // Back off and learn.
  Sim b;
  b.cfg.maxEdges = 0;
  b.begin();
  b.run(10000, [](uint32_t) { return 90; });
  b.c.backOff(b.t);
  CHECK(b.c.out().phase == Phase::Rest && b.c.out().speed == b.cfg.floor && b.c.out().edges == 0);
  b.c.learnEdge(140);
  CHECK(b.c.out().edgeBpm == 140);
  b.c.learnEdge(75);  // never below resting + 10
  CHECK(b.c.out().edgeBpm == 80);
  // A monitor that never recovers cannot stall the rest forever.
  Sim m;
  m.cfg.maxEdges = 0;
  m.begin();
  m.run(300000, [](uint32_t) { return 150; });
  CHECK(m.c.out().edges >= 2);
}

static void testLowerEdgesMidSession() {
  // Lowering Edges below the current count ends the session at once, and a lowered floor applies to Rest.
  Sim s;
  s.cfg.maxEdges = 5;
  s.cfg.minRestMs = 5000;
  s.begin();
  auto bpm = [](uint32_t t) {
    uint32_t p = t % 60000;
    return p < 30000 ? 80 + (int)(p / 600) : 130 - (int)((p - 30000) / 600);
  };
  s.run(100000, bpm);
  int n = s.c.out().edges;
  CHECK(n >= 1 && s.c.out().phase != Phase::Done);
  edge::Config low = s.cfg;
  low.maxEdges = n;
  s.c.reconfigure(low);
  s.cfg = low;
  s.run(200, bpm);
  CHECK(s.c.out().phase == Phase::Done && s.c.out().end == edge::End::Edges && s.c.out().speed == 0);
  Sim r;
  r.cfg.maxEdges = 0;
  r.begin();
  r.c.backOff(r.t);
  CHECK(r.c.out().phase == Phase::Rest && r.c.out().speed == 20);
  edge::Config lf = r.cfg;
  lf.floor = 10;
  r.c.reconfigure(lf);
  CHECK(r.c.out().speed == 10);
}

int main() {
  testHrmParse();
  testNormalCycle();
  testThresholdHold();
  testFastRise();
  testSignalLoss();
  testArtefacts();
  testSessionEnd();
  testLimits();
  testLowerEdgesMidSession();
  printf("%d checks, %d failed\n", checks, fails);
  return fails ? 1 : 0;
}
