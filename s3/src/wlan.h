// Wi-Fi station: connect, keep retrying, and say in plain words why it is down.
#pragma once
#include <Arduino.h>

namespace wlan {

struct Info {
  bool up = false;
  int rssi = 0;           // dBm, 0 while down
  const char *problem;    // why it is down, "" while up
  bool fixable = false;   // the problem is one the Settings tab can fix
};

void begin();  // connects with settings.wifiSsid / wifiPass; with no SSID set, opens the setup portal
// Setup portal: the panel is an access point (PORTAL_SSID / PORTAL_PASS) and
// the web server serves /setup to anyone who joins, until Wi-Fi is configured.
bool portalActive();
extern const char *PORTAL_SSID, *PORTAL_PASS;
void loop();   // reconnect watchdog and RSSI sampling; call from loop()
Info info();
String report();  // one line for GET /: signal, channel, power save, drops

// Signal quality from RSSI: 0 (none) .. 4 (excellent).
int bars(int rssi);

}  // namespace wlan
