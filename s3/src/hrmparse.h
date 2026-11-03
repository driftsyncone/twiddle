// Heart Rate Measurement (GATT 0x2A37) decoder. Pure: no ESP / BLE includes, host-testable.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace hrm {

struct Reading {
  bool ok = false;          // a usable bpm (decoded, in 40..220, and the strap has contact if it can tell)
  int bpm = 0;
  bool contactSupported = false;  // the sensor reports skin contact at all
  bool contact = false;           // ... and it has it
};

// Flags byte: bit 0 = value is uint16 (else uint8), bit 1 = contact detected, bit 2 = contact feature supported.
// A sensor that supports contact detection and says "no contact" gives ok = false.
Reading parse(const uint8_t *d, size_t n);

}  // namespace hrm
