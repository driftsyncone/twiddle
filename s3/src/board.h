// Display board: RGB panel, GT911 touch, backlight, hooked into LVGL.
//
// Two boards are supported, picked at build time:
//   Guition JC8048W550C (default, env:panel)
//   Waveshare ESP32-S3-Touch-LCD-5, 800x480 (-DBOARD_WAVESHARE_LCD5, env:waveshare5)
#pragma once
#include <stdint.h>

#if defined(BOARD_WAVESHARE_LCD5)
#define BOARD_NAME "Waveshare ESP32-S3-Touch-LCD-5"
// GPIO17/18 drive the display on this board, and the CH422G IO expander answers
// on 0x30-0x3F (a MAX17048 lives at 0x36), so no fuel gauge here.
#define BOARD_HAS_P4_I2C 0
#define BOARD_FUEL_GAUGE 0
#define BOARD_BATTERY_ADC_PIN -1  // the CS8501 charger's status pins only drive LEDs
#define BOARD_DIMMABLE 0  // backlight is a CH422G output: on/off
#else
#define BOARD_NAME "Guition JC8048W550C"
#define BOARD_HAS_P4_I2C 1  // P4 connector: GND, 3.3 V, GPIO17, GPIO18
#define BOARD_FUEL_GAUGE 1
// Battery voltage on GPIO17 (ADC2 channel 6) through a 33 k / 100 k divider.
// The Sunton ESP32-8048S050 schematic has that divider on the board; the Guition
// JC8048W550C doesn't (the pin reads 0 V), so it needs a wired one: BAT+ -33k-
// IO17 -100k- GND, on the P4 header. Without it the level just reads "none".
// GPIO17 is also P4's SDA: a gauge on P4 is only looked for at boot.
#define BOARD_BATTERY_ADC_PIN 17
#define BOARD_BATTERY_DIVIDER 0.752f  // 100 / (33 + 100)
#define BOARD_DIMMABLE 1  // backlight enable on GPIO2 takes PWM
#endif

static const int SCREEN_W = 800, SCREEN_H = 480;

void boardBegin();
// Show the picture (and read touches) upside down. Applies at once; call after uiBegin().
void boardSetFlip(bool flip);
bool boardIsFlipped();
// Backlight level, 0..100 (Guition: PWM on the backlight driver's enable pin;
// Waveshare: anything above 0 is on). Doesn't change the sleep state.
void boardSetBrightness(int pct);
// Calibration only: raw PWM duty 0..255 (-1 = back to the brightness setting). No-op on the Waveshare.
void boardBacklightRaw(int duty);
// Backlight. While it is off, the first touch only wakes the screen.
void boardScreenOff();
bool boardScreenIsOn();
// The 800x480 RGB565 frame buffer as shown on screen (for screenshots).
const uint16_t *boardFrameBuffer();
// Backlight on/off without changing the sleep state (no-op while asleep).
void boardBacklight(bool on);
// Bounce-buffer parity fixes since boot (each one healed a scrambled frame),
// or -1 if the guard couldn't be installed.
int boardDisplayGuard();
// Restart the display output (fixes a picture left scrambled by a flash write).
void boardDisplayResync();
