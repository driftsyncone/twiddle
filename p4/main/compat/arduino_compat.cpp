// Implementation of the Arduino compatibility shim declared in arduino_compat.h (millis, String helpers, Preferences on NVS).
#include "arduino_compat.h"
#include <ctype.h>
#include "nvs.h"
#include "nvs_flash.h"

void String::trim() {
  size_t b = 0, e = size();
  while (b < e && isspace((unsigned char)(*this)[b])) b++;
  while (e > b && isspace((unsigned char)(*this)[e - 1])) e--;
  assign(substr(b, e - b));
}

bool Preferences::begin(const char *ns, bool ro) {
  end();
  nvs_handle_t h;
  if (nvs_open(ns, ro ? NVS_READONLY : NVS_READWRITE, &h) != ESP_OK) return false;
  h_ = h; open_ = true;
  return true;
}
void Preferences::end() { if (open_) { nvs_close((nvs_handle_t)h_); open_ = false; } }
bool Preferences::clear() { return open_ && nvs_erase_all((nvs_handle_t)h_) == ESP_OK && nvs_commit((nvs_handle_t)h_) == ESP_OK; }
bool Preferences::remove(const char *k) { return open_ && nvs_erase_key((nvs_handle_t)h_, k) == ESP_OK && nvs_commit((nvs_handle_t)h_) == ESP_OK; }
bool Preferences::isKey(const char *k) { size_t n; return open_ && nvs_get_blob((nvs_handle_t)h_, k, nullptr, &n) == ESP_OK; }

#define PREF_NUM(T, NAME, GET, SET)                                                     \
  T Preferences::get##NAME(const char *k, T d) { T v = d; if (open_) GET((nvs_handle_t)h_, k, &v); return v; } \
  size_t Preferences::put##NAME(const char *k, T v) { return open_ && SET((nvs_handle_t)h_, k, v) == ESP_OK && nvs_commit((nvs_handle_t)h_) == ESP_OK ? sizeof(T) : 0; }
PREF_NUM(int32_t, Int, nvs_get_i32, nvs_set_i32)
PREF_NUM(uint32_t, UInt, nvs_get_u32, nvs_set_u32)
PREF_NUM(uint8_t, UChar, nvs_get_u8, nvs_set_u8)
bool Preferences::getBool(const char *k, bool d) { return getUChar(k, d ? 1 : 0) != 0; }
size_t Preferences::putBool(const char *k, bool v) { return putUChar(k, v ? 1 : 0); }

String Preferences::getString(const char *k, const String &d) {
  size_t n = 0;
  if (!open_ || nvs_get_str((nvs_handle_t)h_, k, nullptr, &n) != ESP_OK) return d;
  std::string s(n, '\0');
  nvs_get_str((nvs_handle_t)h_, k, &s[0], &n);
  s.resize(n ? n - 1 : 0);
  return String(s);
}
size_t Preferences::putString(const char *k, const char *v) {
  return open_ && nvs_set_str((nvs_handle_t)h_, k, v) == ESP_OK && nvs_commit((nvs_handle_t)h_) == ESP_OK ? strlen(v) : 0;
}
size_t Preferences::getBytesLength(const char *k) { size_t n = 0; if (open_) nvs_get_blob((nvs_handle_t)h_, k, nullptr, &n); return n; }
size_t Preferences::getBytes(const char *k, void *buf, size_t len) {
  size_t n = len;
  return open_ && nvs_get_blob((nvs_handle_t)h_, k, buf, &n) == ESP_OK ? n : 0;
}
size_t Preferences::putBytes(const char *k, const void *buf, size_t len) {
  return open_ && nvs_set_blob((nvs_handle_t)h_, k, buf, len) == ESP_OK && nvs_commit((nvs_handle_t)h_) == ESP_OK ? len : 0;
}
