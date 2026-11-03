// Host test of the pure logic behind the web-flasher prerequisites: the per-device password generator (main/pwgen.h)
// and the display-variant fallback (main/panelfb.h). No hardware. Run: test/host/run.sh
#include <stdio.h>
#include <string.h>

#include <set>
#include <string>

#include "../../main/chiprev.h"
#include "../../main/panelfb.h"
#include "../../main/portalroute.h"
#include "../../main/loginlimit.h"
#include "../../main/pwgen.h"

static int fails, checks;
#define CHECK(cond)                                                \
  do {                                                             \
    checks++;                                                      \
    if (!(cond)) {                                                 \
      fails++;                                                     \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
    }                                                              \
  } while (0)

// xorshift32, good enough as a deterministic test source.
static uint32_t seed = 2463534242u;
static uint32_t rnd() {
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  return seed;
}

static void testPassword() {
  CHECK(pwgen::LENGTH == 6);  // short on purpose; the login lockout (loginlimit.h) is what makes that safe
  // The alphabet has no look-alikes and no duplicates.
  for (const char *bad = "0O1IL5S2Z"; *bad; bad++) CHECK(!strchr(pwgen::ALPHABET, *bad));
  std::set<char> uniq(pwgen::ALPHABET, pwgen::ALPHABET + pwgen::ALPHABET_LEN);
  CHECK(uniq.size() == pwgen::ALPHABET_LEN);

  std::set<std::string> seen;
  int counts[256] = {};
  const int N = 20000;
  for (int i = 0; i < N; i++) {
    char pw[32];
    CHECK(pwgen::generate(pw, sizeof pw, rnd));
    CHECK(strlen(pw) == pwgen::LENGTH);
    CHECK(pwgen::valid(pw));
    seen.insert(pw);
    for (const char *c = pw; *c; c++) counts[(unsigned char)*c]++;
  }
  CHECK(seen.size() > (size_t)N * 99 / 100);  // 20000 draws of 28.5 bits: a few birthday repeats are expected
  // Roughly uniform: every character within 15 % of the mean.
  double mean = (double)N * pwgen::LENGTH / pwgen::ALPHABET_LEN;
  for (size_t k = 0; k < pwgen::ALPHABET_LEN; k++) {
    double c = counts[(unsigned char)pwgen::ALPHABET[k]];
    CHECK(c > mean * 0.85 && c < mean * 1.15);
  }
  char small[6];
  CHECK(!pwgen::generate(small, sizeof small, rnd));  // no room for the NUL (6 characters need 7 bytes)
  char fits[7];
  CHECK(pwgen::generate(fits, sizeof fits, rnd) && strlen(fits) == 6);

  // Rejection sampling: a source stuck at the top of the range is skipped, not folded into the alphabet.
  int calls = 0;
  char pw[32];
  CHECK(pwgen::generate(pw, sizeof pw, [&]() -> uint32_t { return calls++ % 2 == 0 ? 0xFFFFFFFFu : 0u; }));
  CHECK(pw[0] == pwgen::ALPHABET[0] && calls == 2 * (int)pwgen::LENGTH);

  CHECK(!pwgen::valid(""));
  CHECK(!pwgen::valid("ABCDE"));           // 5 characters
  CHECK(!pwgen::valid("ABCDEFG"));         // 7 characters
  CHECK(!pwgen::valid("ABCDE0"));          // 0 is not in the alphabet
  CHECK(!pwgen::valid("ABCDEFGHJKMN"));    // an old 12-character password is replaced by a 6-character one
  CHECK(pwgen::valid("ABCDEM"));

  CHECK(pwgen::constantTimeEqual("abc", 3, "abc", 3));
  CHECK(!pwgen::constantTimeEqual("abc", 3, "abd", 3));
  CHECK(!pwgen::constantTimeEqual("abc", 3, "abcd", 4));
  CHECK(!pwgen::constantTimeEqual("", 0, "a", 1));
  CHECK(pwgen::constantTimeEqual("", 0, "", 0));
}

static void testLoginLimit() {
  using namespace loginlimit;
  State s;
  uint32_t t0 = 1000;
  CHECK(!blocked(s, t0) && remainingSec(s, t0) == 0);
  for (int i = 0; i < MAX_FAILS - 1; i++) { failure(s, t0 + i); CHECK(!blocked(s, t0 + i)); }  // 4 wrong tries: still open
  failure(s, t0 + 10);                                                                         // the 5th locks
  CHECK(blocked(s, t0 + 11) && remainingSec(s, t0 + 11) == 60);
  CHECK(blocked(s, t0 + 10 + LOCK_MS - 1) && !blocked(s, t0 + 10 + LOCK_MS));                  // exactly 60 s
  CHECK(remainingSec(s, t0 + 10 + LOCK_MS - 1) == 1);
  failure(s, t0 + 500);  // wrong tries during the lock do not extend it
  CHECK(!blocked(s, t0 + 10 + LOCK_MS));
  // After the lock a fresh count of 5 starts.
  uint32_t t1 = t0 + 10 + LOCK_MS + 5;
  for (int i = 0; i < MAX_FAILS - 1; i++) failure(s, t1);
  CHECK(!blocked(s, t1));
  success(s);  // a right password resets the count
  for (int i = 0; i < MAX_FAILS - 1; i++) failure(s, t1);
  CHECK(!blocked(s, t1));
  failure(s, t1);
  CHECK(blocked(s, t1));
  // millis() wrapping past 2^32 while locked.
  State w;
  uint32_t near = 0xFFFFFFFFu - 10000;
  for (int i = 0; i < MAX_FAILS; i++) failure(w, near);
  CHECK(blocked(w, near + 20000) && !blocked(w, near + 60001));
  // A lock that would end on the value 0 is stored as 1, never 0 (0 means "not locked").
  State z;
  for (int i = 0; i < MAX_FAILS; i++) failure(z, 0u - LOCK_MS);
  CHECK(z.lockedUntil != 0 && blocked(z, 0u - LOCK_MS + 1));
}

using panelfb::Boot;
using panelfb::State;

static void testFallback() {
  State s;  // fresh unit: V2, confirmed
  CHECK(panelfb::onBoot(s) == Boot::Plain);

  // Pick the _Y panel; it works and the user taps Keep.
  panelfb::select(s, 4);
  CHECK(s.cur == 4 && s.prev == 0 && s.trial && s.boots == 0);
  CHECK(panelfb::onBoot(s) == Boot::Trial && s.cur == 4);
  panelfb::confirm(s);
  CHECK(s.cur == 4 && s.prev == 4 && !s.trial);
  CHECK(panelfb::onBoot(s) == Boot::Plain && s.cur == 4);

  // Pick the original panel, the screen stays black, nobody can tap: after TRIAL_BOOTS boots the next one reverts.
  panelfb::select(s, 1);
  for (int i = 0; i < panelfb::TRIAL_BOOTS; i++) CHECK(panelfb::onBoot(s) == Boot::Trial && s.cur == 1);
  CHECK(panelfb::onBoot(s) == Boot::Reverted);
  CHECK(s.cur == 4 && s.prev == 4 && !s.trial);
  CHECK(panelfb::onBoot(s) == Boot::Plain);

  // Two wrong guesses in a row still fall back to the last CONFIRMED variant (4), not to the first wrong guess.
  panelfb::select(s, 2);
  CHECK(panelfb::onBoot(s) == Boot::Trial);
  panelfb::select(s, 3);
  CHECK(s.cur == 3 && s.prev == 4 && s.trial && s.boots == 0);  // new guess gets a fresh trial
  for (int i = 0; i < panelfb::TRIAL_BOOTS; i++) panelfb::onBoot(s);
  CHECK(panelfb::onBoot(s) == Boot::Reverted && s.cur == 4);

  // Choosing the confirmed variant again ends the trial without a revert.
  panelfb::select(s, 0);
  CHECK(s.trial);
  panelfb::select(s, 4);
  CHECK(!s.trial && s.cur == 4 && s.prev == 4);
  CHECK(panelfb::onBoot(s) == Boot::Plain);

  // Invalid input is ignored.
  panelfb::select(s, 9);
  panelfb::select(s, -1);
  CHECK(s.cur == 4 && !s.trial);

  // A state restored from NVS is repaired: an out-of-range variant, a trial with nothing to try.
  State bad;
  bad.cur = 77; bad.prev = -3; bad.trial = true; bad.boots = -5;
  panelfb::sanitize(bad);
  CHECK(bad.cur == 0 && bad.prev == 0 && !bad.trial && bad.boots == 0);
  State odd;
  odd.cur = 4; odd.prev = 99; odd.trial = true; odd.boots = 1;
  panelfb::sanitize(odd);
  CHECK(odd.cur == 4 && odd.prev == 4 && !odd.trial);
  // A unit from before this feature has only the "panel" key (cur=4, prev defaults to cur): no trial.
  State old;
  old.cur = 4; old.prev = 4;
  CHECK(panelfb::onBoot(old) == Boot::Plain && old.cur == 4);
}

static void testChipRev() {
  CHECK(chiprev::warning(103) == nullptr);        // v1.3: the tested one
  CHECK(chiprev::warning(100) != nullptr);        // v1.0: boots, but untested
  CHECK(chiprev::warning(1) != nullptr);          // v0.1
  const char *w3 = chiprev::warning(300);         // v3.0
  CHECK(w3 && strstr(w3, "newer"));
  CHECK(chiprev::warning(199) && !strstr(chiprev::warning(199), "newer"));  // v1.99 is inside the IDF range
  CHECK(chiprev::warning(200) && strstr(chiprev::warning(200), "newer"));
}

static void testRoute() {
  const char *ap = "192.168.4.1";
  CHECK(portalroute::rootGoesToSetup(true, "192.168.4.1", ap));
  CHECK(portalroute::rootGoesToSetup(true, "192.168.4.1:80", ap));
  CHECK(!portalroute::rootGoesToSetup(false, "192.168.4.1", ap));       // portal down: status page
  CHECK(!portalroute::rootGoesToSetup(true, "twiddle-p4.local", ap));   // LAN name
  CHECK(!portalroute::rootGoesToSetup(true, "192.168.4.10", ap));       // prefix is not a match
  CHECK(!portalroute::rootGoesToSetup(true, "", ap));
  CHECK(!portalroute::rootGoesToSetup(true, nullptr, ap));
}

int main() {
  testRoute();
  testPassword();
  testLoginLimit();
  testFallback();
  testChipRev();
  printf("%d checks, %d failed\n", checks, fails);
  return fails ? 1 : 0;
}
