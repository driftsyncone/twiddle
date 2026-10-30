// Colour themes. ui.cpp never names a colour: it asks for a ROLE, and the active palette answers.
// Adding a theme = one row in the PALETTES table in theme.cpp (its name shows up in Settings by itself).
#pragma once
#include <lvgl.h>

namespace theme {

enum class Role : uint8_t {
  Bg, Surface, SurfaceAlt, Text, TextDim,
  Primary, Start, Pause, Stop,
  Neutral, Neutral2, Accent, Disabled, Danger2,
  ChartBg, ChartPast, ChartFuture,
  OverlayBg, OverlayText,
  Scrim,      // dimmed backdrop behind pop-ups
  Highlight,  // touch feedback on plain areas
  Count
};

enum class Prop : uint8_t { Bg, Text, Border };

int count();
const char *name(int idx);  // "Daylight", ...
int current();

// Boot: install the theme layer on the display and activate theme `idx`. Under UiLock.
void begin(lv_display_t *disp, int idx);
// Live switch (LVGL task / under UiLock): restyles everything, then runs the change hooks.
void apply(int idx);
// Called after every switch, to repaint what ui.cpp caches or recomputes itself.
void onChange(void (*fn)());

lv_color_t color(Role r);

// Set a role colour on an object now and remember it, so a theme switch re-applies it.
// Calling it again for the same {obj, prop, selector} just changes the role. The entry goes away
// with the object. Bg on a button (or a label with its own background) also keeps its text readable.
void bind(lv_obj_t *obj, Prop prop, Role role, lv_style_selector_t sel = 0);
// A button's label colour follows its effective background: call after its state changed.
void syncText(lv_obj_t *obj);

}  // namespace theme
