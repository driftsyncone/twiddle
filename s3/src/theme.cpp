#include "theme.h"

#include <esp_heap_caps.h>
#include <lvgl_private.h>
#include <math.h>
#include <string.h>

namespace theme {

// ---------------------------------------------------------------- palettes
// Column order = enum Role. ctrl/ctrl2: what LVGL's own widgets (tab underline, slider, switch,
// chips) use as primary/secondary; Daylight keeps LVGL's stock blue/red so it looks as it always did.
struct Palette {
  const char *name;
  bool dark;
  bool legacy;  // Daylight: button/banner text stays the fixed white/black it always was
  uint32_t ctrl, ctrl2;
  uint32_t c[(int)Role::Count];
};

static const Palette PALETTES[] = {
  //            bg        surface   surfAlt   text      textDim   primary   start     pause     stop
  //            neutral   neutral2  accent    disabled  danger2   chartBg   chartPast chartFut  ovlBg     ovlText   scrim     highlight
  {"Daylight", false, true, 0x2196F3, 0xF44336, {
     0xF5F5F5, 0xFFFFFF, 0xE0E0E0, 0x212121, 0x9E9E9E, 0x00838F, 0x2E7D32, 0xEF6C00, 0xD32F2F,
     0x37474F, 0x455A64, 0xC2185B, 0x424242, 0x8E2A2A, 0x1A1F23, 0x546E7A, 0x4FC3F7, 0x121212, 0x4FC3F7, 0x000000, 0xFFFFFF}},
  {"Midnight", true, false, 0x26C6DA, 0xD6336C, {
     0x0E1014, 0x171A21, 0x1F232C, 0xE8EAED, 0x9AA0A6, 0x26C6DA, 0x2EA24F, 0xF59E0B, 0xE5484D,
     0x2B313C, 0x394150, 0xD6336C, 0x2A2E36, 0x8E2A2A, 0x0A0C10, 0x5B6B7B, 0x4FC3F7, 0x0E1014, 0x4FC3F7, 0x000000, 0xFFFFFF}},
  {"Ocean", true, false, 0x29B6F6, 0x7C4DFF, {
     0x0B1622, 0x112236, 0x173049, 0xE3F2FD, 0x90A4AE, 0x29B6F6, 0x26A69A, 0xFFB74D, 0xEF5350,
     0x1E3A56, 0x28506F, 0x7C4DFF, 0x24384A, 0x8E2A2A, 0x08111A, 0x4A6E8C, 0x80DEEA, 0x0B1622, 0x80DEEA, 0x000000, 0xFFFFFF}},
  {"Forest", true, false, 0x4DB6AC, 0xD4A017, {
     0x0F1A14, 0x15241B, 0x1D3125, 0xE8F5E9, 0xA5B8A9, 0x4DB6AC, 0x66BB6A, 0xFFA726, 0xEF5350,
     0x243B2D, 0x2F4E3B, 0xD4A017, 0x26352B, 0x8E2A2A, 0x0B140F, 0x5B7F66, 0xA5D6A7, 0x0F1A14, 0xA5D6A7, 0x000000, 0xFFFFFF}},
  {"Ember", true, false, 0xFF7043, 0xF06292, {
     0x17110F, 0x221915, 0x2C211C, 0xFBE9E7, 0xBCAAA4, 0xFF7043, 0x7CB342, 0xFFCA28, 0xD9362F,
     0x3B2D27, 0x4E3B33, 0xF06292, 0x3A2E29, 0x8E2A2A, 0x0F0B0A, 0x8D6E63, 0xFFAB91, 0x17110F, 0xFFAB91, 0x000000, 0xFFFFFF}},
  {"Blush", false, false, 0xD81B60, 0x6A1B9A, {
     0xFFF8F6, 0xFFFFFF, 0xF7E9E6, 0x3E2723, 0x7D5F54, 0xD81B60, 0x2E7D32, 0xEF6C00, 0xC62828,
     0x6D4C41, 0x8D6E63, 0x6A1B9A, 0xBCAAA4, 0x8E2A2A, 0x3E2723, 0xA1887F, 0xF48FB1, 0xFFF8F6, 0xD81B60, 0x000000, 0x3E2723}},
};
static const int N = sizeof PALETTES / sizeof *PALETTES;

static int cur = 0;
static lv_color_t cache[(int)Role::Count];

int count() { return N; }
const char *name(int idx) { return PALETTES[idx >= 0 && idx < N ? idx : 0].name; }
int current() { return cur; }
lv_color_t color(Role r) { return cache[(int)r]; }

// ---------------------------------------------------------------- contrast
static float lin(uint8_t v) {
  float s = v / 255.0f;
  return s <= 0.03928f ? s / 12.92f : powf((s + 0.055f) / 1.055f, 2.4f);
}
static float lum(lv_color_t c) { return 0.2126f * lin(c.red) + 0.7152f * lin(c.green) + 0.0722f * lin(c.blue); }
static float contrast(lv_color_t a, lv_color_t b) {
  float x = lum(a), y = lum(b);
  if (x < y) { float t = x; x = y; y = t; }
  return (x + 0.05f) / (y + 0.05f);
}
// Text colour for a filled area: the fixed legacy colour in Daylight, else whichever of white or
// near-black reads better (white preferred while it reaches AA).
static lv_color_t onColor(lv_color_t bg, lv_color_t legacy) {
  if (PALETTES[cur].legacy) return legacy;
  lv_color_t white = lv_color_white(), dark = lv_color_hex(0x10141A);
  float cw = contrast(white, bg), cd = contrast(dark, bg);
  return cw >= 4.5f || cw >= cd ? white : dark;
}

// ---------------------------------------------------------------- theme layer
// LVGL's default theme (re-initialised per palette: dark flag, ctrl colours) supplies the widget shapes;
// this layer re-colours its neutral surfaces from the palette. Styles are shared by every object, so
// a switch only rewrites them and reports a style change.
static lv_style_t sScr, sCard, sSurface, sGrey, sItem, sOnCtrl, sKnob, sChart;
static lv_theme_t *layer;
static lv_theme_t *base;
static lv_display_t *disp;
static bool stylesInited;

static void buildStyles() {
  const Palette &p = PALETTES[cur];
  lv_style_t *all[] = {&sScr, &sCard, &sSurface, &sGrey, &sItem, &sOnCtrl, &sKnob, &sChart};
  for (lv_style_t *s : all) {
    if (stylesInited) lv_style_reset(s);
    else lv_style_init(s);
  }
  stylesInited = true;
  lv_color_t bg = color(Role::Bg), surf = color(Role::Surface), alt = color(Role::SurfaceAlt), tx = color(Role::Text);
  lv_style_set_bg_color(&sScr, bg);
  lv_style_set_text_color(&sScr, tx);
  lv_style_set_bg_color(&sCard, surf);
  lv_style_set_border_color(&sCard, alt);
  lv_style_set_text_color(&sCard, tx);
  lv_style_set_bg_color(&sSurface, surf);
  lv_style_set_border_color(&sSurface, alt);
  lv_style_set_text_color(&sSurface, tx);
  lv_style_set_bg_color(&sGrey, alt);
  lv_style_set_text_color(&sGrey, tx);
  lv_style_set_bg_color(&sItem, alt);
  lv_style_set_text_color(&sItem, tx);
  lv_style_set_text_color(&sOnCtrl, onColor(lv_color_hex(p.ctrl), lv_color_white()));
  lv_style_set_bg_color(&sKnob, p.dark ? tx : lv_color_white());
  lv_style_set_line_color(&sChart, alt);
}

static void applyCb(lv_theme_t *, lv_obj_t *obj) {
  lv_obj_t *parent = lv_obj_get_parent(obj);
  if (!parent) { lv_obj_add_style(obj, &sScr, 0); return; }
  if (lv_obj_check_type(obj, &lv_obj_class)) {
    lv_obj_t *grand = lv_obj_get_parent(parent);
    if (lv_obj_check_type(parent, &lv_tabview_class) && lv_obj_get_child(parent, 1) == obj) return;  // content area
    if (lv_obj_check_type(parent, &lv_tabview_class) && lv_obj_get_child(parent, 0) == obj) {  // tab bar
      lv_obj_add_style(obj, &sSurface, 0);
      return;
    }
    if (grand && lv_obj_check_type(grand, &lv_tabview_class)) return;  // a tab page
    lv_obj_add_style(obj, &sCard, 0);
  } else if (lv_obj_check_type(obj, &lv_button_class)) {
    lv_obj_t *tv = lv_obj_get_parent(parent);
    if (tv && lv_obj_get_child(tv, 0) == parent && lv_obj_check_type(tv, &lv_tabview_class)) return;  // tab button
    lv_obj_add_style(obj, &sOnCtrl, 0);
  } else if (lv_obj_check_type(obj, &lv_buttonmatrix_class)) {
    lv_obj_add_style(obj, &sCard, 0);
    lv_obj_add_style(obj, &sItem, LV_PART_ITEMS);
    lv_obj_add_style(obj, &sOnCtrl, (lv_style_selector_t)LV_PART_ITEMS | (lv_style_selector_t)LV_STATE_CHECKED);
  } else if (lv_obj_check_type(obj, &lv_switch_class)) {
    lv_obj_add_style(obj, &sGrey, 0);
    lv_obj_add_style(obj, &sKnob, LV_PART_KNOB);
  } else if (lv_obj_check_type(obj, &lv_chart_class)) {
    lv_obj_add_style(obj, &sCard, 0);
    lv_obj_add_style(obj, &sChart, 0);
  } else if (lv_obj_check_type(obj, &lv_textarea_class)) {
    lv_obj_add_style(obj, &sCard, 0);
  } else if (lv_obj_check_type(obj, &lv_keyboard_class)) {
    lv_obj_add_style(obj, &sScr, 0);
    lv_obj_add_style(obj, &sSurface, LV_PART_ITEMS);
    lv_obj_add_style(obj, &sGrey, (lv_style_selector_t)LV_PART_ITEMS | (lv_style_selector_t)LV_STATE_CHECKED);
  } else if (lv_obj_check_type(obj, &lv_list_class)) {
    lv_obj_add_style(obj, &sCard, 0);
  } else if (lv_obj_check_type(obj, &lv_tabview_class)) {
    lv_obj_add_style(obj, &sScr, 0);
  }
}

// ---------------------------------------------------------------- registry
struct Entry {
  lv_obj_t *obj;
  Prop prop;
  Role role;
  lv_style_selector_t sel;
};
static Entry *reg;
static int regN, regCap;

static const int MAX_HOOKS = 4;
static void (*hooks[MAX_HOOKS])();
static int hookN;
void onChange(void (*fn)()) {
  if (hookN < MAX_HOOKS) hooks[hookN++] = fn;
}

static void unbindObj(lv_event_t *e) {
  lv_obj_t *o = lv_event_get_target_obj(e);
  int w = 0;
  for (int i = 0; i < regN; i++)
    if (reg[i].obj != o) reg[w++] = reg[i];
  regN = w;
}

void syncText(lv_obj_t *o) {
  lv_obj_t *target;
  lv_color_t legacy;
  if (lv_obj_check_type(o, &lv_button_class)) {
    target = lv_obj_get_child(o, 0);
    legacy = lv_color_white();
  } else if (lv_obj_check_type(o, &lv_label_class)) {
    target = o;
    legacy = lv_color_black();
  } else {
    return;
  }
  if (!target) return;
  // Only a filled area decides: read the background it really has in its current state.
  lv_color_t bg = lv_obj_get_style_bg_color(o, LV_PART_MAIN);
  lv_obj_set_style_text_color(target, onColor(bg, legacy), 0);
}

static void put(const Entry &e) {
  lv_color_t c = color(e.role);
  switch (e.prop) {
    case Prop::Bg:
      lv_obj_set_style_bg_color(e.obj, c, e.sel);
      syncText(e.obj);
      break;
    case Prop::Text: lv_obj_set_style_text_color(e.obj, c, e.sel); break;
    case Prop::Border: lv_obj_set_style_border_color(e.obj, c, e.sel); break;
  }
}

static void stateEvent(lv_event_t *e) { syncText(lv_event_get_target_obj(e)); }

void bind(lv_obj_t *obj, Prop prop, Role role, lv_style_selector_t sel) {
  bool known = false;
  for (int i = 0; i < regN; i++) {
    if (reg[i].obj != obj) continue;
    known = true;
    if (reg[i].prop == prop && reg[i].sel == sel) {
      reg[i].role = role;
      put(reg[i]);
      return;
    }
  }
  if (regN == regCap) {
    int cap = regCap ? regCap * 2 : 128;
    Entry *n = (Entry *)heap_caps_realloc(reg, cap * sizeof(Entry), MALLOC_CAP_SPIRAM);
    if (!n) {  // out of memory: the colour is still set now, it just won't follow a switch
      put({obj, prop, role, sel});
      return;
    }
    reg = n;
    regCap = cap;
  }
  if (!known) {
    lv_obj_add_event_cb(obj, unbindObj, LV_EVENT_DELETE, nullptr);
    if (lv_obj_check_type(obj, &lv_button_class)) lv_obj_add_event_cb(obj, stateEvent, LV_EVENT_STATE_CHANGED, nullptr);
  }
  reg[regN] = {obj, prop, role, sel};
  put(reg[regN++]);
}

// ---------------------------------------------------------------- switching
static void select(int idx) {
  cur = idx >= 0 && idx < N ? idx : 0;
  const Palette &p = PALETTES[cur];
  for (int i = 0; i < (int)Role::Count; i++) cache[i] = lv_color_hex(p.c[i]);
}

static void copyBase() {
  layer->color_primary = base->color_primary;
  layer->color_secondary = base->color_secondary;
  layer->font_small = base->font_small;
  layer->font_normal = base->font_normal;
  layer->font_large = base->font_large;
  layer->flags = base->flags;
}

static void initBase() {
  const Palette &p = PALETTES[cur];
  const lv_font_t *font = base->font_normal;
  lv_theme_default_init(disp, lv_color_hex(p.ctrl), lv_color_hex(p.ctrl2), p.dark, font);
  if (layer) copyBase();
}

void begin(lv_display_t *d, int idx) {
  disp = d;
  select(idx);
  base = lv_display_get_theme(disp);  // LVGL's own default theme, with its current font
  initBase();
  buildStyles();
  layer = lv_theme_create();
  lv_theme_set_parent(layer, base);
  lv_theme_set_apply_cb(layer, applyCb);
  layer->disp = disp;
  copyBase();
  lv_display_set_theme(disp, layer);
  // The screen that already exists was themed by the old theme only.
  lv_obj_add_style(lv_display_get_screen_active(disp), &sScr, 0);
}

void apply(int idx) {
  select(idx);
  initBase();
  buildStyles();
  for (int i = 0; i < regN; i++) put(reg[i]);
  for (int i = 0; i < hookN; i++) hooks[i]();
  lv_obj_report_style_change(nullptr);
  lv_obj_invalidate(lv_display_get_screen_active(disp));
  lv_obj_invalidate(lv_display_get_layer_top(disp));
}

}  // namespace theme
