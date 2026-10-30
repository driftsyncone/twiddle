// 800x480 LVGL UI: status bar with STOP, and Remote / Autopilot / Session / Scripts / Devices /
// Settings tabs. The status bar's top left picks the device whose controls show
// (Scripts and the valve rows only for a VacuGlide); every device with Follow on
// still gets the output.
#pragma once

void uiBegin();

// LVGL, control:: and the touch/ADC hardware belong to the loop task. Another
// task (the web server) holds this lock while it uses any of them; loop() holds
// it around lv_timer_handler() and friends. Recursive, so nesting is fine.
void uiLock();
void uiUnlock();
struct UiLock {
  UiLock() { uiLock(); }
  ~UiLock() { uiUnlock(); }
};
// Run fn(arg) on the loop task itself and wait for it. For hardware whose
// driver was set up on that core (I2C, ADC): used from core 0, the I2C driver
// returns ACKs for addresses that aren't there. Don't hold UiLock while calling.
void uiCall(void (*fn)(void *), void *arg);
void uiPump();  // loop(): runs a pending uiCall
// Switch to tab 0..5 (Remote, Autopilot, Session, Scripts, Devices, Settings) and redraw now.
// Settings (5) shows Wi-Fi and the token's tail: a screenshot of it is never published.
void uiShowTab(int tab);
// Firmware update screen, shown before the write starts (the backlight is off
// while flash is written, which scrambles any picture). pct < 0: no percentage.
void uiShowUpdating(int pct);
void uiHideUpdating();
// Switch to colour theme `idx` (see theme.h) live; async (runs on the loop task's next LVGL tick),
// so it is safe from an LVGL event and from the web task under UiLock. Does not save it.
void uiSetTheme(int idx);
