// The encoder's live Autopilot scale (percent of the planned speed, lowered by turning the knob).
// Pure logic, host-tested. The scale is only kept when the SAME session resumes after a pause; any other
// start (a new session, or coming back from another mode) begins at 100 %, so a lowered scale never carries over.
#pragma once

namespace encscale {

constexpr int FULL = 100;

// resumed: this start continues an Autopilot session that was just paused (not stopped, not interrupted by another mode).
inline int onStart(int current, bool resumed) { return resumed ? current : FULL; }

}  // namespace encscale
