# Every .c/.cpp under main/ is built; port a module by adding its file, no list to edit.
file(GLOB_RECURSE SRCS "*.c" "*.cpp")
idf_component_register(SRCS ${SRCS}
                    INCLUDE_DIRS "." "compat"
                    REQUIRES bt esp_hosted esp_lcd esp_driver_gpio esp_driver_i2c esp_driver_ledc esp_mm esp_psram
                             esp_wifi esp_event esp_netif nvs_flash json esp_http_client esp_http_server esp-tls
                             mbedtls app_update esp_timer esp_adc mdns)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=unused-function -Wno-unused-parameter)
# esp-nimble-cpp would pull in Arduino String (WString.h) because our shim has <Arduino.h>; we use std::string.
target_compile_definitions(${COMPONENT_LIB} PRIVATE NIMBLE_CPP_ARDUINO_STRING_AVAILABLE=0)
# lv_mem_psram.c provides LVGL's allocator (CONFIG_LV_USE_CUSTOM_MALLOC); force it out of libmain.a.
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,-u,lv_malloc_core" "-Wl,-u,lv_realloc_core" "-Wl,-u,lv_free_core"
                      "-Wl,-u,lv_mem_init" "-Wl,-u,lv_mem_deinit" "-Wl,-u,lv_mem_add_pool" "-Wl,-u,lv_mem_remove_pool"
                      "-Wl,-u,lv_mem_monitor_core" "-Wl,-u,lv_mem_test_core")
# Debug screenshots (tools/screenshot/capture.sh copies the dump file in): nothing references it, so force it into the link.
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/zz_screenshot_dump.cpp")
  target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,-u,screenshot_dump_keep")
endif()
