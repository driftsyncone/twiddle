// Host test of the pure logic behind the web-flasher prerequisites (header-only, no ESP / Arduino / LVGL): the per-device
// password generator (src/pwgen.h), the display pixel clock trial/revert (src/pclkfb.h) and the setup form rules (src/setupform.h).
// Run: test/host/run.sh
#include <stdio.h>
#include <string.h>

#include <set>
#include <string>

#include "pclkfb.h"
#include "loginlimit.h"
#include "pwgen.h"
#include "setupform.h"

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

using pclkfb::Boot;
using pclkfb::State;

static void testPclkFallback() {
  State s;  // fresh unit: 16 MHz, confirmed
  CHECK(s.cur == 16 && s.prev == 16 && !s.trial);
  CHECK(pclkfb::onBoot(s) == Boot::Plain);

  // Pick 14; it works and the user taps Keep.
  pclkfb::select(s, 14);
  CHECK(s.cur == 14 && s.prev == 16 && s.trial && s.boots == 0);
  CHECK(pclkfb::onBoot(s) == Boot::Trial && s.cur == 14 && s.boots == 1);
  pclkfb::confirm(s);
  CHECK(s.cur == 14 && s.prev == 14 && !s.trial);
  CHECK(pclkfb::onBoot(s) == Boot::Plain && s.cur == 14);

  // Pick 18, the picture is garbled and nobody can tap: after TRIAL_BOOTS boots the next one reverts to 14.
  pclkfb::select(s, 18);
  for (int i = 0; i < pclkfb::TRIAL_BOOTS; i++) CHECK(pclkfb::onBoot(s) == Boot::Trial && s.cur == 18);
  CHECK(pclkfb::onBoot(s) == Boot::Reverted);
  CHECK(s.cur == 14 && s.prev == 14 && !s.trial);
  CHECK(pclkfb::onBoot(s) == Boot::Plain);

  // Two wrong guesses in a row still fall back to the last CONFIRMED clock (14), not to the first wrong guess.
  pclkfb::select(s, 12);
  CHECK(pclkfb::onBoot(s) == Boot::Trial);
  pclkfb::select(s, 20);
  CHECK(s.cur == 20 && s.prev == 14 && s.trial && s.boots == 0);  // new guess gets a fresh trial
  for (int i = 0; i < pclkfb::TRIAL_BOOTS; i++) pclkfb::onBoot(s);
  CHECK(pclkfb::onBoot(s) == Boot::Reverted && s.cur == 14);

  // Choosing the confirmed clock again ends the trial without a revert.
  pclkfb::select(s, 12);
  CHECK(s.trial);
  pclkfb::select(s, 14);
  CHECK(!s.trial && s.cur == 14 && s.prev == 14);
  CHECK(pclkfb::onBoot(s) == Boot::Plain);

  // Selecting the clock that is already confirmed starts nothing.
  pclkfb::select(s, 14);
  CHECK(!s.trial);

  // Invalid input is ignored.
  pclkfb::select(s, 9);
  pclkfb::select(s, 21);
  pclkfb::select(s, -1);
  CHECK(s.cur == 14 && !s.trial);

  // A state restored from NVS is repaired: an out-of-range clock, a trial with nothing to try.
  State bad;
  bad.cur = 77; bad.prev = -3; bad.trial = true; bad.boots = -5;
  pclkfb::sanitize(bad);
  CHECK(bad.cur == 16 && bad.prev == 16 && !bad.trial && bad.boots == 0);
  State odd;
  odd.cur = 14; odd.prev = 99; odd.trial = true; odd.boots = 1;
  pclkfb::sanitize(odd);
  CHECK(odd.cur == 14 && odd.prev == 14 && !odd.trial);

  // Existing saved values keep working: a unit from before this feature has only "pclk" (the 14 MHz unit, MAC ...80:c4;
  // prev defaults to cur when its key is missing, trial false): it boots plain at 14 and nothing changes.
  State old;
  old.cur = 14; old.prev = 14;
  pclkfb::sanitize(old);
  CHECK(pclkfb::onBoot(old) == Boot::Plain && old.cur == 14 && !old.trial);
  // ... and so does a legacy 16, and a legacy value anywhere in 10..20.
  for (int v = pclkfb::MIN_MHZ; v <= pclkfb::MAX_MHZ; v++) {
    State l;
    l.cur = v; l.prev = v;
    pclkfb::sanitize(l);
    CHECK(l.cur == v && pclkfb::onBoot(l) == Boot::Plain && l.cur == v);
  }
}

static void testSetupForm() {
  using namespace setupform;
  CHECK(wifiPass("old", "", true) == "old");          // empty field, same network: keep
  CHECK(wifiPass("old", "", false).empty());          // empty field, new network: the old password does not belong to it
  CHECK(wifiPass("old", "new", true) == "new");       // typed: replace
  CHECK(wifiPass("old", "new", false) == "new");
  CHECK(wifiPass("", "", true).empty());
  CHECK(token("tok", "") == "tok");                   // empty: keep
  CHECK(token("tok", "   \r\n") == "tok");            // blanks only: keep
  CHECK(token("tok", "  fresh \n") == "fresh");       // typed: replace, trimmed
  CHECK(token("", "x") == "x");
}

int main() {
  testPassword();
  testLoginLimit();
  testPclkFallback();
  testSetupForm();
  printf("%d checks, %d failed\n", checks, fails);
  return fails ? 1 : 0;
}
