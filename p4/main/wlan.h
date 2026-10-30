// Wi-Fi station: connect, keep retrying, and say in plain words why it is down.
#pragma once
#include <Arduino.h>

namespace wlan {

struct Info {
  bool up = false;
  int rssi = 0;           // dBm, 0 while down
  char problem[80] = "";  // why it is down, "" while up (a copy: safe to keep and to read from any task)
  bool fixable = false;   // the problem is one the Settings tab can fix
};

void begin();  // connects with settings.wifiSsid / wifiPass; with no SSID set, opens the setup portal
// Setup portal: the panel is an access point (PORTAL_SSID / PORTAL_PASS) and
// the web server serves /setup to anyone who joins, until Wi-Fi is configured.
bool portalActive();
extern const char *PORTAL_SSID, *PORTAL_PASS;
void loop();   // reconnect watchdog and RSSI sampling; call from loop() (NOT under UiLock: it calls the C6)
Info info();
bool up();     // just "has an IP address": cheap, for tasks that do not need the text
String report();  // one line for GET /: signal, channel, power save, drops

// Networks seen by the portal's background scan (the /setup page lists them).
// scanStart() is a no-op while one runs (returns false if none can run); scanCount() is -1 until a scan finished.
// scanGeneration() counts finished scans, so a caller can tell its scan from an older result. All of these
// are safe from any task.
bool scanStart();
bool scanRunning();
uint32_t scanGeneration();
int scanCount();
String scanSsid(int i);
int scanRssi(int i);
// The panel's AP address while the portal is up ("192.168.4.1").
String portalIp();

// Signal quality from RSSI: 0 (none) .. 4 (excellent).
int bars(int rssi);

}  // namespace wlan
