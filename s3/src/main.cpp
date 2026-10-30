// Twiddle: standalone multi-device toy remote, talks straight to the Autoblow cloud and Bluetooth toys.
#include <Arduino.h>
#include <WiFi.h>
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
  uiBegin();
  boardSetFlip(settings.flip);
  boardSetBrightness(settings.brightness);
  control::begin();

  wlan::begin();
  otaBegin();
  memReport("boot");
}

void loop() {
  {
    UiLock l;  // the web server task takes the same lock for UI work
    lv_timer_handler();
    uiPump();
    wlan::loop();
    battery::loop();
    rollback::loop();
  }
#ifdef TEST_ROLLBACK_CRASH  // test builds only: crash 15 s after boot
  if (millis() > 15000) abort();
#endif
  delay(5);
}
