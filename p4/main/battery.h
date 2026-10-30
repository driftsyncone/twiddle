// Battery level from a MAX17048 / MAX17043 fuel gauge (address 0x36) on the touch I2C bus
// (GPIO7/8), probed again every 30 s. The Guition JC1060P470C has no gauge of its own, so
// without one this reports "not present". There is no voltage-divider fallback.
#pragma once

namespace battery {

struct Info {
  bool present = false;  // a gauge answered
  float percent = 0;     // state of charge, 0..100
  float volts = 0;       // cell voltage
  bool charging = false; // MAX17048 only (from its charge-rate register)
  const char *chip = ""; // "MAX17048" / "MAX17043" / "divider"
};

void begin();
void loop();  // call from loop(), outside UiLock (it talks I2C, 50 ms timeouts); reads every few seconds
Info info();  // a consistent copy, safe from any task

}  // namespace battery
