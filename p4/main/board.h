// Display board: Guition JC1060P470C (ESP32-P4), 1024x600 MIPI-DSI panel (JD9165),
// GT911 touch on I2C0 (SDA 7 / SCL 8), backlight PWM on GPIO23, hooked into LVGL
// through esp_lvgl_port (LVGL runs on its own task; see ui.h for the lock).
#pragma once
#include <stdint.h>

#include "driver/i2c_master.h"

#define BOARD_NAME "Guition JC1060P470C"
// The touch I2C bus (0x14/0x5D GT911, others on it: 0x18, 0x32, 0x36) is shared:
// fetch it with boardI2cBus() instead of opening a second driver on the pins.
#define BOARD_FUEL_GAUGE 1   // look for a MAX1704x (0x36) on the touch bus; none confirmed
#define BOARD_DIMMABLE 1     // backlight on GPIO23 takes PWM

static const int SCREEN_W = 1024, SCREEN_H = 600;

void boardBegin();
// The shared touch I2C bus (I2C0, SDA 7, SCL 8); valid after boardBegin().
i2c_master_bus_handle_t boardI2cBus();
// Show the picture (and read touches) upside down. NOT IMPLEMENTED on the P4: the DSI panel
// has no swap/mirror and LVGL rotation is off, so this does nothing.
void boardSetFlip(bool flip);
bool boardIsFlipped();
// Backlight level, 0..100 (PWM on GPIO23). Doesn't change the sleep state.
void boardSetBrightness(int pct);
// Calibration only: raw PWM duty 0..255 (-1 = back to the brightness setting).
void boardBacklightRaw(int duty);
// Backlight. While it is off, the first touch only wakes the screen.
void boardScreenOff();
bool boardScreenIsOn();
// The 1024x600 frame buffer LVGL last rendered: 3 bytes per pixel, stride
// SCREEN_W*3, bytes in memory order B, G, R (LVGL RGB888). nullptr before
// boardBegin(). Read it under UiLock.
const uint8_t *boardFrameBufferRgb888();  // raw active buffer: 3 bytes/pixel, or 2 when boardIs565()
bool boardIs565();
// No bounce-buffer guard on the DSI panel: always -1.
int boardDisplayGuard();
// Not needed on DSI (no-op).
void boardDisplayResync();
