// What a Devices-tab row shows, decided from the device's current state. Pure logic (no LVGL), so the host
// tests cover it; ui.cpp applies the result on every refresh, so a row is right whenever the state changes
// (a heart rate monitor's caps are known late, a remembered toy drops out, ...). Identical in both firmwares.
#pragma once
#include <stdint.h>

namespace devrow {

enum class Primary : uint8_t { None, Connect, Connecting };  // Connecting: disabled, shows progress

struct Layout {
  Primary primary;  // the Connect button
  bool forget;      // the Forget button
  bool follow;      // Follow switch + share buttons (an output)
  bool selectable;  // a tap on the row shows this device's controls
  uint8_t key() const { return (uint8_t)primary | (forget ? 4 : 0) | (follow ? 8 : 0) | (selectable ? 16 : 0); }
};

// hasAddr: a Bluetooth device (the cloud device has no buttons). hr: caps has CAP_HR (an input).
inline Layout layout(bool hasAddr, bool known, bool connected, bool connecting, bool hr) {
  Layout l = {Primary::None, false, !hr, !hr};
  if (!hasAddr) return l;
  if (connected) l.forget = true;                        // up: Forget (it stays remembered until then)
  else if (connecting) { l.primary = Primary::Connecting; l.forget = known; }
  else { l.primary = Primary::Connect; l.forget = known; }  // a remembered one that dropped: Connect and Forget
  return l;
}

}  // namespace devrow
