// Internal RAM is the tight resource: the RGB display's bounce buffers and the
// LVGL draw buffers need it, and so do the Wi-Fi driver, the Bluetooth
// controller and every TLS session. Things that don't need it go to PSRAM.
#include "mem.h"

#include <Arduino.h>
#include <esp32-hal-alloc-ble-mem.h>  // else initArduino() frees the Bluetooth controller's memory at boot
#include <esp_heap_caps.h>
#include <mbedtls/platform.h>

#include "log.h"

static void *psramCalloc(size_t n, size_t size) {
  void *p = heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

// NimBLE host pools: the precompiled sdkconfig forces them into internal RAM,
// so the allocator is swapped at link time (-Wl,--wrap in platformio.ini).
extern "C" void *__wrap_nimble_platform_mem_malloc(size_t n) {
  void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : heap_caps_malloc(n, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
extern "C" void *__wrap_nimble_platform_mem_calloc(size_t n, size_t size) { return psramCalloc(n, size); }

void memBegin() {
  // TLS record buffers (2 x 16 KB per session) and handshakes: PSRAM. Same as
  // CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC, which the precompiled libs don't set.
  mbedtls_platform_set_calloc_free(psramCalloc, heap_caps_free);
}

void memReport(const char *when) {
  plog("mem %s: internal %u free, largest %u; PSRAM %u free\n", when,
       heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
       heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
       heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}
