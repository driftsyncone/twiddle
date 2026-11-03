// The web login (user "ota"): one random password per device, made at first boot, kept in NVS (namespace vgpanel,
// key "otapw"), shown ONLY on the panel's Settings tab (never on a web page, in the serial log or at GET /). Used for
// /update and every admin endpoint; until a password exists they answer 403, with a wrong one 401.
//
// Dev builds: if secrets.h defines OTA_PASSWORD, that fixed password is used instead, as before, and "reset" is
// disabled. A release image never has one (build_config.h).
#pragma once

namespace otapw {

// Call AFTER Wi-Fi has been started (wlan::begin()): the ESP32-S3 random number generator is only a true one while
// the Wi-Fi/Bluetooth radio runs (IDF "Random Number Generation"), and the password must not be guessable. Makes the
// password on a unit that has none.
void begin();
bool known();        // false only if no password could be made or stored: every login then answers 403
const char *get();   // the password ("" while !known()); the panel shows it, never logs it
bool fixedByBuild(); // true on a dev build with OTA_PASSWORD compiled in (cannot be reset)
bool reset();        // new random password, stored; the old one stops working at once. False on a dev build

}  // namespace otapw
