// Host test for src/seesaw.cpp: frames, probe, clicks, button debounce/short/hold.
#include <stdio.h>
#include <string.h>
#include "seesaw.h"
using namespace seesaw;

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static void frames() {
  uint8_t b[8];
  CHECK(readPrefix(ENCODER_BASE, ENCODER_POSITION, b) == 2 && b[0] == 0x11 && b[1] == 0x30);
  CHECK(readPrefix(STATUS_BASE, STATUS_HW_ID, b) == 2 && b[0] == 0x00 && b[1] == 0x01);
  uint8_t f[3][6];
  buttonSetupFrames(f);  // pin 24 = 0x01000000, big-endian
  const uint8_t m[4] = {0x01, 0, 0, 0};
  CHECK(f[0][0] == 1 && f[0][1] == 0x03 && !memcmp(f[0] + 2, m, 4));
  CHECK(f[1][0] == 1 && f[1][1] == 0x0B && !memcmp(f[1] + 2, m, 4));
  CHECK(f[2][0] == 1 && f[2][1] == 0x05 && !memcmp(f[2] + 2, m, 4));
  CHECK(npSpeed(b) == 3 && b[0] == 0x0E && b[1] == 0x02 && b[2] == 1);
  CHECK(npLength(b) == 4 && b[1] == 0x03 && b[2] == 0 && b[3] == 3);
  CHECK(npPin(b) == 3 && b[1] == 0x01 && b[2] == 6);
  CHECK(npColor(1, 2, 3, b) == 7 && b[1] == 0x04 && b[2] == 0 && b[3] == 0 && b[4] == 2 && b[5] == 1 && b[6] == 3);  // G R B
  CHECK(npShow(b) == 2 && b[0] == 0x0E && b[1] == 0x05);
}

static void parse() {
  const uint8_t neg[4] = {0xFF, 0xFF, 0xFF, 0xFE}, big[4] = {0x00, 0x01, 0x00, 0x02};
  CHECK(i32be(neg) == -2 && i32be(big) == 65538);
  const uint8_t up[4] = {0xFF, 0xFF, 0xFF, 0xFF}, down[4] = {0xFE, 0xFF, 0xFF, 0xFF}, pressed[4] = {0xFE, 0xFF, 0xFF, 0xFF};
  CHECK(!buttonPressed(up));
  CHECK(buttonPressed(pressed) && buttonPressed(down));
  const uint8_t other[4] = {0xFF ^ 0x02, 0xFF, 0xFF, 0xFF};  // a different pin low: not the button
  CHECK(!buttonPressed(other));
}

static void probe() {
  const uint8_t v5880[4] = {0x16, 0xF8, 0x23, 0x0A};  // 5880 = 0x16F8
  const uint8_t v4991[4] = {0x13, 0x7F, 0x00, 0x01};  // 4991 = 0x137F
  CHECK(productFromVersion(v5880) == 5880 && productFromVersion(v4991) == 4991);
  CHECK(isEncoderBoard(0x87, v5880));   // ATtiny817
  CHECK(isEncoderBoard(0x55, v4991));   // SAMD09
  CHECK(!isEncoderBoard(0x00, v5880));  // a failed read leaves 0
  // A MAX17048 at 0x36 sitting at 3.7 V: HW_ID read gives VCELL high byte (~0xBD) and "version" is gauge registers.
  const uint8_t gaugeVer[4] = {0x9A, 0x00, 0x1C, 0x00};
  CHECK(!isEncoderBoard(0xBD, gaugeVer));
  // Even a gauge byte that happens to be a valid id (VCELL 0x84.. = 2.6 V) is rejected by the product id.
  CHECK(validHwId(0x84) && !isEncoderBoard(0x84, gaugeVer));
  for (int a = 0x30; a < 0x40; a++) CHECK(validAddress(a) == (a >= 0x36 && a <= 0x3D));
}

static void clicks() {
  Clicks c;
  CHECK(c.feed(12345) == 0);  // first sample after connect primes, whatever it holds
  CHECK(c.feed(12346) == 1 && c.feed(12344) == -2 && c.feed(12344) == 0);
  CHECK(c.feed(12344 + 500) == 0);  // glitch swallowed
  CHECK(c.feed(12344 + 501) == 1);
  c.reset();  // reconnect: the encoder may have restarted at 0 or kept counting
  CHECK(c.feed(-7) == 0 && c.feed(-5) == 2);
  Clicks w;  // int32 wrap
  w.feed(INT32_MAX);
  CHECK(w.feed(INT32_MIN) == 1);
}

static Press run(Button &b, bool raw, uint32_t from, uint32_t to, Press want_first = Press::None) {
  Press first = Press::None;
  for (uint32_t t = from; t <= to; t += 5) {
    Press p = b.update(raw, t);
    if (p != Press::None && first == Press::None) first = p;
  }
  (void)want_first;
  return first;
}

static void button() {
  Button b;
  CHECK(b.update(false, 0) == Press::None);
  // bounce: 10 ms blips never register
  CHECK(b.update(true, 100) == Press::None && b.update(false, 110) == Press::None);
  CHECK(run(b, false, 115, 300) == Press::None);
  // short press 200 ms
  CHECK(run(b, true, 400, 600) == Press::None);
  CHECK(run(b, false, 605, 700) == Press::Short);
  // hold: fires once at ~1 s while still down, release after is silent
  Button h;
  h.update(false, 0);
  uint32_t t = 1000;
  Press first = Press::None; int holds = 0, shorts = 0;
  for (; t < 1000 + 2500; t += 5) {
    Press p = h.update(true, t);
    if (p == Press::Hold) { holds++; if (first == Press::None) first = p; CHECK(t - 1000 >= 1000 && t - 1000 <= 1100); }
    if (p == Press::Short) shorts++;
  }
  for (; t < 1000 + 2800; t += 5) { Press p = h.update(false, t); if (p == Press::Hold) holds++; if (p == Press::Short) shorts++; }
  CHECK(holds == 1 && shorts == 0);
  // 900 ms press is a Short, not a hold
  Button s;
  s.update(false, 0);
  CHECK(run(s, true, 100, 1000) == Press::None);
  CHECK(run(s, false, 1005, 1100) == Press::Short);
  // already down at connect: ignored (no short, no hold), next press works
  Button d;
  CHECK(d.update(true, 0) == Press::None);
  CHECK(run(d, true, 5, 2500) == Press::None);
  CHECK(run(d, false, 2505, 2700) == Press::None);
  CHECK(run(d, true, 3000, 3200) == Press::None);
  CHECK(run(d, false, 3205, 3400) == Press::Short);
  // millis wrap
  Button w;
  w.update(false, 0xFFFFFF00u);
  Press r = Press::None;
  for (uint32_t u = 0xFFFFFF10u, n = 0; n < 60; n++, u += 5) { Press p = w.update(true, u); if (p != Press::None) r = p; }
  for (uint32_t u = 0xFFFFFF10u + 300, n = 0; n < 60; n++, u += 5) { Press p = w.update(false, u); if (p != Press::None) r = p; }
  CHECK(r == Press::Short);
}

static void colours() {
  Rgb o = lightColor(Light::Off), g = lightColor(Light::Running), a = lightColor(Light::Paused);
  CHECK(o.r == 0 && o.g == 0 && o.b == 0);
  CHECK(g.g > 0 && g.g <= 12 && g.r == 0);
  CHECK(a.r > 0 && a.r <= 12);
}

int main() {
  frames(); parse(); probe(); clicks(); button(); colours();
  if (fails) { printf("seesaw_test: %d failure(s)\n", fails); return 1; }
  printf("seesaw_test: ok\n");
  return 0;
}
