// The Autopilot graph as a moving window: the last 10 s and the next 30 s, "now" at 25 % from the left.
// Pure arithmetic (no LVGL), host-tested; ui.cpp maps chart points to session times with it. Identical in both firmwares.
#pragma once
#include <stdint.h>

namespace apwin {

constexpr int64_t TRAIL_MS = 10000, AHEAD_MS = 30000, SPAN_MS = TRAIL_MS + AHEAD_MS;

// Chart point i (0..points-1) -> session time in ms. Negative or past the end = outside the session (draw nothing).
inline int64_t timeAt(int i, int points, int64_t nowMs) { return nowMs - TRAIL_MS + (int64_t)i * SPAN_MS / (points - 1); }

// The point "now" sits on (25 % of the width).
inline int nowIndex(int points) { return (int)((int64_t)(points - 1) * TRAIL_MS / SPAN_MS); }

inline bool inSession(int64_t t, uint32_t sessMs) { return t >= 0 && t <= (int64_t)sessMs; }

// A tapped time, kept inside the session.
inline uint32_t clampMs(int64_t t, uint32_t sessMs) { return t < 0 ? 0 : t > (int64_t)sessMs ? sessMs : (uint32_t)t; }

// Session time -> nearest chart point, -1 when outside the window.
inline int indexFor(int64_t t, int points, int64_t nowMs) {
  int64_t rel = t - (nowMs - TRAIL_MS);
  if (rel < 0 || rel > SPAN_MS) return -1;
  return (int)((rel * (points - 1) + SPAN_MS / 2) / SPAN_MS);
}

// Axis label for time t: whole seconds relative to now (-10 .. +30).
inline int secondsRel(int64_t t, int64_t nowMs) {
  int64_t d = t - nowMs;
  return (int)(d >= 0 ? (d + 500) / 1000 : -((-d + 500) / 1000));
}

// HR mode: whole seconds since a time in the last 10 s (index into the 1 Hz history, 0 = now), -1 if older than the window.
inline int ageSec(int64_t t, int64_t nowMs) {
  int64_t d = nowMs - t;
  if (d < 0) return -1;
  int a = (int)((d + 500) / 1000);
  return a > TRAIL_MS / 1000 ? -1 : a;
}

// The dim flat projection of an HR session is drawn dashed: 2 points on, 2 off.
inline bool dashOn(int i) { return (i & 3) < 2; }

}  // namespace apwin
