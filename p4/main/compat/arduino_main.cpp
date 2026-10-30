// Arduino-style entry: setup() once, then loop() forever, on one task (the "loop task" the
// panel's UI and control code assume). The ported main.cpp keeps its original shape.
#include "arduino_compat.h"
#include "nvs_flash.h"

void setup();
void loop();

static void loopTask(void *) {
  setup();
  for (;;) loop();
}

extern "C" void app_main(void) {
  esp_err_t e = nvs_flash_init();
  if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) { nvs_flash_erase(); nvs_flash_init(); }
  xTaskCreatePinnedToCore(loopTask, "loopTask", 16384, nullptr, 1, nullptr, 1);
}
