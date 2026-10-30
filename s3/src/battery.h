// Battery level, from the first of:
//   1. a MAX17048 / MAX17043 fuel gauge (address 0x36) on the touch bus (GPIO19/20,
//      probed again every 30 s) or on the P4 connector (GPIO17/18, probed at boot);
//   2. the board's own voltage divider on an ADC pin (Guition: GPIO17), turned
//      into a percentage with a LiPo discharge curve. Less exact: it dips a few
//      percent under load and sits near 100 % while charging.
// The IP5306 charger itself has no data bus.
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
void loop();  // call from loop(); reads every few seconds
Info info();
// True while a gauge on the P4 connector (GPIO17/18) is in use: those pins are
// then neither free for an I2C scan nor for the battery ADC.
bool usesP4();

}  // namespace battery
