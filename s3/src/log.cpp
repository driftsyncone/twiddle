#include "log.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <stdarg.h>

static const int SIZE = 16384;
static char *ring;  // PSRAM; wraps, always ends in a whole line
static int head;    // next write position
static bool wrapped;
static SemaphoreHandle_t mtx;  // a mutex, not a spinlock: copying 16 KB of PSRAM takes a while

void logBegin() {
  ring = (char *)heap_caps_malloc(SIZE, MALLOC_CAP_SPIRAM);
  mtx = xSemaphoreCreateMutex();
}

void plog(const char *fmt, ...) {
  char line[192];
  int n = snprintf(line, sizeof line, "%7lu ", (unsigned long)(millis() / 100));  // tenths of a second
  va_list ap;
  va_start(ap, fmt);
  n += vsnprintf(line + n, sizeof line - n, fmt, ap);
  va_end(ap);
  n = min<int>(n, sizeof line - 1);
  if (line[n - 1] != '\n') {
    if (n == (int)sizeof line - 1) n--;
    line[n++] = '\n';
    line[n] = 0;
  }
  Serial.print(line);

  if (!ring || !mtx) return;
  xSemaphoreTake(mtx, portMAX_DELAY);
  for (int i = 0; i < n; i++) {
    ring[head] = line[i];
    if (++head == SIZE) {
      head = 0;
      wrapped = true;
    }
  }
  xSemaphoreGive(mtx);
}

void logDump(void (*out)(const char *, int, void *), void *arg) {
  if (!ring || !mtx) return;
  // Copy first (lines keep coming in meanwhile), then skip the cut-off first line.
  char *copy = (char *)heap_caps_malloc(SIZE, MALLOC_CAP_SPIRAM);
  if (!copy) return;
  int len;
  bool wrap;
  xSemaphoreTake(mtx, portMAX_DELAY);
  wrap = wrapped;
  if (wrap) {
    memcpy(copy, ring + head, SIZE - head);
    memcpy(copy + SIZE - head, ring, head);
    len = SIZE;
  } else {
    memcpy(copy, ring, head);
    len = head;
  }
  xSemaphoreGive(mtx);
  int start = 0;
  if (wrap)
    while (start < len && copy[start++] != '\n') {}
  if (len > start) out(copy + start, len - start, arg);
  heap_caps_free(copy);
}
