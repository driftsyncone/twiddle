// Offline test of the toy protocol encoders (no radio, no toy): the real
// main/toys/*.cpp compiled on the host against a mock of the NimBLE classes.
// Run: test/host/run.sh
#include <Arduino.h>
#include <stdarg.h>

#include <deque>
#include <string>
#include <vector>

#include "../../main/toys/toys.h"

uint32_t g_now = 1000;

void plog(const char *f, ...) {
  va_list ap;
  va_start(ap, f);
  if (getenv("VERBOSE")) {
    printf("    log: ");
    vprintf(f, ap);
  }
  va_end(ap);
}

// ---- what the drivers would have written, and scripted replies ----
static std::vector<std::string> wrote;
static std::deque<std::string> replies;
static bool failWrites = false;

namespace toys {
bool write(Ctx &c, const uint8_t *d, size_t n) {
  if (failWrites) return false;
  wrote.emplace_back((const char *)d, n);
  return true;
}
bool writeStr(Ctx &c, const char *s) { return write(c, (const uint8_t *)s, strlen(s)); }
bool subscribe(Ctx &, NimBLERemoteCharacteristic *) { return true; }
bool reply(Ctx &c, uint32_t &seq, char *out, size_t n) {
  if (replies.empty()) return false;
  strlcpy(out, replies.front().c_str(), n);
  replies.pop_front();
  seq++;
  return true;
}
bool request(Ctx &, const char *, char *, size_t, uint32_t) { return false; }
void idle(uint32_t ms) { g_now += ms; }
}  // namespace toys

static int fails, checks;
#define CHECK(cond)                                                 \
  do {                                                              \
    checks++;                                                       \
    if (!(cond)) {                                                  \
      fails++;                                                      \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
    }                                                               \
  } while (0)

static std::string hex(const std::string &s) {
  std::string o;
  char b[4];
  for (unsigned char ch : s) {
    snprintf(b, sizeof b, "%02x", ch);
    o += b;
  }
  return o;
}
static std::string last() { return wrote.empty() ? "" : wrote.back(); }
static void clear() { wrote.clear(); }

static toys::Ctx *newCtx() {
  static toys::Ctx c;
  memset((void *)&c, 0, sizeof c);
  c.battery = -1;
  return &c;
}

static NimBLEAdvertisedDevice adv(const char *name) {
  NimBLEAdvertisedDevice a;
  a.name = name;
  return a;
}

// ---------------- matching ----------------
static void testMatch() {
  CHECK(toys::lovense.matches(adv("LVS-Z36D")));
  CHECK(toys::lovense.matches(adv("LOVE-Edge")));
  CHECK(!toys::lovense.matches(adv("KEON2")));
  CHECK(!toys::lovense.matches(adv("V51 MIDI")));
  NimBLEAdvertisedDevice m = adv("");
  m.mfg.push_back(std::string("\x6c\x02\xff\x21", 4));
  CHECK(toys::lovense.matches(m));
  NimBLEAdvertisedDevice nus = adv("");
  nus.svc.push_back("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
  CHECK(toys::lovense.matches(nus));   // nameless NUS counts
  nus.name = "Some Gadget";
  CHECK(!toys::lovense.matches(nus));  // named NUS does not
  NimBLEAdvertisedDevice fam = adv("x");
  fam.svc.push_back("50300001-0023-4bd4-bbd5-a6920e4c5653");
  CHECK(toys::lovense.matches(fam));

  CHECK(toys::keon.matches(adv("KEON")));
  CHECK(toys::keon.matches(adv("Keon R2")));
  CHECK(!toys::keon.matches(adv("KEON2")));
  CHECK(toys::keon2.matches(adv("KEON2")));
  CHECK(toys::keon2.matches(adv("KEON WIFI")));
  CHECK(toys::keon2.matches(adv("Keon Wifi")));
  CHECK(!toys::keon2.matches(adv("KEON")));

  CHECK(toys::ossm.matches(adv("OSSM")));
  NimBLEAdvertisedDevice o = adv("My machine");
  o.svc.push_back("522b443a-4f53-534d-0001-420badbabe69");
  CHECK(toys::ossm.matches(o));
  CHECK(!toys::ossm.matches(adv("OSSM2")));

  // driver list: every advert matches at most one family we care about
  NimBLEAdvertisedDevice lv = adv("LVS-Z36D");
  CHECK(toys::match(lv) == &toys::lovense);
  NimBLEAdvertisedDevice k2 = adv("KEON2");
  CHECK(toys::match(k2) == &toys::keon2);
  NimBLEAdvertisedDevice none = adv("Aranet");
  CHECK(toys::match(none) == nullptr);
  CHECK(toys::byId('L') == &toys::lovense && toys::byId('K') == &toys::keon && toys::byId('2') == &toys::keon2 &&
        toys::byId('O') == &toys::ossm && toys::byId('x') == nullptr);
}

// ---------------- Lovense ----------------
static void lvSetup(toys::Ctx &c, uint8_t flags) {
  c.lv = toys::LovenseState();
  c.lv.sentVib = c.lv.sentAir = c.lv.sentRot = -1;
  c.lv.flags = flags;
  clear();
}
static void lvRun(toys::Ctx &c, int level) {
  toys::lovense.level(c, level, g_now);
  g_now += 120;
  toys::lovense.tick(c, g_now);
}
static void testLovense() {
  toys::Ctx &c = *newCtx();
  lvSetup(c, 0);  // plain vibrator (Lush, Hush, Gush, Ferri ...)
  lvRun(c, 50);
  CHECK(last() == "Vibrate:10;");
  lvRun(c, 100);
  CHECK(last() == "Vibrate:20;");
  lvRun(c, 1);
  CHECK(last() == "Vibrate:1;");  // a nonzero level never rounds to 0
  lvRun(c, 0);
  CHECK(last() == "Vibrate:0;");
  // unchanged level: nothing more is sent
  clear();
  g_now += 120;
  toys::lovense.tick(c, g_now);
  CHECK(wrote.empty());
  // 100 ms rate limit: two changes in a row go out one per 100 ms
  toys::lovense.level(c, 30, g_now);
  toys::lovense.tick(c, g_now + 120);
  clear();
  toys::lovense.level(c, 60, g_now);
  toys::lovense.tick(c, g_now + 130);  // only 10 ms after the last send
  CHECK(wrote.empty());
  toys::lovense.tick(c, g_now + 260);
  CHECK(last() == "Vibrate:12;");
  g_now += 300;
  // failed write is retried
  clear();
  failWrites = true;
  toys::lovense.level(c, 40, g_now);
  toys::lovense.tick(c, g_now + 1000);
  failWrites = false;
  CHECK(wrote.empty());
  toys::lovense.tick(c, g_now + 1200);
  CHECK(last() == "Vibrate:8;");
  g_now += 2000;

  // Max: air pump follows (0..3)
  lvSetup(c, 1);
  lvRun(c, 100);
  CHECK(last() == "Vibrate:20;");
  g_now += 120;
  toys::lovense.tick(c, g_now);
  CHECK(last() == "Air:Level:3;");
  clear();
  toys::lovense.stop(c);
  CHECK(wrote.size() == 2 && wrote[0] == "Vibrate:0;" && wrote[1] == "Air:Level:0;");

  // rotating head: Rotate follows after Vibrate
  lvSetup(c, 2);
  lvRun(c, 50);
  g_now += 120;
  toys::lovense.tick(c, g_now);
  CHECK(wrote.size() == 2 && wrote[0] == "Vibrate:10;" && wrote[1] == "Rotate:10;");

  // Solace (oscillation): Mply, STOP zeroes the range too
  lvSetup(c, 4 | 8 | 16);
  lvRun(c, 100);
  CHECK(last() == "Mply:20:20;");
  clear();
  toys::lovense.stop(c);
  CHECK(last() == "Mply:0:0;");
  lvSetup(c, 4 | 16);  // Solace Pro
  clear();
  toys::lovense.stop(c);
  CHECK(last() == "Mply:0:20;");

  // STOP writes go out even right after another command (no rate limit)
  lvSetup(c, 0);
  lvRun(c, 80);
  clear();
  toys::lovense.stop(c);
  CHECK(last() == "Vibrate:0;");

  // battery replies: "85;", "s85;", a late "OK;" skipped, a first "0;" ignored
  lvSetup(c, 0);
  c.battery = -1;
  strcpy(c.name, "Lush");
  replies = {"85;"};
  toys::lovense.battery(c);
  CHECK(c.battery == 85);
  replies = {"OK;", "s70;"};
  toys::lovense.battery(c);
  CHECK(c.battery == 70);
  c.battery = -1;
  replies = {"0;"};
  toys::lovense.battery(c);
  CHECK(c.battery == -1);
  c.battery = 50;
  replies = {};
  toys::lovense.battery(c);
  CHECK(c.battery == 50);  // timeout leaves the last reading
  clear();
}

// ---------------- Keon ----------------
static void knSetup(toys::Ctx &c, bool keon2) {
  memset((void *)&c, 0, sizeof c);
  c.kn.variant = keon2 ? 1 : 0;
  c.kn.lo = 5;
  c.kn.hi = 95;
  c.kn.target = c.kn.from = 5;
  clear();
}
static void testKeon() {
  toys::Ctx &c = *newCtx();
  // Keon 1: full strokes 5 <-> 95, command 03 00 speed pos
  knSetup(c, false);
  g_now = 10000;
  toys::keon.level(c, 100, g_now);
  toys::keon.tick(c, g_now);
  CHECK(wrote.size() == 1);
  CHECK(hex(last()) == "0300" "4b" "5f");  // speed 75 (0x4b), up to 95 (0x5f)
  clear();
  toys::keon.tick(c, g_now + 100);  // mid stroke: nothing new
  CHECK(wrote.empty());
  toys::keon.tick(c, g_now + 250);  // boundary: back down to 5
  CHECK(hex(last()) == "0300" "4b" "05");
  // slowest level: half stroke 1500 ms
  knSetup(c, false);
  toys::keon.level(c, 1, g_now);
  toys::keon.tick(c, g_now);
  CHECK(hex(last()) == "0300" "0b" "5f");  // slowest: speed 11
  // STOP on Keon 1: slow move (speed 10) to the estimated position
  knSetup(c, false);
  toys::keon.level(c, 100, g_now);
  toys::keon.tick(c, g_now);  // heading 5 -> 95 over 250 ms
  clear();
  g_now += 125;               // half way: ~50
  toys::keon.stop(c);
  CHECK(wrote.size() == 1);
  CHECK(hex(last()).substr(0, 6) == "03000a");
  int pos = std::stoi(hex(last()).substr(6, 2), nullptr, 16);
  printf("  keon1 stop position estimate %d\n", pos);
  CHECK(pos >= 45 && pos <= 55);
  // after the stop no more strokes
  clear();
  toys::keon.tick(c, g_now + 500);
  CHECK(wrote.empty());
  // keepalive: Keon 1 repeats the last packet every 2 s
  toys::keon.tick(c, g_now + 2500);
  CHECK(wrote.size() == 1 && hex(last()).substr(0, 6) == "03000a");

  // Keon 2: 03 01 ..., pause = 00
  knSetup(c, true);
  g_now = 20000;
  toys::keon2.level(c, 100, g_now);
  toys::keon2.tick(c, g_now);
  CHECK(hex(last()) == "0301" "4b" "5f");
  clear();
  toys::keon2.stop(c);
  CHECK(hex(last()) == "00");
  clear();
  toys::keon2.tick(c, g_now + 1000);
  CHECK(wrote.empty());  // no keepalive on Keon 2, no strokes after stop
  // level 0 on Keon 2 pauses at once
  toys::keon2.level(c, 50, g_now + 2000);
  toys::keon2.tick(c, g_now + 2000);
  clear();
  toys::keon2.level(c, 0, g_now + 2100);
  CHECK(hex(last()) == "00");
  // failed stop is retried by tick()
  failWrites = true;
  toys::keon2.level(c, 50, g_now + 3000);
  failWrites = false;
  toys::keon2.tick(c, g_now + 3000);
  failWrites = true;
  toys::keon2.stop(c);
  failWrites = false;
  clear();
  toys::keon2.tick(c, g_now + 3010);
  CHECK(hex(last()) == "00");
  clear();
}

// ---------------- OSSM ----------------
static void testOssm() {
  toys::Ctx &c = *newCtx();
  c.os.speed = -1;
  c.os.want = 0;
  c.os.sent = -1;
  c.os.lastSpeed = -1;
  c.os.phase = 1;  // OSSM_RUNNING: don't try to switch modes
  g_now = 30000;
  toys::ossm.tick(c, g_now);
  CHECK(last() == "set:speed:0");  // first tick sends 0
  clear();
  toys::ossm.level(c, 55, g_now);
  toys::ossm.tick(c, g_now + 50);  // rate limited
  CHECK(wrote.empty());
  toys::ossm.tick(c, g_now + 150);
  CHECK(last() == "set:speed:55");
  clear();
  toys::ossm.tick(c, g_now + 400);
  CHECK(wrote.empty());  // unchanged
  toys::ossm.level(c, 250, g_now + 500);  // clamped
  toys::ossm.tick(c, g_now + 700);
  CHECK(last() == "set:speed:100");
  clear();
  toys::ossm.stop(c);
  CHECK(last() == "set:speed:0");
  // arming: in the menu it asks for the stroke engine, at most once per 1.5 s
  memset((void *)&c, 0, sizeof c);
  c.os.speed = -1;
  c.os.sent = 0;
  c.os.lastSpeed = -1;
  c.os.phase = 0;
  c.os.lastMode = 0;
  strcpy(c.os.state, "menu.idle");
  c.os.seq = 1;
  c.os.lastNotify = g_now + 5000;
  clear();
  toys::ossm.tick(c, g_now + 5000);
  CHECK(last() == "go:strokeEngine");
  clear();
  toys::ossm.tick(c, g_now + 5500);
  CHECK(wrote.empty());
  strcpy(c.os.state, "strokeEngine.idle");
  c.os.seq++;
  toys::ossm.tick(c, g_now + 5600);
  CHECK(c.os.phase == 1);  // reached play mode
  toys::ossm.battery(c);
  clear();
}

int main() {
  testMatch();
  testLovense();
  testKeon();
  testOssm();
  printf("%d checks, %d failed\n", checks, fails);
  return fails != 0;
}
