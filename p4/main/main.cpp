// Twiddle: remote panel for the Autoblow VacuGlide; standalone, talks straight to the Autoblow cloud.
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include "autoblow.h"
#include "battery.h"
#include "board.h"
#include "control.h"
#include "devices.h"
#include "log.h"
#include "mem.h"
#include "ota.h"
#include "rollback.h"
#include "settings.h"
#include "ui.h"
#include "wlan.h"

void setup() {
  Serial.setTxBufferSize(2048);  // logging from the BLE tasks mustn't wait on the UART
  Serial.begin(115200);
  logBegin();
  memBegin();  // before any TLS
  rollback::begin();  // first: arms the deadline for a fresh update
  settingsLoad();
  autoblow::begin();  // before the UI, which reads its status
  devices::begin();   // Bluetooth first, while internal RAM is still in one piece

  boardBegin();
  battery::begin();  // after boardBegin(): shares the touch I2C bus
  {
    UiLock l;  // LVGL's task is running since boardBegin(); control::begin() makes a timer
    uiBegin();
    boardSetFlip(settings.flip);
    boardSetBrightness(settings.brightness);
    control::begin();
  }

  wlan::begin();
  otaBegin();
  memReport("boot");
}

// LVGL runs on esp_lvgl_port's own task (board.cpp); this loop task only does the
// periodic work that used to share its thread. Only what touches LVGL or control:: holds the lock LVGL
// uses; wlan::loop() and battery::loop() make calls to the C6 / I2C that can wait seconds, and holding the
// lock then would freeze the LVGL task (touch, STOP, the 100 ms control tick).
void loop() {
  {
    UiLock l;  // the web server task takes the same lock for UI work
    uiPump();
    rollback::loop();
  }
  wlan::loop();
  battery::loop();
#ifdef TEST_ROLLBACK_CRASH  // test builds only: crash 15 s after boot
  if (millis() > 15000) abort();
#endif
  delay(5);
}
