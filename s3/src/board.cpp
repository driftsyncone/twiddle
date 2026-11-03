// Board-specific parts: pins, panel timings, backlight and touch reset.
// Guition pins/timings from the community ESPHome config for the JC8048W550;
// Waveshare ones from its schematic and ESP-IDF demo (waveshare_rgb_lcd_port.c).
#include "board.h"
#include "settings.h"

#include <Arduino.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>
#include <lvgl.h>

static const int W = SCREEN_W, H = SCREEN_H;
static int rawDuty = -1;  // GET /backlight?duty=N override, for calibration (Guition)
static const int BB_PX = W * 16;

#if defined(BOARD_WAVESHARE_LCD5)
static const int PIN_SDA = 8, PIN_SCL = 9;
static const int PIN_TOUCH_INT = 4;
static const int PIN_HSYNC = 46, PIN_VSYNC = 3, PIN_DE = 5, PIN_PCLK = 7;
// RGB565 bus order D0..D15 = B3..B7, G2..G7, R3..R7.
static const int DATA_PINS[16] = {14, 38, 18, 17, 10, 39, 0, 45, 48, 47, 21, 1, 2, 42, 41, 40};
static const uint32_t PCLK_HZ = 14 * 1000 * 1000;  // Waveshare's demo uses 16 MHz

// CH422G IO expander: 0x24 = mode (0x01: IO0-7 outputs), 0x38 = output levels.
// EXIO1 touch reset, EXIO2 backlight (DISP), EXIO3 LCD reset, EXIO4 SD card CS
// (high = not selected). EXIO0/5 are the board's isolated inputs DI0/DI1.
static const uint8_t EXIO_TP_RST = 1 << 1, EXIO_DISP = 1 << 2, EXIO_LCD_RST = 1 << 3,
                     EXIO_SD_CS = 1 << 4;

static void ch422g(uint8_t addr, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(value);
  Wire.endTransmission();
}

static int level = 100;  // unused here: the CH422G output is on or off

static void backlight(bool on) {
  ch422g(0x38, EXIO_TP_RST | EXIO_LCD_RST | EXIO_SD_CS | (on && level > 0 ? EXIO_DISP : 0));
}

// Out of reset with the GT911's INT pin held low, so it comes up at 0x5D.
static void boardPreInit() {
  Wire.begin(PIN_SDA, PIN_SCL, 400000);
  ch422g(0x24, 0x01);
  ch422g(0x38, EXIO_LCD_RST | EXIO_SD_CS);  // touch in reset, backlight off
  pinMode(PIN_TOUCH_INT, OUTPUT);
  digitalWrite(PIN_TOUCH_INT, LOW);
  delay(100);
  ch422g(0x38, EXIO_TP_RST | EXIO_LCD_RST | EXIO_SD_CS);  // release touch reset
  delay(50);
  pinMode(PIN_TOUCH_INT, INPUT);  // GT911 drives it from now on (unused: we poll)
  delay(150);
}
#else
static const int PIN_BACKLIGHT = 2;
static const int PIN_SDA = 19, PIN_SCL = 20;
static const int PIN_HSYNC = 39, PIN_VSYNC = 41, PIN_DE = 40, PIN_PCLK = 42;
// RGB565 bus order: B0..B4, G0..G5, R0..R4.
static const int DATA_PINS[16] = {8, 3, 46, 9, 1, 5, 6, 7, 15, 16, 4, 45, 48, 47, 21, 14};
// Pixel clock: settings.pclkMHz (NVS "pclk", default 16, read at boot; a saved value is kept). Guition units differ: most
// need 16 (the value ESPHome and the vendor demo use; a unit that needs it shows the panel's RGB self-test at 14), the
// first unit (MAC ...80:c4) needs 14 (16 garbles it under PSRAM load, 12 gives the self-test). Set it with GET /pclk?mhz=N (login), restart.

static int level = 100;  // brightness while on

// 8-bit PWM on the backlight driver's enable pin. The driver only stays on
// above roughly BL_MIN_DUTY (it switches off below, no dimming), so the
// brightness setting maps onto the band above that.
static const int BL_MIN_DUTY = 128;  // measured 2026-10-19: dark below ~45 % at 2 kHz
static void backlight(bool on) {
  int duty = 0;
  if (on) duty = rawDuty >= 0 ? rawDuty : BL_MIN_DUTY + (255 - BL_MIN_DUTY) * level / 100;
  ledcWrite(PIN_BACKLIGHT, duty);
}

static void boardPreInit() {
  ledcAttach(PIN_BACKLIGHT, 1000, 8);
  backlight(false);
  Wire.begin(PIN_SDA, PIN_SCL, 400000);
}
#endif

static esp_lcd_panel_handle_t panel;
static uint8_t gtAddr = 0x5D;
static bool screenOn = true;
static bool flipped;

void boardSetFlip(bool flip) {
  // The RGB driver mirrors both axes while copying each flushed area into the
  // frame buffer: a 180 degree turn for free. LVGL itself keeps drawing upright.
  flipped = flip;
  esp_lcd_panel_mirror(panel, flip, flip);
  lv_obj_invalidate(lv_screen_active());  // everything on screen must be copied again
  lv_obj_invalidate(lv_layer_top());
}

bool boardIsFlipped() { return flipped; }

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

// ---------- bounce-buffer guard ----------
// The RGB driver (IDF 5.5) refills bounce buffer [eof_count % 2] on every DMA EOF.
// If an EOF interrupt is late (flash write, long critical section), two EOFs
// merge into one, the parity flips for good, and the driver keeps refilling the
// buffer that is on screen: shifted picture, flicker, lines, until a reboot.
// CONFIG_LCD_RGB_RESTART_IN_VSYNC restarts the DMA each frame but never resets
// that counter. In a healthy frame the counter is even at VSYNC, so a VSYNC
// callback forces it even; a broken frame heals in one frame.
// The counter is a private field: find it right after the (public) timings in
// the panel object, and only use it if every neighbouring value checks out.

static volatile size_t *eofCount;
static uint32_t parityFixes;  // written in the ISR only, read in loop(): a torn read is harmless

static IRAM_ATTR bool onVsync(esp_lcd_panel_handle_t, const esp_lcd_rgb_panel_event_data_t *, void *) {
  size_t c = *eofCount;
  if (c & 1) {
    *eofCount = c & ~(size_t)1;
    parityFixes++;
  }
  return false;
}

static void bounceGuardBegin() {
  const uint32_t *w = (const uint32_t *)panel;
  const uint32_t want[] = {(uint32_t)W, (uint32_t)H, 4, 8, 8, 4, 8, 8};  // h_res .. vsync_front_porch
  for (int i = 1; i < 160; i++) {
    if (memcmp(w + i, want, sizeof want)) continue;
    const esp_lcd_rgb_timing_t *t = (const esp_lcd_rgb_timing_t *)(w + i - 1);
    const int *pos = (const int *)(t + 1);  // bounce_pos_px
    bool ok = t->pclk_hz > 10000000 && t->pclk_hz < 20000000 && *pos >= 0 &&
              *pos % BB_PX == 0 && *pos <= W * H;
    if (!ok) break;
    eofCount = (volatile size_t *)(pos + 1);  // bb_eof_count
    esp_lcd_rgb_panel_event_callbacks_t cbs = {};
    cbs.on_vsync = onVsync;
    esp_lcd_rgb_panel_register_event_callbacks(panel, &cbs, nullptr);
    Serial.printf("display: bounce guard on (field at +%d)\n", (int)((uint8_t *)eofCount - (uint8_t *)panel));
    return;
  }
  Serial.println("display: bounce guard OFF (driver layout not recognised)");
}

int boardDisplayGuard() { return eofCount ? (int)parityFixes : -1; }

void boardDisplayResync() {
  // Re-runs the RGB controller setup and restarts DMA from the frame start.
  // A flash write stalls the bounce-buffer refill; sometimes the picture then
  // stays shifted/flickering, which the driver's per-frame restart doesn't fix.
  esp_lcd_panel_init(panel);
}

void boardBacklight(bool on) {
  if (screenOn) backlight(on);
}

const uint16_t *boardFrameBuffer() {
  void *fb = nullptr;
  esp_lcd_rgb_panel_get_frame_buffer(panel, 1, &fb);
  return (const uint16_t *)fb;
}

// ---------- display ----------

#ifdef UI_TRACE
uint32_t traceFlushUs, traceFlushes, traceFlushPx;  // read and cleared by main.cpp's trace line
#endif
static void flushCb(lv_display_t *disp, const lv_area_t *a, uint8_t *px) {
#ifdef UI_TRACE
  uint32_t t0 = micros();
#endif
  esp_lcd_panel_draw_bitmap(panel, a->x1, a->y1, a->x2 + 1, a->y2 + 1, px);
#ifdef UI_TRACE
  traceFlushUs += micros() - t0; traceFlushes++; traceFlushPx += (a->x2 - a->x1 + 1) * (a->y2 - a->y1 + 1);
#endif
  lv_display_flush_ready(disp);
}

static void panelBegin() {
  esp_lcd_rgb_panel_config_t cfg = {};
  cfg.clk_src = LCD_CLK_SRC_DEFAULT;
#if defined(BOARD_WAVESHARE_LCD5)
  cfg.timings.pclk_hz = PCLK_HZ;
#else
  cfg.timings.pclk_hz = (uint32_t)settings.pclkMHz * 1000 * 1000;
  Serial.printf("display: pixel clock %d MHz\n", settings.pclkMHz);
#endif
  cfg.timings.h_res = W;
  cfg.timings.v_res = H;
  cfg.timings.hsync_pulse_width = 4;
  cfg.timings.hsync_back_porch = 8;
  cfg.timings.hsync_front_porch = 8;
  cfg.timings.vsync_pulse_width = 4;
  cfg.timings.vsync_back_porch = 8;
  cfg.timings.vsync_front_porch = 8;
  cfg.timings.flags.pclk_active_neg = 1;
  cfg.data_width = 16;
  cfg.bits_per_pixel = 16;
  cfg.num_fbs = 1;
  // Bounce buffer keeps the picture steady while Wi-Fi competes for PSRAM bandwidth
  // (without one: constant small flickers). See bounceGuardBegin() for its catch.
  cfg.bounce_buffer_size_px = BB_PX;
  cfg.hsync_gpio_num = PIN_HSYNC;
  cfg.vsync_gpio_num = PIN_VSYNC;
  cfg.de_gpio_num = PIN_DE;
  cfg.pclk_gpio_num = PIN_PCLK;
  cfg.disp_gpio_num = -1;
  for (int i = 0; i < 16; i++) cfg.data_gpio_nums[i] = DATA_PINS[i];
  cfg.flags.fb_in_psram = 1;

  ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&cfg, &panel));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
}

// ---------- touch (GT911) ----------

static bool gtRead(uint16_t reg, uint8_t *buf, size_t len) {
  Wire.beginTransmission(gtAddr);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(gtAddr, (uint8_t)len) != len) return false;
  for (size_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

static void gtWrite(uint16_t reg, uint8_t val) {
  Wire.beginTransmission(gtAddr);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  Wire.write(val);
  Wire.endTransmission();
}

static void touchBegin() {
  for (uint8_t addr : {0x5D, 0x14}) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      gtAddr = addr;
      Serial.printf("GT911 at 0x%02X\n", addr);
      return;
    }
  }
  Serial.println("GT911 not found");
}

static void touchCb(lv_indev_t *, lv_indev_data_t *data) {
  // GT911 sets bit 7 of the status register when a new sample is ready.
  // Between samples, keep reporting the last state.
  static int16_t lastX, lastY;
  static bool pressed;
  static bool swallow;  // the touch that woke the screen, until the finger lifts
  uint8_t st;
#ifdef UI_TRACE
  static uint32_t tn, tsum, tmax, tlast;
  uint32_t t0 = micros();
  bool got = gtRead(0x814E, &st, 1);
  uint32_t d = micros() - t0;
  tn++; tsum += d; if (d > tmax) tmax = d;
  if (millis() - tlast >= 2000) {
    Serial.printf("uitrace: touch read x%u avg %.2f ms max %.2f ms\n", (unsigned)tn, tsum / 1000.0f / tn, tmax / 1000.0f);
    tn = tsum = tmax = 0; tlast = millis();
  }
  if (got && (st & 0x80)) {
#else
  if (gtRead(0x814E, &st, 1) && (st & 0x80)) {
#endif
    pressed = false;
    uint8_t p[8];
    if ((st & 0x0F) > 0 && gtRead(0x814F, p, 8)) {
      lastX = p[1] | (p[2] << 8);
      lastY = p[3] | (p[4] << 8);
      pressed = true;
    }
    gtWrite(0x814E, 0);
  }
  if (pressed && !screenOn) {
    screenOn = swallow = true;
    backlight(true);
    lv_display_trigger_activity(nullptr);
  }
  if (!pressed) swallow = false;
  data->state = pressed && !swallow ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  data->point.x = flipped ? W - 1 - lastX : lastX;
  data->point.y = flipped ? H - 1 - lastY : lastY;
}

// ---------- LVGL ----------

void boardBegin() {
  Serial.println("board: " BOARD_NAME);
  boardPreInit();  // I2C up, backlight off, touch out of reset
  panelBegin();
  bounceGuardBegin();
  touchBegin();

  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return millis(); });

  lv_display_t *disp = lv_display_create(W, H);
  // One draw buffer in internal RAM (fast). A second one buys nothing: flushCb
  // copies into the frame buffer and returns, so LVGL never waits on a flush.
  // The 32 KB it would take is what Bluetooth needs next to Wi-Fi.
  const size_t bufBytes = W * 20 * 2;
  void *b1 = heap_caps_malloc(bufBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  lv_display_set_buffers(disp, b1, nullptr, bufBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, flushCb);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchCb);

  backlight(true);
}
