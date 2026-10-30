// Log lines to serial and to a ring buffer in PSRAM, readable at GET /log
// (OTA login), so a panel on Wi-Fi only can still be diagnosed.
#pragma once

void logBegin();  // first thing in setup(); before it, lines go to serial only
void plog(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
// The buffer as text, oldest line first; calls out(chunk, len) one or more times.
void logDump(void (*out)(const char *chunk, int len, void *arg), void *arg);
