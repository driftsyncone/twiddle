// Guition JC1060P470C: MIPI-DSI panel (JD9165, V2 batch), GT911 touch, PWM backlight.
// Display and touch go through esp_lvgl_port, which runs LVGL on its own task;
// pins and timings come from the original bring-up test (see docs/history.md).
#include "board.h"
#include "settings.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lvgl_port.h>
#include <lvgl.h>

#include "board_dsi.h"
#include "log.h"


static const int PIN_BL = 23, PIN_SDA = 7, PIN_SCL = 8;

static i2c_master_bus_handle_t i2cBus;
static lv_display_t *disp;
static lv_indev_t *indev;
static int level = 100;       // brightness while on, 0..100
static int rawDuty = -1;      // calibration override
static bool screenOn = true;
static bool flipped;
static esp_lcd_panel_handle_t lcdPanel;
static esp_lcd_panel_io_handle_t lcdIo;

i2c_master_bus_handle_t boardI2cBus() { return i2cBus; }

// ---------- backlight (LEDC, 8 bit) ----------

static void backlight(bool on) {
  int duty = 0;
  if (on) duty = rawDuty >= 0 ? rawDuty : 255 * level / 100;
  ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void backlightInit() {
  ledc_timer_config_t t = {};
  t.speed_mode = LEDC_LOW_SPEED_MODE;
  t.duty_resolution = LEDC_TIMER_8_BIT;
  t.timer_num = LEDC_TIMER_0;
  t.freq_hz = 2000;
  t.clk_cfg = LEDC_AUTO_CLK;
  ledc_timer_config(&t);
  ledc_channel_config_t c = {};
  c.gpio_num = PIN_BL;
  c.speed_mode = LEDC_LOW_SPEED_MODE;
  c.channel = LEDC_CHANNEL_0;
  c.timer_sel = LEDC_TIMER_0;
  c.duty = 0;
  ledc_channel_config(&c);
}

// Flip = the JD9165 scans both axes the other way (MADCTL GS+SS through esp_lcd_panel_mirror), so the
// frame buffer, LVGL and /screenshot stay unflipped and cost nothing; only the touch is mirrored here.
bool boardIsFlipped() { return flipped; }

void boardSetFlip(bool flip) {
  flipped = flip;
  if (!lcdPanel) return;
  esp_err_t e = esp_lcd_panel_mirror(lcdPanel, flip, flip);
  plog("board: flip %d: mirror=%s\n", flip, esp_err_to_name(e));
}

void boardSetBrightness(int pct) {
  level = constrain(pct, 0, 100);
  rawDuty = -1;
  if (screenOn) backlight(true);
}

void boardBacklightRaw(int duty) {
  rawDuty = duty;
  if (screenOn) backlight(true);
}

void boardScreenOff() {
  screenOn = false;
  backlight(false);
}

bool boardScreenIsOn() { return screenOn; }

const uint8_t *boardFrameBufferRgb888() {
  if (!disp) return nullptr;
  lv_draw_buf_t *b = lv_display_get_buf_active(disp);
  return b ? (const uint8_t *)b->data : nullptr;
}

bool boardIs565() { return settings.panelVariant == 4; }

int boardDisplayGuard() { return -1; }
void boardDisplayResync() {}

// ---------- touch: wake handling ----------
// The touch that wakes a sleeping screen must not press anything: it is held
// back until the finger lifts.

static void (*touchRead)(lv_indev_t *, lv_indev_data_t *);

static void touchCb(lv_indev_t *dev, lv_indev_data_t *data) {
  static bool swallow;
  touchRead(dev, data);
  if (flipped) {  // the panel is scanned upside down: the finger moves with it
    data->point.x = SCREEN_W - 1 - data->point.x;
    data->point.y = SCREEN_H - 1 - data->point.y;
  }
  bool pressed = data->state == LV_INDEV_STATE_PRESSED;
  if (pressed && !screenOn) {
    screenOn = swallow = true;
    backlight(true);
    lv_display_trigger_activity(nullptr);
  }
  if (!pressed) swallow = false;
  if (swallow) data->state = LV_INDEV_STATE_RELEASED;
}

// ---------- bring-up ----------

void boardBegin() {
  Serial.println("board: " BOARD_NAME);
  backlightInit();
  backlight(false);

  esp_lcd_panel_io_handle_t io;
  esp_lcd_panel_handle_t panel;
  plog("board: display panel variant %s\n", settings.panelVariant == 0 ? "V2" : settings.panelVariant == 1 ? "original" : settings.panelVariant == 2 ? "V2 table, reset 5" : settings.panelVariant == 4 ? "_Y new panel (RGB565)" : "original table, reset 0");
  board_dsi_panel(settings.panelVariant, settings.pclkMhz, &io, &panel);
  lcdIo = io;
  lcdPanel = panel;

  i2c_master_bus_config_t i2c = {};
  i2c.i2c_port = I2C_NUM_0;
  i2c.sda_io_num = (gpio_num_t)PIN_SDA;
  i2c.scl_io_num = (gpio_num_t)PIN_SCL;
  i2c.clk_source = I2C_CLK_SRC_DEFAULT;
  i2c.glitch_ignore_cnt = 7;
  i2c.flags.enable_internal_pullup = true;
  ESP_ERROR_CHECK(i2c_new_master_bus(&i2c, &i2cBus));
  esp_lcd_touch_handle_t touch;
  board_dsi_touch(i2cBus, SCREEN_W, SCREEN_H, &touch);

  // LVGL on its own task, on core 1 where the Arduino version ran everything.
  lvgl_port_cfg_t lcfg = ESP_LVGL_PORT_INIT_CONFIG();
  lcfg.task_stack = 12288;
  lcfg.task_affinity = 1;
  ESP_ERROR_CHECK(lvgl_port_init(&lcfg));
  lvgl_port_display_cfg_t dcfg = {};
  dcfg.io_handle = io;
  dcfg.panel_handle = panel;
  dcfg.buffer_size = SCREEN_W * 60;
  dcfg.double_buffer = true;
  dcfg.hres = SCREEN_W;
  dcfg.vres = SCREEN_H;
  dcfg.color_format = settings.panelVariant == 4 ? LV_COLOR_FORMAT_RGB565 : LV_COLOR_FORMAT_RGB888;
  dcfg.flags.buff_spiram = true;
  dcfg.flags.direct_mode = true;
  lvgl_port_display_dsi_cfg_t dsi = {};
  dsi.flags.avoid_tearing = true;
  disp = lvgl_port_add_disp_dsi(&dcfg, &dsi);
  lvgl_port_touch_cfg_t pcfg = {};
  pcfg.disp = disp;
  pcfg.handle = touch;
  indev = lvgl_port_add_touch(&pcfg);

  lvgl_port_lock(0);
  touchRead = lv_indev_get_read_cb(indev);
  lv_indev_set_read_cb(indev, touchCb);
  lvgl_port_unlock();

  backlight(true);
  plog("board: display and touch up (%dx%d)\n", SCREEN_W, SCREEN_H);
}
