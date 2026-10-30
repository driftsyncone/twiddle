// The panel's web server (esp_http_server on its own task): firmware update, status, log,
// screenshot, setup portal. Slow requests never stall the UI; anything touching the UI,
// control:: or shared hardware takes UiLock.
#include "ota.h"

#include <Arduino.h>
#include <string>

#include <lvgl.h>
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "mbedtls/base64.h"

#include "autoblow.h"
#include "ble.h"
#include "board.h"
#include "control.h"
#include "devices.h"
#include "log.h"
#include "rollback.h"
#include "secrets.h"
#include "settings.h"
#include "theme.h"
#include "ui.h"
#include "version.h"
#include "wlan.h"

// ota.sh reads OTA_PASSWORD from secrets.h; the P4 bring-up secrets.h called it OTA_PASS.
#if defined(OTA_PASSWORD)
#define OTA_SECRET OTA_PASSWORD
#elif defined(OTA_PASS)
#define OTA_SECRET OTA_PASS
#else
#define OTA_SECRET ""  // no password configured: every login fails
#endif
#ifndef BOARD_NAME
#define BOARD_NAME "Guition JC1060P470C"
#endif

static const char *TAG = "web";
static httpd_handle_t server;
static std::string expectedAuth;  // "Basic base64(ota:password)"

// ---------- helpers ----------

static bool login(httpd_req_t *req) {
  if (sizeof(OTA_SECRET) > 1) {
    size_t n = httpd_req_get_hdr_value_len(req, "Authorization");
    if (n > 0 && n < 200) {
      char h[200];
      if (httpd_req_get_hdr_value_str(req, "Authorization", h, sizeof h) == ESP_OK && expectedAuth == h) return true;
    }
  }
  httpd_resp_set_status(req, "401 Unauthorized");
  httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"twiddle\"");
  httpd_resp_send(req, "login required\n", HTTPD_RESP_USE_STRLEN);
  return false;
}

static esp_err_t sendText(httpd_req_t *req, const char *status, const std::string &s, const char *type = "text/plain") {
  httpd_resp_set_status(req, status);
  httpd_resp_set_type(req, type);
  return httpd_resp_send(req, s.data(), s.size());
}

static std::string htmlEscape(const std::string &s) {
  std::string o;
  for (char c : s) {
    if (c == '&') o += "&amp;";
    else if (c == '<') o += "&lt;";
    else if (c == '"') o += "&quot;";
    else o += c;
  }
  return o;
}

static std::string urlDecode(const char *s, size_t n) {
  std::string o;
  for (size_t i = 0; i < n; i++) {
    if (s[i] == '+') o += ' ';
    else if (s[i] == '%' && i + 2 < n + 0 && isxdigit((unsigned char)s[i + 1]) && isxdigit((unsigned char)s[i + 2])) {
      char hex[3] = {s[i + 1], s[i + 2], 0};
      o += (char)strtol(hex, nullptr, 16);
      i += 2;
    } else {
      o += s[i];
    }
  }
  return o;
}

// Value of `key` in an application/x-www-form-urlencoded body ("" if absent).
static std::string formArg(const std::string &body, const char *key) {
  size_t kl = strlen(key), pos = 0;
  while (pos < body.size()) {
    size_t amp = body.find('&', pos);
    if (amp == std::string::npos) amp = body.size();
    if (body.compare(pos, kl, key) == 0 && pos + kl < amp && body[pos + kl] == '=')
      return urlDecode(body.data() + pos + kl + 1, amp - pos - kl - 1);
    pos = amp + 1;
  }
  return "";
}

static std::string queryArg(httpd_req_t *req, const char *key) {
  size_t n = httpd_req_get_url_query_len(req);
  if (!n || n > 200) return "";
  char q[208], v[48];
  if (httpd_req_get_url_query_str(req, q, sizeof q) != ESP_OK) return "";
  return httpd_query_key_value(q, key, v, sizeof v) == ESP_OK ? v : "";
}

// ---------- POST /update ----------

static esp_err_t onUpdate(httpd_req_t *req) {
  if (!login(req)) return ESP_OK;
  size_t total = req->content_len;
  if (!total) return sendText(req, "400 Bad Request", "empty upload\n");

  // ota.sh posts multipart (curl -F); a raw body (curl --data-binary) works too.
  size_t trailer = 0;
  bool multipart = false;
  char ct[160] = "";
  if (httpd_req_get_hdr_value_str(req, "Content-Type", ct, sizeof ct) == ESP_OK && strstr(ct, "multipart/form-data")) {
    const char *b = strstr(ct, "boundary=");
    if (b) {
      multipart = true;
      b += 9;
      if (*b == '"') b++;
      size_t bl = strcspn(b, "\";");
      trailer = bl + 8;  // "\r\n--" boundary "--\r\n"
    }
  }

  const esp_partition_t *part = esp_ota_get_next_update_partition(nullptr);
  if (!part) return sendText(req, "500 Internal Server Error", "no update partition\n");
  uint8_t *buf = (uint8_t *)heap_caps_malloc(4097, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!buf) return sendText(req, "500 Internal Server Error", "out of memory\n");

  plog("OTA: receiving %u bytes into %s\n", (unsigned)total, part->label);
  // STOP first, and wait for it to go out before anything else happens: stopAll() only queues the stop for the
  // net / Bluetooth tasks. Same rule as control::stopAndRestart(): at least 800 ms, until nothing reports moving,
  // at most 5 s (a dead network cannot be waited for; the new firmware sends STOP again when it boots).
  // No UiLock while waiting: the LVGL task keeps running, and the web-server task must not render anything.
  {
    UiLock l;
    control::stopAll();
  }
  uint32_t t0 = millis();
  for (;;) {
    bool moving;
    {
      UiLock l;
      moving = devices::anyMoving();
    }
    uint32_t ms = millis() - t0;
    if (ms > 800 && !moving) break;
    if (ms > 5000) {
      plog("OTA: stop not confirmed after 5 s, continuing (the device stops when the panel restarts)\n");
      break;
    }
    delay(50);
  }
  {
    UiLock l;
    uiShowUpdating(-1);  // only marks the screen dirty; the LVGL task draws it
  }
  delay(700);  // let the LVGL task draw the message before the flash write starts

  esp_ota_handle_t h = 0;
  std::string err;
  size_t off = 0, written = 0, payloadStart = 0, payloadEnd = total - (multipart ? trailer : 0);
  bool started = false;
  int lastPct = -1, idle = 0;
  if (esp_ota_begin(part, OTA_SIZE_UNKNOWN, &h) != ESP_OK) err = "esp_ota_begin failed";

  size_t have = 0;  // bytes in buf not yet consumed (only used to find the multipart header)
  while (err.empty() && off < total) {
    int n = httpd_req_recv(req, (char *)buf + have, 4096 - have);
    if (n == HTTPD_SOCK_ERR_TIMEOUT) {
      if (++idle > 6) err = "upload stalled";
      continue;
    }
    if (n <= 0) {
      err = "connection lost during upload";
      break;
    }
    idle = 0;
    size_t len = have + n, skip = 0;
    if (multipart && !started) {
      // Find the end of the part headers; they come first, in the first KB or so.
      buf[len] = 0;
      uint8_t *e = nullptr;
      for (size_t i = 0; i + 3 < len; i++)
        if (!memcmp(buf + i, "\r\n\r\n", 4)) { e = buf + i + 4; break; }
      if (!e) {
        if (len >= 4000) err = "bad multipart header";
        have = len;
        off += n;
        continue;
      }
      skip = e - buf;
      payloadStart = skip;
      off += n;
      have = 0;
    } else {
      off += n;
      have = 0;
    }
    started = true;
    // `off` counts all bytes received; this chunk covers [off - len, off).
    size_t chunkStart = off - len;
    size_t from = chunkStart + skip, to = off < payloadEnd ? off : payloadEnd;
    if (to > from) {
      if (esp_ota_write(h, buf + skip, to - from) != ESP_OK) {
        err = "flash write failed";
        break;
      }
      written += to - from;
    }
    int pct = (int)(off * 100 / total);
    if (pct / 10 != lastPct / 10 && pct < 100) {
      lastPct = pct;
      UiLock l;
      uiShowUpdating(pct);
    }
  }
  free(buf);
  (void)payloadStart;
  if (err.empty()) {
    esp_err_t e = esp_ota_end(h);
    if (e != ESP_OK) err = std::string("image rejected: ") + esp_err_to_name(e);
    else if (esp_ota_set_boot_partition(part) != ESP_OK) err = "could not select the new firmware";
  } else if (h) {
    esp_ota_abort(h);
  }
  if (!err.empty()) {
    plog("OTA: failed: %s\n", err.c_str());
    {
      UiLock l;
      uiHideUpdating();
    }
    return sendText(req, "500 Internal Server Error", err + "\n");
  }
  plog("OTA: %u bytes, ok\n", (unsigned)written);
  {
    UiLock l;
    uiShowUpdating(100);  // "Updating firmware 100%": done, restarting
  }
  sendText(req, "200 OK", "OK, rebooting\n");
  delay(300);
  esp_restart();
  return ESP_OK;
}

// ---------- GET / ----------

static const char *resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "crash";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_DEEPSLEEP: return "deep-sleep wake";
    default: return "other";
  }
}

static esp_err_t onStatus(httpd_req_t *req) {
  // __DATE__/__TIME__ only change when this file recompiles; the ELF hash changes with every
  // build, so ota.sh compares that.
  char sha[17];
  esp_app_get_elf_sha256(sha, sizeof sha);
  autoblow::Status st = autoblow::status();
  char b[900];
  snprintf(b, sizeof b,
           "twiddle " FW_VERSION "\nbuilt " __DATE__ " " __TIME__ "\nid: %s\nboard: " BOARD_NAME
           "\nfirmware: %s\ndisplay fixes: %d\nreset: %s\ndevice: %s %s\nheap: %u free, largest %u\n%s"
           "bluetooth: %s\ncloud: lost %u" "x, silent %u ms, grace %u s, uptime %u s\n",
           sha, rollback::state(), boardDisplayGuard(), resetReason(), st.online ? "online" : "offline", st.error,
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), wlan::report().c_str(),
           ble::enabled() ? "on" : "off", (unsigned)st.lost, (unsigned)st.silentMs, (unsigned)(st.graceMs / 1000),
           (unsigned)(millis() / 1000));
  return sendText(req, "200 OK", b);
}

// ---------- GET /log ----------

static esp_err_t onLog(httpd_req_t *req) {
  if (!login(req)) return ESP_OK;
  httpd_resp_set_type(req, "text/plain");
  logDump([](const char *chunk, int len, void *arg) { httpd_resp_send_chunk((httpd_req_t *)arg, chunk, len); }, req);
  return httpd_resp_send_chunk(req, nullptr, 0);
}

// ---------- GET /screenshot[?tab=0..5] ----------
// The screen as a 24-bit BMP (login: it can switch tabs, and Settings shows Wi-Fi and the
// token's tail: never publish a screenshot of it). Only when board.cpp exposes a frame buffer.

static esp_err_t onScreenshot(httpd_req_t *req) {
  if (!login(req)) return ESP_OK;
  std::string tab = queryArg(req, "tab");
  if (!tab.empty()) {
    UiLock l;
    uiShowTab(atoi(tab.c_str()));
  }
  // The frame buffer we can read may be a frame behind in direct mode with two buffers: mark the whole screen
  // dirty twice and give the LVGL task time to draw both (calling lv_refr_now from here would corrupt the heap).
  vTaskDelay(pdMS_TO_TICKS(tab.empty() ? 50 : 400));
  for (int i = 0; i < 2; i++) {
    { UiLock l; lv_obj_invalidate(lv_screen_active()); }
    vTaskDelay(pdMS_TO_TICKS(300));
  }

  // 24-bit BMP (the display is BGR888, which is BMP's own byte order), rows bottom-up.
  const int W = SCREEN_W, H = SCREEN_H, ROW = W * 3;  // 3072: a multiple of 4, no row padding
  const size_t bytes = (size_t)ROW * H;
  uint8_t *copy = (uint8_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
  if (!copy) return sendText(req, "500 Internal Server Error", "no memory for the screenshot\n");
  {
    UiLock l;  // copy fast under the lock, send slowly without it
    const uint8_t *fb = boardFrameBufferRgb888();
    if (!fb) {
      heap_caps_free(copy);
      return sendText(req, "500 Internal Server Error", "no frame buffer\n");
    }
    if (boardIs565()) {  // 16-bit panel (variant 4): expand RGB565 to the BMP's 3 bytes per pixel
      const uint16_t *s = (const uint16_t *)fb;
      for (size_t i = 0; i < (size_t)W * H; i++) {
        uint16_t p = s[i];
        copy[i * 3] = (p & 0x1F) << 3 | (p & 0x1F) >> 2;
        copy[i * 3 + 1] = ((p >> 5) & 0x3F) << 2 | ((p >> 5) & 0x3F) >> 4;
        copy[i * 3 + 2] = (p >> 11) << 3 | (p >> 11) >> 2;
      }
    } else {
      memcpy(copy, fb, bytes);
    }
  }
  const uint32_t HDR = 14 + 40, size = HDR + bytes;
  uint8_t h[HDR] = {'B', 'M'};
  auto put32 = [&](int at, uint32_t v) { memcpy(h + at, &v, 4); };
  put32(2, size);
  put32(10, HDR);
  put32(14, 40);
  put32(18, W);
  put32(22, H);  // positive height: rows bottom-up
  h[26] = 1;     // planes
  h[28] = 24;    // bits per pixel
  put32(34, bytes);
  httpd_resp_set_type(req, "image/bmp");
  httpd_resp_send_chunk(req, (const char *)h, HDR);
  for (int y = H - 1; y >= 0; y--)
    if (httpd_resp_send_chunk(req, (const char *)copy + (size_t)y * ROW, ROW) != ESP_OK) break;
  heap_caps_free(copy);
  return httpd_resp_send_chunk(req, nullptr, 0);
}

// GET /pclk -> display pixel clock; ?mhz=N (20..60, 0 = default) saves it, then restart (POST /restart). For a screen that shakes.
static esp_err_t onPclk(httpd_req_t *req) {
  if (!login(req)) return ESP_OK;
  std::string v = queryArg(req, "mhz");
  if (!v.empty()) {
    int n = atoi(v.c_str());
    if (n != 0 && (n < 20 || n > 60)) return sendText(req, "400 Bad Request", "mhz must be 0 (default) or 20..60\n");
    UiLock l;
    settings.pclkMhz = n;
    settingsSavePanel();
  }
  return sendText(req, "200 OK", "pclk " + std::to_string(settings.pclkMhz) + " MHz, 0 = default (applies after a restart)\n");
}

// GET /flip             -> flip state; GET /flip?n=0|1 -> flip live (not saved), for testing
static esp_err_t onFlip(httpd_req_t *req) {
  if (!login(req)) return ESP_OK;
  std::string n = queryArg(req, "n");
  if (!n.empty()) {
    UiLock l;
    boardSetFlip(n == "1");
  }
  return sendText(req, "200 OK", boardIsFlipped() ? "flipped\n" : "normal\n");
}

// GET /theme            -> current theme and the list
// GET /theme?n=N        -> switch live (not saved), GET /theme?n=N&save=1 -> switch and keep it after a reboot
static esp_err_t onTheme(httpd_req_t *req) {
  if (!login(req)) return ESP_OK;
  std::string n = queryArg(req, "n");
  if (!n.empty()) {
    int idx = atoi(n.c_str());
    if (idx < 0 || idx >= theme::count()) return sendText(req, "400 Bad Request", "no such theme\n");
    UiLock l;
    uiSetTheme(idx);
    if (queryArg(req, "save") == "1") {
      settings.theme = idx;
      settingsSaveTheme();
    }
  }
  std::string out = "current " + std::to_string(theme::current()) + "\n";
  for (int i = 0; i < theme::count(); i++) out += std::to_string(i) + " " + theme::name(i) + "\n";
  return sendText(req, "200 OK", out);
}

// GET /panel -> display panel variant; ?v=0 (V2, default) or ?v=1 (original) saves it, then restart (POST /restart).
// For a unit whose screen stays black although the UI is drawn (GET /screenshot shows it): try the other variant.
static esp_err_t onPanel(httpd_req_t *req) {
  if (!login(req)) return ESP_OK;
  std::string v = queryArg(req, "v");
  if (!v.empty()) {
    int n = atoi(v.c_str());
    if (n < 0 || n > 4) return sendText(req, "400 Bad Request", "v must be 0..3 (bit0 original table, bit1 swapped reset pin)\n");
    UiLock l;
    settings.panelVariant = n;
    settingsSavePanel();
  }
  return sendText(req, "200 OK", std::string("panel ") + std::to_string(settings.panelVariant) + " (applies after a restart)\n");
}

// ---------- small POST/GET controls ----------

static esp_err_t onRestart(httpd_req_t *req) {
  if (!login(req)) return ESP_OK;
  sendText(req, "200 OK", "OK, restarting\n");
  UiLock l;
  control::stopAndRestart();  // after the reply has gone out
  return ESP_OK;
}

static esp_err_t onResync(httpd_req_t *req) {
  if (!login(req)) return ESP_OK;
  {
    UiLock l;
    boardDisplayResync();
  }
  return sendText(req, "200 OK", "OK, display resynced\n");
}

static esp_err_t onScan(httpd_req_t *req) {
  if (!login(req)) return ESP_OK;
  {
    UiLock l;
    devices::scan();
  }
  return sendText(req, "200 OK", "OK, scanning for 8 s; see /log\n");
}

// GET /backlight?duty=0..255 (login): raw PWM, to find the driver's usable band.
static esp_err_t onBacklight(httpd_req_t *req) {
  if (!login(req)) return ESP_OK;
  std::string d = queryArg(req, "duty");
  int duty = d.empty() ? -1 : atoi(d.c_str());
  {
    UiLock l;
    boardBacklightRaw(constrain(duty, -1, 255));
  }
  return sendText(req, "200 OK", "backlight duty " + std::to_string(duty) + "\n");
}

// ---------- setup portal: GET/POST /setup ----------
// Reachable by anyone on the portal network while the panel isn't configured;
// on the LAN it needs the OTA login (it changes Wi-Fi and the token).

static esp_err_t onSetupGet(httpd_req_t *req) {
  if (!wlan::portalActive() && !login(req)) return ESP_OK;
  int n = wlan::scanCount();
  wlan::scanStart();  // runs in the background; refresh to see it
  std::string options;
  for (int i = 0; i < n; i++) {
    std::string ssid = wlan::scanSsid(i).c_str();
    if (ssid.empty()) continue;
    std::string esc = htmlEscape(ssid);
    if (options.find("value=\"" + esc + "\"") != std::string::npos) continue;
    options += "<option value=\"" + esc + "\"" + (ssid == settings.wifiSsid.c_str() ? " selected" : "") + ">" + esc +
               " (" + std::to_string(wlan::scanRssi(i)) + " dBm)</option>";
  }
  std::string page =
      "<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
      "<title>Twiddle setup</title>"
      "<style>body{font:17px system-ui;margin:24px auto;max-width:420px;padding:0 16px;background:#1b1f1d;color:#eee}"
      "label{display:block;margin:14px 0 4px}input,select,button{width:100%;font:inherit;padding:10px;"
      "border-radius:8px;border:1px solid #555;background:#2a302c;color:#eee;box-sizing:border-box}"
      "button{background:#2e7d32;border:0;margin-top:20px}small{color:#9aa}</style>"
      "<h2>Twiddle setup</h2><form method=post action=/setup>"
      "<label>Wi-Fi network</label><select name=pick>" + options +
      "<option value=''>Other / hidden (type below)</option></select>"
      "<input name=ssid placeholder='Network name, if not in the list' value=''>"
      "<label>Wi-Fi password</label><input name=pass type=password value=\"" + htmlEscape(settings.wifiPass.c_str()) + "\">"
      "<label>Autoblow device token</label><input name=token value=\"" + htmlEscape(settings.token.c_str()) + "\">"
      "<small>From the Autoblow web app's device settings.</small>"
      "<button>Save and restart</button></form>"
      "<p><small>" FW_VERSION ", " BOARD_NAME ". Networks: " + std::to_string(n > 0 ? n : 0) + " found, "
      "<a href=/setup style='color:#8fd'>refresh</a>.</small></p>";
  return sendText(req, "200 OK", page, "text/html");
}

static void applySetup(void *) {
  settingsSave();
  control::stopAndRestart();
}

static esp_err_t onSetupPost(httpd_req_t *req) {
  if (!wlan::portalActive() && !login(req)) return ESP_OK;
  if (req->content_len == 0 || req->content_len > 1500) return sendText(req, "400 Bad Request", "bad form\n");
  std::string body(req->content_len, '\0');
  size_t got = 0;
  while (got < body.size()) {
    int n = httpd_req_recv(req, &body[got], body.size() - got);
    if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;
    if (n <= 0) return ESP_FAIL;
    got += n;
  }
  String ssid = formArg(body, "ssid").c_str();
  ssid.trim();
  if (!ssid.length()) ssid = formArg(body, "pick").c_str();
  if (!ssid.length()) return sendText(req, "400 Bad Request", "Pick or type a Wi-Fi network.\n");
  settings.wifiSsid = ssid;
  settings.wifiPass = formArg(body, "pass").c_str();
  settings.token = formArg(body, "token").c_str();
  settings.token.trim();
  sendText(req, "200 OK",
           "<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
           "<body style='font:17px system-ui;margin:24px;background:#1b1f1d;color:#eee'>"
           "<h2>Saved</h2><p>The panel restarts and joins <b>" + htmlEscape(ssid) + "</b>. "
           "Its status bar shows how that goes.</p>", "text/html");
  delay(200);
  uiCall(applySetup, nullptr);
  return ESP_OK;
}

// Captive portal: whatever the phone asks for, send it to /setup.
static esp_err_t onNotFound(httpd_req_t *req, httpd_err_code_t) {
  if (wlan::portalActive()) {
    std::string to = "http://" + std::string(wlan::portalIp().c_str()) + "/setup";
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", to.c_str());
    return httpd_resp_send(req, "", 0);
  }
  httpd_resp_set_status(req, "404 Not Found");
  return httpd_resp_send(req, "not found\n", HTTPD_RESP_USE_STRLEN);
}

void otaBegin() {
  std::string cred = std::string("ota:") + OTA_SECRET;
  unsigned char enc[128];
  size_t el = 0;
  mbedtls_base64_encode(enc, sizeof enc, &el, (const unsigned char *)cred.data(), cred.size());
  expectedAuth = "Basic " + std::string((char *)enc, el);

  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.stack_size = 10240;
  cfg.core_id = 0;  // next to Wi-Fi and the Autoblow task; the UI has core 1 to itself
  cfg.max_uri_handlers = 20;
  cfg.lru_purge_enable = true;
  cfg.recv_wait_timeout = 10;
  cfg.send_wait_timeout = 10;
  if (httpd_start(&server, &cfg) != ESP_OK) {
    plog("web: httpd_start failed\n");
    return;
  }
  struct R { const char *uri; httpd_method_t m; esp_err_t (*h)(httpd_req_t *); };
  static const R routes[] = {
      {"/", HTTP_GET, onStatus},           {"/update", HTTP_POST, onUpdate},
      {"/log", HTTP_GET, onLog},           {"/screenshot", HTTP_GET, onScreenshot},
      {"/restart", HTTP_POST, onRestart},  {"/resync", HTTP_POST, onResync},
      {"/scan", HTTP_POST, onScan},        {"/backlight", HTTP_GET, onBacklight},
      {"/theme", HTTP_GET, onTheme},
      {"/panel", HTTP_GET, onPanel}, {"/pclk", HTTP_GET, onPclk},
      {"/flip", HTTP_GET, onFlip},
      {"/setup", HTTP_GET, onSetupGet},    {"/setup", HTTP_POST, onSetupPost},
  };
  for (const R &r : routes) {
    httpd_uri_t u = {};
    u.uri = r.uri;
    u.method = r.m;
    u.handler = r.h;
    httpd_register_uri_handler(server, &u);
  }
  httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, onNotFound);
  ESP_LOGI(TAG, "web server up on port 80");
}
