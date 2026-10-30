// LVGL heap in PSRAM (LV_STDLIB_CUSTOM), so widgets don't eat the internal RAM
// that mbedTLS needs. Draw buffers are allocated separately in board.cpp.
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <stdlib.h>

#define CAPS (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)

void lv_mem_init(void) {}
void lv_mem_deinit(void) {}

lv_mem_pool_t lv_mem_add_pool(void *mem, size_t bytes) {
  LV_UNUSED(mem);
  LV_UNUSED(bytes);
  return NULL;
}

void lv_mem_remove_pool(lv_mem_pool_t pool) { LV_UNUSED(pool); }

void *lv_malloc_core(size_t size) {
  void *p = heap_caps_malloc(size, CAPS);
  return p ? p : malloc(size);
}

void *lv_realloc_core(void *p, size_t new_size) {
  void *q = heap_caps_realloc(p, new_size, CAPS);
  return q ? q : realloc(p, new_size);
}

void lv_free_core(void *p) { free(p); }

void lv_mem_monitor_core(lv_mem_monitor_t *mon_p) { LV_UNUSED(mon_p); }

lv_result_t lv_mem_test_core(void) { return LV_RESULT_OK; }
