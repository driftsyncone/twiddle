// The P4 connector's I2C bus (Guition: Wire1 on GPIO17 SDA / GPIO18 SCL), shared by the encoder and the fuel gauge. One owner of
// begin()/end() so the two never start it twice or pull it away from each other. Loop task only.
#pragma once
#include <Wire.h>

#include "board.h"

namespace p4bus {
#if BOARD_HAS_P4_I2C
inline bool &started() { static bool s; return s; }
// Short timeout: a dead or half-plugged bus must not stall the UI / net tasks (the default is 50 ms per transfer).
inline bool begin() {
  if (!started()) {
    started() = Wire1.begin(17, 18, 100000);
    if (started()) Wire1.setTimeOut(10);
  }
  return started();
}
inline void end() {
  if (started()) Wire1.end();
  started() = false;
}
#else
inline bool begin() { return false; }
inline void end() {}
inline bool &started() { static bool s; return s; }
#endif
}  // namespace p4bus
