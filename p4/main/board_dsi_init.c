#include <string.h>
// JD9165 init sequence for the Guition JC1060P470 V2 panel (C file: compound literals). The register values
// come from the ESPHome Guition panel definitions (see THIRD_PARTY_NOTICES.md); the code is ours.
#include "esp_lcd_jd9165.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_ldo_regulator.h"
#include "driver/i2c_master.h"
#include "board_dsi.h"

// Guition JC1060P470 V2 (2026 panel) init sequence, from ESPHome's mipi_dsi JC1060P470-V2 model.
#define INIT(cmd, ...) { cmd, (uint8_t[]){ __VA_ARGS__ }, sizeof((uint8_t[]){ __VA_ARGS__ }), 0 }
const jd9165_lcd_init_cmd_t board_jd9165_init_cmds[] = {
    INIT(0x30, 0x00),
    INIT(0xF7, 0x49, 0x61, 0x02, 0x00),
    INIT(0x30, 0x01),
    INIT(0x04, 0x0C),
    INIT(0x05, 0x08),
    INIT(0x0B, 0x11),
    INIT(0x20, 0x04),
    INIT(0x1F, 0x05),
    INIT(0x23, 0x38),
    INIT(0x28, 0x18),
    INIT(0x29, 0x29),
    INIT(0x2A, 0x01),
    INIT(0x2B, 0x29),
    INIT(0x2C, 0x01),
    INIT(0x30, 0x02),
    INIT(0x00, 0x05),
    INIT(0x01, 0x22),
    INIT(0x02, 0x08),
    INIT(0x03, 0x12),
    INIT(0x04, 0x16),
    INIT(0x05, 0x64),
    INIT(0x06, 0x00),
    INIT(0x07, 0x00),
    INIT(0x08, 0x78),
    INIT(0x09, 0x00),
    INIT(0x0A, 0x04),
    INIT(0x0B, 0x16, 0x17, 0x0B, 0x0D, 0x0D, 0x0D, 0x11, 0x10, 0x07, 0x07, 0x09),
    INIT(0x0C, 0x09, 0x1E, 0x1E, 0x1C, 0x1C, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D),
    INIT(0x0D, 0x0A, 0x05, 0x0B, 0x0D, 0x0D, 0x0D, 0x11, 0x10, 0x06, 0x06, 0x08),
    INIT(0x0E, 0x08, 0x1F, 0x1F, 0x1D, 0x1D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D),
    INIT(0x0F, 0x0A, 0x05, 0x0D, 0x0B, 0x0D, 0x0D, 0x11, 0x10, 0x1D, 0x1D, 0x1F),
    INIT(0x10, 0x1F, 0x08, 0x08, 0x06, 0x06, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D),
    INIT(0x11, 0x16, 0x17, 0x0D, 0x0B, 0x0D, 0x0D, 0x11, 0x10, 0x1C, 0x1C, 0x1E),
    INIT(0x12, 0x1E, 0x09, 0x09, 0x07, 0x07, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D),
    INIT(0x13, 0x00, 0x00, 0x00, 0x00),
    INIT(0x14, 0x00, 0x00, 0x41, 0x41),
    INIT(0x15, 0x00, 0x00, 0x00, 0x00),
    INIT(0x17, 0x00),
    INIT(0x18, 0x85),
    INIT(0x19, 0x06, 0x09),
    INIT(0x1A, 0x05, 0x08),
    INIT(0x1B, 0x0A, 0x04),
    INIT(0x26, 0x00),
    INIT(0x27, 0x00),
    INIT(0x30, 0x06),
    INIT(0x12, 0x3F, 0x25, 0x27, 0x35, 0x1D, 0x1B, 0x1B, 0x1A, 0x18, 0x0A, 0x2A, 0x21, 0x19, 0x30),
    INIT(0x13, 0x3F, 0x26, 0x27, 0x35, 0x1E, 0x1C, 0x1C, 0x1A, 0x18, 0x0B, 0x2A, 0x21, 0x19, 0x30),
    INIT(0x30, 0x0A),
    INIT(0x02, 0x4F),
    INIT(0x0B, 0x40),
    INIT(0x30, 0x0D),
    INIT(0x0D, 0x04),
    INIT(0x10, 0x05),
    INIT(0x11, 0x0C),
    INIT(0x12, 0x05),
    INIT(0x13, 0x0C),
    INIT(0x30, 0x00),
    { 0x11, (uint8_t[]){0x00}, 1, 120 },
    { 0x29, (uint8_t[]){0x00}, 1, 20 },
};


const jd9165_lcd_init_cmd_t board_jd9165_init_cmds_orig[] = {
    INIT(0x30, 0x00),
    INIT(0xF7, 0x49, 0x61, 0x02, 0x00),
    INIT(0x30, 0x01),
    INIT(0x04, 0x0C),
    INIT(0x05, 0x00),
    INIT(0x06, 0x00),
    INIT(0x0B, 0x11),
    INIT(0x17, 0x00),
    INIT(0x20, 0x04),
    INIT(0x1F, 0x05),
    INIT(0x23, 0x00),
    INIT(0x25, 0x19),
    INIT(0x28, 0x18),
    INIT(0x29, 0x04),
    INIT(0x2A, 0x01),
    INIT(0x2B, 0x04),
    INIT(0x2C, 0x01),
    INIT(0x30, 0x02),
    INIT(0x01, 0x22),
    INIT(0x03, 0x12),
    INIT(0x04, 0x00),
    INIT(0x05, 0x64),
    INIT(0x0A, 0x08),
    INIT(0x0B, 0x0A, 0x1A, 0x0B, 0x0D, 0x0D, 0x11, 0x10, 0x06, 0x08, 0x1F, 0x1D),
    INIT(0x0C, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D),
    INIT(0x0D, 0x16, 0x1B, 0x0B, 0x0D, 0x0D, 0x11, 0x10, 0x07, 0x09, 0x1E, 0x1C),
    INIT(0x0E, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D),
    INIT(0x0F, 0x16, 0x1B, 0x0D, 0x0B, 0x0D, 0x11, 0x10, 0x1C, 0x1E, 0x09, 0x07),
    INIT(0x10, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D),
    INIT(0x11, 0x0A, 0x1A, 0x0D, 0x0B, 0x0D, 0x11, 0x10, 0x1D, 0x1F, 0x08, 0x06),
    INIT(0x12, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D),
    INIT(0x14, 0x00, 0x00, 0x11, 0x11),
    INIT(0x18, 0x99),
    INIT(0x30, 0x06),
    INIT(0x12, 0x36, 0x2C, 0x2E, 0x3C, 0x38, 0x35, 0x35, 0x32, 0x2E, 0x1D, 0x2B, 0x21, 0x16, 0x29),
    INIT(0x13, 0x36, 0x2C, 0x2E, 0x3C, 0x38, 0x35, 0x35, 0x32, 0x2E, 0x1D, 0x2B, 0x21, 0x16, 0x29),
    INIT(0x30, 0x0A),
    INIT(0x02, 0x4F),
    INIT(0x0B, 0x40),
    INIT(0x12, 0x3E),
    INIT(0x13, 0x78),
    INIT(0x30, 0x0D),
    INIT(0x0D, 0x04),
    INIT(0x10, 0x0C),
    INIT(0x11, 0x0C),
    INIT(0x12, 0x0C),
    INIT(0x13, 0x0C),
    INIT(0x30, 0x00),
    { 0x11, (uint8_t[]){0x00}, 1, 120 },
    { 0x29, (uint8_t[]){0x00}, 1, 20 },
};
const unsigned board_jd9165_init_cmds_orig_len = sizeof(board_jd9165_init_cmds_orig) / sizeof(board_jd9165_init_cmds_orig[0]);

const unsigned board_jd9165_init_cmds_len = sizeof(board_jd9165_init_cmds) / sizeof(board_jd9165_init_cmds[0]);

// Panel timings and pins from the original bring-up test (see docs/history.md).
// variant 4 = "_Y" new panel: V2 table and timings, reset GPIO5, 16-bit (RGB565) colour.
// variant 0 = V2 panel (2026 batch: reset GPIO0, 52 MHz, V2 table), 1 = original panel (reset GPIO5, 54 MHz, original table).
void board_dsi_panel(int variant, int pclk_mhz, esp_lcd_panel_io_handle_t *io_out, esp_lcd_panel_handle_t *panel_out)
{
    esp_ldo_channel_handle_t phy = NULL;
    esp_ldo_channel_config_t ldo = { .chan_id = 3, .voltage_mv = 2500 };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo, &phy));

    esp_lcd_dsi_bus_handle_t bus;
    esp_lcd_dsi_bus_config_t bus_cfg = JD9165_PANEL_BUS_DSI_2CH_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_cfg, &bus));
    esp_lcd_dbi_io_config_t io_cfg = JD9165_PANEL_IO_DBI_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(bus, &io_cfg, io_out));

    esp_lcd_dpi_panel_config_t dpi = JD9165_1024_600_PANEL_60HZ_DPI_CONFIG_CF(variant == 4 ? LCD_COLOR_FMT_RGB565 : LCD_COLOR_FMT_RGB888);
    dpi.num_fbs = 2;
    if (variant & 1) {  // original panel
        dpi.dpi_clock_freq_mhz = pclk_mhz ? pclk_mhz : 54;
        dpi.video_timing.hsync_back_porch = 160;
        dpi.video_timing.hsync_pulse_width = 40;
        dpi.video_timing.hsync_front_porch = 160;
        dpi.video_timing.vsync_back_porch = 23;
        dpi.video_timing.vsync_pulse_width = 10;
        dpi.video_timing.vsync_front_porch = 12;
    } else {  // V2
        dpi.dpi_clock_freq_mhz = pclk_mhz ? pclk_mhz : variant == 4 ? 48 : 52;  // 48: the "_Y" panel shakes at 52
        dpi.video_timing.hsync_back_porch = 136;
        dpi.video_timing.hsync_pulse_width = 24;
        dpi.video_timing.hsync_front_porch = 160;
        dpi.video_timing.vsync_back_porch = 21;
        dpi.video_timing.vsync_pulse_width = 2;
        dpi.video_timing.vsync_front_porch = 12;
    }
    // The driver never sets the panel's colour mode: the init table does. The "_Y" panel is driven in 16-bit (RGB565),
    // so its table gets COLMOD 0x3A = 0x55 inserted (Guition's demo for that panel does the same); the others keep the default 24-bit.
    static jd9165_lcd_init_cmd_t cmds565[sizeof(board_jd9165_init_cmds) / sizeof(board_jd9165_init_cmds[0]) + 1];
    if (variant == 4) {
        // COLMOD goes before the table's last two entries (sleep out 0x11, display on 0x29), as in the demo.
        unsigned n = board_jd9165_init_cmds_len;
        memcpy(cmds565, board_jd9165_init_cmds, (n - 2) * sizeof cmds565[0]);
        cmds565[n - 2] = (jd9165_lcd_init_cmd_t){ 0x3A, (uint8_t[]){0x55}, 1, 0 };
        memcpy(&cmds565[n - 1], &board_jd9165_init_cmds[n - 2], 2 * sizeof cmds565[0]);
    }
    jd9165_vendor_config_t vendor = { .init_cmds = (variant & 1) ? board_jd9165_init_cmds_orig : variant == 4 ? cmds565 : board_jd9165_init_cmds,
                                      .init_cmds_size = (variant & 1) ? board_jd9165_init_cmds_orig_len : variant == 4 ? board_jd9165_init_cmds_len + 1 : board_jd9165_init_cmds_len, .mipi_config = { .dsi_bus = bus, .dpi_config = &dpi } };
    esp_lcd_panel_dev_config_t dev = {
        .reset_gpio_num = variant == 4 ? 5 : ((variant & 1) ^ ((variant >> 1) & 1)) ? 5 : 0,  // LCD_RST: GPIO5 on the original panel, GPIO0 on the V2
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = variant == 4 ? 16 : 24,
        .vendor_config = &vendor,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_jd9165(*io_out, &dev, panel_out));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(*panel_out));
    ESP_ERROR_CHECK(esp_lcd_panel_init(*panel_out));
}

void board_dsi_touch(i2c_master_bus_handle_t i2c, int w, int h, esp_lcd_touch_handle_t *touch_out)
{
    esp_lcd_panel_io_handle_t tio;
    esp_lcd_panel_io_i2c_config_t tio_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c, &tio_cfg, &tio));
    static esp_lcd_touch_io_gt911_config_t gt;  // must outlive the call: without driver_data the reset sequence is skipped
    gt.dev_addr = tio_cfg.dev_addr;
    esp_lcd_touch_config_t tcfg = {
        .x_max = w, .y_max = h, .rst_gpio_num = 22, .int_gpio_num = 21,
        .levels = { .reset = 0, .interrupt = 0 }, .driver_data = &gt,
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tio, &tcfg, touch_out));
}
