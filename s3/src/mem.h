// Memory placement: TLS and the Bluetooth host in PSRAM (see mem.cpp).
#pragma once

void memBegin();  // first thing in setup(), before any TLS
void memReport(const char *when);  // serial: internal + PSRAM free
