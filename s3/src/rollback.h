// Firmware probation after an update. The ESP32 keeps the previous firmware in
// the other OTA slot; a new one stays "pending" until we confirm it. If it
// crashes, freezes or never gets onto Wi-Fi before that, the previous one comes back.
#pragma once

namespace rollback {

void begin();       // in setup(): arms the deadline if this boot is a pending update
void loop();        // in loop(): confirms once healthy
const char *state(); // "confirmed", "pending" or "factory" (flashed over USB, no probation)

}  // namespace rollback
