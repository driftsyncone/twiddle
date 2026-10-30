#include "rollback.h"

#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>

#include "control.h"
#include "wlan.h"

// Arduino confirms a new image in initArduino(), before setup() runs, so an update
// that boots and then misbehaves would stick. Tell it we confirm it ourselves.
extern "C" bool verifyRollbackLater() { return true; }

namespace rollback {

static const uint32_t HEALTHY_MS = 30000;     // Wi-Fi up this long, loop still running
static const uint64_t DEADLINE_US = 180000000; // otherwise roll back after 3 min

static bool pending;
static bool wifiUp;         // Wi-Fi seen up (continuously) since wifiSince
static uint32_t wifiSince;
static esp_timer_handle_t deadline;

static volatile bool expired;  // deadline passed: loop() sends STOP, then we reboot

// Runs in the esp_timer task, so it fires even if loop() is stuck. It must not
// touch control:: (LVGL thread only), so it only flags loop() and rearms itself
// for the reboot 3 s later; a stuck loop() just skips the STOP.
static void onDeadline(void *) {
  if (!pending) return;
  if (!expired) {
    Serial.println("rollback: new firmware not confirmed in time, going back to the previous one");
    expired = true;
    esp_timer_start_once(deadline, 3000000);
    return;
  }
  esp_ota_mark_app_invalid_rollback_and_reboot();
}

void begin() {
  esp_ota_img_states_t st;
  const esp_partition_t *running = esp_ota_get_running_partition();
  pending = esp_ota_get_state_partition(running, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY;
  Serial.printf("rollback: running %s, state %s\n", running->label, state());
  if (!pending) return;
  Serial.printf("rollback: running a new update from %s, on probation\n", running->label);
  esp_timer_create_args_t args = {};
  args.callback = onDeadline;
  args.name = "ota-deadline";
  esp_timer_create(&args, &deadline);
  esp_timer_start_once(deadline, DEADLINE_US);
}

void loop() {
  if (!pending) return;
  if (expired) {
    static bool stopped;
    if (!stopped) {
      stopped = true;
      control::stopAll();
    }
    return;
  }
  if (!wlan::info().up) {
    wifiUp = false;
    return;
  }
  if (!wifiUp) {
    wifiUp = true;
    wifiSince = millis();
  }
  if (millis() - wifiSince < HEALTHY_MS) return;
#ifdef TEST_ROLLBACK_NEVER_CONFIRM  // test builds only: let the deadline fire
  return;
#endif
  esp_ota_mark_app_valid_cancel_rollback();
  esp_timer_stop(deadline);
  pending = false;
  Serial.println("rollback: new firmware confirmed");
}

const char *state() {
  esp_ota_img_states_t st;
  if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &st) != ESP_OK) return "factory";
  switch (st) {
    case ESP_OTA_IMG_VALID: return "confirmed";
    case ESP_OTA_IMG_PENDING_VERIFY: return "pending";
    case ESP_OTA_IMG_NEW: return "new";
    case ESP_OTA_IMG_INVALID: return "invalid";
    case ESP_OTA_IMG_ABORTED: return "aborted";
    default: return "undefined";
  }
}

}  // namespace rollback
