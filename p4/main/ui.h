// LVGL UI (1024x600): status bar with STOP, and Remote / Autopilot / Session / Scripts / Devices /
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
// Switch to tab 0..5 (Remote, Autopilot, Session, Scripts, Devices, Settings). Only marks things dirty: the LVGL
// task draws (never lv_refr_now from another task); a caller that needs the picture invalidates and waits.
// Settings (5) shows Wi-Fi and the token's tail: a screenshot of it is never published.
void uiShowTab(int tab);
// Firmware update screen (drawn by the LVGL task shortly after this returns), shown before the write starts (the backlight is off
// while flash is written, which scrambles any picture). pct < 0: no percentage.
void uiShowUpdating(int pct);
void uiHideUpdating();
// Switch to colour theme `idx` (see theme.h) live; async, safe from any LVGL-side callback.
// Does not save it: the Settings row stores settings.theme itself.
void uiSetTheme(int idx);
