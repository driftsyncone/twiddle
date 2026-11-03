#include "hrm.h"

#include <Arduino.h>

#include "ble.h"
#include "hrmparse.h"

namespace hrm {

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static int lastBpm;
static uint32_t lastMs, count;

void feed(const uint8_t *d, size_t n) {
  Reading r = parse(d, n);
  if (!r.ok) return;
  portENTER_CRITICAL(&mux);
  lastBpm = r.bpm;
  lastMs = millis();
  count++;
  portEXIT_CRITICAL(&mux);
}

int bpm() { return lastBpm; }

uint32_t ageMs() {
  portENTER_CRITICAL(&mux);
  uint32_t seen = lastMs, n = count;
  portEXIT_CRITICAL(&mux);
  return n ? millis() - seen : UINT32_MAX;
}

uint32_t seq() { return count; }

bool connected() {
  for (int i = 0; i < ble::count(); i++) {
    ble::Toy t = ble::toy(i);
    if (t.kind == devices::Kind::Hrm && t.connected) return true;
  }
  return false;
}

}  // namespace hrm
