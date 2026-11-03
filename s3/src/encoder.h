// Adafruit I2C STEMMA QT rotary encoder (product 5880, seesaw) on the P4 connector, as a physical speed control.
//   turn = change the active speed (control::encoderAdjust), short press = pause / resume (control::encoderTogglePause),
//   hold >= 1 s = hard STOP (control::stopAll, the same path as the on-screen STOP).
// Off unless Settings says On, or Auto and one answered at boot. A missing / unplugged encoder is silent. Guition only
// (BOARD_HAS_P4_I2C); on the Waveshare every call is a no-op. Protocol and pure logic: seesaw.h. Loop task only.
#pragma once
#include <stdint.h>

namespace encoder {

void begin();  // after settingsLoad(), BEFORE battery::begin(): decides who owns GPIO17/18
void loop();   // from loop(), under UiLock; polls every ~40 ms
bool present();  // an encoder is answering right now
// GPIO17/18 are reserved for the encoder (mode On, or Auto and one was found at boot): the battery divider ADC is off, and the
// fuel-gauge probe on this bus must skip the encoder's address.
bool ownsP4();
// Address the encoder answers on, 0 if none (for the status line / diagnostics).
uint8_t address();
// True if a seesaw encoder board (not a MAX1704x gauge) answers at addr on the P4 bus. Used by battery.cpp so a gauge probe at
// 0x36 does not take the encoder for a gauge. Starts no bus: call only while p4bus is started.
bool isEncoderAt(uint8_t addr);

}  // namespace encoder
