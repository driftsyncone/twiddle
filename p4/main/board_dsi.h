// C side of the display bring-up (compound literals and designated initialisers
// from the vendor macros don't compile as C++).
#pragma once
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif
// variant: see settings.panelVariant (0 V2, 1 original, 2/3 reset-pin swaps, 4 "_Y" panel); pclk_mhz 0 = the variant default
void board_dsi_panel(int variant, int pclk_mhz, esp_lcd_panel_io_handle_t *io_out, esp_lcd_panel_handle_t *panel_out);
void board_dsi_touch(i2c_master_bus_handle_t i2c, int w, int h, esp_lcd_touch_handle_t *touch_out);
#ifdef __cplusplus
}
#endif
