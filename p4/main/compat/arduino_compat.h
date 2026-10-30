// Minimal Arduino-compatible surface on top of ESP-IDF, so the panel's logic ports with few edits.
// Extend it when a ported file needs one more call; keep it small, it is not a full Arduino core.
#pragma once
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

inline uint32_t millis() { return (uint32_t)(esp_timer_get_time() / 1000); }
inline uint32_t micros() { return (uint32_t)esp_timer_get_time(); }
// At least one tick for a non-zero delay: a coarse tick rate must never turn delay(n) into a yield (busy loop).
inline void delay(uint32_t ms) {
  TickType_t t = pdMS_TO_TICKS(ms);
  vTaskDelay(ms && !t ? 1 : t);
}
#ifndef constrain
#define constrain(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#endif

// Arduino String on std::string (only what the panel uses).
class String : public std::string {
 public:
  String() {}
  String(const char *s) : std::string(s ? s : "") {}
  String(const std::string &s) : std::string(s) {}
  String(int v) : std::string(std::to_string(v)) {}
  bool isEmpty() const { return empty(); }
  int toInt() const { return atoi(c_str()); }
  float toFloat() const { return (float)atof(c_str()); }
  bool startsWith(const char *p) const { return rfind(p, 0) == 0; }
  int indexOf(char c) const { size_t i = find(c); return i == npos ? -1 : (int)i; }
  String substring(unsigned from) const { return String(substr(from)); }
  String substring(unsigned from, unsigned to) const { return String(substr(from, to - from)); }
  void trim();
  String &operator+=(const char *s) { append(s); return *this; }
  String &operator+=(char c) { push_back(c); return *this; }
  String &operator+=(const String &s) { append(s); return *this; }
  bool operator==(const char *s) const { return compare(s) == 0; }
  bool operator!=(const char *s) const { return compare(s) != 0; }
};

// Serial -> the IDF console (UART/USB-JTAG), line-buffered by stdio.
struct SerialShim {
  void begin(unsigned long) {}
  void setTxBufferSize(size_t) {}
  void print(const char *s) { fputs(s, stdout); }
  void print(int v) { printf("%d", v); }
  void println(const char *s = "") { puts(s); }
  void println(int v) { printf("%d\n", v); }
  int printf(const char *f, ...) __attribute__((format(printf, 2, 3))) {
    va_list ap; va_start(ap, f); int n = vprintf(f, ap); va_end(ap); return n;
  }
  void flush() { fflush(stdout); }
};
inline SerialShim Serial;

// Preferences on NVS (namespace = Preferences namespace, key limits as NVS: 15 chars).
class Preferences {
 public:
  ~Preferences() { end(); }
  bool begin(const char *ns, bool readOnly = false);
  void end();
  bool clear();
  bool remove(const char *key);
  bool isKey(const char *key);
  int32_t getInt(const char *k, int32_t d = 0);
  size_t putInt(const char *k, int32_t v);
  uint32_t getUInt(const char *k, uint32_t d = 0);
  size_t putUInt(const char *k, uint32_t v);
  uint8_t getUChar(const char *k, uint8_t d = 0);
  size_t putUChar(const char *k, uint8_t v);
  bool getBool(const char *k, bool d = false);
  size_t putBool(const char *k, bool v);
  String getString(const char *k, const String &d = String());
  size_t putString(const char *k, const char *v);
  size_t getBytesLength(const char *k);
  size_t getBytes(const char *k, void *buf, size_t len);
  size_t putBytes(const char *k, const void *buf, size_t len);

 private:
  uint32_t h_ = 0;
  bool open_ = false;
};
