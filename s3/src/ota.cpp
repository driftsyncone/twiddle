#include "ota.h"

#include <Arduino.h>
#include <Update.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_app_desc.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include "autoblow.h"
#include "battery.h"
#include "ble.h"
#include "board.h"
#include "control.h"
#include "devices.h"
#include "encoder.h"
#include "log.h"
#include "loginlimit.h"
#include "otapw.h"
#include "p4bus.h"
#include "pclkfb.h"
#include "rollback.h"
#include "build_config.h"
#include "settings.h"
#include "setupform.h"
#include "theme.h"
#include "ui.h"
#include "version.h"
#include "wlan.h"

static WebServer server(80);
static DNSServer dns;    // setup portal: every name resolves to the panel
static bool authorized;  // for the upload in progress; cleared when it ends
static loginlimit::State loginState;  // 5 wrong passwords in a row lock the web login for 60 s

// Web admin login (user "ota", password from otapw.h). 403 until the panel has a password at all, 401 for a wrong one.
// An empty password never matches: known() is checked first.
// Counts wrong passwords (5 in a row lock it for 60 s). Every route goes through here, the upload start included.
static bool authOk() {
  if (!otapw::known()) return false;
  uint32_t now = millis();
  if (loginlimit::blocked(loginState, now)) return false;  // no comparison at all, not even for the right password
  if (server.authenticate("ota", otapw::get())) {
    loginlimit::success(loginState);
    return true;
  }
  if (server.hasHeader("Authorization")) loginlimit::failure(loginState, now);  // credentials were sent and are wrong (none = a browser's first visit)
  return false;
}

static bool login() {
  if (!otapw::known()) {
    server.send(403, "text/plain", "The web admin is locked: this panel has no web password yet. Restart it; the password is shown on its Settings tab.\n");
    return false;
  }
  if (loginlimit::blocked(loginState, millis())) {
    server.send(429, "text/plain", "Too many wrong passwords: wait " + String(loginlimit::remainingSec(loginState, millis())) + " s and try again.\n");
    return false;
  }
  if (authOk()) return true;
  server.requestAuthentication(BASIC_AUTH, "twiddle");  // realm (was the library default)
  return false;
}

static void onUpload() {
  HTTPUpload &u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    authorized = authOk();
    if (!authorized) return;
    Serial.printf("OTA: receiving %s\n", u.filename.c_str());
    {
      UiLock l;
      control::stopAll();
      uiShowUpdating(-1);
    }
    // Flash writes stall the display's PSRAM reads and scramble any picture,
    // so show the message, then keep the backlight off while writing.
    delay(1500);
    {
      UiLock l;
      boardBacklight(false);
    }
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Serial.printf("OTA: %s\n", Update.errorString());
  } else if (!authorized) {
    return;
  } else if (u.status == UPLOAD_FILE_WRITE) {
    Update.write(u.buf, u.currentSize);
  } else if (u.status == UPLOAD_FILE_END) {
    Update.end(true);
    Serial.printf("OTA: %u bytes, %s\n", u.totalSize, Update.hasError() ? Update.errorString() : "ok");
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    UiLock l;
    uiHideUpdating();
    boardBacklight(true);
  }
}

static void onDone() {
  bool ok = authorized;
  authorized = false;
  if (!ok) {  // refused when the upload began: say why (403 locked / 401 wrong password) instead of hanging up
    if (login()) server.send(400, "text/plain", "no firmware received\n");
    return;
  }
  if (Update.hasError()) {
    {
      UiLock l;
      uiHideUpdating();
      boardBacklight(true);
    }
    server.send(500, "text/plain", String(Update.errorString()) + "\n");
    return;
  }
  {
    UiLock l;
    uiShowUpdating(100);  // "Updating firmware 100%": done, restarting
    boardBacklight(true);
  }
  server.send(200, "text/plain", "OK, rebooting\n");
  delay(300);
  ESP.restart();
}

// GET /screenshot[?tab=0..5]: the screen as a 16-bit BMP (login: it can switch
// tabs, and Settings, tab 5 since Devices came in at 4, shows Wi-Fi, the
// token's tail and the web password: never publish a screenshot of it). The frame buffer is
// read without the lock: the UI keeps running, at the cost of a torn frame if
// something redraws meanwhile.
static void onScreenshot() {
  if (!login()) return;
  if (server.hasArg("tab")) {
    UiLock l;
    uiShowTab(server.arg("tab").toInt());
  }
  const uint16_t *fb = boardFrameBuffer();
  if (!fb) return server.send(500, "text/plain", "no frame buffer\n");

  const int W = 800, H = 480, ROW = W * 2;  // 1600 bytes, already 4-byte aligned
  const uint32_t HDR = 14 + 40 + 12, size = HDR + ROW * H;
  uint8_t h[HDR] = {'B', 'M'};
  auto put32 = [&](int at, uint32_t v) { memcpy(h + at, &v, 4); };
  put32(2, size);
  put32(10, HDR);
  put32(14, 40);
  put32(18, W);
  put32(22, H);  // positive height: rows bottom-up
  h[26] = 1;     // planes
  h[28] = 16;    // bits per pixel
  put32(30, 3);  // BI_BITFIELDS: RGB565 masks follow
  put32(34, ROW * H);
  put32(54, 0xF800);
  put32(58, 0x07E0);
  put32(62, 0x001F);

  server.setContentLength(size);
  server.send(200, "image/bmp", "");
  server.sendContent((const char *)h, HDR);
  if (!boardIsFlipped()) {
    for (int y = H - 1; y >= 0; y--) server.sendContent((const char *)(fb + y * W), ROW);
    return;
  }
  // Flipped: the frame buffer is upside down, so send it turned back.
  static uint16_t line[W];
  for (int y = 0; y < H; y++) {
    const uint16_t *src = fb + y * W;
    for (int x = 0; x < W; x++) line[x] = src[W - 1 - x];
    server.sendContent((const char *)line, ROW);
  }
}

// GET /i2c: which chips answer on the touch bus and on the P4 connector, plus the
// IP5306's status registers if it is the I2C variant (address 0x75). Diagnostic only.
static String scanBus(TwoWire &w) {
  String s;
  for (uint8_t a = 1; a < 127; a++) {
    w.beginTransmission(a);
    if (w.endTransmission() == 0) s += " 0x" + String(a, HEX);
  }
  return s.length() ? s : String(" (none)");
}

#if BOARD_HAS_P4_I2C
static int readReg(TwoWire &w, uint8_t addr, uint8_t reg) {
  w.beginTransmission(addr);
  w.write(reg);
  if (w.endTransmission(false) != 0 || w.requestFrom(addr, (uint8_t)1) != 1) return -1;
  return w.read();
}
#endif

static void i2cReport(void *arg) {
  String &out = *(String *)arg;
  out = String("board: " BOARD_NAME "\nmain I2C bus:") + scanBus(Wire) + "\n";
#if BOARD_HAS_P4_I2C  // Guition only: on the Waveshare GPIO17/18 drive the display
  for (uint8_t reg : {0x00, 0x01, 0x70, 0x71, 0x78}) {
    int v = readReg(Wire, 0x75, reg);
    out += "IP5306 reg 0x" + String(reg, HEX) + " = " + (v < 0 ? String("no answer") : "0x" + String(v, HEX)) + "\n";
  }
  if (encoder::ownsP4()) {  // the encoder (and a gauge at another address) hold the bus: scan it as it is
    p4bus::begin();
    out += "P4 pins (SDA 17, SCL 18), encoder " + String(encoder::present() ? "found at 0x" + String(encoder::address(), HEX) : "not answering") + ":" + scanBus(Wire1) + "\n";
  } else if (battery::usesP4()) {
    out += "P4 pins (SDA 17, SCL 18): in use by the fuel gauge\n";
  } else {
    p4bus::begin();
    out += "P4 pins (SDA 17, SCL 18):" + scanBus(Wire1) + "\n";
    p4bus::end();
  }
#endif
}

static void onI2c() {
  if (!login()) return;
  String out;
  uiCall(i2cReport, &out);  // on the loop task: the I2C driver lives there
  server.send(200, "text/plain", out);
}

// POST /restart: reboot from the network, e.g. when the screen is unusable.
// (No remote power off: cutting power from software can lock the IP5306 out
// until the battery is reconnected.)
static void onRestart() {
  if (!login()) return;
  server.send(200, "text/plain", "OK, restarting\n");
  UiLock l;
  control::stopAndRestart();  // after the reply has gone out
}

// GET /adc: raw readings of the battery ADC pin (for checking a divider / calibration).
static void adcReport(void *arg) {
  String &out = *(String *)arg;
#if BOARD_BATTERY_ADC_PIN >= 0
  out = "pin " + String(BOARD_BATTERY_ADC_PIN) + " mV:";
  for (int i = 0; i < 9; i++) {
    out += " " + String(analogReadMilliVolts(BOARD_BATTERY_ADC_PIN));
    delay(2);
  }
  out += "\nraw:";
  for (int i = 0; i < 5; i++) out += " " + String(analogRead(BOARD_BATTERY_ADC_PIN));
  out += "\n";
#else
  out = "no battery ADC on this board\n";
#endif
}

static void onAdc() {
  if (!login()) return;
  if (battery::usesP4()) return server.send(409, "text/plain", "GPIO17 is in use by the fuel gauge on P4\n");
  String out;
  uiCall(adcReport, &out);  // on the loop task, like battery::loop()
  server.send(200, "text/plain", out);
}

// ---------- setup portal: GET/POST /setup ----------
// Reachable by anyone on the portal network while the panel isn't configured;
// on the LAN it needs the OTA login (it changes Wi-Fi and the token).

static String htmlEscape(const String &s) {
  String o;
  for (char c : s) {
    if (c == '&') o += "&amp;";
    else if (c == '<') o += "&lt;";
    else if (c == '"') o += "&quot;";
    else o += c;
  }
  return o;
}

// The web login note. The password itself is NEVER on a web page: the Twiddle-Setup AP password is public (it is in the
// docs), so anyone in range could read it. It is shown only on the panel's own Settings tab ("Web login").
// Only while the setup portal is up; on the LAN the page already needs the login.
static String webLoginBox() {
  if (!wlan::portalActive()) return "";
  if (!otapw::known())
    return "<div style='border:1px solid #555;border-radius:8px;padding:10px 14px;margin:14px 0'><b>Web login</b><br>"
           "The panel is still making its password; reload in a few seconds. If this stays, the web admin stays locked: "
           "restart the panel.</div>";
  return "<div style='border:1px solid #555;border-radius:8px;padding:10px 14px;margin:14px 0'><b>Web login</b> for updates and the web admin<br>"
         "Read the password <b>on the panel</b>: Settings tab, <b>Web login</b> (user <code>ota</code>). "
         "It is not shown on this page on purpose. Write it down; it is needed for <code>./ota.sh</code> and for opening "
         "this page once the panel is on your network. Tapping it on the panel resets it.</div>";
}

// Guition only: the display pixel clock. A unit with the wrong one shows a colour pattern or shakes.
static String displayField() {
#if defined(BOARD_WAVESHARE_LCD5)
  return "";
#else
  String o = "<label>Display pixel clock</label><select name=pclk>";
  // 12..18, plus a saved value outside that (set over /pclk): otherwise the browser would preselect 12 and a save would change it
  for (int mhz = min(12, settings.pclkMHz); mhz <= max(18, settings.pclkMHz); mhz++)
    o += "<option" + String(mhz == settings.pclkMHz ? " selected" : "") + ">" + String(mhz) + "</option>";
  o += "</select><small>Display test: if the screen shows a colour pattern (white, red, green, blue, black in turn) or "
       "shakes, pick another value (most units 16, a few 14), Save, and look at the screen after the restart. "
       "If the picture is right, <b>tap Keep on the panel</b>; if not, switch the panel off and on three times and it returns to "
       "the previous value by itself. Leave it alone when the screen is fine.</small>";
  if (settings.pclkTrial)
    o += "<p><small>Now on trial: " + String(settings.pclkMHz) + " MHz (boot " + String(settings.pclkBoots) + " of " +
         String(pclkfb::TRIAL_BOOTS) + "). Without a tap on <b>Keep</b> on the panel it goes back to " +
         String(settings.pclkPrev) + " MHz.</small></p>";
  return o;
#endif
}

static void onSetupGet() {
  if (!wlan::portalActive() && !login()) return;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) WiFi.scanNetworks(true);  // runs in the background; refresh to see it
  String options;
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length() || options.indexOf("value=\"" + htmlEscape(ssid) + "\"") >= 0) continue;
    options += "<option value=\"" + htmlEscape(ssid) + "\"" + (ssid == settings.wifiSsid ? " selected" : "") + ">" +
               htmlEscape(ssid) + " (" + WiFi.RSSI(i) + " dBm)</option>";
  }
  String keepPass = settings.wifiPass.length() ? " placeholder='(saved, leave empty to keep)'" : "";
  String keepToken = settings.token.length() ? " placeholder='(saved, leave empty to keep)'" : "";
  String page =
      "<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
      "<title>Twiddle setup</title>"
      "<style>body{font:17px system-ui;margin:24px auto;max-width:420px;padding:0 16px;background:#1b1f1d;color:#eee}"
      "label{display:block;margin:14px 0 4px}input,select,button{width:100%;font:inherit;padding:10px;"
      "border-radius:8px;border:1px solid #555;background:#2a302c;color:#eee;box-sizing:border-box}"
      "button{background:#2e7d32;border:0;margin-top:20px}small{color:#9aa}</style>"
      "<h2>Twiddle setup</h2>" + webLoginBox() + "<form method=post action=/setup autocomplete=off>"
      "<label>Wi-Fi network</label><select name=pick>" + options +
      "<option value=''>Other / hidden (type below)</option></select>"
      "<input name=ssid autocomplete=off placeholder='Network name, if not in the list' value=''>"
      "<label>Wi-Fi password</label><input name=pass type=password autocomplete=off value=''" + keepPass + ">"
      "<label>Autoblow device token</label><input name=token autocomplete=off value=''" + keepToken + ">"
      "<small>From the Autoblow web app's device settings. Stored values are never shown; leave a field empty to keep it.</small>"
      + displayField() +
      "<button>Save and restart</button></form>"
      "<p><small>" FW_VERSION ", " BOARD_NAME ". Networks: " + String(n > 0 ? n : 0) + " found, "
      "<a href=/setup style='color:#8fd'>refresh</a>.</small></p>";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/html", page);
}

static int pclkNew;  // a clock chosen on the setup page that differs from the current one, else 0

static void applySetup(void *) {
  settingsSave();
  if (pclkNew) settingsSelectPclk(pclkNew);  // starts a trial (pclkfb.h)
  control::stopAndRestart();
}

static void onSetupPost() {
  if (!wlan::portalActive() && !login()) return;
  String ssid = server.arg("ssid");
  ssid.trim();
  if (!ssid.length()) ssid = server.arg("pick");
  if (!ssid.length()) return server.send(400, "text/plain", "Pick or type a Wi-Fi network.\n");
  // Stored secrets are never echoed into the form, so an empty field means "keep" (setupform.h).
  bool sameNet = ssid == settings.wifiSsid;
  settings.wifiSsid = ssid;
  settings.wifiPass = setupform::wifiPass(settings.wifiPass.c_str(), server.arg("pass").c_str(), sameNet).c_str();
  settings.token = setupform::token(settings.token.c_str(), server.arg("token").c_str()).c_str();
#if !defined(BOARD_WAVESHARE_LCD5)
  int mhz = server.hasArg("pclk") ? server.arg("pclk").toInt() : 0;
  pclkNew = pclkfb::validMHz(mhz) && mhz != settings.pclkMHz ? mhz : 0;
#endif
  server.send(200, "text/html",
              "<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
              "<body style='font:17px system-ui;margin:24px;background:#1b1f1d;color:#eee'>"
              "<h2>Saved</h2><p>The panel restarts and joins <b>" + htmlEscape(ssid) + "</b>. "
              "Its status bar shows how that goes.</p>" + webLoginBox() +
              "<p>If the screen is blank or garbled, join <b>Twiddle-Setup</b> again and check <i>Display pixel clock</i> on the setup page.</p>");
  delay(200);
  uiCall(applySetup, nullptr);
}

static void onNotFound() {
  if (wlan::portalActive()) {  // captive portal: whatever the phone asks for, send it to /setup
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/setup");
    return server.send(302, "text/plain", "");
  }
  server.send(404, "text/plain", "not found\n");
}

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

void otaBegin() {
  server.on("/adc", HTTP_GET, onAdc);
  // GET /backlight?duty=0..255 (login): raw PWM, to find the driver's usable band.
  server.on("/backlight", HTTP_GET, [] {
    if (!login()) return;
    int d = server.hasArg("duty") ? server.arg("duty").toInt() : -1;
    UiLock l;
    boardBacklightRaw(constrain(d, -1, 255));
    server.send(200, "text/plain", String("backlight duty ") + d + "\n");
  });
  // GET /pclk -> current display pixel clock; ?mhz=N (10..20) saves it as a trial, then restart (POST /restart) to apply.
  // A screen showing a colour pattern (white, red, green, blue, black in turn) or shaking: try 14 / 15 / 16.
  server.on("/pclk", HTTP_GET, [] {
    if (!login()) return;
    if (server.hasArg("mhz")) {
      int mhz = server.arg("mhz").toInt();
      if (!pclkfb::validMHz(mhz)) return server.send(400, "text/plain", "mhz must be 10..20\n");
      UiLock l;
      settingsSelectPclk(mhz);  // a different clock starts a trial: tap Keep on the panel after the restart
    }
    String out = String("pclk ") + settings.pclkMHz + " MHz (applies after a restart)";
    if (settings.pclkTrial)
      out += String(", on trial (boot ") + settings.pclkBoots + " of " + pclkfb::TRIAL_BOOTS + "; tap Keep on the panel, else back to " +
             settings.pclkPrev + " MHz)";
    server.send(200, "text/plain", out + "\n");
  });
  // GET /theme -> current theme and the list; ?n=N switches live (not saved), ?n=N&save=1 keeps it.
  server.on("/theme", HTTP_GET, [] {
    if (!login()) return;
    int shown = theme::current();
    if (server.hasArg("n")) {
      int idx = server.arg("n").toInt();
      if (idx < 0 || idx >= theme::count()) return server.send(400, "text/plain", "no such theme\n");
      UiLock l;
      shown = idx;
      uiSetTheme(idx);  // runs on the loop task at its next LVGL tick
      if (server.arg("save") == "1") {
        settings.theme = idx;
        settingsSaveTheme();
      }
    }
    String out = "current " + String(shown) + "\n";
    for (int i = 0; i < theme::count(); i++) out += String(i) + " " + theme::name(i) + "\n";
    server.send(200, "text/plain", out);
  });
  server.on("/setup", HTTP_GET, onSetupGet);
  server.on("/setup", HTTP_POST, onSetupPost);
  server.onNotFound(onNotFound);
  if (wlan::portalActive()) dns.start(53, "*", WiFi.softAPIP());
  server.on("/restart", HTTP_POST, onRestart);
  // POST /resync: restart the display output without rebooting.
  server.on("/resync", HTTP_POST, [] {
    if (!login()) return;
    {
      UiLock l;
      boardDisplayResync();
    }
    server.send(200, "text/plain", "OK, display resynced\n");
  });
  server.on("/i2c", HTTP_GET, onI2c);
  // The log (Bluetooth, cloud, memory), for panels that aren't on USB.
  server.on("/log", HTTP_GET, [] {
    if (!login()) return;
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/plain", "");
    logDump([](const char *chunk, int len, void *) { server.sendContent(chunk, len); }, nullptr);
    server.sendContent("");
  });
  // Same as the Devices tab's Scan button.
  server.on("/scan", HTTP_POST, [] {
    if (!login()) return;
    {
      UiLock l;
      devices::scan();
    }
    server.send(200, "text/plain", "OK, scanning for 8 s; see /log\n");
  });
  server.on("/update", HTTP_POST, onDone, onUpload);
  server.on("/screenshot", HTTP_GET, onScreenshot);
  // Build time + probation state, so ota.sh can tell whether an update stuck.
  server.on("/", HTTP_GET, [] {
    if (wlan::portalActive()) {  // on the setup access point the root is the setup form
      server.sendHeader("Location", "/setup");
      return server.send(302, "text/plain", "");
    }
    // __DATE__/__TIME__ only change when this file recompiles; the ELF hash
    // changes with every build, so ota.sh compares that.
    char sha[17];
    esp_app_get_elf_sha256(sha, sizeof sha);
    server.send(200, "text/plain",
                String("twiddle " FW_VERSION "\nbuilt " __DATE__ " " __TIME__ "\nid: ") + sha +
                    "\nboard: " BOARD_NAME "\nfirmware: " + rollback::state() + "\ndisplay fixes: " +
                    boardDisplayGuard() + "\nreset: " + resetReason() + "\ndevice: " + (autoblow::status().online ? "online" : "offline") + " " + autoblow::status().error +
                    "\nheap: " +
                    heap_caps_get_free_size(MALLOC_CAP_INTERNAL) + " free, largest " +
                    heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) + "\n" + wlan::report() +
                    "bluetooth: " + (ble::enabled() ? "on" : "off") + "\ncloud: lost " +
                    autoblow::status().lost + "x, silent " + autoblow::status().silentMs + " ms, grace " +
                    autoblow::status().graceMs / 1000 + " s, uptime " + millis() / 1000 + " s\n");
  });
  server.begin();
  // Core 0, next to Wi-Fi and the Autoblow task; the UI has core 1 to itself.
  xTaskCreatePinnedToCore([](void *) {
    for (;;) {
      if (wlan::portalActive()) dns.processNextRequest();
      server.handleClient();
      vTaskDelay(pdMS_TO_TICKS(2));
    }
  }, "web", 8192, nullptr, 1, nullptr, 0);
}
