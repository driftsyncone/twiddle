#include "otapw.h"

#include <Arduino.h>
#include <Preferences.h>
#include <esp_mac.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <string.h>
#include "mbedtls/sha512.h"

#include "build_config.h"
#include "log.h"
#include "pwgen.h"

namespace otapw {

static char pw[32];

#ifndef TWIDDLE_DEV_WEB_PASSWORD
static const char *NS = "vgpanel";  // same namespace as settings.cpp
static const char *KEY = "otapw";

// 6 characters from a 27-letter alphabet (pwgen.h). With the radio running esp_random() is a true random source
// (RF noise); 64 bytes are drawn, hashed together with the microsecond clock and the base MAC, and the SHA-512 is the
// pool (16 words) the generator reads.
static bool makeNew(char *out, size_t n) {
  uint8_t raw[64 + 16];
  esp_fill_random(raw, 64);
  uint64_t us = (uint64_t)esp_timer_get_time();
  memcpy(raw + 64, &us, 8);
  esp_read_mac(raw + 72, ESP_MAC_WIFI_STA);
  uint8_t pool[64];
  mbedtls_sha512(raw, sizeof raw, pool, 0);
  size_t used = 0;
  bool ok = pwgen::generate(out, n, [&]() -> uint32_t {
    uint32_t w;
    if (used + 4 > sizeof pool) return esp_random();  // rejection sampling used up the pool (rare): plain RNG words
    memcpy(&w, pool + used, 4);
    used += 4;
    return w;
  });
  memset(raw, 0, sizeof raw);
  memset(pool, 0, sizeof pool);
  return ok;
}

static bool store(const char *s) {
  Preferences p;
  if (!p.begin(NS, false)) return false;
  size_t w = p.putString(KEY, s);
  p.end();
  return w == strlen(s);
}
#endif  // !TWIDDLE_DEV_WEB_PASSWORD

void begin() {
#ifdef TWIDDLE_DEV_WEB_PASSWORD
  strlcpy(pw, TWIDDLE_DEV_WEB_PASSWORD, sizeof pw);
  plog("web login: fixed password from the build (dev build)\n");
#else
  Preferences p;
  String s;
  if (p.begin(NS, true)) {
    s = p.getString(KEY, "");
    p.end();
  }
  if (pwgen::valid(s.c_str())) {
    strlcpy(pw, s.c_str(), sizeof pw);
  } else {
    char fresh[32];
    if (makeNew(fresh, sizeof fresh) && store(fresh)) {
      strlcpy(pw, fresh, sizeof pw);
      plog("web login: made a new per-device password (shown on the Settings tab)\n");
    } else {
      plog("web login: could not make or store a password; the web admin stays locked (403)\n");
    }
    memset(fresh, 0, sizeof fresh);
  }
#endif
}

bool known() { return pw[0] != 0; }
const char *get() { return pw; }

bool fixedByBuild() {
#ifdef TWIDDLE_DEV_WEB_PASSWORD
  return true;
#else
  return false;
#endif
}

bool reset() {
#ifdef TWIDDLE_DEV_WEB_PASSWORD
  return false;
#else
  char fresh[32];
  bool ok = makeNew(fresh, sizeof fresh) && store(fresh);
  if (ok) {
    strlcpy(pw, fresh, sizeof pw);
    plog("web login: password reset\n");
  }
  memset(fresh, 0, sizeof fresh);
  return ok;
#endif
}

}  // namespace otapw
