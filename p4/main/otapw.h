// The web login (user "ota"): one random password per device, made at first boot, kept in NVS (namespace vgpanel,
// key "otapw"), shown ONLY on the Settings tab (never on a web page or in the log). Used for /update and every admin endpoint.
//
// Dev builds: if secrets.h defines OTA_PASSWORD (or OTA_PASS) that fixed password is used instead, as before, and
// "reset" is disabled. A release image never has one (build_config.h).
#pragma once
#include <stddef.h>

namespace otapw {

void begin();             // after NVS is up; makes the password on a unit that has none
bool known();             // false only if no password could be made or stored: every login then answers 403
const char *get();        // the password ("" while !known()); the panel shows it, never logs it
bool fixedByBuild();      // true on a dev build with OTA_PASSWORD compiled in (cannot be reset)
bool reset();             // new random password, stored; the old one stops working at once. False on a dev build
bool authorised(const char *authorizationHeader);  // "Basic base64(ota:password)", compared in constant time

}  // namespace otapw
