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
#include "encoder.h"
#include "log.h"
#include "mem.h"
#include "ota.h"
#include "otapw.h"
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
  encoder::begin();  // before battery::begin(): it decides who owns the P4 connector (GPIO17/18)
  battery::begin();  // after boardBegin(): shares the touch I2C bus
  uiBegin();
  boardSetFlip(settings.flip);
  boardSetBrightness(settings.brightness);
  control::begin();

  wlan::begin();
  otapw::begin();  // after Wi-Fi is up: the radio makes the RNG a true one; the Settings tab shows the result
  otaBegin();
  memReport("boot");
}

void loop() {
  {
    UiLock l;  // the web server task takes the same lock for UI work
#ifdef UI_TRACE
    uint32_t t0 = micros();
#endif
    lv_timer_handler();
#ifdef UI_TRACE
    {  // every 2 s: loops, time inside lv_timer_handler (avg / max ms), time the lock was waited for is not included
      static uint32_t n, sum, mx, last;
      uint32_t d = micros() - t0;
      n++; sum += d; if (d > mx) mx = d;
      if (millis() - last >= 2000) {
        Serial.printf("uitrace: %u loops/2s, lv_timer_handler avg %.1f ms max %.1f ms, heap %u, psram %u\n", (unsigned)n, sum / 1000.0f / n,
                      mx / 1000.0f, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
        extern uint32_t traceFlushUs, traceFlushes, traceFlushPx;
        Serial.printf("uitrace: flush %u calls, %u px, %.1f ms in draw_bitmap\n", (unsigned)traceFlushes, (unsigned)traceFlushPx,
                      traceFlushUs / 1000.0f);
        traceFlushUs = traceFlushes = traceFlushPx = 0;
        n = sum = mx = 0; last = millis();
      }
    }
#endif
    uiPump();
    wlan::loop();
    battery::loop();
    encoder::loop();
    rollback::loop();
  }
#ifdef TEST_ROLLBACK_CRASH  // test builds only: crash 15 s after boot
  if (millis() > 15000) abort();
#endif
  delay(5);
}
