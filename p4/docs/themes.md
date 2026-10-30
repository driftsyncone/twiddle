# Themes

The panel has six colour themes. You pick one in the **Settings** tab, **Theme** row; it applies at once and is
remembered across restarts (NVS key `theme`, see [settings.md](settings.md)). The source of truth is the
`PALETTES[]` table in `main/theme.cpp`; the table below was generated from it.

| Index | Name | Style |
|---|---|---|
| 0 | Daylight | Light |
| 1 | **Midnight** (default) | Dark, teal accent |
| 2 | Ocean | Dark blue |
| 3 | Forest | Dark green |
| 4 | Ember | Dark, warm orange |
| 5 | Blush | Light, pink |

## Roles

`main/ui.cpp` never names a colour value. It asks for a **role** (`theme::Role` in `main/theme.h`) and the active
palette answers.

| Role | Used for |
|---|---|
| `Bg` | Screen background |
| `Surface`, `SurfaceAlt` | Cards, tab bar, lists; and the alternate (item, switch track) surface |
| `Text`, `TextDim` | Normal text; secondary text and inactive things |
| `Primary` | Main action colour (Scan, Connect, hold buttons, selected card border) |
| `Start`, `Pause`, `Stop` | Start/connected/good; pause/connecting/warning; STOP/error/hold feedback. Also the Wi-Fi bars |
| `Neutral`, `Neutral2` | Plain buttons; secondary buttons (Back off, Forget, Restart) |
| `Accent` | Special action (Finish Me) |
| `Disabled` | A disabled button |
| `Danger2` | The Power off button |
| `ChartBg`, `ChartPast`, `ChartFuture` | The Autopilot graph: background, played part, upcoming part |
| `OverlayBg`, `OverlayText` | Full-screen pop-ups (text entry, update screen) |
| `Scrim` | The dimmed backdrop behind pop-ups |
| `Highlight` | Touch feedback on plain areas (the status-bar device selector) |

## Palettes

Colours are `#RRGGBB`.

| Role | Daylight | Midnight | Ocean | Forest | Ember | Blush |
|---|---|---|---|---|---|---|
| `Bg` | `#F5F5F5` | `#0E1014` | `#0B1622` | `#0F1A14` | `#17110F` | `#FFF8F6` |
| `Surface` | `#FFFFFF` | `#171A21` | `#112236` | `#15241B` | `#221915` | `#FFFFFF` |
| `SurfaceAlt` | `#E0E0E0` | `#1F232C` | `#173049` | `#1D3125` | `#2C211C` | `#F7E9E6` |
| `Text` | `#212121` | `#E8EAED` | `#E3F2FD` | `#E8F5E9` | `#FBE9E7` | `#3E2723` |
| `TextDim` | `#9E9E9E` | `#9AA0A6` | `#90A4AE` | `#A5B8A9` | `#BCAAA4` | `#7D5F54` |
| `Primary` | `#00838F` | `#26C6DA` | `#29B6F6` | `#4DB6AC` | `#FF7043` | `#D81B60` |
| `Start` | `#2E7D32` | `#2EA24F` | `#26A69A` | `#66BB6A` | `#7CB342` | `#2E7D32` |
| `Pause` | `#EF6C00` | `#F59E0B` | `#FFB74D` | `#FFA726` | `#FFCA28` | `#EF6C00` |
| `Stop` | `#D32F2F` | `#E5484D` | `#EF5350` | `#EF5350` | `#D9362F` | `#C62828` |
| `Neutral` | `#37474F` | `#2B313C` | `#1E3A56` | `#243B2D` | `#3B2D27` | `#6D4C41` |
| `Neutral2` | `#455A64` | `#394150` | `#28506F` | `#2F4E3B` | `#4E3B33` | `#8D6E63` |
| `Accent` | `#C2185B` | `#D6336C` | `#7C4DFF` | `#D4A017` | `#F06292` | `#6A1B9A` |
| `Disabled` | `#424242` | `#2A2E36` | `#24384A` | `#26352B` | `#3A2E29` | `#BCAAA4` |
| `Danger2` | `#8E2A2A` | `#8E2A2A` | `#8E2A2A` | `#8E2A2A` | `#8E2A2A` | `#8E2A2A` |
| `ChartBg` | `#1A1F23` | `#0A0C10` | `#08111A` | `#0B140F` | `#0F0B0A` | `#3E2723` |
| `ChartPast` | `#546E7A` | `#5B6B7B` | `#4A6E8C` | `#5B7F66` | `#8D6E63` | `#A1887F` |
| `ChartFuture` | `#4FC3F7` | `#4FC3F7` | `#80DEEA` | `#A5D6A7` | `#FFAB91` | `#F48FB1` |
| `OverlayBg` | `#121212` | `#0E1014` | `#0B1622` | `#0F1A14` | `#17110F` | `#FFF8F6` |
| `OverlayText` | `#4FC3F7` | `#4FC3F7` | `#80DEEA` | `#A5D6A7` | `#FFAB91` | `#D81B60` |
| `Scrim` | `#000000` | `#000000` | `#000000` | `#000000` | `#000000` | `#000000` |
| `Highlight` | `#FFFFFF` | `#FFFFFF` | `#FFFFFF` | `#FFFFFF` | `#FFFFFF` | `#3E2723` |
| `ctrl` (LVGL primary) | `#2196F3` | `#26C6DA` | `#29B6F6` | `#4DB6AC` | `#FF7043` | `#D81B60` |
| `ctrl2` (LVGL secondary) | `#F44336` | `#D6336C` | `#7C4DFF` | `#D4A017` | `#F06292` | `#6A1B9A` |
| dark base theme | no | yes | yes | yes | yes | no |
| `legacy` text rule | yes | no | no | no | no | no |

`ctrl` and `ctrl2` are the primary and secondary colours handed to LVGL's own default theme, which draws the tab
underline, slider, switch and the selected chips. `dark` selects LVGL's dark base theme. `legacy` is true only
for Daylight: it keeps the old fixed white button text and black banner text; every other theme picks white or
near-black text per fill for contrast (white while it reaches 4.5:1).

## How live switching works

1. `theme::begin()` installs a theme layer on the display. It wraps LVGL's default theme (re-initialised per
   palette with the `dark`, `ctrl`, `ctrl2` values) and re-colours its neutral surfaces from the palette using a
   handful of shared styles (`sScr`, `sCard`, `sSurface`, ...).
2. Colours that `ui.cpp` sets on a particular object go through `theme::bind(obj, prop, role, selector)`. The
   registry remembers each binding (and drops it when the object is deleted).
3. `theme::apply(idx)` (called asynchronously from the Settings chip, never inside the event of a widget it
   restyles) selects the palette, rebuilds the shared styles, re-applies every binding, runs the change hooks
   (`ui.cpp` repaints the chart series and the status elements it computes itself), and invalidates the screen.
4. The chosen index is saved by the Settings tab (`settingsSaveTheme()`). The web endpoint
   `GET /theme?n=N[&save=1]` switches live too ([architecture.md](architecture.md)).

## Add a theme

1. Append **one row** to `PALETTES[]` in `main/theme.cpp`: `{"Name", dark, legacy, ctrl, ctrl2, { 21 colours }}`,
   colours in the order of the table above. Copy a neighbouring row and edit it.
2. **Append, never insert or reorder**: the row index is what is stored in NVS.
3. Nothing else needs changing: the Settings row is built from `theme::count()` and `theme::name()` (at most 15
   themes fit the button row).
4. Check contrast of `Text` on `Bg`/`Surface` and of `Start`/`Pause`/`Stop` against `Surface`.
5. Look at it with `tools/screenshot/capture.sh` ([howto.md](howto.md) has the details).
6. Update this page and, because themes are a cross-hardware feature, the matrix in
   [feature-matrix.md](feature-matrix.md).
