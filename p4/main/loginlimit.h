// Web login lockout: 5 wrong passwords in a row lock every admin login for 60 s. Pure logic (the caller passes
// millis()), so test/host can check it. One counter for the whole panel, not per client: simple, and a short
// password needs a hard brake. A request with NO credentials (a browser's first visit) is not a failure.
#pragma once
#include <stdint.h>

namespace loginlimit {

static const int MAX_FAILS = 5;
static const uint32_t LOCK_MS = 60000;

struct State {
  int fails = 0;
  uint32_t lockedUntil = 0;  // 0 = not locked
};

// True while locked. Wrap-safe for a millis() counter.
inline bool blocked(const State &s, uint32_t now) { return s.lockedUntil != 0 && (int32_t)(s.lockedUntil - now) > 0; }

inline uint32_t remainingSec(const State &s, uint32_t now) {
  return blocked(s, now) ? (uint32_t)((s.lockedUntil - now) + 999) / 1000 : 0;
}

// Call for a wrong password only (a request that carried credentials). The 5th in a row starts the lockout.
inline void failure(State &s, uint32_t now) {
  if (blocked(s, now)) return;
  if (++s.fails >= MAX_FAILS) {
    s.fails = 0;
    s.lockedUntil = now + LOCK_MS;
    if (s.lockedUntil == 0) s.lockedUntil = 1;
  }
}

inline void success(State &s) { s = State(); }

}  // namespace loginlimit
