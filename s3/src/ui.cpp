#include "ui.h"

#include <Arduino.h>
#include <WiFi.h>
#include <lvgl.h>
#include <lvgl_private.h>

#include "autoblow.h"
#include "battery.h"
#include "ble.h"
#include "board.h"
#include "control.h"
#include "devices.h"
#include "apwindow.h"
#include "devrow.h"
#include "encoder.h"
#include "hrm.h"
#include "otapw.h"
#include "pclkfb.h"
#include "settings.h"
#include "theme.h"
#include "version.h"
#include "wlan.h"

using control::Active;

// Colours are theme roles (theme.h): nothing here names a colour value.
using theme::Prop;
using theme::Role;
static lv_color_t themed(Role r) { return theme::color(r); }

static lv_obj_t *statusDot, *statusLabel, *speedLabel, *wifiLabel, *lostBanner;
static lv_obj_t *battLabel, *battBanner;
static lv_obj_t *tabview;
static lv_obj_t *rSlider, *rSliderLabel, *rStartBtn;
static lv_obj_t *apStartBtn, *apStateLabel, *apChart, *apFavBtn[Settings::FAVOURITES], *apDrawBtn;
static lv_chart_series_t *apPast, *apFuture, *apHrSeries;
static lv_obj_t *hrLiveLabel;  // Session tab: the live bpm
static lv_chart_cursor_t *apCursor;
static lv_obj_t *apLblL, *apLblNow, *apLblR, *apLblNote;  // axis labels and the HR note, inside the chart
static int64_t apWinNow;  // session time (ms) at the window's "now" point (the Autopilot graph window)
static bool apWinWhole;  // Draw is on: the chart shows the whole session instead of the window
static const int AP_POINTS = 96;
static const int ST_ROW = 48;  // Session/Settings rows: six have to fit above the fold
static bool apDraw;  // Session tab's Draw: dragging on the graph sets a wave's peak
static lv_obj_t *scButtons[SCRIPT_SLOTS], *scToggleBtn;
static lv_obj_t *stSsid, *stPass, *stToken, *stWebPw, *stScanBtn, *stSaveBtn, *stOffBtn, *stInfo;
static lv_obj_t *dvScanBtn, *dvList;
static const int DV_MAX = 10;  // devices shown; more than that won't be in one room
static lv_obj_t *rWhatLabel;  // under the Remote slider: what it drives on the selected device
// Looser/Tighter and vacuum rows (and the Scripts tab) only work on a VacuGlide:
// shown only while the selected device has valves.
static lv_obj_t *valveRows[4];
static int valveRowCount;

// The selected device: the one whose controls are shown (all devices with
// Follow on still get the output). Kept by address, so a scan doesn't move it;
// RAM only. See selectedIndex().
static char selAddr[18];
static bool selPicked;

static void refresh(lv_timer_t *);

// ---------- helpers ----------

// The Autoblow device (any type: VacuGlide, VacuPump, Ultra) reports a running mode.
static bool deviceMoving(const autoblow::Status &st) { return st.online && st.moving; }

static lv_obj_t *button(lv_obj_t *parent, const char *text, int w, int h, Role color,
                        lv_event_cb_t cb, void *data = nullptr) {
  lv_obj_t *b = lv_button_create(parent);
  lv_obj_set_size(b, w, h);
  lv_obj_t *l = lv_label_create(b);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_28, 0);
  lv_obj_center(l);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, data);
  theme::bind(b, Prop::Bg, color);  // after the label exists: it picks its text colour from this
  return b;
}

// lv_label_set_text always redraws; the 200 ms refresh sets a dozen labels, so
// skip the ones that didn't change (keeps the display's PSRAM traffic down).
static void setLabel(lv_obj_t *l, const char *text) {
  if (strcmp(lv_label_get_text(l), text) != 0) lv_label_set_text(l, text);
}

static void setButtonText(lv_obj_t *b, const char *text, Role color) {
  setLabel(lv_obj_get_child(b, 0), text);
  theme::bind(b, Prop::Bg, color);
}

static void valveRow(lv_obj_t *r) {
  if (valveRowCount < (int)(sizeof valveRows / sizeof *valveRows)) valveRows[valveRowCount++] = r;
}

// The device the panel shows: the one picked (by address) if it is connected,
// else the first connected one (the Autoblow device first), else the picked
// one, else device 0. n >= 1.
static int selectedIndex(const devices::Info *inf, int n) {
  int picked = -1, firstUp = -1;
  for (int i = 0; i < n; i++) {
    if (inf[i].caps & devices::CAP_HR) continue;  // a heart rate monitor has no controls to show
    if (selPicked && picked < 0 && !strcmp(inf[i].addr, selAddr)) picked = i;
    if (firstUp < 0 && inf[i].connected) firstUp = i;
  }
  if (picked >= 0 && inf[picked].connected) return picked;
  if (firstUp >= 0) return firstUp;
  return picked >= 0 ? picked : 0;
}

static void selectDevice(const char *addr) {
  snprintf(selAddr, sizeof selAddr, "%s", addr);
  selPicked = true;
  // Not from inside the tap: refresh() may rebuild the row that was tapped.
  lv_async_call([](void *) { refresh(nullptr); }, nullptr);
}

// A button that is "on" only while a finger is on it.
static void holdEvent(lv_event_t *e) {
  auto fn = (void (*)(bool))lv_event_get_user_data(e);
  lv_event_code_t c = lv_event_get_code(e);
  if (c == LV_EVENT_PRESSED) fn(true);
  else if (c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) fn(false);
}

static lv_obj_t *holdButton(lv_obj_t *parent, const char *text, int w, int h, void (*fn)(bool)) {
  lv_obj_t *b = button(parent, text, w, h, Role::Primary, nullptr);
  theme::bind(b, Prop::Bg, Role::Stop, LV_STATE_PRESSED);
  lv_obj_add_event_cb(b, holdEvent, LV_EVENT_ALL, (void *)fn);
  return b;
}

// One-of-N selector. `fn` gets the chosen index.
static void segEvent(lv_event_t *e) {
  auto fn = (void (*)(int))lv_event_get_user_data(e);
  uint32_t i = lv_buttonmatrix_get_selected_button(lv_event_get_target_obj(e));
  if (i != LV_BUTTONMATRIX_BUTTON_NONE) fn(i);
}

static lv_obj_t *segmented(lv_obj_t *parent, const char **map, int selected, void (*fn)(int)) {
  lv_obj_t *m = lv_buttonmatrix_create(parent);
  lv_buttonmatrix_set_map(m, map);
  lv_buttonmatrix_set_button_ctrl_all(m, LV_BUTTONMATRIX_CTRL_CHECKABLE);
  lv_buttonmatrix_set_one_checked(m, true);
  lv_buttonmatrix_set_button_ctrl(m, selected, LV_BUTTONMATRIX_CTRL_CHECKED);
  lv_obj_add_event_cb(m, segEvent, LV_EVENT_VALUE_CHANGED, (void *)fn);
  return m;
}

static lv_obj_t *row(lv_obj_t *parent, int h) {
  lv_obj_t *r = lv_obj_create(parent);
  lv_obj_remove_style_all(r);
  lv_obj_set_size(r, lv_pct(100), h);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(r, 10, 0);
  return r;
}

static lv_obj_t *labeledSeg(lv_obj_t *parent, const char *title, const char **map, int sel,
                            void (*fn)(int), int h = 56) {
  lv_obj_t *r = row(parent, h);
  lv_obj_t *l = lv_label_create(r);
  lv_label_set_text(l, title);
  lv_obj_set_width(l, 130);
  lv_obj_t *m = segmented(r, map, sel, fn);
  lv_obj_set_height(m, h);
  lv_obj_set_flex_grow(m, 1);
  lv_obj_set_style_pad_all(m, 4, 0);
  return m;
}

// Full-screen popup with a one-line text field and keyboard. `done` gets the
// text on OK; Cancel just closes.
using TextDone = void (*)(const char *text, intptr_t data);

struct EditCtx {
  TextDone done;
  intptr_t data;
  lv_obj_t *bg;
};

static void editEvent(lv_event_t *e) {
  auto *ctx = (EditCtx *)lv_event_get_user_data(e);
  lv_event_code_t c = lv_event_get_code(e);
  if ((c != LV_EVENT_READY && c != LV_EVENT_CANCEL) || !ctx->bg) return;
  // The keyboard's OK reaches the field twice; ctx->bg guards against that.
  if (c == LV_EVENT_READY) ctx->done(lv_textarea_get_text(lv_event_get_target_obj(e)), ctx->data);
  lv_obj_delete_async(ctx->bg);
  ctx->bg = nullptr;
}

static void editText(const char *title, const char *initial, bool password, TextDone done,
                     intptr_t data = 0) {
  lv_obj_t *bg = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(bg);
  lv_obj_set_size(bg, SCREEN_W, SCREEN_H);
  theme::bind(bg, Prop::Bg, Role::OverlayBg);
  lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0);
  lv_obj_add_flag(bg, LV_OBJ_FLAG_CLICKABLE);  // nothing underneath gets touches

  lv_obj_t *t = lv_label_create(bg);
  lv_label_set_text(t, title);
  theme::bind(t, Prop::Text, Role::OverlayText);
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, 20, 20);

  lv_obj_t *ta = lv_textarea_create(bg);
  lv_textarea_set_one_line(ta, true);
  lv_textarea_set_password_mode(ta, password);
  lv_textarea_set_text(ta, initial);
  lv_obj_set_width(ta, password ? 620 : 760);
  lv_obj_align(ta, LV_ALIGN_TOP_LEFT, 20, 60);
  lv_obj_add_state(ta, LV_STATE_FOCUSED);

  if (password) {
    lv_obj_t *show = button(bg, "Show", 130, 50, Role::Primary, [](lv_event_t *e) {
      auto *f = (lv_obj_t *)lv_event_get_user_data(e);
      bool hidden = lv_textarea_get_password_mode(f);
      lv_textarea_set_password_mode(f, !hidden);
      lv_label_set_text(lv_obj_get_child(lv_event_get_target_obj(e), 0), hidden ? "Hide" : "Show");
    }, ta);
    lv_obj_set_style_text_font(lv_obj_get_child(show, 0), &lv_font_montserrat_20, 0);
    lv_obj_align(show, LV_ALIGN_TOP_RIGHT, -20, 60);
  }

  lv_obj_t *kb = lv_keyboard_create(bg);
  lv_obj_set_size(kb, SCREEN_W, 320);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(kb, ta);

  auto *ctx = new EditCtx{done, data, bg};
  lv_obj_add_event_cb(ta, editEvent, LV_EVENT_ALL, ctx);
  // Freed with the field itself, the last object that can still use it.
  lv_obj_add_event_cb(ta, [](lv_event_t *e) { delete (EditCtx *)lv_event_get_user_data(e); },
                      LV_EVENT_DELETE, ctx);
}

// ---------- top bar ----------

static void buildTopBar(lv_obj_t *scr) {
  lv_obj_t *bar = lv_obj_create(scr);
  lv_obj_set_size(bar, SCREEN_W, 64);
  lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_radius(bar, 0, 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_pad_hor(bar, 12, 0);
  lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  // Top left: the selected device and its state. Tap: the next connected
  // device, or the Devices tab when there is no other.
  lv_obj_t *sel = lv_obj_create(bar);
  lv_obj_remove_style_all(sel);
  lv_obj_set_size(sel, 410, 64);
  lv_obj_align(sel, LV_ALIGN_LEFT_MID, -12, 0);  // to the screen edge: a bigger target
  lv_obj_set_style_pad_left(sel, 12, 0);
  theme::bind(sel, Prop::Bg, Role::Highlight, LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(sel, LV_OPA_10, LV_STATE_PRESSED);
  lv_obj_remove_flag(sel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(sel, [](lv_event_t *) {
    static devices::Info inf[DV_MAX];
    int n = min(devices::count(), DV_MAX);
    if (n < 1) return;
    for (int i = 0; i < n; i++) inf[i] = devices::info(i);
    int cur = selectedIndex(inf, n);
    for (int k = 1; k < n; k++) {
      int i = (cur + k) % n;
      if (inf[i].connected && !(inf[i].caps & devices::CAP_HR)) return selectDevice(inf[i].addr);
    }
    lv_tabview_set_active(tabview, 4, LV_ANIM_OFF);  // nothing else to switch to: Devices
  }, LV_EVENT_CLICKED, nullptr);

  statusDot = lv_obj_create(sel);
  lv_obj_set_size(statusDot, 16, 16);
  lv_obj_set_style_radius(statusDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(statusDot, 0, 0);
  lv_obj_remove_flag(statusDot, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align(statusDot, LV_ALIGN_LEFT_MID, 0, 0);

  statusLabel = lv_label_create(sel);
  lv_obj_set_width(statusLabel, 370);
  lv_label_set_long_mode(statusLabel, LV_LABEL_LONG_DOT);
  lv_obj_align(statusLabel, LV_ALIGN_LEFT_MID, 26, 0);

  wifiLabel = lv_label_create(bar);
  lv_obj_set_style_text_font(wifiLabel, &lv_font_montserrat_14, 0);
  lv_obj_align(wifiLabel, LV_ALIGN_LEFT_MID, 418, 0);

  // Under the Wi-Fi signal once a fuel gauge answers (see refresh()).
  battLabel = lv_label_create(bar);
  lv_obj_set_style_text_font(battLabel, &lv_font_montserrat_14, 0);
  lv_obj_align(battLabel, LV_ALIGN_LEFT_MID, 418, 11);
  lv_obj_add_flag(battLabel, LV_OBJ_FLAG_HIDDEN);

  battBanner = lv_label_create(lv_layer_top());
  lv_obj_set_size(battBanner, SCREEN_W, LV_SIZE_CONTENT);
  lv_obj_align(battBanner, LV_ALIGN_BOTTOM_MID, 0, 0);  // bottom: keeps the tabs usable
  theme::bind(battBanner, Prop::Bg, Role::Stop);
  lv_obj_set_style_bg_opa(battBanner, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(battBanner, 10, 0);
  lv_label_set_long_mode(battBanner, LV_LABEL_LONG_WRAP);
  lv_obj_add_flag(battBanner, LV_OBJ_FLAG_HIDDEN);

  // Shown when the link drops while a device was moving: STOP can't reach it.
  // refresh() writes the text, with the device's name.
  lostBanner = lv_label_create(lv_layer_top());
  lv_obj_set_size(lostBanner, SCREEN_W, LV_SIZE_CONTENT);
  lv_obj_align(lostBanner, LV_ALIGN_TOP_MID, 0, 64);
  theme::bind(lostBanner, Prop::Bg, Role::Stop);
  lv_obj_set_style_bg_opa(lostBanner, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(lostBanner, 10, 0);
  lv_label_set_long_mode(lostBanner, LV_LABEL_LONG_WRAP);
  lv_label_set_text(lostBanner, "");
  lv_obj_add_flag(lostBanner, LV_OBJ_FLAG_HIDDEN);

  speedLabel = lv_label_create(bar);
  lv_obj_set_style_text_font(speedLabel, &lv_font_montserrat_28, 0);
  lv_obj_align(speedLabel, LV_ALIGN_RIGHT_MID, -200, 0);

  lv_obj_t *stop = button(bar, LV_SYMBOL_STOP " STOP", 180, 52, Role::Stop,
                          [](lv_event_t *) { control::stopAll(); });
  lv_obj_align(stop, LV_ALIGN_RIGHT_MID, 0, 0);
}

// ---------- remote tab ----------

static void buildRemote(lv_obj_t *tab) {
  lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(tab, 20, 0);

  lv_obj_t *left = lv_obj_create(tab);
  lv_obj_remove_style_all(left);
  lv_obj_set_size(left, 150, lv_pct(100));
  lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(left, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(left, 14, 0);

  rSlider = lv_slider_create(left);
  lv_obj_set_size(rSlider, 60, 220);  // leaves room for rWhatLabel under it
  lv_slider_set_range(rSlider, 5, 100);
  lv_slider_set_value(rSlider, control::remoteSpeed(), LV_ANIM_OFF);
  // While dragging, a running Remote follows every 5 % step crossed (the
  // devices only ever get the latest value, so a fast drag doesn't queue up);
  // the exact value goes out on release, which also starts it.
  lv_obj_add_event_cb(rSlider, [](lv_event_t *e) {
    static int lastStep = -1;
    int v = lv_slider_get_value(rSlider);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
      lv_label_set_text_fmt(rSliderLabel, "%d%%", v);
      if (control::active() == Active::Remote && v / 5 != lastStep) {
        lastStep = v / 5;
        control::remoteSetSpeed(v);
      }
    } else if (code == LV_EVENT_RELEASED) {
      lastStep = -1;
      control::remoteSetSpeed(v);
    }
  }, LV_EVENT_ALL, nullptr);

  rSliderLabel = lv_label_create(left);
  lv_obj_set_style_text_font(rSliderLabel, &lv_font_montserrat_28, 0);
  lv_label_set_text_fmt(rSliderLabel, "%d%%", control::remoteSpeed());

  rWhatLabel = lv_label_create(left);
  lv_obj_set_width(rWhatLabel, lv_pct(100));
  lv_label_set_long_mode(rWhatLabel, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(rWhatLabel, &lv_font_montserrat_14, 0);
  theme::bind(rWhatLabel, Prop::Text, Role::TextDim);
  lv_obj_set_style_text_align(rWhatLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(rWhatLabel, "");

  lv_obj_t *right = lv_obj_create(tab);
  lv_obj_remove_style_all(right);
  lv_obj_set_height(right, lv_pct(100));
  lv_obj_set_flex_grow(right, 1);
  lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(right, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(right, 16, 0);

  rStartBtn = button(right, "", lv_pct(100), 80, Role::Start, [](lv_event_t *) {
    if (control::active() == Active::Remote) control::remotePause();
    else control::remoteStart();
  });

  lv_obj_t *r = row(right, 80);
  lv_obj_set_style_pad_column(r, 16, 0);
  lv_obj_set_flex_grow(holdButton(r, "Looser", 0, 80, control::looser), 1);
  lv_obj_set_flex_grow(holdButton(r, "Tighter", 0, 80, control::tighter), 1);
  valveRow(r);

  static const char *vac[] = {"Off", "Low", "Medium", "High", ""};
  valveRow(lv_obj_get_parent(labeledSeg(right, "Vacuum", vac, control::remoteVacuum(), control::remoteSetVacuum)));
}

// ---------- autopilot tab ----------

static void chartEvent(lv_event_t *e) {
  lv_event_code_t c = lv_event_get_code(e);
  uint32_t id = lv_chart_get_pressed_point(apChart);
  if (id == LV_CHART_POINT_NONE || !control::autopilotSessionMs()) return;
  // Whole session while drawing; otherwise the window: the tapped x is a time around "now".
  uint32_t t = apWinWhole ? (uint32_t)((uint64_t)id * control::autopilotSessionMs() / AP_POINTS)
                          : apwin::clampMs(apwin::timeAt((int)id, AP_POINTS, apWinNow), control::autopilotSessionMs());
  if (c == LV_EVENT_PRESSING && apDraw) {
    // Drag: the finger's height sets the peak of the wave under it.
    lv_point_t pt;
    lv_indev_get_point(lv_indev_active(), &pt);
    lv_area_t area;
    lv_obj_get_content_coords(apChart, &area);
    int pct = 100 * (area.y2 - pt.y) / max<int>(1, area.y2 - area.y1);
    int wave = control::autopilotWaveAt(t);
    if (abs(control::autopilotPeak(wave) - pct) >= 2) control::autopilotSetPeak(wave, constrain(pct, 5, 100));
  } else if (c == LV_EVENT_SHORT_CLICKED && !apDraw) {
    control::autopilotSeek(t);  // tap: jump there
  }
}

static void buildAutopilot(lv_obj_t *tab) {
  lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(tab, 8, 0);

  lv_obj_t *top = row(tab, 64);
  apStartBtn = button(top, "", 220, 64, Role::Start, [](lv_event_t *) {
    if (control::active() == Active::Autopilot) control::autopilotPause();
    else control::autopilotStart();
  });
  apStateLabel = lv_label_create(top);
  lv_obj_set_flex_grow(apStateLabel, 1);
  lv_obj_set_style_text_font(apStateLabel, &lv_font_montserrat_20, 0);

  // The session: planned speed over time, played part grey, playhead in orange.
  apChart = lv_chart_create(tab);
  lv_obj_set_size(apChart, lv_pct(100), 116);
  lv_chart_set_type(apChart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(apChart, AP_POINTS);
  lv_chart_set_axis_range(apChart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
  lv_chart_set_div_line_count(apChart, 3, 0);
  lv_obj_set_style_size(apChart, 0, 0, LV_PART_INDICATOR);  // no dots on the line
  lv_obj_set_style_line_width(apChart, 3, LV_PART_ITEMS);
  theme::bind(apChart, Prop::Bg, Role::ChartBg);
  lv_obj_set_style_border_width(apChart, 0, 0);
  lv_obj_set_style_pad_all(apChart, 6, 0);
  apPast = lv_chart_add_series(apChart, themed(Role::ChartPast), LV_CHART_AXIS_PRIMARY_Y);
  apFuture = lv_chart_add_series(apChart, themed(Role::ChartFuture), LV_CHART_AXIS_PRIMARY_Y);
  // HR edging: the heart rate over the session, scaled 40..220 bpm onto the 0..100 axis.
  apHrSeries = lv_chart_add_series(apChart, themed(Role::Stop), LV_CHART_AXIS_PRIMARY_Y);
  apCursor = lv_chart_add_cursor(apChart, themed(Role::Pause), LV_DIR_VER);
  // Axis in seconds relative to now (the window is the last 10 s and the next 30 s), written by refresh().
  auto axisLabel = [](const char *text, lv_align_t al) {
    lv_obj_t *l = lv_label_create(apChart);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    theme::bind(l, Prop::Text, Role::TextDim);
    lv_label_set_text(l, text);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_align(l, al);
    return l;
  };
  apLblL = axisLabel("", LV_ALIGN_BOTTOM_LEFT);
  apLblNow = axisLabel("now", LV_ALIGN_BOTTOM_LEFT);
  lv_obj_set_x(apLblNow, lv_pct(25));
  lv_obj_set_style_translate_x(apLblNow, lv_pct(-50), 0);
  apLblR = axisLabel("", LV_ALIGN_BOTTOM_RIGHT);
  apLblNote = axisLabel("HR-driven: depends on heart rate", LV_ALIGN_TOP_RIGHT);
  lv_obj_add_flag(apLblNote, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(apChart, chartEvent, LV_EVENT_PRESSING, nullptr);
  lv_obj_add_event_cb(apChart, chartEvent, LV_EVENT_SHORT_CLICKED, nullptr);

  lv_obj_t *r = row(tab, 64);
  lv_obj_set_style_pad_column(r, 12, 0);
  lv_obj_t *back = button(r, LV_SYMBOL_DOWN "  Back off", 0, 64, Role::Neutral2,
                          [](lv_event_t *) { control::autopilotBackOff(); });
  lv_obj_set_flex_grow(back, 1);
  lv_obj_t *more = button(r, LV_SYMBOL_UP "  More", 0, 64, Role::Pause,
                          [](lv_event_t *) { control::autopilotMore(); });
  lv_obj_set_flex_grow(more, 1);
  lv_obj_t *fin = button(r, "Finish Me", 0, 64, Role::Accent,
                         [](lv_event_t *) { control::finishMe(); });
  lv_obj_set_flex_grow(fin, 1);

  lv_obj_t *r2 = row(tab, 64);
  lv_obj_set_style_pad_column(r2, 12, 0);
  lv_obj_set_flex_grow(holdButton(r2, "Looser", 0, 64, control::looser), 1);
  lv_obj_set_flex_grow(holdButton(r2, "Tighter", 0, 64, control::tighter), 1);
  valveRow(r2);
}

// ---------- session tab: what Autopilot plays ----------

static void favLabel(int slot) {
  char t[16];
  snprintf(t, sizeof t, "%d%s", slot + 1, settings.favPeaks[slot][0] ? "" : " -");
  lv_label_set_text(lv_obj_get_child(apFavBtn[slot], 0), t);
}

// Tap loads a favourite, long press saves the current peaks into it.
static void favEvent(lv_event_t *e) {
  int slot = (intptr_t)lv_event_get_user_data(e);
  if (lv_event_get_code(e) == LV_EVENT_SHORT_CLICKED) {
    control::autopilotLoadFavourite(slot);
  } else if (lv_event_get_code(e) == LV_EVENT_LONG_PRESSED) {
    control::autopilotSaveFavourite(slot);
    favLabel(slot);
  }
}

// ---- HR edging settings (Session tab, below the plain Autopilot rows) ----
// A row per number: [-] value [+]. Every change is saved at once and handed to the running session.

struct Stepper {
  lv_obj_t *val;
  int *v;
  int lo, hi, step;
  const char *unit;
  int zeroBelow;  // values below this read "off" (0 = never)
  const char *zeroText;
};
static Stepper steppers[6];
static int nSteppers;

static void stepperShow(const Stepper &s) {
  char t[24];
  if (*s.v < s.zeroBelow) snprintf(t, sizeof t, "%s", s.zeroText);
  else snprintf(t, sizeof t, "%d%s%s", *s.v, *s.unit ? " " : "", s.unit);
  setLabel(s.val, t);
}

static void stepperShowAll() {
  for (int i = 0; i < nSteppers; i++) stepperShow(steppers[i]);
}

static void stepperEvent(lv_event_t *e) {
  intptr_t k = (intptr_t)lv_event_get_user_data(e);
  Stepper &s = steppers[k >> 1];
  int v = constrain(*s.v + ((k & 1) ? s.step : -s.step), s.lo, s.hi);
  if (v == *s.v) return;
  *s.v = v;
  // The edge has to stay above resting.
  settings.hrEdge = constrain(settings.hrEdge, settings.hrResting + 10, 200);
  settingsSaveHr();
  control::autopilotHrApply();
  stepperShowAll();
}

static lv_obj_t *stepperRow(lv_obj_t *tab, const char *title, int *v, int lo, int hi, int step, const char *unit,
                            int zeroBelow = 0, const char *zeroText = "off") {
  lv_obj_t *r = row(tab, ST_ROW);
  lv_obj_t *l = lv_label_create(r);
  lv_label_set_text(l, title);
  lv_obj_set_width(l, 200);
  Stepper &s = steppers[nSteppers];
  s = {nullptr, v, lo, hi, step, unit, zeroBelow, zeroText};
  button(r, LV_SYMBOL_MINUS, 80, ST_ROW, Role::Neutral, stepperEvent, (void *)(intptr_t)(nSteppers * 2));
  s.val = lv_label_create(r);
  lv_obj_set_width(s.val, 150);
  lv_obj_set_style_text_align(s.val, LV_TEXT_ALIGN_CENTER, 0);
  button(r, LV_SYMBOL_PLUS, 80, ST_ROW, Role::Neutral, stepperEvent, (void *)(intptr_t)(nSteppers * 2 + 1));
  stepperShow(s);
  nSteppers++;
  return r;
}

static lv_obj_t *hrBox, *hrMoreBtn;

static void hrMoreEvent(lv_event_t *) {
  bool show = lv_obj_has_flag(hrBox, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_flag(hrBox, LV_OBJ_FLAG_HIDDEN, !show);
  lv_label_set_text(lv_obj_get_child(hrMoreBtn, 0), show ? LV_SYMBOL_UP : LV_SYMBOL_DOWN);
}

static void hrToggle(int i) { control::autopilotSetHrEdging(i == 1); }

// One row on the Session tab (switch + a button that opens the numbers). The numbers sit in a box that is
// collapsed by default, so the tab does not need to scroll (a scroll repaints the whole tab, ~0.2 s a frame).
static void buildHrSection(lv_obj_t *tab, int h) {
  static const char *onOff[] = {"Off", "On", ""};
  lv_obj_t *seg = labeledSeg(tab, "HR edging", onOff, control::autopilotHrEdging(), hrToggle, h);
  hrMoreBtn = button(lv_obj_get_parent(seg), LV_SYMBOL_DOWN, 80, h, Role::Neutral, hrMoreEvent);
  hrBox = lv_obj_create(tab);
  lv_obj_remove_style_all(hrBox);
  lv_obj_set_size(hrBox, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(hrBox, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(hrBox, 6, 0);
  lv_obj_add_flag(hrBox, LV_OBJ_FLAG_HIDDEN);
  hrLiveLabel = lv_label_create(hrBox);
  lv_obj_set_width(hrLiveLabel, lv_pct(100));
  lv_obj_set_style_text_font(hrLiveLabel, &lv_font_montserrat_14, 0);
  theme::bind(hrLiveLabel, Prop::Text, Role::TextDim);
  lv_label_set_text(hrLiveLabel, "");
  nSteppers = 0;
  stepperRow(hrBox, "Resting bpm", &settings.hrResting, 40, 150, 1, "bpm");
  lv_obj_t *er = stepperRow(hrBox, "Edge bpm", &settings.hrEdge, 50, 200, 1, "bpm");
  lv_obj_t *learn = button(er, "Learn", 150, ST_ROW, Role::Accent, [](lv_event_t *) {
    control::autopilotHrLearnEdge();  // from the current reading; ignored without one
    stepperShowAll();
  });
  lv_obj_set_style_text_font(lv_obj_get_child(learn, 0), &lv_font_montserrat_20, 0);
  stepperRow(hrBox, "Recovery drop", &settings.hrDrop, 3, 60, 1, "bpm");
  stepperRow(hrBox, "Edges", &settings.hrEdges, 0, 50, 1, "", 1, "no limit");
  stepperRow(hrBox, "Raise per edge", &settings.hrStep, 0, 10, 1, "bpm", 1, "none");
  stepperRow(hrBox, "Fast rise", &settings.hrRise, 0, 60, 4, "bpm / 10 s", 8, "off");
}

static void buildSession(lv_obj_t *tab) {
  lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(tab, 4, 0);
  const int SR = 42;  // seven rows (with HR edging) have to fit above the fold: no scrolling by default

  static const char *intensity[] = {"Warmup", "Low", "Medium", "High", ""};
  static const char *edge[] = {"Gentle", "Moderate", "Intense", ""};
  static const char *vac[] = {"Off", "Low", "High", ""};
  static const char *length[] = {"Auto", "5 min", "10 min", "20 min", "30 min", ""};
  static const char *surprise[] = {"Rising", "Shuffled", "Wild", ""};
  labeledSeg(tab, "Intensity", intensity, control::autopilotIntensity(), control::autopilotSetIntensity, SR);
  labeledSeg(tab, "Edge", edge, control::autopilotEdge(), control::autopilotSetEdge, SR);
  valveRow(lv_obj_get_parent(
      labeledSeg(tab, "Vacuum", vac, control::autopilotVacuum(), control::autopilotSetVacuum, SR)));
  labeledSeg(tab, "Length", length, control::autopilotLength(), control::autopilotSetLength, SR);
  labeledSeg(tab, "Peaks", surprise, control::autopilotSurprise(), control::autopilotSetSurprise, SR);

  // Favourites (tap: load, hold: save), Draw (drag peaks on the graph), Reset.
  lv_obj_t *r = row(tab, SR);
  lv_obj_t *l = lv_label_create(r);
  lv_label_set_text(l, "Favourites");
  lv_obj_set_width(l, 130);
  for (int i = 0; i < Settings::FAVOURITES; i++) {
    apFavBtn[i] = button(r, "", 90, SR, Role::Neutral, nullptr);
    lv_obj_add_event_cb(apFavBtn[i], favEvent, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)i);
    lv_obj_add_event_cb(apFavBtn[i], favEvent, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)i);
    lv_obj_set_style_text_font(lv_obj_get_child(apFavBtn[i], 0), &lv_font_montserrat_20, 0);
    favLabel(i);
  }
  apDrawBtn = button(r, LV_SYMBOL_EDIT " Draw", 0, SR, Role::Neutral, [](lv_event_t *) {
    apDraw = !apDraw;
    lv_obj_set_state(apDrawBtn, LV_STATE_CHECKED, apDraw);
  });
  lv_obj_set_flex_grow(apDrawBtn, 1);
  theme::bind(apDrawBtn, Prop::Bg, Role::Primary, LV_STATE_CHECKED);
  lv_obj_set_style_text_font(lv_obj_get_child(apDrawBtn, 0), &lv_font_montserrat_20, 0);
  lv_obj_t *reset = button(r, "Reset", 110, SR, Role::Neutral, [](lv_event_t *) {
    control::autopilotClearPeaks();
    apDraw = false;
    lv_obj_set_state(apDrawBtn, LV_STATE_CHECKED, false);
  });
  lv_obj_set_style_text_font(lv_obj_get_child(reset, 0), &lv_font_montserrat_20, 0);

  buildHrSection(tab, SR);
}

// ---------- scripts tab ----------

static void scriptLabel(int slot) {
  lv_obj_t *l = lv_obj_get_child(scButtons[slot], 0);
  const String &n = settings.scriptNames[slot];
  if (n.length()) lv_label_set_text_fmt(l, "%d  %s", slot + 1, n.c_str());
  else lv_label_set_text_fmt(l, "%d", slot + 1);
}

static void scriptRenamed(const char *text, intptr_t slot) {
  String n = text;
  n.trim();
  settings.scriptNames[slot] = n;
  settingsSaveScriptName(slot);
  scriptLabel(slot);
}

// Tap plays the slot, long press names it.
static void scriptEvent(lv_event_t *e) {
  int slot = (intptr_t)lv_event_get_user_data(e);
  if (lv_event_get_code(e) == LV_EVENT_SHORT_CLICKED) {
    control::scriptPlay(slot);
  } else if (lv_event_get_code(e) == LV_EVENT_LONG_PRESSED) {
    char title[48];
    snprintf(title, sizeof title, "Name for script %d (empty = none)", slot + 1);
    editText(title, settings.scriptNames[slot].c_str(), false, scriptRenamed, slot);
  }
}

static void buildScripts(lv_obj_t *tab) {
  lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(tab, 12, 0);
  lv_obj_remove_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *grid = lv_obj_create(tab);
  lv_obj_remove_style_all(grid);
  lv_obj_set_width(grid, lv_pct(100));
  lv_obj_set_flex_grow(grid, 1);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_gap(grid, 8, 0);
  lv_obj_set_scroll_dir(grid, LV_DIR_VER);

  for (int i = 0; i < SCRIPT_SLOTS; i++) {
    lv_obj_t *b = button(grid, "", lv_pct(19), 64, Role::Neutral, nullptr);
    theme::bind(b, Prop::Bg, Role::Primary, LV_STATE_CHECKED);
    lv_obj_t *l = lv_obj_get_child(b, 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
    lv_obj_set_width(l, lv_pct(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_event_cb(b, scriptEvent, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)i);
    lv_obj_add_event_cb(b, scriptEvent, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)i);
    scButtons[i] = b;
    scriptLabel(i);
  }

  scToggleBtn = button(tab, "", lv_pct(100), 72, Role::Pause,
                       [](lv_event_t *) { control::scriptToggle(); });
}

// ---------- devices tab ----------
// One row per device: Follow (it takes the panel's output), how much of it, and
// Connect/Forget for Bluetooth toys. Rows are rebuilt only when the list itself
// changes (scan, forget); refresh() updates their labels in place.

static const int DV_ROW = 72;

struct DevRow {
  lv_obj_t *card, *dot, *name, *status, *follow, *scale, *btn, *btn2;  // btn Connect, btn2 Forget: nullptr for the cloud device
  char addr[18];   // which device the row was built for (indexes move on a scan)
  int8_t dotState;  // 0 grey, 1 orange, 2 green; -1 not set yet
  int8_t scaleSel;
  int16_t layoutKey;  // devrow::Layout last applied (-1: none yet)
  int8_t selected;  // highlighted as the selected device
  bool wasMoving;  // moving when last reachable: for the lost banner
};
static DevRow dvRows[DV_MAX];
static lv_obj_t *stThemeSeg;  // Settings chip row: follows a switch made over the web
static int dvShown;
static uint32_t dvSig;  // count + names + addresses the rows were built for

// The row's device, if index i is still the device the row was built for.
static bool devAt(int i, devices::Info &d) {
  if (i >= devices::count()) return false;
  d = devices::info(i);
  return !strcmp(d.addr, dvRows[i].addr);
}

static void devFollowEvent(lv_event_t *e) {
  int i = (intptr_t)lv_event_get_user_data(e);
  devices::Info d;
  if (devAt(i, d)) devices::setEnabled(i, lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED));
}

static const int SCALES[] = {25, 50, 75, 100};

static void devScaleEvent(lv_event_t *e) {
  int i = (intptr_t)lv_event_get_user_data(e);
  uint32_t sel = lv_buttonmatrix_get_selected_button(lv_event_get_target_obj(e));
  devices::Info d;
  if (sel == LV_BUTTONMATRIX_BUTTON_NONE || sel > 3 || !devAt(i, d)) return;
  dvRows[i].scaleSel = sel;
  devices::setScale(i, SCALES[sel]);
}

// Connect: also for a remembered device that dropped out (it stays remembered; the retry backoff restarts).
static void devButtonEvent(lv_event_t *e) {
  int i = (intptr_t)lv_event_get_user_data(e);
  devices::Info d;
  if (devAt(i, d) && !d.connected && !d.connecting) devices::connect(i);
}

static void devForgetEvent(lv_event_t *e) {
  int i = (intptr_t)lv_event_get_user_data(e);
  devices::Info d;
  if (!devAt(i, d)) return;
  devices::forget(i);
  dvSig = 0;  // the list may shrink: rebuild at the next refresh
}

// Tap on a row (outside its switch, scale and button): show that device's controls.
static void devRowEvent(lv_event_t *e) {
  int i = (intptr_t)lv_event_get_user_data(e);
  devices::Info d;
  // A heart rate monitor is an input: it has no controls to show.
  if (devAt(i, d) && devrow::layout(d.addr[0], d.known, d.connected, d.connecting, d.caps & devices::CAP_HR).selectable)
    selectDevice(d.addr);
}

static int scaleSel(int pct) { return constrain((pct + 12) / 25 - 1, 0, 3); }  // nearest of 25/50/75/100

static void devBuildRows(const devices::Info *inf, int n) {
  // Keep the lost-banner memory of devices that are still listed.
  DevRow old[DV_MAX];
  memcpy(old, dvRows, sizeof old);
  int oldShown = dvShown;
  lv_obj_clean(dvList);
  dvShown = n;
  static const char *scaleMap[] = {"25%", "50%", "75%", "100%", ""};
  for (int i = 0; i < n; i++) {
    const devices::Info &d = inf[i];
    DevRow &r = dvRows[i];
    r = DevRow{};
    snprintf(r.addr, sizeof r.addr, "%s", d.addr);
    r.dotState = r.scaleSel = r.selected = -1;
    r.layoutKey = -1;
    for (int j = 0; j < oldShown; j++)
      if (!strcmp(old[j].addr, d.addr)) r.wasMoving = old[j].wasMoving;

    // Only the card itself takes taps (its plain children don't), so a tap
    // anywhere but the switch, scale or button selects the device.
    lv_obj_t *card = r.card = lv_obj_create(dvList);
    lv_obj_add_event_cb(card, devRowEvent, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_set_size(card, lv_pct(100), DV_ROW);
    lv_obj_set_style_pad_hor(card, 12, 0);
    lv_obj_set_style_pad_ver(card, 4, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(card, 12, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    r.dot = lv_obj_create(card);
    lv_obj_set_size(r.dot, 16, 16);
    lv_obj_set_style_radius(r.dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(r.dot, 0, 0);
    lv_obj_remove_flag(r.dot, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *col = lv_obj_create(card);
    lv_obj_remove_style_all(col);
    lv_obj_remove_flag(col, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 2, 0);
    r.name = lv_label_create(col);
    lv_obj_set_width(r.name, lv_pct(100));
    lv_label_set_long_mode(r.name, LV_LABEL_LONG_DOT);
    lv_label_set_text(r.name, d.name);
    r.status = lv_label_create(col);
    lv_obj_set_width(r.status, lv_pct(100));
    lv_label_set_long_mode(r.status, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(r.status, &lv_font_montserrat_14, 0);
    theme::bind(r.status, Prop::Text, Role::TextDim);
    lv_label_set_text(r.status, "");

    r.follow = lv_switch_create(card);
    lv_obj_set_size(r.follow, 76, 40);
    lv_obj_set_state(r.follow, LV_STATE_CHECKED, d.enabled);
    lv_obj_add_event_cb(r.follow, devFollowEvent, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)i);

    r.scale = lv_buttonmatrix_create(card);
    lv_buttonmatrix_set_map(r.scale, scaleMap);
    lv_buttonmatrix_set_button_ctrl_all(r.scale, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(r.scale, true);
    lv_obj_set_size(r.scale, 250, 56);
    lv_obj_set_style_pad_all(r.scale, 4, 0);
    lv_obj_add_event_cb(r.scale, devScaleEvent, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)i);
    // Follow / share / buttons are shown or hidden by devUpdateRow from the current state, every refresh.

    if (d.addr[0]) {
      r.btn = button(card, "", 120, 56, Role::Primary, devButtonEvent, (void *)(intptr_t)i);
      lv_obj_set_style_text_font(lv_obj_get_child(r.btn, 0), &lv_font_montserrat_20, 0);
      r.btn2 = button(card, "Forget", 100, 56, Role::Neutral2, devForgetEvent, (void *)(intptr_t)i);
      lv_obj_set_style_text_font(lv_obj_get_child(r.btn2, 0), &lv_font_montserrat_20, 0);
    } else {
      lv_obj_t *gap = lv_obj_create(card);  // keeps the columns lined up
      lv_obj_remove_style_all(gap);
      lv_obj_remove_flag(gap, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_size(gap, 228, 1);
    }
  }
}

static uint32_t devSignature(const devices::Info *inf, int n) {
  uint32_t h = 2166136261u ^ n;  // FNV-1a
  auto mix = [&h](const char *s) {
    for (; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
    h = (h ^ 0xFF) * 16777619u;
  };
  for (int i = 0; i < n; i++) {
    mix(inf[i].name);
    mix(inf[i].addr);
  }
  return h | 1;  // never 0: 0 means "rebuild"
}

// Labels, dot, switch and scale of an existing row: only what changed.
static void devUpdateRow(DevRow &r, const devices::Info &d, bool selected) {
  if (selected != r.selected) {
    r.selected = selected;
    lv_obj_set_style_border_color(r.card, themed(selected ? Role::Primary : Role::Neutral), 0);
    lv_obj_set_style_border_width(r.card, selected ? 3 : 1, 0);
  }
  int8_t ds = d.connected ? 2 : d.connecting ? 1 : 0;
  if (ds != r.dotState) {
    r.dotState = ds;
    lv_obj_set_style_bg_color(r.dot, themed(ds == 2 ? Role::Start : ds == 1 ? Role::Pause : Role::TextDim), 0);
  }
  char text[96];
  if (!d.addr[0] && !settings.token.length()) {
    snprintf(text, sizeof text, "Set the token in Settings");
  } else {
    // Bluetooth: say so when a link comes up (the driver reports "Ready").
    int at = snprintf(text, sizeof text, "%s", d.addr[0] && d.connected && !strcmp(d.status, "Ready") ? "Connected" : d.status);
    if ((d.caps & devices::CAP_HR) && d.connected && at < (int)sizeof text)
      at += d.bpm ? snprintf(text + at, sizeof text - at, "  HR %d bpm (input only)", d.bpm)
                  : snprintf(text + at, sizeof text - at, "  waiting for a reading (input only)");
    if (d.level > 0 && at < (int)sizeof text) at += snprintf(text + at, sizeof text - at, "  at %d%%", d.level);
    if (d.battery >= 0 && at < (int)sizeof text)
      at += snprintf(text + at, sizeof text - at, "  " LV_SYMBOL_BATTERY_FULL " %d%%", d.battery);
    if (d.rssi && at < (int)sizeof text)
      snprintf(text + at, sizeof text - at, "  " LV_SYMBOL_BLUETOOTH " %d dBm", d.rssi);
  }
  setLabel(r.status, text);
  if (!lv_obj_has_state(r.follow, LV_STATE_PRESSED) && lv_obj_has_state(r.follow, LV_STATE_CHECKED) != d.enabled)
    lv_obj_set_state(r.follow, LV_STATE_CHECKED, d.enabled);
  int8_t sel = scaleSel(d.scale);
  if (sel != r.scaleSel) {
    r.scaleSel = sel;
    lv_buttonmatrix_set_button_ctrl(r.scale, sel, LV_BUTTONMATRIX_CTRL_CHECKED);  // one_checked clears the rest
  }
  // Which controls show comes from the current state every time (a monitor's kind can be known late).
  devrow::Layout lay = devrow::layout(d.addr[0], d.known, d.connected, d.connecting, d.caps & devices::CAP_HR);
  if (lay.key() != r.layoutKey) {
    r.layoutKey = lay.key();
    if (lay.follow) lv_obj_remove_flag(r.follow, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(r.follow, LV_OBJ_FLAG_HIDDEN);
    if (lay.follow) lv_obj_remove_flag(r.scale, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(r.scale, LV_OBJ_FLAG_HIDDEN);
    if (r.btn) {
      if (lay.primary != devrow::Primary::None) lv_obj_remove_flag(r.btn, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(r.btn, LV_OBJ_FLAG_HIDDEN);
      if (lay.forget) lv_obj_remove_flag(r.btn2, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(r.btn2, LV_OBJ_FLAG_HIDDEN);
      if (lay.primary == devrow::Primary::Connecting) {
        lv_obj_set_state(r.btn, LV_STATE_DISABLED, true);
        setButtonText(r.btn, "Connecting", Role::TextDim);
      } else {
        lv_obj_set_state(r.btn, LV_STATE_DISABLED, false);
        setButtonText(r.btn, "Connect", Role::Primary);
      }
    }
  }
}

static void buildDevices(lv_obj_t *tab) {
  lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(tab, 8, 0);
  lv_obj_remove_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *top = row(tab, 56);
  dvScanBtn = button(top, "", 220, 56, Role::Primary, [](lv_event_t *) {
    if (!ble::enabled()) {  // Bluetooth only starts at boot
      settings.bluetooth = 1;
      settingsSaveBluetooth();
      lv_obj_add_state(dvScanBtn, LV_STATE_DISABLED);
      control::stopAndRestart();
      return;
    }
    if (!devices::scanning()) devices::scan();
  });
  lv_obj_t *hint = lv_label_create(top);
  lv_obj_set_flex_grow(hint, 1);
  lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
  lv_label_set_text(hint, "Tap a device to show its controls. New toy: switch it on, Scan, Connect.\n"
                          "Follow: it takes the panel's speed, at the % next to it.\n"
                          "A heart rate monitor is an input (no speed). 3 Bluetooth links in all.");

  dvList = lv_obj_create(tab);
  lv_obj_remove_style_all(dvList);
  lv_obj_set_width(dvList, lv_pct(100));
  lv_obj_set_flex_grow(dvList, 1);
  lv_obj_set_flex_flow(dvList, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(dvList, 8, 0);
  lv_obj_set_scroll_dir(dvList, LV_DIR_VER);
}

// ---------- settings tab ----------
// Wi-Fi and token are edited here and only take effect after Save & restart.
// The screen timeout applies at once.

static String pSsid, pPass, pToken;  // pending, not saved yet

static void wifiSetupShow();  // first-run Wi-Fi screen (below)
static void pinSaveShow();    // the always-visible Save bar (below)

static bool settingsDirty() {
  return pSsid != settings.wifiSsid || pPass != settings.wifiPass || pToken != settings.token;
}

static void settingsShow() {
  lv_label_set_text(lv_obj_get_child(stSsid, 0), pSsid.length() ? pSsid.c_str() : "(none)");
  lv_label_set_text(lv_obj_get_child(stPass, 0),
                    pPass.length() ? LV_SYMBOL_BULLET LV_SYMBOL_BULLET LV_SYMBOL_BULLET
                                     LV_SYMBOL_BULLET LV_SYMBOL_BULLET LV_SYMBOL_BULLET
                                   : "(none)");
  // Token: only the last 3 characters show, so a photo of the panel doesn't leak it.
  String tokenShown;
  if (pToken.length() > 3) {
    for (unsigned i = 3; i < pToken.length(); i++) tokenShown += LV_SYMBOL_BULLET;
    tokenShown += pToken.substring(pToken.length() - 3);
  } else {
    tokenShown = pToken;
  }
  lv_label_set_text(lv_obj_get_child(stToken, 0), pToken.length() ? tokenShown.c_str() : "(none)");
  // The web login (ota.sh, browser): shown in full on purpose, it is made for the person holding the panel.
  if (stWebPw) {
    char t[64];
    if (!otapw::known()) snprintf(t, sizeof t, "making it...");
    else snprintf(t, sizeof t, "ota / %s%s", otapw::get(), otapw::fixedByBuild() ? "  (set in the build)" : "");
    lv_label_set_text(lv_obj_get_child(stWebPw, 0), t);
  }
  // Nothing changed: the same button is a plain Restart.
  bool dirty = settingsDirty();
  if (!lv_obj_has_state(stSaveBtn, LV_STATE_DISABLED))
    setButtonText(stSaveBtn, dirty ? "Save & restart" : LV_SYMBOL_REFRESH " Restart",
                  dirty ? Role::Start : Role::Neutral2);
  wifiSetupShow();
  pinSaveShow();
}

static lv_obj_t *valueField(lv_obj_t *parent, const char *title, lv_event_cb_t cb) {
  lv_obj_t *r = row(parent, ST_ROW);
  lv_obj_t *l = lv_label_create(r);
  lv_label_set_text(l, title);
  lv_obj_set_width(l, 150);
  lv_obj_t *b = button(r, "", 0, ST_ROW, Role::Neutral, cb);
  lv_obj_set_flex_grow(b, 1);
  lv_obj_t *v = lv_obj_get_child(b, 0);
  lv_obj_set_style_text_font(v, &lv_font_montserrat_20, 0);
  lv_obj_set_width(v, lv_pct(100));
  lv_label_set_long_mode(v, LV_LABEL_LONG_DOT);
  lv_obj_align(v, LV_ALIGN_LEFT_MID, 0, 0);
  return b;
}

static void editPass() {
  editText("Wi-Fi password", pPass.c_str(), true, [](const char *t, intptr_t) {
    pPass = t;
    settingsShow();
  });
}

// Tap on the web login: "Reset the web password?" with Cancel / Reset. The old password stops working at once.
static void webPwTap(lv_event_t *) {
  if (otapw::fixedByBuild() || !otapw::known()) return;  // dev build: the password is in secrets.h
  lv_obj_t *bg = lv_obj_create(lv_layer_top());
  lv_obj_set_size(bg, SCREEN_W, SCREEN_H);
  lv_obj_set_style_radius(bg, 0, 0);
  lv_obj_set_style_border_width(bg, 0, 0);
  theme::bind(bg, Prop::Bg, Role::Scrim);
  lv_obj_set_style_bg_opa(bg, LV_OPA_70, 0);
  lv_obj_remove_flag(bg, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *box = lv_obj_create(bg);
  lv_obj_set_size(box, 760, LV_SIZE_CONTENT);
  lv_obj_center(box);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(box, 24, 0);
  lv_obj_set_style_pad_row(box, 20, 0);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *t = lv_label_create(box);
  lv_label_set_text(t, "Reset the web password?\nA new one is made and shown here. The old one stops\n"
                       "working at once: ota.sh and any open browser login need the new one.");
  lv_obj_set_width(t, lv_pct(100));

  lv_obj_t *r = row(box, 64);
  button(r, "Cancel", 0, 64, Role::TextDim, [](lv_event_t *e) {
    lv_obj_delete_async((lv_obj_t *)lv_event_get_user_data(e));
  }, bg);
  lv_obj_set_flex_grow(lv_obj_get_child(r, 0), 1);
  button(r, "Reset password", 0, 64, Role::Danger2, [](lv_event_t *e) {
    otapw::reset();
    settingsShow();
    lv_obj_delete_async((lv_obj_t *)lv_event_get_user_data(e));
  }, bg);
  lv_obj_set_flex_grow(lv_obj_get_child(r, 1), 1);
}

static void networkChosen(lv_event_t *e) {
  pSsid = lv_list_get_button_text(lv_obj_get_parent(lv_event_get_target_obj(e)),
                                  lv_event_get_target_obj(e));
  lv_obj_delete_async((lv_obj_t *)lv_event_get_user_data(e));
  settingsShow();
  editPass();
}

static void showNetworks(int n) {
  lv_obj_t *bg = lv_obj_create(lv_layer_top());
  lv_obj_set_size(bg, SCREEN_W, SCREEN_H);
  lv_obj_set_style_radius(bg, 0, 0);
  lv_obj_set_style_border_width(bg, 0, 0);
  lv_obj_set_flex_flow(bg, LV_FLEX_FLOW_COLUMN);
  lv_obj_remove_flag(bg, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *top = row(bg, 56);
  lv_obj_t *t = lv_label_create(top);
  lv_label_set_text(t, n > 0 ? "Pick a network" : "No networks found");
  lv_obj_set_flex_grow(t, 1);
  button(top, "Cancel", 160, 52, Role::TextDim, [](lv_event_t *e) {
    lv_obj_delete_async((lv_obj_t *)lv_event_get_user_data(e));
  }, bg);

  lv_obj_t *list = lv_list_create(bg);
  lv_obj_set_width(list, lv_pct(100));
  lv_obj_set_flex_grow(list, 1);
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length()) continue;  // hidden network
    bool dup = false;
    for (int j = 0; j < i; j++) dup |= WiFi.SSID(j) == ssid;
    if (dup) continue;  // several APs, same network
    lv_obj_t *b = lv_list_add_button(list, LV_SYMBOL_WIFI, ssid.c_str());
    lv_obj_set_height(b, 56);
    lv_obj_add_event_cb(b, networkChosen, LV_EVENT_CLICKED, bg);
  }
  WiFi.scanDelete();
}

// `btn` shows "..." while the scan runs (the Settings tab's and the first-run screen's Scan buttons both use this).
static void scanPoll(lv_timer_t *t) {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  lv_obj_t *btn = (lv_obj_t *)lv_timer_get_user_data(t);
  lv_timer_delete(t);
  setButtonText(btn, "Scan", Role::Primary);
  showNetworks(n);  // n < 0: scan failed, shows "No networks found"
}

static void startScan(lv_obj_t *btn) {
  if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) return;
  WiFi.scanNetworks(true);
  setButtonText(btn, "...", Role::TextDim);
  lv_timer_create(scanPoll, 300, btn);
}

// Save the pending Wi-Fi / token and restart (the Save & restart button, the pinned bar and the first-run screen).
static void saveAndRestart() {
  if (settingsDirty()) {
    settings.wifiSsid = pSsid;
    settings.wifiPass = pPass;
    settings.token = pToken;
    settingsSave();  // plain Restart writes nothing
  }
  control::stopAndRestart();
}

// ---------- first-run Wi-Fi screen ----------
// Shown while no Wi-Fi is saved (the setup portal is up). Wi-Fi only: the Autoblow token and Bluetooth toys are set up
// later on the Settings and Devices tabs. The phone page (http://192.168.4.1/setup) stays as the fallback.

static lv_obj_t *wsBg, *wsSsid, *wsPass, *wsSave, *wsScan;
static bool wsDone;  // built once per boot (also after Skip)

static void wifiSetupShow() {
  if (!wsBg) return;
  lv_label_set_text(lv_obj_get_child(wsSsid, 0), pSsid.length() ? pSsid.c_str() : "(tap to pick or type)");
  lv_label_set_text(lv_obj_get_child(wsPass, 0),
                    pPass.length() ? LV_SYMBOL_BULLET LV_SYMBOL_BULLET LV_SYMBOL_BULLET LV_SYMBOL_BULLET LV_SYMBOL_BULLET
                                       LV_SYMBOL_BULLET
                                   : "(none: open network)");
  if (pSsid.length()) lv_obj_remove_state(wsSave, LV_STATE_DISABLED);
  else lv_obj_add_state(wsSave, LV_STATE_DISABLED);
}

static void buildWifiSetup(const char *why) {
  wsDone = true;
  lv_obj_t *bg = wsBg = lv_obj_create(lv_layer_top());
  lv_obj_set_size(bg, SCREEN_W, SCREEN_H);
  lv_obj_set_style_radius(bg, 0, 0);
  lv_obj_set_style_border_width(bg, 0, 0);
  theme::bind(bg, Prop::Bg, Role::OverlayBg);
  lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0);
  lv_obj_set_flex_flow(bg, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(bg, 20, 0);
  lv_obj_set_style_pad_row(bg, 12, 0);
  lv_obj_remove_flag(bg, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *t = lv_label_create(bg);
  lv_label_set_text(t, why ? "Wi-Fi is not connecting" : "Connect to Wi-Fi");
  lv_obj_set_style_text_font(t, &lv_font_montserrat_40, 0);
  theme::bind(t, Prop::Text, Role::OverlayText);

  if (why) {  // a saved network that fails: say why, the fields below are filled in for correcting
    lv_obj_t *wl = lv_label_create(bg);
    lv_label_set_text_fmt(wl, "Reason: %s. Check the name and password below.", why);
    theme::bind(wl, Prop::Text, Role::OverlayText);
  }
  wsSsid = valueField(bg, "Network", [](lv_event_t *) {
    editText("Wi-Fi network name", pSsid.c_str(), false, [](const char *s, intptr_t) {
      pSsid = s;
      pSsid.trim();
      settingsShow();
    });
  });
  wsScan = button(lv_obj_get_parent(wsSsid), "Scan", 140, ST_ROW, Role::Primary, [](lv_event_t *e) {
    startScan(lv_event_get_target_obj(e));
  });
  lv_obj_set_style_text_font(lv_obj_get_child(wsScan, 0), &lv_font_montserrat_20, 0);
  wsPass = valueField(bg, "Password", [](lv_event_t *) { editPass(); });

  lv_obj_t *r = row(bg, 64);
  wsSave = button(r, "Save & restart", 300, 60, Role::Start, [](lv_event_t *e) {
    lv_obj_add_state(lv_event_get_target_obj(e), LV_STATE_DISABLED);
    setButtonText(lv_event_get_target_obj(e), "Restarting...", Role::TextDim);
    saveAndRestart();
  });
  theme::bind(wsSave, Prop::Bg, Role::Disabled, LV_STATE_DISABLED);
  button(r, "Skip", 160, 60, Role::TextDim, [](lv_event_t *) {
    lv_obj_delete_async(wsBg);
    wsBg = nullptr;
  });

  lv_obj_t *hint = lv_label_create(bg);
  lv_obj_set_width(hint, lv_pct(100));
  lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
  theme::bind(hint, Prop::Text, Role::TextDim);
  lv_label_set_text(hint,
                    "Tap Scan to pick your network, then enter its password. The Autoblow token and Bluetooth toys are set up "
                    "later (Settings and Devices tabs).\n"
                    "If this screen looks garbled, or you prefer a phone: join Wi-Fi Twiddle-Setup (password twiddle-setup) "
                    "and open http://192.168.4.1/setup");
  wifiSetupShow();
}

// ---------- Save bar ----------
// Save & restart is at the bottom of a long Settings list: a bar stays in view while Wi-Fi or token have unsaved changes.

static lv_obj_t *pinSave;

static void pinSaveShow() {
  bool dirty = settingsDirty() && !wsBg;
  if (!pinSave) {
    if (!dirty) return;
    pinSave = button(lv_layer_top(), "Save & restart", 300, 64, Role::Start, [](lv_event_t *e) {
      lv_obj_add_state(lv_event_get_target_obj(e), LV_STATE_DISABLED);
      setButtonText(lv_event_get_target_obj(e), "Restarting...", Role::TextDim);
      saveAndRestart();
    });
    lv_obj_align(pinSave, LV_ALIGN_BOTTOM_RIGHT, -16, -16);
  }
  if (dirty) lv_obj_remove_flag(pinSave, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(pinSave, LV_OBJ_FLAG_HIDDEN);
}

// Software can't power the board off: the IP5306 charger has no data bus, and
// the board draws too much even in deep sleep for its light-load cut-off (tried
// 2026-10-18). So Power off explains the battery button and offers Screen off.
static void powerOffTap(lv_event_t *) {
  lv_obj_t *bg = lv_obj_create(lv_layer_top());
  lv_obj_set_size(bg, SCREEN_W, SCREEN_H);
  lv_obj_set_style_radius(bg, 0, 0);
  lv_obj_set_style_border_width(bg, 0, 0);
  theme::bind(bg, Prop::Bg, Role::Scrim);
  lv_obj_set_style_bg_opa(bg, LV_OPA_70, 0);
  lv_obj_remove_flag(bg, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *box = lv_obj_create(bg);
  lv_obj_set_size(box, 760, LV_SIZE_CONTENT);
  lv_obj_center(box);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(box, 24, 0);
  lv_obj_set_style_pad_row(box, 20, 0);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *t = lv_label_create(box);
  lv_label_set_text(t, "To power off, hold the battery button\non the panel for 2 seconds.\n"
                       "Press it briefly to turn back on.");
  lv_obj_set_width(t, lv_pct(100));

  lv_obj_t *r = row(box, 64);
  button(r, "Cancel", 0, 64, Role::TextDim, [](lv_event_t *e) {
    lv_obj_delete_async((lv_obj_t *)lv_event_get_user_data(e));
  }, bg);
  lv_obj_set_flex_grow(lv_obj_get_child(r, 0), 1);
  button(r, "Screen off", 0, 64, Role::Neutral2, [](lv_event_t *e) {
    // Stops the device; the next touch only wakes the screen.
    control::stopAll();
    lv_obj_delete_async((lv_obj_t *)lv_event_get_user_data(e));
    boardScreenOff();
  }, bg);
  lv_obj_set_flex_grow(lv_obj_get_child(r, 1), 1);
}

static void buildSettings(lv_obj_t *tab) {
  lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(tab, 6, 0);

  pSsid = settings.wifiSsid;
  pPass = settings.wifiPass;
  pToken = settings.token;

  stSsid = valueField(tab, "Wi-Fi", [](lv_event_t *) {
    editText("Wi-Fi network name", pSsid.c_str(), false, [](const char *t, intptr_t) {
      pSsid = t;
      settingsShow();
    });
  });
  stScanBtn = button(lv_obj_get_parent(stSsid), "Scan", 140, ST_ROW, Role::Primary, [](lv_event_t *e) {
    startScan(lv_event_get_target_obj(e));
  });
  lv_obj_set_style_text_font(lv_obj_get_child(stScanBtn, 0), &lv_font_montserrat_20, 0);
  stPass = valueField(tab, "Password", [](lv_event_t *) { editPass(); });
  stToken = valueField(tab, "Token", [](lv_event_t *) {
    editText("Autoblow device token", pToken.c_str(), false, [](const char *t, intptr_t) {
      pToken = t;
      pToken.trim();
      settingsShow();
    });
  });

  stWebPw = valueField(tab, "Web login", webPwTap);

  static const char *sleep[] = {"Never", "1 min", "5 min", "15 min", ""};
  static const int SLEEP_MIN[] = {0, 1, 5, 15};
  int sel = 2;
  for (int i = 0; i < 4; i++)
    if (SLEEP_MIN[i] == settings.sleepMin) sel = i;
  labeledSeg(tab, "Screen off", sleep, sel, [](int i) {
    settings.sleepMin = SLEEP_MIN[i];
    settingsSaveSleep();
  }, ST_ROW);

  // Wi-Fi gaps: how long the cloud device keeps its last speed without an
  // answer before the panel calls it lost and stops. Bluetooth: off lets Wi-Fi
  // run without modem sleep (the change restarts the panel).
  {
    lv_obj_t *gr = row(tab, ST_ROW);
    lv_obj_t *gl = lv_label_create(gr);
    lv_label_set_text(gl, "Wi-Fi gaps");
    lv_obj_set_width(gl, 130);
    static const char *grace[] = {"10 s", "20 s", "30 s", ""};
    lv_obj_t *gs = segmented(gr, grace, settings.graceS / 10 - 1, [](int i) {
      settings.graceS = (i + 1) * 10;
      settingsSaveGrace();
    });
    lv_obj_set_height(gs, ST_ROW);
    lv_obj_set_flex_grow(gs, 3);
    lv_obj_set_style_pad_all(gs, 4, 0);
    lv_obj_t *bl = lv_label_create(gr);
    lv_label_set_text(bl, LV_SYMBOL_BLUETOOTH " Bluetooth");
    lv_obj_set_style_margin_left(bl, 12, 0);
    static const char *bt[] = {"Off", "On", ""};
    lv_obj_t *bs = segmented(gr, bt, ble::enabled() ? 1 : 0, [](int i) {
      if (i == (ble::enabled() ? 1 : 0)) return;
      settings.bluetooth = i;
      settingsSaveBluetooth();
      control::stopAndRestart();  // Bluetooth starts (or not) at boot
    });
    lv_obj_set_height(bs, ST_ROW);
    lv_obj_set_flex_grow(bs, 2);
    lv_obj_set_style_pad_all(bs, 4, 0);
  }

#if BOARD_HAS_P4_I2C
  // Rotary encoder on the P4 connector (encoder.h): turn = speed, press = pause / resume, hold 1 s = STOP.
  // The mode decides who owns GPIO17/18 at boot, so changing it restarts the panel; step and direction apply at once.
  {
    static const char *mode[] = {"Auto", "Off", "On", ""};
    labeledSeg(tab, "Encoder", mode, settings.encoder < 0 ? 0 : settings.encoder == 0 ? 1 : 2, [](int i) {
      int m = i == 0 ? -1 : i == 1 ? 0 : 1;
      if (m == settings.encoder) return;
      settings.encoder = m;
      settingsSaveEncoder();
      control::stopAndRestart();
    }, ST_ROW);
    static const char *step[] = {"1 %", "2 %", "5 %", ""};
    labeledSeg(tab, "Encoder step", step, settings.encStep == 1 ? 0 : settings.encStep == 5 ? 2 : 1, [](int i) {
      settings.encStep = i == 0 ? 1 : i == 2 ? 5 : 2;
      settingsSaveEncoder();
    }, ST_ROW);
    static const char *dir[] = {"Clockwise = faster", "Reversed", ""};
    labeledSeg(tab, "Encoder turn", dir, settings.encInvert ? 1 : 0, [](int i) {
      settings.encInvert = i == 1;
      settingsSaveEncoder();
    }, ST_ROW);
  }
#endif

  lv_obj_t *dr = row(tab, ST_ROW);
  lv_obj_t *dl = lv_label_create(dr);
  lv_label_set_text(dl, "Display");
  lv_obj_set_width(dl, 150);
  lv_obj_t *fb = button(dr, LV_SYMBOL_REFRESH "  Flip screen", BOARD_DIMMABLE ? 230 : 0, ST_ROW,
                        Role::Neutral, [](lv_event_t *) {
    settings.flip = !settings.flip;  // a plain toggle: a Normal/Upside down selector turns with the screen
    settingsSaveFlip();
    boardSetFlip(settings.flip);
  });
  if (!BOARD_DIMMABLE) lv_obj_set_flex_grow(fb, 1);
  lv_obj_set_style_text_font(lv_obj_get_child(fb, 0), &lv_font_montserrat_20, 0);
#if BOARD_DIMMABLE
  // Brightness slider: applies live, saved when the finger lifts.
  lv_obj_t *bl = lv_label_create(dr);
  lv_label_set_text(bl, LV_SYMBOL_EYE_OPEN);
  lv_obj_t *bs = lv_slider_create(dr);
  lv_obj_set_size(bs, 290, 14);  // fixed: the knob overhangs the track by ~13 px at each end
  lv_obj_set_style_margin_left(bs, 10, 0);
  lv_obj_add_flag(dr, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_slider_set_range(bs, 10, 100);
  lv_slider_set_value(bs, settings.brightness, LV_ANIM_OFF);
  lv_obj_add_event_cb(bs, [](lv_event_t *e) {
    settings.brightness = lv_slider_get_value(lv_event_get_target_obj(e));
    boardSetBrightness(settings.brightness);
  }, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(bs, [](lv_event_t *) { settingsSaveBrightness(); }, LV_EVENT_RELEASED, nullptr);
#endif

  // Theme: applies live and is saved at once.
  {
    static const char *map[16];  // names, then the "" end marker
    int n = min(theme::count(), 15);
    for (int i = 0; i < n; i++) map[i] = theme::name(i);
    map[n] = "";
    stThemeSeg = labeledSeg(tab, "Theme", map, theme::current(), [](int i) {
      settings.theme = i;
      settingsSaveTheme();
      uiSetTheme(i);
    }, ST_ROW);
  }

  lv_obj_t *r = row(tab, 60);
  stInfo = lv_label_create(r);
  lv_obj_set_flex_grow(stInfo, 1);
  lv_obj_set_style_text_font(stInfo, &lv_font_montserrat_14, 0);
  stSaveBtn = button(r, "", 230, 60, Role::TextDim, [](lv_event_t *) {
    lv_obj_add_state(stSaveBtn, LV_STATE_DISABLED);
    lv_obj_add_state(stOffBtn, LV_STATE_DISABLED);
    setButtonText(stSaveBtn, "Restarting...", Role::TextDim);
    saveAndRestart();
  });
  theme::bind(stSaveBtn, Prop::Bg, Role::Disabled, LV_STATE_DISABLED);
  stOffBtn = button(r, LV_SYMBOL_POWER " Power off", 200, 60, Role::Danger2, powerOffTap);
  theme::bind(stOffBtn, Prop::Bg, Role::Disabled, LV_STATE_DISABLED);

  settingsShow();
}

// ---------- refresh ----------

static const char *modeText(const char *m) {
  if (!strcmp(m, "TARGET_SPEED_PLAYING")) return "Running";
  if (!strcmp(m, "TARGET_SPEED_PAUSED")) return "Paused";
  if (!strcmp(m, "LOCAL_SCRIPT_PLAYING")) return "Script playing";
  if (!strcmp(m, "LOCAL_SCRIPT_PAUSED")) return "Script paused";
  if (!strcmp(m, "ONLINE_CONNECTED")) return "Ready";
  if (!strcmp(m, "FIRMWARE_UPDATING")) return "Updating firmware";
  return m;
}

static void refresh(lv_timer_t *) {
  // The web password is made after Wi-Fi starts (otapw.h), i.e. after this tab was built: show it once it exists.
  static bool webPwShown;
  if (!webPwShown && otapw::known()) {
    webPwShown = true;
    settingsShow();
  }
  // Wi-Fi starts after the UI is built: once it reports "no network saved", offer the first-run Wi-Fi screen.
  wlan::Info w = wlan::info();
  // ... or a saved network that has not joined 45 s after boot for a reason the user can fix (name, password).
  if (!wsDone && wlan::portalActive()) buildWifiSetup(nullptr);
  else if (!wsDone && !w.up && w.fixable && millis() > 45000 && w.problem[0]) buildWifiSetup(w.problem);
  autoblow::Status st = autoblow::status();
  Active a = control::active();

  int bars = wlan::bars(w.rssi);

  // All devices once per refresh (row labels, status line, banner, speed).
  static devices::Info inf[DV_MAX];
  int n = min(devices::count(), DV_MAX);
  for (int i = 0; i < n; i++) inf[i] = devices::info(i);
  if (n < 1) n = 1;  // device 0 is always there; inf[0] stays zeroed if not
  int sel = selectedIndex(inf, n);
  const devices::Info &sd = inf[sel];

  // Top left: "<selected> v  <its state>", plus how many others follow too.
  lv_color_t dot;
  char text[224], state[128];  // text: the lost banner is the longest
  if (sel == 0) {
    // The Autoblow device: Wi-Fi and the cloud are its link.
    dot = themed(!st.wifi ? Role::Stop : st.online && !st.silentMs ? Role::Start : Role::Pause);
    if (st.online && (!st.wifi || st.silentMs)) {
      // Within the grace period (autoblow.cpp): the session carries on, and
      // stops only if the link isn't back in time.
      dot = themed(Role::Pause);
      if (st.silentMs)
        snprintf(state, sizeof state, "%s, reconnecting... %u s (stops at %u s)",
                 st.wifi ? "No answer" : "Wi-Fi dropped", (unsigned)(st.silentMs / 1000),
                 (unsigned)(st.graceMs / 1000));
      else
        snprintf(state, sizeof state, "%s, reconnecting...", st.wifi ? "No answer" : "Wi-Fi dropped");
    } else if (!st.wifi) {
      // Problems only the Settings tab can fix get a pointer there.
      snprintf(state, sizeof state, "No Wi-Fi: %s%s", w.problem[0] ? w.problem : "connecting...",
               w.fixable ? " Check Settings" : "");
    } else if (!st.online) {
      snprintf(state, sizeof state, "%s", st.error[0] ? st.error : "Connecting...");
    } else if (st.error[0]) {
      snprintf(state, sizeof state, "%s - %s", modeText(st.mode), st.error);
    } else if (bars == 1) {
      snprintf(state, sizeof state, "%s - weak Wi-Fi", modeText(st.mode));
    } else {
      snprintf(state, sizeof state, "%s", modeText(st.mode));
    }
  } else {
    dot = themed(sd.connected ? Role::Start : sd.connecting ? Role::Pause : Role::TextDim);
    if (sd.connected && sd.level > 0) snprintf(state, sizeof state, "Running %d%%", sd.level);
    else snprintf(state, sizeof state, "%s", sd.status);
  }
  int others = 0;
  for (int i = 0; i < n; i++) others += i != sel && inf[i].enabled && inf[i].connected;
  if (!sd.enabled) snprintf(text, sizeof text, "%s " LV_SYMBOL_DOWN "  %s, Follow off", sd.name, state);
  else if (others) snprintf(text, sizeof text, "%s " LV_SYMBOL_DOWN "  %s  +%d", sd.name, state, others);
  else snprintf(text, sizeof text, "%s " LV_SYMBOL_DOWN "  %s", sd.name, state);
  setLabel(statusLabel, text);
  lv_obj_set_style_bg_color(statusDot, dot, 0);

  static const Role BAR_COLOR[] = {Role::Stop, Role::Stop, Role::Pause, Role::Start, Role::Start};
  lv_obj_set_style_text_color(wifiLabel, themed(BAR_COLOR[bars]), 0);
  if (w.up) snprintf(text, sizeof text, LV_SYMBOL_WIFI " %d", w.rssi);
  else snprintf(text, sizeof text, LV_SYMBOL_WIFI " --");
  setLabel(wifiLabel, text);

  // Battery (only with a fuel gauge): stack it under the Wi-Fi signal.
  battery::Info b = battery::info();
  static bool hadGauge;
  if (b.present != hadGauge) {
    hadGauge = b.present;
    lv_obj_align(wifiLabel, LV_ALIGN_LEFT_MID, 418, b.present ? -11 : 0);
    lv_obj_set_flag(battLabel, LV_OBJ_FLAG_HIDDEN, !b.present);
  }
  if (b.present) {
    int pct = lroundf(b.percent);
    const char *icon = pct > 80 ? LV_SYMBOL_BATTERY_FULL : pct > 55 ? LV_SYMBOL_BATTERY_3
                     : pct > 30 ? LV_SYMBOL_BATTERY_2 : pct > 10 ? LV_SYMBOL_BATTERY_1
                     : LV_SYMBOL_BATTERY_EMPTY;
    snprintf(text, sizeof text, "%s%s %d%%", b.charging ? LV_SYMBOL_CHARGE " " : "", icon, pct);
    setLabel(battLabel, text);
    lv_obj_set_style_text_color(battLabel, themed(b.charging || pct > 20 ? Role::Start : pct > 10 ? Role::Pause : Role::Stop), 0);
  }

  bool moving = deviceMoving(st);

  // Nearly empty on battery: keep the devices stopped, so the panel can't die mid-run
  // and leave one running with nothing to stop it. Plugging in USB lifts this
  // (MAX17048 sees the charge; with a MAX17043 it lifts once the level passes 5 %).
  bool empty = b.present && !b.charging && b.percent <= 5;
  if (empty && a != Active::None) control::stopAll();
  if (empty) {
    snprintf(text, sizeof text, LV_SYMBOL_BATTERY_EMPTY " Battery at %d%%: control is paused until "
             "you plug in USB.", (int)lroundf(b.percent));
    setLabel(battBanner, text);
  }
  lv_obj_set_flag(battBanner, LV_OBJ_FLAG_HIDDEN, !empty);

  // Devices tab: rebuild the rows only when the list changed, else update in place.
  uint32_t sig = devSignature(inf, n);
  if (sig != dvSig) {
    dvSig = sig;
    devBuildRows(inf, n);
  }
  for (int i = 0; i < dvShown; i++) devUpdateRow(dvRows[i], inf[i], i == sel);
  bool scanning = devices::scanning();
  if (!ble::enabled())
    setButtonText(dvScanBtn, lv_obj_has_state(dvScanBtn, LV_STATE_DISABLED) ? "Restarting..." : LV_SYMBOL_BLUETOOTH " Turn on", Role::Primary);
  else
    setButtonText(dvScanBtn, scanning ? "Scanning..." : LV_SYMBOL_REFRESH " Scan", scanning ? Role::TextDim : Role::Primary);

  // Lost: a switched-on device that was moving when we last heard from it is
  // unreachable now (STOP can't get to it). Each device remembers the last
  // thing it told us while it was reachable.
  bool lost = false;
  const char *lostWho = nullptr;
  for (int i = 0; i < n; i++) {
    DevRow &r = dvRows[i];
    if (!inf[i].enabled) {
      r.wasMoving = false;
      continue;
    }
    bool up = i == 0 ? st.online : inf[i].connected;
    if (up) {
      r.wasMoving = i == 0 ? moving : inf[i].level > 0;
    } else if (r.wasMoving) {
      lost = true;
      // Name the one devices:: reported, if it is one of these.
      if (!lostWho || !strcmp(inf[i].name, devices::lostName())) lostWho = inf[i].name;
    }
  }
  if (lost) {
    snprintf(text, sizeof text, LV_SYMBOL_WARNING " Connection lost while %s was running. It may "
             "still be moving: switch it off on the device. The panel sends STOP as soon as it "
             "reconnects.", lostWho);
    setLabel(lostBanner, text);
  }
  lv_obj_set_flag(lostBanner, LV_OBJ_FLAG_HIDDEN, !lost);

  // Speed: the VacuGlide's when it moves, else the highest Bluetooth level.
  int level = 0;
  for (int i = 1; i < n; i++)
    if (inf[i].enabled && inf[i].connected) level = max(level, inf[i].level);
  if (moving) snprintf(text, sizeof text, "%d%%", st.targetSpeed);
  else if (level > 0) snprintf(text, sizeof text, "%d%%", level);
  else text[0] = 0;
  setLabel(speedLabel, text);

  // Controls of the selected device: valves (Looser/Tighter, vacuum) and the
  // Scripts tab only for a VacuGlide. Changed only when it flips.
  bool valves = sd.caps & devices::CAP_VALVES;
  static int valvesShown = -1;
  if (valves != valvesShown) {
    valvesShown = valves;
    if (!valves) {
      control::tighter(false);  // a hidden button never sees its release
      control::looser(false);
    }
    for (int i = 0; i < valveRowCount; i++) lv_obj_set_flag(valveRows[i], LV_OBJ_FLAG_HIDDEN, !valves);
    lv_obj_t *scriptsTab = lv_obj_get_child(lv_tabview_get_tab_bar(tabview), 3);
    lv_obj_set_flag(scriptsTab, LV_OBJ_FLAG_HIDDEN, !valves);
    if (!valves && lv_tabview_get_tab_active(tabview) == 3) lv_tabview_set_active(tabview, 0, LV_ANIM_OFF);
  }
  // What the Remote slider does on it.
  const char *what;
  if (sel == 0) {
    what = st.type == autoblow::Type::VacuPump ? "vacuum depth"
         : st.type == autoblow::Type::Ultra ? "stroke speed" : "speed";
  } else {
    what = sd.kind == devices::Kind::Ossm ? "speed (knob caps it)"
         : sd.caps & devices::CAP_STROKE ? "stroke speed"
         : sd.caps & devices::CAP_AIR ? "vibration + air"
         : sd.caps & devices::CAP_VIBRATE ? "vibration" : "speed";
  }
  snprintf(state, sizeof state, "%s: %s", sd.name, what);
  setLabel(rWhatLabel, state);

  // Remote
  if (a == Active::Remote) setButtonText(rStartBtn, LV_SYMBOL_PAUSE " Pause", Role::Pause);
  else setButtonText(rStartBtn, LV_SYMBOL_PLAY " Start", Role::Start);
  if (!lv_obj_has_state(rSlider, LV_STATE_PRESSED) &&
      lv_slider_get_value(rSlider) != control::remoteSpeed()) {
    lv_slider_set_value(rSlider, control::remoteSpeed(), LV_ANIM_OFF);
    lv_label_set_text_fmt(rSliderLabel, "%d%%", control::remoteSpeed());
  }

  // Autopilot
  uint32_t sess = control::autopilotSessionMs(), played = control::autopilotElapsedMs();
  bool hrMode = control::autopilotHrEdging();
  control::HrStatus hr = control::autopilotHrStatus();
  if (hrMode) {
    static const char *PHASE[] = {"climbing", "EDGE, backing off", "resting", "signal lost, backing off", "done"};
    char edges[24];
    if (hr.maxEdges) snprintf(edges, sizeof edges, "%d/%d", hr.edges, hr.maxEdges);
    else snprintf(edges, sizeof edges, "%d", hr.edges);
    char bpmText[16];
    if (hr.bpm) snprintf(bpmText, sizeof bpmText, "%d bpm", hr.bpm);
    else snprintf(bpmText, sizeof bpmText, "no bpm");
    if (a == Active::Autopilot)
      snprintf(text, sizeof text, "%s  %d%%  %s\nedge at %d bpm, edges %s", bpmText, control::autopilotSpeed(),
               PHASE[constrain(hr.phase, 0, 4)], hr.edgeBpm, edges);
    else if (hr.end)
      snprintf(text, sizeof text, "%s\n%s", hr.end == 1 ? "HR session done" : hr.end == 2 ? "HR session: time is up"
               : "Stopped: heart rate signal lost", bpmText);
    else
      snprintf(text, sizeof text, "HR edging standby\n%s", hrm::connected() ? bpmText : "no monitor connected");
    setButtonText(apStartBtn, a == Active::Autopilot ? LV_SYMBOL_PAUSE " Pause" : LV_SYMBOL_PLAY " Start",
                  a == Active::Autopilot ? Role::Pause : Role::Start);
  } else if (a == Active::Autopilot) {
    setButtonText(apStartBtn, LV_SYMBOL_PAUSE " Pause", Role::Pause);
    uint32_t peakIn = control::autopilotNextPeakMs() / 1000, left = (sess - min(played, sess)) / 1000;
    if (peakIn) snprintf(text, sizeof text, "%d%%  peak in %lu:%02lu\n%lu:%02lu left", control::autopilotSpeed(),
                         peakIn / 60, peakIn % 60, left / 60, left % 60);
    else snprintf(text, sizeof text, "%d%%  at the peak\n%lu:%02lu left", control::autopilotSpeed(), left / 60, left % 60);
  } else {
    setButtonText(apStartBtn, LV_SYMBOL_PLAY " Start", Role::Start);
    if (apDraw) snprintf(text, sizeof text, "Draw: drag the graph\nto set the peaks");
    else if (sess) snprintf(text, sizeof text, "Standby\nsession %lu min", (sess + 30000) / 60000);
    else snprintf(text, sizeof text, "Standby");
  }
  setLabel(apStateLabel, text);

  if (hrMode) {
    if (hrm::connected()) {
      if (hr.bpm) snprintf(state, sizeof state, "%d bpm", hr.bpm);
      else snprintf(state, sizeof state, "waiting for a reading");
      snprintf(text, sizeof text, "Monitor connected: %s. Edge now at %d bpm.", state, hr.edgeBpm);
    } else {
      snprintf(text, sizeof text,
               "No heart rate monitor connected: pair one on the Devices tab (it uses one of the 3 Bluetooth links).");
    }
  } else {
    snprintf(text, sizeof text, "Off: Autopilot plays its planned waves. On: the heart rate decides when to edge; "
             "Intensity and Length still apply.");
  }
  setLabel(hrLiveLabel, text);

  // The graph: a moving window on the session (the last 10 s, the next 30 s, "now" at 25 %), or the whole session
  // while Draw is on. The chart's own series buffers are reused: a point is written only when its value changed,
  // and the window is rebuilt at most twice a second.
  static uint8_t hrSpdHist[11], hrBpmHist[11];  // HR mode: speed and bpm each second, [0] = now
  static int hrHistN;
  static uint32_t hrHistSec = ~0u, lastDrawMs;
  static int8_t lastMode = -1;
  if (hrMode) {
    uint32_t sec = played / 1000;
    if (sec != hrHistSec) {
      if (hrHistSec == ~0u || sec < hrHistSec) hrHistN = 0;  // a new session
      memmove(hrSpdHist + 1, hrSpdHist, sizeof hrSpdHist - 1);
      memmove(hrBpmHist + 1, hrBpmHist, sizeof hrBpmHist - 1);
      hrHistN = min(hrHistN + 1, 11);
      hrHistSec = sec;
    }
    hrSpdHist[0] = control::autopilotSpeed();  // [0] follows the live values within the second
    hrBpmHist[0] = hr.bpm > 255 ? 255 : hr.bpm;
  } else {
    hrHistSec = ~0u;
  }
  bool whole = apDraw && !hrMode;
  int8_t mode = whole ? 2 : hrMode ? 1 : 0;
  if (mode == 2 || mode != lastMode || (uint32_t)(lv_tick_get() - lastDrawMs) >= 500) {
    lastDrawMs = lv_tick_get();
    if (mode != lastMode) {
      lastMode = mode;
      if (hrMode) lv_obj_remove_flag(apLblNote, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(apLblNote, LV_OBJ_FLAG_HIDDEN);
      if (whole) lv_obj_add_flag(apLblNow, LV_OBJ_FLAG_HIDDEN); else lv_obj_remove_flag(apLblNow, LV_OBJ_FLAG_HIDDEN);
    }
    apWinWhole = whole;
    apWinNow = played;
    const int ni = whole ? (sess ? min<int>(AP_POINTS - 1, (uint64_t)played * AP_POINTS / sess) : 0) : apwin::nowIndex(AP_POINTS);
    int32_t *py = lv_chart_get_series_y_array(apChart, apPast);
    int32_t *fy = lv_chart_get_series_y_array(apChart, apFuture);
    int32_t *hy = lv_chart_get_series_y_array(apChart, apHrSeries);
    auto put = [](int32_t *arr, lv_chart_series_t *ser, int i, int v) {
      int32_t want = v > 0 ? v : LV_CHART_POINT_NONE;
      if (arr[i] != want) lv_chart_set_value_by_id(apChart, ser, i, want);
    };
    for (int i = 0; i < AP_POINTS; i++) {
      int past = 0, fut = 0, hrv = 0;
      if (whole) {
        past = fut = sess ? control::autopilotSpeedAt((uint64_t)i * sess / AP_POINTS) : 0;
        if (i < ni) fut = 0;
        if (i > ni) past = 0;
        if (i == ni) past = fut;
      } else {
        int64_t t = apwin::timeAt(i, AP_POINTS, played);
        if (hrMode) {
          // Actual speed and bpm of the last 10 s; the future depends on the heart rate, so it is not planned: only the
          // current target as a dashed flat line.
          if (i <= ni) {
            int age = apwin::ageSec(t, played);
            if (age >= 0 && age < hrHistN) {
              past = hrSpdHist[age];
              hrv = hrBpmHist[age] ? constrain((hrBpmHist[age] - 40) * 100 / 180, 1, 100) : 0;
            }
          }
          if (i >= ni && (i == ni || apwin::dashOn(i)) && control::autopilotSpeed() > 0) fut = control::autopilotSpeed();
        } else if (sess && apwin::inSession(t, sess)) {
          int v = control::autopilotSpeedAt((uint32_t)t);
          if (i <= ni) past = v;
          if (i >= ni) fut = v;
        }
      }
      put(py, apPast, i, past);
      put(fy, apFuture, i, fut);
      put(hy, apHrSeries, i, hrv);
    }
    lv_chart_set_cursor_point(apChart, apCursor, apFuture, ni);
    // Axis: seconds relative to now, or the session length when the whole session is shown.
    char ax[16];
    if (whole) {
      snprintf(ax, sizeof ax, "0:00");
      setLabel(apLblL, ax);
      snprintf(ax, sizeof ax, "%lu:%02lu", (unsigned long)(sess / 60000), (unsigned long)(sess / 1000 % 60));
      setLabel(apLblR, ax);
    } else {
      snprintf(ax, sizeof ax, "%+d s", apwin::secondsRel(apwin::timeAt(0, AP_POINTS, played), played));
      setLabel(apLblL, ax);
      snprintf(ax, sizeof ax, "%+d s", apwin::secondsRel(apwin::timeAt(AP_POINTS - 1, AP_POINTS, played), played));
      setLabel(apLblR, ax);
    }
  }

  // Scripts
  static int lastSlot = -2;
  int slot = control::scriptSlot();
  if (slot != lastSlot) {
    for (int i = 0; i < SCRIPT_SLOTS; i++) lv_obj_set_state(scButtons[i], LV_STATE_CHECKED, i == slot);
    lastSlot = slot;
  }
  if (slot < 0) setButtonText(scToggleBtn, "Tap a slot to play", Role::TextDim);
  else if (control::scriptPlaying()) setButtonText(scToggleBtn, LV_SYMBOL_PAUSE " Pause", Role::Pause);
  else setButtonText(scToggleBtn, LV_SYMBOL_PLAY " Resume", Role::Start);

  // Settings
  char battLine[64];
  if (b.present)
    snprintf(battLine, sizeof battLine, "Battery %d%%, %.2f V%s (%s)", (int)lroundf(b.percent),
             b.volts, b.charging ? ", charging" : "", b.chip);
  else
    snprintf(battLine, sizeof battLine, "Battery level: no battery or gauge found");
  if (w.up)
    snprintf(text, sizeof text, "Panel %s, signal %d dBm\n%s\nVersion " FW_VERSION ", built " __DATE__,
             WiFi.localIP().toString().c_str(), w.rssi, battLine);
  else snprintf(text, sizeof text, "Wi-Fi: %s\n%s\nVersion " FW_VERSION ", built " __DATE__, w.problem, battLine);
  setLabel(stInfo, text);

  // Dim after a minute without a touch (STOP stays readable), full brightness
  // again at the next touch. Screen sleep only while nothing runs and no device
  // moves, so STOP is always one tap away.
  uint32_t idle = lv_display_get_inactive_time(nullptr);
  static bool dimmed;
  bool dim = idle > 60000 && BOARD_DIMMABLE;
  if (dim != dimmed) {
    dimmed = dim;
    boardSetBrightness(dim ? max(8, settings.brightness / 4) : settings.brightness);
  }
  if (boardScreenIsOn() && settings.sleepMin > 0 && a == Active::None && !lost &&
      !devices::anyMoving() && idle > settings.sleepMin * 60000u)
    boardScreenOff();
}

// ---------- themes ----------

// After a switch: what ui.cpp keeps or recomputes itself. The registry (theme.cpp) already
// re-applied every bound colour; the chart's series have no style, and the Devices rows cache
// what they last painted.
static void themeChanged() {
  apPast->color = themed(Role::ChartPast);
  apFuture->color = themed(Role::ChartFuture);
  apHrSeries->color = themed(Role::Stop);
  apCursor->color = themed(Role::Pause);
  lv_obj_invalidate(apChart);
  if (stThemeSeg) {
    lv_buttonmatrix_set_button_ctrl(stThemeSeg, theme::current(), LV_BUTTONMATRIX_CTRL_CHECKED);
    for (int i = 0; i < theme::count(); i++)
      if (i != theme::current()) lv_buttonmatrix_clear_button_ctrl(stThemeSeg, i, LV_BUTTONMATRIX_CTRL_CHECKED);
  }
  for (DevRow &r : dvRows) r.dotState = r.selected = r.layoutKey = -1;
  refresh(nullptr);  // status dot, battery, Wi-Fi bars, state buttons: read the new palette
}

// Async: never inside the event of a widget that the switch restyles. Callable under UiLock from
// the web task too (the switch then runs on the loop task's next lv_timer_handler()).
void uiSetTheme(int idx) {
  lv_async_call([](void *p) { theme::apply((int)(intptr_t)p); }, (void *)(intptr_t)idx);
}

static SemaphoreHandle_t lvMutex, callSlot, callDone;
static void (*callFn)(void *);
static void *callArg;

void uiLock() { xSemaphoreTakeRecursive(lvMutex, portMAX_DELAY); }
void uiUnlock() { xSemaphoreGiveRecursive(lvMutex); }

void uiCall(void (*fn)(void *), void *arg) {
  xSemaphoreTake(callSlot, portMAX_DELAY);  // one caller at a time
  callArg = arg;
  callFn = fn;  // loop() picks it up
  xSemaphoreTake(callDone, portMAX_DELAY);
  xSemaphoreGive(callSlot);
}

void uiPump() {
  if (!callFn) return;
  callFn(callArg);
  callFn = nullptr;
  xSemaphoreGive(callDone);
}

// ---------- display trial / revert banner ----------
// A pixel clock chosen on the setup page stays "on trial" until Keep is tapped (pclkfb.h). After a revert, a notice says so
// once. Both sit on the top layer along the bottom edge, above the tabs. Guition only: the Waveshare's clock is fixed.
#if !defined(BOARD_WAVESHARE_LCD5)
static void showDisplayBanner() {
  if (!settings.pclkTrial && settings.pclkRevertedFrom < 0) return;
  lv_obj_t *bar = lv_obj_create(lv_layer_top());
  lv_obj_set_size(bar, SCREEN_W, LV_SIZE_CONTENT);
  lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_radius(bar, 0, 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  theme::bind(bar, Prop::Bg, Role::Accent);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(bar, 10, 0);
  lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  char text[200];
  if (settings.pclkTrial)
    snprintf(text, sizeof text,
             "Display pixel clock %d MHz is on trial (boot %d of %d). Tap Keep if the picture is right; otherwise it goes back to %d MHz.",
             settings.pclkMHz, settings.pclkBoots, pclkfb::TRIAL_BOOTS, settings.pclkPrev);
  else
    snprintf(text, sizeof text, "The display pixel clock %d MHz was not confirmed, so the panel went back to %d MHz.",
             settings.pclkRevertedFrom, settings.pclkMHz);
  lv_obj_t *l = lv_label_create(bar);
  lv_label_set_text(l, text);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
  lv_obj_set_flex_grow(l, 1);
  bool trial = settings.pclkTrial;
  button(bar, trial ? "Keep" : "OK", 180, 60, Role::Start, [](lv_event_t *e) {
    if (settings.pclkTrial) settingsConfirmPclk();
    settings.pclkRevertedFrom = -1;
    lv_obj_delete_async((lv_obj_t *)lv_event_get_user_data(e));
  }, bar);
}
#endif

void uiBegin() {
  lvMutex = xSemaphoreCreateRecursiveMutex();
  callSlot = xSemaphoreCreateMutex();
  callDone = xSemaphoreCreateBinary();
  lv_obj_t *scr = lv_screen_active();
  theme::begin(lv_display_get_default(), settings.theme);
  theme::onChange(themeChanged);
  theme::bind(scr, Prop::Bg, Role::Bg);

  buildTopBar(scr);

  lv_obj_t *tv = tabview = lv_tabview_create(scr);
  lv_obj_set_size(tv, SCREEN_W, SCREEN_H - 64);
  lv_obj_align(tv, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_tabview_set_tab_bar_size(tv, 52);

  buildRemote(lv_tabview_add_tab(tv, "Remote"));
  buildAutopilot(lv_tabview_add_tab(tv, "Autopilot"));
  buildSession(lv_tabview_add_tab(tv, "Session"));
  buildScripts(lv_tabview_add_tab(tv, "Scripts"));
  buildDevices(lv_tabview_add_tab(tv, "Devices"));
  buildSettings(lv_tabview_add_tab(tv, LV_SYMBOL_SETTINGS " Settings"));

  lv_timer_create(refresh, 200, nullptr);
  refresh(nullptr);
#if !defined(BOARD_WAVESHARE_LCD5)
  showDisplayBanner();
#endif
}

void uiShowTab(int tab) {
  lv_tabview_set_active(tabview, constrain(tab, 0, 5), LV_ANIM_OFF);
  refresh(nullptr);
  lv_refr_now(nullptr);
}

// ---------- firmware update screen ----------

static lv_obj_t *updScreen, *updLabel;

void uiShowUpdating(int pct) {
  if (!updScreen) {
    updScreen = lv_obj_create(lv_layer_top());
    lv_obj_set_size(updScreen, SCREEN_W, SCREEN_H);
    lv_obj_set_style_radius(updScreen, 0, 0);
    lv_obj_set_style_border_width(updScreen, 0, 0);
    theme::bind(updScreen, Prop::Bg, Role::OverlayBg);
    lv_obj_set_style_bg_opa(updScreen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(updScreen, LV_OBJ_FLAG_SCROLLABLE);
    updLabel = lv_label_create(updScreen);
    lv_obj_set_style_text_font(updLabel, &lv_font_montserrat_40, 0);
    theme::bind(updLabel, Prop::Text, Role::OverlayText);
    lv_obj_set_style_text_align(updLabel, LV_TEXT_ALIGN_CENTER, 0);
    // Near the top: the rest of the screen is plain black, which looks the
    // same even when the display briefly repeats a strip during a flash write.
    lv_obj_align(updLabel, LV_ALIGN_TOP_MID, 0, 24);
  }
  if (pct < 0) lv_label_set_text(updLabel, "Updating firmware");
  else lv_label_set_text_fmt(updLabel, "Updating firmware  %d%%", pct);
  lv_refr_now(nullptr);
}

void uiHideUpdating() {
  if (!updScreen) return;
  lv_obj_delete(updScreen);
  updScreen = updLabel = nullptr;
}
