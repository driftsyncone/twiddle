// Memory placement: internal RAM is the tight resource (Wi-Fi, Bluetooth, TLS
// sessions). TLS buffers go to PSRAM through CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC and
// the LVGL heap through lv_mem_psram.c (both set in sdkconfig.defaults).
#include "mem.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

#include "log.h"

void memBegin() {}

void memReport(const char *when) {
  plog("mem %s: internal %u free, largest %u; PSRAM %u free\n", when,
       (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
       (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
       (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}
