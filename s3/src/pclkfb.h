// Display pixel clock change with a safe fallback. Pure logic (no ESP headers), tested in test/host.
//
// A pixel clock the panel does not like garbles the picture (a colour pattern, or shaking), and a garbled screen cannot
// confirm anything. So a clock chosen on the setup page (or GET /pclk) starts a TRIAL: the panel shows a "Keep this
// setting" button, and only a tap on it makes the choice permanent. Boots are counted before the display starts; after
// TRIAL_BOOTS unconfirmed boots the next boot goes back to the last CONFIRMED clock.
// (Same shape as the P4's panelfb.h; the "variant" there is a clock in MHz here.)
#pragma once

namespace pclkfb {

static const int MIN_MHZ = 10, MAX_MHZ = 20;  // what settings.cpp accepts
static const int DEFAULT_MHZ = 16;
static const int TRIAL_BOOTS = 3;  // unconfirmed boots a new clock gets; the one after that reverts

struct State {
  int cur = DEFAULT_MHZ;   // clock to use now
  int prev = DEFAULT_MHZ;  // last confirmed clock (what a revert goes back to)
  bool trial = false;
  int boots = 0;           // boots spent in the current trial
};

enum class Boot { Plain, Trial, Reverted };

inline bool validMHz(int v) { return v >= MIN_MHZ && v <= MAX_MHZ; }

// Repairs a state read from NVS (out-of-range values, a trial that makes no sense).
inline void sanitize(State &s) {
  if (!validMHz(s.cur)) s.cur = DEFAULT_MHZ;
  if (!validMHz(s.prev)) s.prev = s.cur;
  if (s.boots < 0) s.boots = 0;
  if (s.cur == s.prev) { s.trial = false; s.boots = 0; }
}

// The user picked clock v. During a trial prev stays the last confirmed clock, so a second wrong guess still falls back to
// the one that worked. Choosing the confirmed clock again simply ends the trial.
inline void select(State &s, int v) {
  if (!validMHz(v)) return;
  if (!s.trial) s.prev = s.cur;
  s.cur = v;
  s.boots = 0;
  s.trial = s.cur != s.prev;
}

// Called once per boot, before the display starts. Reverted: s.cur is the previous clock again.
inline Boot onBoot(State &s) {
  if (!s.trial) return Boot::Plain;
  if (s.boots >= TRIAL_BOOTS) {
    s.cur = s.prev;
    s.trial = false;
    s.boots = 0;
    return Boot::Reverted;
  }
  s.boots++;
  return Boot::Trial;
}

// The user tapped "Keep".
inline void confirm(State &s) {
  s.prev = s.cur;
  s.trial = false;
  s.boots = 0;
}

}  // namespace pclkfb
