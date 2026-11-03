// Display-variant change with a safe fallback. Pure logic (no ESP headers), tested in test/host.
//
// A wrong panel variant leaves the screen black, and a black screen cannot confirm anything. So a variant chosen
// on the setup portal (or /panel) starts a TRIAL: the panel shows a "Keep this setting" button, and only a tap
// on it makes the choice permanent. Boots are counted before the display starts; after TRIAL_BOOTS unconfirmed
// boots the next boot goes back to the last CONFIRMED variant.
#pragma once

namespace panelfb {

static const int VARIANTS = 5;     // panel variants 0..4 (settings.h)
static const int TRIAL_BOOTS = 3;  // unconfirmed boots a new variant gets; the one after that reverts

struct State {
  int cur = 0;     // variant to use now
  int prev = 0;    // last confirmed variant (what a revert goes back to)
  bool trial = false;
  int boots = 0;   // boots spent in the current trial
};

enum class Boot { Plain, Trial, Reverted };

inline bool validVariant(int v) { return v >= 0 && v < VARIANTS; }

// Repairs a state read from NVS (out-of-range values, a trial that makes no sense).
inline void sanitize(State &s) {
  if (!validVariant(s.cur)) s.cur = 0;
  if (!validVariant(s.prev)) s.prev = s.cur;
  if (s.boots < 0) s.boots = 0;
  if (s.cur == s.prev) { s.trial = false; s.boots = 0; }
}

// The user picked variant v. During a trial prev stays the last confirmed variant, so a second wrong guess still
// falls back to the one that worked. Choosing the confirmed variant again simply ends the trial.
inline void select(State &s, int v) {
  if (!validVariant(v)) return;
  if (!s.trial) s.prev = s.cur;
  s.cur = v;
  s.boots = 0;
  s.trial = s.cur != s.prev;
}

// Called once per boot, before the display starts. Reverted: s.cur is the previous variant again.
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

inline const char *name(int v) {
  switch (v) {
    case 0: return "V2 (default)";
    case 1: return "original";
    case 2: return "V2 table, reset pin 5";
    case 3: return "original table, reset pin 0";
    case 4: return "new panel (_Y)";
    default: return "?";
  }
}

}  // namespace panelfb
