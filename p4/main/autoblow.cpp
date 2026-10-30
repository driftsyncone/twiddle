// Autoblow cloud client: discovery, state polling and commands on its own network task (see autoblow.h).
#include "autoblow.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <math.h>

#include <string>

#include "log.h"
#include "secrets.h"
#include "settings.h"
#include "wlan.h"

// AUTOBLOW_API_BASE (secrets.h, optional) points the client at another server, e.g. the Autoblow
// emulator ("http://your-host.example.com:8792"). Then plain http is allowed and the `cluster`
// the server reports is ignored (the emulator reports itself as 127.0.0.1). Unset: the real cloud.
#ifdef AUTOBLOW_API_BASE
#define DISCOVER_URL AUTOBLOW_API_BASE "/autoblow/connected"
#else
#define DISCOVER_URL "https://latency.autoblowapi.com/autoblow/connected"
#endif

namespace autoblow {

static SemaphoreHandle_t mtx;
static TaskHandle_t task;
static Desired want;
static Status stat;
static bool resyncReq;
static uint32_t lostAcked;
static uint32_t pulseDone;  // last Desired::pulseSeq acted on

// Weak Wi-Fi drops out for a few seconds now and then. Only count the device
// as lost (which stops the session) once nothing has got through for
// settings.graceS (10, 20 or 30 s, Settings tab). The device keeps doing
// whatever it was told meanwhile, as it would anyway; commands that didn't
// land are retried, and a STOP pressed during the gap goes out first as soon
// as the link is back.
static uint32_t graceMs() { return settings.graceS * 1000u; }
static volatile uint32_t lastGood;  // millis() of the last HTTP response of any kind

static char token[80];  // own copy: settings.token is a String the UI may reassign

// What the device was last told. -1 = unknown.
struct Sent {
  int want = -1;  // Want, or -1
  int speed = -1;
  int script = -1;
  int scriptPlaying = -1;
  int valvePlus = -1, valveMinus = -1;
};

static esp_http_client_handle_t http;
static std::string body;
static std::string host;  // cluster base URL, empty until discovered
static Type type;         // what the token belongs to, from discover()

// API route prefix for the device.
static const char *route() {
  switch (type) {
    case Type::VacuPump: return "/vacupump";
    case Type::Ultra: return "/autoblow";
    default: return "/vacuglide";
  }
}

const char *typeName(Type t) {
  switch (t) {
    case Type::VacuGlide: return "VacuGlide";
    case Type::VacuPump: return "VacuPump";
    case Type::Ultra: return "Autoblow Ultra";
    default: return "Autoblow";
  }
}

struct Lock {
  Lock() { xSemaphoreTake(mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(mtx); }
};

Desired desired() {
  Lock l;
  return want;
}

void setDesired(const Desired &d) {
  {
    Lock l;
    want = d;
  }
  if (task) xTaskNotifyGive(task);
}

void resync() {
  {
    Lock l;
    resyncReq = true;
  }
  if (task) xTaskNotifyGive(task);
}

void ackLost() {
  {
    Lock l;
    lostAcked = stat.lost;
  }
  if (task) xTaskNotifyGive(task);
}

Status status() {
  Lock l;
  Status s = stat;
  uint32_t quiet = millis() - lastGood;
  s.silentMs = s.online && quiet > 5000 ? quiet : 0;  // the 3 s poll plus a slow answer is normal
  s.graceMs = graceMs();
  return s;
}

static void setError(const char *fmt, ...) {
  Lock l;
  char msg[sizeof stat.error];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);
  if (strcmp(msg, stat.error)) plog("autoblow: %s\n", msg);
  strcpy(stat.error, msg);
}

// ---------- HTTP ----------

static esp_err_t onHttpEvent(esp_http_client_event_t *e) {
  if (e->event_id == HTTP_EVENT_ON_DATA && body.size() < 4096)
    body.append((const char *)e->data, e->data_len);
  return ESP_OK;
}

static const char *netErrorText(esp_err_t err) {
  switch (err) {
    case ESP_ERR_HTTP_CONNECT: return "can't reach Autoblow (internet down?)";
    case ESP_ERR_HTTP_EAGAIN:
    case ESP_ERR_TIMEOUT: return "Autoblow not answering (timeout)";
    case ESP_ERR_HTTP_FETCH_HEADER:
    case ESP_ERR_HTTP_CONNECTION_CLOSED:
    case ESP_ERR_HTTP_WRITE_DATA: return "connection to Autoblow dropped";
    default: return esp_err_to_name(err);
  }
}

// Returns the HTTP status, or -1 on a network error. Response body in `body`.
static int request(esp_http_client_method_t method, const std::string &url,
                   const char *json = nullptr) {
  body.clear();
  esp_http_client_set_url(http, url.c_str());
  esp_http_client_set_method(http, method);
  esp_http_client_set_header(http, "x-device-token", token);
  // The API wants no Content-Type on requests without a body.
  if (json) {
    esp_http_client_set_header(http, "Content-Type", "application/json");
    esp_http_client_set_post_field(http, json, strlen(json));
  } else {
    esp_http_client_delete_header(http, "Content-Type");
    esp_http_client_set_post_field(http, nullptr, 0);
  }
  esp_err_t err = esp_http_client_perform(http);
  if (err != ESP_OK) {
    setError("%s [%s]", netErrorText(err), esp_err_to_name(err));
    esp_http_client_close(http);
    return -1;
  }
  lastGood = millis();
  return esp_http_client_get_status_code(http);
}

static void parseState() {
  JsonDocument doc;
  if (deserializeJson(doc, body)) return;
  if (!doc["operationalMode"].is<const char *>()) return;
  Lock l;
  strlcpy(stat.mode, doc["operationalMode"] | "", sizeof stat.mode);
  const char *m = stat.mode;
  // Running modes of all three; anything paused, stopped or idle isn't.
  static const char *RUNNING[] = {"TARGET_SPEED_PLAYING", "LOCAL_SCRIPT_PLAYING", "MANUAL", "PULSE",
                                  "STROKE", "WORKOUTS", "SYNC_PLAYING", "OSCILLATOR_PLAYING",
                                  "SYNC_SCRIPT_PLAYING", "GO_TO"};
  stat.moving = false;
  for (const char *r : RUNNING)
    if (!strcmp(m, r)) stat.moving = true;
  if (type == Type::VacuPump) {
    // Manual with a 0 kPa target is the panel venting the pump, not running it.
    float kpa = doc["targetVacuumKpa"] | 0.0f;
    stat.targetSpeed = constrain(lroundf(-kpa * 100 / VACUPUMP_MAX_KPA), 0, 100);
    if (!strcmp(m, "MANUAL") && !stat.targetSpeed) stat.moving = false;
  } else if (type == Type::Ultra) {
    stat.targetSpeed = doc["oscillatorTargetSpeed"] | 0;
  } else {
    stat.targetSpeed = doc["targetSpeed"] | 0;
  }
  stat.valvePlus = doc["strokePlusValve"] | false;
  stat.valveMinus = doc["strokeMinusValve"] | false;
  stat.localScript = doc["localScript"] | 0;
}

static void setOnline(bool on) {
  Lock l;
  if (stat.online && !on) stat.lost++;
  stat.online = on;
  if (on) stat.error[0] = 0;
}

// Wi-Fi down or a request failed: lost once the grace period has run out.
// The cluster is kept (no rediscovery, which would cost two TLS handshakes on a
// link that is weak already); what was sent is forgotten, so the STOP the loss
// asks for goes out in full, and the first answer brings the device back.
static void trouble() {
  if (host.empty() || !stat.online || millis() - lastGood < graceMs()) return;
  plog("autoblow: no answer for %u s, device lost\n", (unsigned)((millis() - lastGood) / 1000));
  setOnline(false);
  Lock l;
  resyncReq = true;
}

// What to work toward: Idle until the UI has acknowledged the last loss.
static Desired target() {
  Lock l;
  return lostAcked == stat.lost ? want : Desired();
}

static bool discover() {
  // The generic route works for every device type and says which one it is
  // (as Autoblow's own SDK does it).
  int code = request(HTTP_METHOD_GET, DISCOVER_URL);
  if (code != 200) {
    if (code > 0) setError("discover: HTTP %d", code);
    return false;
  }
  JsonDocument doc;
  if (deserializeJson(doc, body)) return false;
  if (!(doc["connected"] | false)) {
    if (type == Type::VacuPump) setError("VacuPump offline: open its Online screen");
    else if (type == Type::Ultra) setError("Autoblow offline: switch it to online mode");
    else setError("VacuGlide offline: hold its mode button 2.5 s");
    return false;
  }
  const char *dt = doc["deviceType"] | "";
  Type t = !strcmp(dt, "vacuglide") ? Type::VacuGlide
         : !strcmp(dt, "vacupump") ? Type::VacuPump
         : !strcmp(dt, "autoblow-ultra") ? Type::Ultra
         : *dt ? Type::Unknown : Type::VacuGlide;  // no deviceType: the old VacuGlide-only reply
  if (t == Type::Unknown) {
    setError("unsupported Autoblow device: %s", dt);
    return false;
  }
  type = t;
  {
    Lock l;
    stat.type = t;
  }
#ifdef AUTOBLOW_API_BASE
  host = AUTOBLOW_API_BASE;
#else
  std::string c = doc["cluster"] | "";
  for (const char *p : {"https://", "http://"})
    if (c.rfind(p, 0) == 0) c = c.substr(strlen(p));
  if (c.find('.') == std::string::npos) c += ".autoblowapi.com";
  host = "https://" + c;
#endif
  plog("%s on %s\n", typeName(type), host.c_str());
  return true;
}

// ---------- sync loop ----------

// One step toward the desired state. Returns false if nothing to do.
static bool step(const Desired &d, Sent &s) {
  char json[48];
  int code = 0;
  auto put = [&](const char *path, const char *j = nullptr) {
    code = request(HTTP_METHOD_PUT, host + path, j);
    return code;
  };

  // A command counts as delivered on 2xx. A 4xx other than 429 (e.g. script start
  // in the wrong mode) is recorded as sent too, since resending it won't help.
  // 429, 5xx and network errors leave Sent alone, so the command is retried.
  auto landed = [&] { return (code >= 200 && code < 300) || (code >= 400 && code < 500 && code != 429); };

  // VacuPump: speed = vacuum depth in Manual mode. Its stop and pause keep the
  // vacuum, so stopping vents first: target 0 kPa, give it 3 s, then stop
  // (what Autoblow's SDK example does). Scripts and valves don't apply.
  if (type == Type::VacuPump) {
    if (d.want == Want::Speed && (s.want != (int)Want::Speed || s.speed != d.speed)) {
      snprintf(json, sizeof json, "{\"targetVacuumKpa\":%d}", -d.speed * VACUPUMP_MAX_KPA / 100);
      put("/vacupump/manual/target", json);
      if (landed() && s.want != (int)Want::Speed) put("/vacupump/manual/start");
      if (landed()) {
        s.want = (int)Want::Speed;
        s.speed = d.speed;
      }
    } else if (d.want != Want::Speed && s.want != (int)Want::Idle) {
      put("/vacupump/manual/target", "{\"targetVacuumKpa\":0}");
      if (landed()) put("/vacupump/manual/start");
      if (landed()) {
        uint32_t t0 = millis();
        while (millis() - t0 < 3000 && target().want != Want::Speed) vTaskDelay(pdMS_TO_TICKS(50));
        if (target().want == Want::Speed) {
          s.want = -1;  // started again while venting: target + start afresh
          return true;
        }
        put("/vacupump/manual/stop");
        if (landed()) s.want = (int)Want::Idle;
      }
    } else {
      return false;
    }
  }
  // Autoblow Ultra: speed = oscillation speed over the full stroke.
  else if (type == Type::Ultra) {
    if (d.want == Want::Speed && (s.want != (int)Want::Speed || s.speed != d.speed)) {
      snprintf(json, sizeof json, "{\"speed\":%d,\"minY\":0,\"maxY\":100}", d.speed);
      put("/autoblow/oscillate", json);
      if (landed()) {
        s.want = (int)Want::Speed;
        s.speed = d.speed;
      }
    } else if (d.want != Want::Speed && s.want != (int)Want::Idle) {
      put("/autoblow/oscillate/stop");
      if (landed()) s.want = (int)Want::Idle;
    } else {
      return false;
    }
  }
  // VacuGlide. 1. Stop first, so STOP is never queued behind anything else.
  else if (d.want == Want::Idle && s.want != (int)Want::Idle) {
    bool speedStop = s.want != (int)Want::Script;
    if (speedStop) put("/vacuglide/target-speed/stop");
    if (s.want != (int)Want::Speed && (!speedStop || landed())) put("/vacuglide/local-script/stop");
    if (landed()) {
      s.want = (int)Want::Idle;
      s.scriptPlaying = -1;
    }
  }
  // 2. Valves.
  else if (s.valvePlus != d.valvePlus) {
    snprintf(json, sizeof json, "{\"valveState\":%s}", d.valvePlus ? "true" : "false");
    put("/vacuglide/valve/stroke-plus", json);
    if (landed()) s.valvePlus = d.valvePlus;
  } else if (s.valveMinus != d.valveMinus) {
    snprintf(json, sizeof json, "{\"valveState\":%s}", d.valveMinus ? "true" : "false");
    put("/vacuglide/valve/stroke-minus", json);
    if (landed()) s.valveMinus = d.valveMinus;
  }
  // 3. Vacuum pulse on the Tighter valve (stroke-plus: air out, more suction;
  //    the device does the reverse of the manual's reading): open, hold (but let
  //    STOP or a held valve cut it short), close.
  else if (d.want == Want::Speed && d.pulseSeq != pulseDone) {
    pulseDone = d.pulseSeq;
    if (!d.valvePlus && !d.valveMinus) {
      put("/vacuglide/valve/stroke-plus", "{\"valveState\":true}");
      if (code >= 200 && code < 300) {
        uint32_t t0 = millis();
        while (millis() - t0 < d.pulseMs) {
          vTaskDelay(pdMS_TO_TICKS(25));
          Desired now = target();
          if (now.want != Want::Speed || now.valvePlus || now.valveMinus) break;
        }
        put("/vacuglide/valve/stroke-plus", "{\"valveState\":false}");
        s.valvePlus = landed() ? 0 : 1;  // 1: the valve branch retries the close
      }
    }
  }
  // 4. Speed.
  else if (d.want == Want::Speed && (s.want != (int)Want::Speed || s.speed != d.speed)) {
    snprintf(json, sizeof json, "{\"targetSpeed\":%d}", d.speed);
    put("/vacuglide/target-speed", json);
    if (landed()) {
      s.want = (int)Want::Speed;
      s.speed = d.speed;
    }
  }
  // 5. Local scripts. Selecting one starts it and closes both valves.
  else if (d.want == Want::Script && (s.want != (int)Want::Script || s.script != d.script)) {
    snprintf(json, sizeof json, "{\"localScriptIndex\":%d}", d.script);
    put("/vacuglide/local-script", json);
    if (landed()) {
      s.want = (int)Want::Script;
      s.script = d.script;
      s.scriptPlaying = 1;
      s.valvePlus = s.valveMinus = 0;
    }
  } else if (d.want == Want::Script && s.scriptPlaying != d.scriptPlaying) {
    put(d.scriptPlaying ? "/vacuglide/local-script/start" : "/vacuglide/local-script/stop");
    if (landed()) s.scriptPlaying = d.scriptPlaying;
  } else {
    return false;
  }

  if (code >= 200 && code < 300) {
    parseState();
    setOnline(true);
  } else if (code == 502) {
    // Device dropped off the cloud: rediscover and resend everything.
    setError("%s went offline", typeName(type));
    setOnline(false);
    host.clear();
    s = Sent();
  } else if (code == 429) {
    setError("rate limited");
    vTaskDelay(pdMS_TO_TICKS(1000));
  } else if (code >= 500) {
    setError("Autoblow error %d", code);
    vTaskDelay(pdMS_TO_TICKS(1000));
  } else if (code > 0) {
    setError("HTTP %d", code);
    plog("HTTP %d: %s\n", code, body.c_str());
  } else {
    trouble();
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  return true;
}

static void netTask(void *) {
  esp_http_client_config_t cfg = {};
  cfg.url = DISCOVER_URL;
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.keep_alive_enable = true;
  // Long enough for a TLS handshake on a weak link; shorter than the grace
  // period, so a stuck request is retried (on a fresh connection) in time.
  cfg.timeout_ms = 6000;
  cfg.event_handler = onHttpEvent;
  http = esp_http_client_init(&cfg);

  Sent sent;
  uint32_t lastPoll = 0;
  bool wasUp = false;

  for (;;) {
    bool wifi = wlan::up();
    {
      Lock l;
      if (wifi != stat.wifi) plog("Wi-Fi %s\n", wifi ? "up" : "down");
      stat.wifi = wifi;
      if (resyncReq) {
        resyncReq = false;
        sent = Sent();
      }
    }
    if (!wifi) {
      // The keep-alive socket died with the link; reusing it after the reconnect
      // would sit out the full timeout first.
      if (wasUp) esp_http_client_close(http);
      wasUp = false;
      trouble();
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }
    wasUp = true;
    if (host.empty()) {
      if (!discover()) {
        setOnline(false);
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(3000));
        continue;
      }
      setOnline(true);
      sent = Sent();
      pulseDone = target().pulseSeq;  // a pulse from before the loss is stale
    }
    if (step(target(), sent)) continue;

    // Nothing to send: poll state now and then, otherwise wait for a change.
    if (millis() - lastPoll > 3000) {
      lastPoll = millis();
      int code = request(HTTP_METHOD_GET, host + route() + "/state");
      if (code == 200) {
        parseState();
        setOnline(true);
      } else if (code == 502) {
        setOnline(false);
        host.clear();
      } else if (code < 0) {
        trouble();
      }
      continue;
    }
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(250));
  }
}

void begin() {
  mtx = xSemaphoreCreateMutex();
  strlcpy(token, settings.token.c_str(), sizeof token);
  xTaskCreatePinnedToCore(netTask, "autoblow", 12288, nullptr, 5, &task, 0);
  // No second connection (e.g. for the API's /events/stream with the device's
  // button presses): a second TLS session doesn't fit next to this one in
  // internal RAM, and its handshakes made this one fail (tried 2026-10-19).
}

}  // namespace autoblow
