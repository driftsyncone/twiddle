// The BLE heart rate monitor (Heart Rate Service 0x180D, Measurement 0x2A37). It is a Bluetooth client like
// the toys (ble:: / toys/hrm.cpp), but not a toy: it never receives output and has no speed.
//
// The notification handler (NimBLE host task) calls feed(); everything else reads the latest values from
// any task. Readings outside 40..220 bpm, or from a strap that reports no skin contact, are dropped.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace hrm {

void feed(const uint8_t *data, size_t len);  // a 0x2A37 notification

int bpm();            // last valid reading, 0 if there has been none since boot
uint32_t ageMs();     // ms since that reading; UINT32_MAX if none
uint32_t seq();       // bumps with every valid reading
bool connected();     // a monitor is connected (links are shared with the toys, see ble.cpp)

}  // namespace hrm
