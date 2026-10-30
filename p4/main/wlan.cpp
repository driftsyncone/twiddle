// Wi-Fi station through esp_wifi / esp_wifi_remote (the radio is the ESP32-C6 behind esp_hosted).
#include "wlan.h"

#include <Arduino.h>
#include <string.h>

#include "esp_event.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/sockets.h"
#include "mdns.h"

#include "ble.h"
#include "log.h"
#include "secrets.h"
#include "settings.h"

namespace wlan {

#ifndef PANEL_HOSTNAME  // set in secrets.h to tell several panels apart
#define PANEL_HOSTNAME "twiddle-p4"
#endif
static const char *HOSTNAME = PANEL_HOSTNAME;

// Written by the Wi-Fi event task, read on the loop task.
static volatile int lastReason;  // 0 = none yet
static volatile bool gotIp;
static volatile uint32_t drops, lostAt;  // disconnects since boot; millis() of the last
static volatile int dropReason, dropRssi;
static volatile bool dropNew;
static volatile uint32_t upSince;
static volatile bool staStarted;

// Link sample, written by loop() (loop task), read by info()/report() from any task: copied under a spinlock.
static int rssi;
static int channel;
static uint8_t bssid[6];
static portMUX_TYPE sampleMux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t downSince, lastAttempt, lastSample;
static bool portal;
static bool haveSsid;
static esp_netif_t *staNetif, *apNetif;
const char *PORTAL_SSID = "Twiddle-Setup", *PORTAL_PASS = "twiddle-setup";

// Auto-reconnect normally brings the link back within seconds; this restarts
// the attempt from scratch when it hasn't.
static const uint32_t RETRY_MS = 10000;

// ---------- scan results (setup portal) ----------
static const int MAX_AP = 20;
static const uint32_t SCAN_MAX_MS = 20000;  // a scan whose SCAN_DONE never came is given up after this
static wifi_ap_record_t *aps;
static volatile int apCount = -1;
static volatile bool scanning;
static volatile uint32_t scanT0, scanGen;
static SemaphoreHandle_t apLock;  // guards aps/apCount between the event task (writer) and readers

bool scanStart() {
  if (scanning) return true;
  if (!staStarted) return false;
  scanT0 = millis();
  scanning = true;
  if (esp_wifi_scan_start(nullptr, false) != ESP_OK) {
    scanning = false;
    return false;
  }
  return true;
}
bool scanRunning() { return scanning; }
uint32_t scanGeneration() { return scanGen; }
int scanCount() { return apCount; }
String scanSsid(int i) {
  String s;
  if (apLock && xSemaphoreTake(apLock, pdMS_TO_TICKS(200)) == pdTRUE) {
    if (aps && i >= 0 && i < apCount) s = String((const char *)aps[i].ssid);
    xSemaphoreGive(apLock);
  }
  return s;
}
int scanRssi(int i) {
  int r = 0;
  if (apLock && xSemaphoreTake(apLock, pdMS_TO_TICKS(200)) == pdTRUE) {
    if (aps && i >= 0 && i < apCount) r = aps[i].rssi;
    xSemaphoreGive(apLock);
  }
  return r;
}

static void onScanDone() {
  uint16_t n = MAX_AP;
  if (!aps) aps = (wifi_ap_record_t *)calloc(MAX_AP, sizeof(wifi_ap_record_t));
  xSemaphoreTake(apLock, portMAX_DELAY);
  if (aps && esp_wifi_scan_get_ap_records(&n, aps) == ESP_OK) apCount = n;
  else apCount = 0;
  xSemaphoreGive(apLock);
  scanGen = scanGen + 1;
  scanning = false;
}

// ---------- reconnect with backoff ----------
// A drop of a link that was up is retried at once (fast recovery). A failed attempt while down (wrong password,
// network not found, first connect) waits longer each time, so the C6 is not scanning back to back: that radio
// also carries the Bluetooth toys. Cleared when an IP address arrives.
static volatile bool retryPending;
static volatile uint32_t retryAt;
static volatile int failures;

static bool authFailure(int r) {
  switch (r) {
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_MIC_FAILURE:
    case WIFI_REASON_AUTH_EXPIRE:
      return true;
    default: return false;
  }
}

static uint32_t backoffMs(int r, int n) {  // n = failed attempts in a row, >= 1
  if (authFailure(r)) return n * 5000 > 30000 ? 30000 : n * 5000;  // 5 s, 10 s ... 30 s
  return n > 4 ? 8000 : 1000u << (n - 1);                           // 1 s, 2 s, 4 s, 8 s
}

static void onEvent(void *, esp_event_base_t base, int32_t id, void *data) {
  if (base == WIFI_EVENT) {
    if (id == WIFI_EVENT_STA_START) {
      staStarted = true;
      if (haveSsid) esp_wifi_connect();
    } else if (id == WIFI_EVENT_SCAN_DONE) {
      onScanDone();
    } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
      auto *d = (wifi_event_sta_disconnected_t *)data;
      gotIp = false;
      int r = d->reason;
      // Our own disconnect() before a retry; keep the reason that got us here.
      if (r != WIFI_REASON_ASSOC_LEAVE) lastReason = r;
      bool wasUp = upSince != 0;
      if (wasUp) {  // was up: a drop (not one more failed attempt while down)
        upSince = 0;
        drops = drops + 1;
        lostAt = millis();
        dropReason = r;
        dropRssi = d->rssi;
        dropNew = true;
        failures = 0;
      } else if (r != WIFI_REASON_ASSOC_LEAVE) {
        failures = failures + 1;
      }
      if (!haveSsid || r == WIFI_REASON_ASSOC_LEAVE) {
        // nothing to join, or our own disconnect() before loop() reconnects: no second connect here
      } else if (wasUp) {
        esp_wifi_connect();  // a normal drop: reconnect at once
      } else {
        retryAt = millis() + backoffMs(r, failures);
        retryPending = true;  // loop() issues the connect
      }
    }
  } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    gotIp = true;
    failures = 0;
    retryPending = false;
    lastReason = 0;
    upSince = millis();
  }
}

// ---------- captive DNS: every name resolves to the panel ----------
static void dnsTask(void *) {
  int s = socket(AF_INET, SOCK_DGRAM, 0);
  sockaddr_in a = {};
  a.sin_family = AF_INET;
  a.sin_port = htons(53);
  a.sin_addr.s_addr = INADDR_ANY;
  if (s < 0 || bind(s, (sockaddr *)&a, sizeof a) < 0) {
    plog("portal: DNS socket failed\n");
    vTaskDelete(nullptr);
  }
  esp_netif_ip_info_t ip;
  esp_netif_get_ip_info(apNetif, &ip);
  uint8_t buf[300];
  for (;;) {
    sockaddr_in from;
    socklen_t fl = sizeof from;
    int n = recvfrom(s, buf, sizeof buf - 16, 0, (sockaddr *)&from, &fl);
    if (n < 12) continue;
    // Response: same id/question, flags = response + recursion available, one answer pointing at the question.
    buf[2] = 0x81;
    buf[3] = 0x80;
    buf[6] = 0;
    buf[7] = 1;  // ANCOUNT
    buf[8] = buf[9] = buf[10] = buf[11] = 0;
    int q = 12;
    while (q < n && buf[q]) q += buf[q] + 1;
    q += 5;  // root label + type + class
    if (q > n) continue;
    uint8_t *p = buf + q;
    *p++ = 0xC0; *p++ = 0x0C;                // name: pointer to the question
    *p++ = 0; *p++ = 1; *p++ = 0; *p++ = 1;  // A, IN
    *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 30; // TTL
    *p++ = 0; *p++ = 4;
    memcpy(p, &ip.ip.addr, 4);
    p += 4;
    sendto(s, buf, p - buf, 0, (sockaddr *)&from, fl);
  }
}

String portalIp() {
  esp_netif_ip_info_t ip;
  char b[16] = "192.168.4.1";
  if (apNetif && esp_netif_get_ip_info(apNetif, &ip) == ESP_OK) snprintf(b, sizeof b, IPSTR, IP2STR(&ip.ip));
  return b;
}

void begin() {
  apLock = xSemaphoreCreateMutex();
  esp_netif_init();
  esp_event_loop_create_default();  // may already exist (other modules)
  staNetif = esp_netif_create_default_wifi_sta();
  haveSsid = settings.wifiSsid.length() > 0;
  portal = !haveSsid;
  if (portal) apNetif = esp_netif_create_default_wifi_ap();
  esp_netif_set_hostname(staNetif, HOSTNAME);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, onEvent, nullptr));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, onEvent, nullptr));
  esp_wifi_set_storage(WIFI_STORAGE_RAM);  // no flash writes on connect (they glitch the display)

  wifi_config_t wc = {};
  strlcpy((char *)wc.sta.ssid, settings.wifiSsid.c_str(), sizeof wc.sta.ssid);
  strlcpy((char *)wc.sta.password, settings.wifiPass.c_str(), sizeof wc.sta.password);
  // Weak signal: look at every channel and join the strongest access point,
  // not the first one that answers.
  wc.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
  wc.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
  wc.sta.pmf_cfg.capable = true;

  downSince = millis();
  ESP_ERROR_CHECK(esp_wifi_set_mode(portal ? WIFI_MODE_APSTA : WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
  if (portal) {  // nothing to join: be a network instead, so a phone can do the setup
    wifi_config_t ap = {};
    strlcpy((char *)ap.ap.ssid, PORTAL_SSID, sizeof ap.ap.ssid);
    strlcpy((char *)ap.ap.password, PORTAL_PASS, sizeof ap.ap.password);
    ap.ap.ssid_len = strlen(PORTAL_SSID);
    ap.ap.channel = 1;
    ap.ap.max_connection = 4;
    ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
  }
  ESP_ERROR_CHECK(esp_wifi_start());
  // No power save unless Bluetooth runs: modem sleep only hears the access point
  // at its beacons (slower answers, and on a weak signal missed beacons drop the link).
  esp_wifi_set_ps(ble::enabled() ? WIFI_PS_MIN_MODEM : WIFI_PS_NONE);
  esp_wifi_set_max_tx_power(78);                // 19.5 dBm
  esp_wifi_set_inactive_time(WIFI_IF_STA, 10);  // ride out missed beacons a little longer
  lastAttempt = millis();
  if (portal) {
    xTaskCreatePinnedToCore(dnsTask, "dns", 3072, nullptr, 2, nullptr, 0);
    plog("setup portal: join %s / %s, open http://%s/\n", PORTAL_SSID, PORTAL_PASS, portalIp().c_str());
  } else {
    plog("Wi-Fi: joining '%s'\n", settings.wifiSsid.c_str());
  }
}

bool portalActive() { return portal; }

void loop() {
  uint32_t now = millis();
  if (dropNew) {
    dropNew = false;
    plog("Wi-Fi dropped: reason %d, rssi %d, drop #%u\n", dropReason, dropRssi, (unsigned)drops);
  }
  if (portal && apCount < 0 && !scanning && staStarted) scanStart();
  if (scanning && now - scanT0 > SCAN_MAX_MS) scanning = false;  // SCAN_DONE never came
  if (retryPending && (int32_t)(now - retryAt) >= 0) {
    retryPending = false;
    lastAttempt = now;
    esp_wifi_connect();
  }
  if (gotIp) {
    static bool mdnsUp;
    if (!mdnsUp && mdns_init() == ESP_OK) {
      mdns_hostname_set(HOSTNAME);
      mdns_service_add(nullptr, "_http", "_tcp", 80, nullptr, 0);
      mdnsUp = true;
    }
    static uint32_t loggedLost;
    if (lostAt && loggedLost != lostAt) {
      loggedLost = lostAt;
      wifi_ap_record_t ap = {};
      int r = esp_wifi_sta_get_ap_info(&ap) == ESP_OK ? ap.rssi : 0;
      plog("Wi-Fi back after %.1f s, rssi %d, ch %d\n", (now - lostAt) / 1000.0f, r, (int)ap.primary);
    }
    downSince = now;
    if (now - lastSample > 2000) {
      lastSample = now;
      wifi_ap_record_t ap;
      if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        portENTER_CRITICAL(&sampleMux);
        rssi = ap.rssi;
        channel = ap.primary;
        memcpy(bssid, ap.bssid, 6);
        portEXIT_CRITICAL(&sampleMux);
      }
    }
    return;
  }
  portENTER_CRITICAL(&sampleMux);
  rssi = 0;
  portEXIT_CRITICAL(&sampleMux);
  // Auto-reconnect gives up on some reasons (e.g. a failed handshake), so
  // start over from scratch every so often while down.
  if (haveSsid && staStarted && !retryPending && now - downSince > RETRY_MS && now - lastAttempt > RETRY_MS &&
      !scanning) {
    plog("Wi-Fi: still down, starting over\n");
    lastAttempt = now;
    esp_wifi_disconnect();
    esp_wifi_connect();
  }
}

static const char *reasonText(int r, char *buf, size_t len) {
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
      snprintf(buf, len, "disconnected (code %d)", r);
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

bool up() { return gotIp; }

Info info() {
  Info i;
  i.up = gotIp;
  if (i.up) {
    portENTER_CRITICAL(&sampleMux);
    i.rssi = rssi;
    portEXIT_CRITICAL(&sampleMux);
  } else if (portal) {
    snprintf(i.problem, sizeof i.problem, "setup: join Wi-Fi %s (pw %s), open 192.168.4.1", PORTAL_SSID, PORTAL_PASS);
  } else if (!haveSsid) {
    snprintf(i.problem, sizeof i.problem, "not set up");
    i.fixable = true;
  } else {
    int r = lastReason;
    char buf[32];
    snprintf(i.problem, sizeof i.problem, "%s", reasonText(r, buf, sizeof buf));
    i.fixable = fixable(r);
  }
  return i;
}

String report() {
  wifi_ps_type_t ps = WIFI_PS_NONE;
  esp_wifi_get_ps(&ps);
  int rs, ch;
  uint8_t bs[6];
  portENTER_CRITICAL(&sampleMux);
  rs = rssi;
  ch = channel;
  memcpy(bs, bssid, 6);
  portEXIT_CRITICAL(&sampleMux);
  char b[200];
  snprintf(b, sizeof b, "wifi: %s rssi %d ch %d bssid %02x:%02x:%02x:%02x:%02x:%02x, power save %s, drops %u, last reason %d\n",
           gotIp ? "up" : "down", rs, ch, bs[0], bs[1], bs[2], bs[3], bs[4], bs[5],
           ps == WIFI_PS_NONE ? "off" : "modem", (unsigned)drops, dropReason);
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
