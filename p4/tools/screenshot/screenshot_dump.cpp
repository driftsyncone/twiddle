// DEBUG TOOL, not part of the product build: dumps the screen over the serial output, one tab at a time.
// tools/screenshot/capture.sh copies this into main/, builds, flashes, captures and decodes the PNGs.
//
// Why serial: the panel's USB console takes no input (writes time out), so there is no way to ask for a
// shot; this file shows each tab on its own after boot and prints it. The link is slow, so the shot is
// every 2nd pixel (512x300, ~100 s per tab).
//
// Never include the Settings tab (5): it shows the token's tail and its screenshot must not be shared.
#include <Arduino.h>
#include <lvgl.h>
#include <mbedtls/base64.h>
#include <stdio.h>
#include "board.h"
#include "esp_private/startup_internal.h"
#include "ui.h"

#ifndef SHOT_STEP
#define SHOT_STEP 2  // every 2nd pixel (512x300); 4 = a quarter of the time, fine for theme checks
#endif
#ifndef SHOT_THEME
#define SHOT_THEME -1  // >= 0: switch to that colour theme live before the shots (not saved)
#endif
#ifndef SHOT_TAB_LIST
#define SHOT_TAB_LIST 0, 1, 2, 3, 4
#endif

static void dump() {
  UiLock l;
  const uint8_t *fb = boardFrameBufferRgb888();  // BGR888, 1024x600
  if (!fb) { printf("SHOT_ERR\n"); return; }
  const int W2 = SCREEN_W / SHOT_STEP, H2 = SCREEN_H / SHOT_STEP;
  printf("SHOT_BEGIN %d %d\n", W2, H2);
  static unsigned char out[SCREEN_W * 3 / 2 / 3 * 4 + 8];
  static uint8_t rgb[SCREEN_W / SHOT_STEP * 3];
  for (int y = 0; y < H2; y++) {
    const uint8_t *src = fb + (size_t)(y * SHOT_STEP) * SCREEN_W * 3;
    for (int x = 0; x < W2; x++) { const uint8_t *q = src + x * 3 * SHOT_STEP; rgb[x * 3] = q[2]; rgb[x * 3 + 1] = q[1]; rgb[x * 3 + 2] = q[0]; }
    size_t n;
    mbedtls_base64_encode(out, sizeof out, &n, rgb, sizeof rgb);
    out[n] = 0;
    printf("%s\n", (char *)out);
    fflush(stdout);
    if ((y & 7) == 7) vTaskDelay(1);
  }
  printf("SHOT_END\n");
}

static void shotTask(void *) {
  static const int tabs[] = {SHOT_TAB_LIST};
  vTaskDelay(pdMS_TO_TICKS(30000));  // let the app finish starting
  if (SHOT_THEME >= 0) {
    uiSetTheme(SHOT_THEME);  // the live switch itself, as the Settings row does it
    vTaskDelay(pdMS_TO_TICKS(2000));
    printf("THEME %d\n", SHOT_THEME);
  }
  for (int tab : tabs) {
    if (tab == 5) continue;  // Settings: never
    uiShowTab(tab);
    // In direct mode with two frame buffers the one we read can be a frame behind: mark the whole
    // screen dirty twice and give the LVGL task time to draw both. (Calling lv_refr_now from this
    // task instead crashed the heap.)
    for (int i = 0; i < 2; i++) {
      vTaskDelay(pdMS_TO_TICKS(1200));
      UiLock l;
      lv_obj_invalidate(lv_screen_active());
    }
    vTaskDelay(pdMS_TO_TICKS(1200));
    printf("TAB %d\n", tab);
    dump();
    vTaskDelay(pdMS_TO_TICKS(300));
  }
  vTaskDelete(nullptr);
}

// Nothing references this file, so the linker would drop it from libmain.a: CMakeLists.txt forces this symbol in.
extern "C" void screenshot_dump_keep(void) {}

// Runs in the main task before app_main, before app_main: no hook needed in main.cpp.
ESP_SYSTEM_INIT_FN(screenshot_dump_start, SECONDARY, BIT(0), 999) {
  xTaskCreate(shotTask, "shotdump", 20480, nullptr, 2, nullptr);
  return ESP_OK;
}
