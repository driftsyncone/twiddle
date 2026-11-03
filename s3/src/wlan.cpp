#include "wlan.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include "ble.h"
#include "log.h"
#include "build_config.h"
#include "settings.h"

namespace wlan {

// DHCP hostname and mDNS name (twiddle-s3.local).
#ifndef PANEL_HOSTNAME  // set in secrets.h to tell several panels apart
#define PANEL_HOSTNAME "twiddle-s3"
#endif
static const char *HOSTNAME = PANEL_HOSTNAME;

// Written by the Wi-Fi event task, read on the loop task.
static volatile int lastReason;  // 0 = none yet
static volatile bool gotIp;
static volatile uint32_t drops, lostAt;  // disconnects since boot; millis() of the last
static volatile int dropReason, dropRssi;  // the last one, for the log (event task can't plog safely mid-way)
static volatile bool dropNew;
static uint32_t upSince;

static int rssi;
static uint32_t downSince, lastAttempt, lastSample;
static bool portal;
const char *PORTAL_SSID = "Twiddle-Setup", *PORTAL_PASS = "twiddle-setup";

// Auto-reconnect normally brings the link back within seconds; this restarts
// the attempt from scratch when it hasn't.
static const uint32_t RETRY_MS = 10000;

static void onEvent(arduino_event_id_t id, arduino_event_info_t info) {
  if (id == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    gotIp = true;
    lastReason = 0;
    upSince = millis();
  } else if (id == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    gotIp = false;
    int r = info.wifi_sta_disconnected.reason;
    // Our own disconnect() before a retry; keep the reason that got us here.
    if (r != WIFI_REASON_ASSOC_LEAVE) lastReason = r;
    if (upSince) {  // was up: a drop (not one more failed attempt while down)
      upSince = 0;
      drops = drops + 1;
      lostAt = millis();
      dropReason = r;
      dropRssi = info.wifi_sta_disconnected.rssi;
      dropNew = true;
    }
  }
}

static void connect() {
  lastAttempt = millis();
  WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPass.c_str());
}

void begin() {
  WiFi.setHostname(HOSTNAME);  // must come before mode()/begin()
  WiFi.persistent(false);  // no flash writes on connect (they glitch the display)
  WiFi.mode(WIFI_STA);
  // No power save unless Bluetooth runs: IDF refuses Bluetooth next to Wi-Fi
  // without modem sleep, but modem sleep only hears the access point at its
  // beacons (slower answers, and on a weak signal missed beacons drop the link).
  WiFi.setSleep(ble::enabled() ? WIFI_PS_MIN_MODEM : WIFI_PS_NONE);
  WiFi.setAutoReconnect(true);
  // Weak signal: look at every channel and join the strongest access point,
  // not the first one that answers, and ride out a few missed beacons (default
  // 6 s) instead of dropping the link and starting over.
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  esp_wifi_set_inactive_time(WIFI_IF_STA, 10);
  WiFi.onEvent(onEvent);
  downSince = millis();
  if (settings.wifiSsid.length()) {
    connect();
  } else {
    // Nothing to join: be a network instead, so a phone can do the setup.
    portal = true;
    WiFi.mode(WIFI_AP_STA);  // STA stays up so the portal page can scan
    WiFi.softAP(PORTAL_SSID, PORTAL_PASS);
    WiFi.scanNetworks(true);
    Serial.printf("setup portal: join %s / %s, open http://%s/setup\n", PORTAL_SSID, PORTAL_PASS,
                  WiFi.softAPIP().toString().c_str());
  }
}

bool portalActive() { return portal; }

void loop() {
  uint32_t now = millis();
  if (dropNew) {
    dropNew = false;
    plog("Wi-Fi dropped: reason %d, rssi %d, drop #%u\n", dropReason, dropRssi, (unsigned)drops);
  }
  bool up = WiFi.status() == WL_CONNECTED;
  if (up) {
    static bool mdns;
    if (!mdns && MDNS.begin(HOSTNAME)) {
      MDNS.addService("http", "tcp", 80);
      mdns = true;
    }
    static uint32_t loggedLost;
    if (lostAt && loggedLost != lostAt && gotIp) {
      loggedLost = lostAt;
      plog("Wi-Fi back after %.1f s, rssi %d, ch %d\n", (now - lostAt) / 1000.0f, WiFi.RSSI(), (int)WiFi.channel());
    }
    downSince = now;
    if (now - lastSample > 2000) {
      lastSample = now;
      rssi = WiFi.RSSI();
    }
    return;
  }
  rssi = 0;
  // Auto-reconnect gives up on some reasons (e.g. a failed handshake), so
  // start over from scratch every so often while down.
  if (settings.wifiSsid.length() && now - downSince > RETRY_MS && now - lastAttempt > RETRY_MS &&
      WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
    plog("Wi-Fi: still down, starting over\n");
    WiFi.disconnect();
    connect();
  }
}

static const char *reasonText(int r) {
  switch (r) {
    case 0: return "connecting...";
    case WIFI_REASON_NO_AP_FOUND:
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
    case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:
      return "network not found";
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_MIC_FAILURE:
      return "wrong password?";
    case WIFI_REASON_BEACON_TIMEOUT:
    case WIFI_REASON_MISSING_ACKS:
    case WIFI_REASON_DISASSOC_DUE_TO_INACTIVITY:
      return "signal lost";
    case WIFI_REASON_ASSOC_FAIL:
    case WIFI_REASON_ASSOC_TOOMANY:
    case WIFI_REASON_AP_INITIATED:
      return "router dropped the panel";
    default: {
      static char buf[32];
      snprintf(buf, sizeof buf, "disconnected (code %d)", r);
      return buf;
    }
  }
}

static bool fixable(int r) {
  switch (r) {
    case WIFI_REASON_NO_AP_FOUND:
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
    case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_MIC_FAILURE:
      return true;
    default: return false;
  }
}

Info info() {
  Info i;
  i.up = WiFi.status() == WL_CONNECTED && gotIp;
  i.rssi = i.up ? rssi : 0;
  if (i.up) {
    i.problem = "";
  } else if (portal) {
    i.problem = "setup: join Wi-Fi Twiddle-Setup (pw twiddle-setup), open 192.168.4.1/setup";
  } else if (!settings.wifiSsid.length()) {
    i.problem = "not set up";
    i.fixable = true;
  } else {
    i.problem = reasonText(lastReason);
    i.fixable = fixable(lastReason);
  }
  return i;
}

String report() {
  wifi_ps_type_t ps = WIFI_PS_NONE;
  esp_wifi_get_ps(&ps);
  char b[160];
  snprintf(b, sizeof b, "wifi: %s rssi %d ch %d bssid %s, power save %s, drops %u, last reason %d\n",
           WiFi.status() == WL_CONNECTED ? "up" : "down", (int)WiFi.RSSI(), (int)WiFi.channel(),
           WiFi.BSSIDstr().c_str(), ps == WIFI_PS_NONE ? "off" : "modem", (unsigned)drops, dropReason);
  return b;
}

int bars(int r) {
  if (r == 0) return 0;
  if (r >= -55) return 4;
  if (r >= -65) return 3;
  if (r >= -75) return 2;
  return 1;
}

}  // namespace wlan
