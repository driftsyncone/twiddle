# How-to recipes for contributors

Short recipes, each checked against the code (file and function names are the real ones). Build environment first:
`export PATH=/usr/bin:$PATH; . ~/esp/esp-idf/export.sh` ([dev-setup.md](dev-setup.md)). Everything marked
"(not verified on hardware)" was only checked by reading code and building.

The one rule that goes with all of them: [feature-matrix.md](feature-matrix.md). A user-visible feature must exist on
every supported panel with the same behaviour and look; if you add it here only, open the backport and say so in the matrix.

## Add a theme
1. `main/theme.cpp`, table `PALETTES[]`: append **one row** at the end. Format:
   `{"Name", dark, legacy, ctrl, ctrl2, { 21 colours }}`. `dark` picks LVGL's dark base theme, `legacy` is true only for
   Daylight (keeps the old fixed white/black button text), `ctrl`/`ctrl2` are the colours LVGL's own widgets use as
   primary/secondary (tab underline, slider, switch, chips). The 21 colours are in the order of `enum class Role` in
   `main/theme.h` (Bg, Surface, SurfaceAlt, Text, TextDim, Primary, Start, Pause, Stop, Neutral, Neutral2, Accent, Disabled,
   Danger2, ChartBg, ChartPast, ChartFuture, OverlayBg, OverlayText, Scrim, Highlight); copy a neighbouring row and edit.
2. Append, never insert or reorder: the row index is what is stored in NVS (`theme`) and what `GET /theme?n=` takes.
   An out-of-range stored value falls back to Midnight (index 0).
3. Nothing else needs touching: the Settings "Theme" row is built from `theme::count()`/`theme::name()` (at most 15 chips, the
   button-matrix map in `buildSettings` has 16 slots) and `/theme` lists the table.
4. Check contrast: text on a filled button is picked automatically (white if it reaches 4.5:1, else near-black), but `Text`
   on `Bg`/`Surface` and `Start/Pause/Stop` against `Surface` are yours to eyeball.
5. Look at it: `tools/screenshot/capture.sh build/shots 0,1,2,3,4 <index> 4` (STEP 4 is enough), or on a running panel
   `curl -u ota:PASSWORD 'http://twiddle-p4.local/theme?n=<index>'` then `/screenshot?tab=N`. Never capture tab 5.
6. Docs: the theme names appear in [themes.md](themes.md) and in the matrix row "Themes"; the Arduino repo needs the same row
   (same palette values) to keep the themes identical.
To add a new **role** instead (rare): add it to `enum Role` before `Count` and a column to **every** palette row; then use it
in `ui.cpp` through `theme::bind`/`theme::color`.

## Add a setting
The pattern is split across four places; `Settings::theme` is the shortest complete example.
1. `main/settings.h`: add the field to `struct Settings` with its default and a comment (units, range, when it is read).
   Declare `void settingsSaveFoo();`.
2. `main/settings.cpp`: in `settingsLoad()` read it from namespace `vgpanel` (`p.getInt("foo", settings.foo)`) and clamp or
   validate it there (see `bright`, `grace`, `bt`). Add `settingsSaveFoo()`: open `Preferences` with `p.begin(NS, false)`,
   write one key, `p.end()`. **NVS key names are at most 15 characters** and must not clash with the existing ones
   (`ssid pass token sleep flip bright grace bt theme fav0-2 sn0-29`). Document the key in the NVS table in
   [architecture.md](architecture.md).
3. `main/ui.cpp`, `buildSettings()`: add a row. For a choice use `labeledSeg(tab, "Label", map, selectedIndex, callback, ST_ROW)`
   (a `""`-terminated `const char *` array, as for "Screen off"/"Theme"); for a toggle a `button(...)`; for a slider copy the
   brightness code. In the callback set `settings.foo`, call `settingsSaveFoo()`, and apply it.
   Rows are `ST_ROW` = `PY(48)` high and six have to fit above the fold, so adding a row means checking the screen.
4. Live or restart, decided by when the code reads the value:
   - **live**: the callback applies it immediately (`boardSetBrightness`, `uiSetTheme`), and the consumer reads
     `settings.foo` each time it needs it (e.g. `graceMs()` in `autoblow.cpp`, the sleep test in `ui.cpp`);
   - **restart**: the value is only read in a `begin()` at boot (Wi-Fi and token in `wlan::begin`/`autoblow::begin`, Bluetooth in
     `ble::begin`). The callback saves it and calls `control::stopAndRestart()` (what the Bluetooth chip does); Wi-Fi/token
     are staged in `pSsid/pPass/pToken` and written together by the Save/Restart button via `settingsSave()`.
   Pick restart only if re-initialising at runtime is not safe, and say so in the comment in `settings.h`.
5. If it should also be settable from the web, see "Add a web route". If it affects the safety rules (valves, loss, battery)
   stop and read the safety section of [architecture.md](architecture.md) first.
6. Update the README and the feature matrix; the Arduino firmware needs the same setting and the **same NVS key**.

## Add a UI tab or widget
Rules that every UI change follows (all in `main/ui.cpp` / `ui.h`):
- **Sizes**: the layout was designed at 800x480. Write the old design value and scale it: `PX(v)` for horizontal values,
  `PY(v)` for vertical values, paddings and radii (`PX(v) = v*1024/800`, `PY(v) = v*600/480`). Full-width things use `SCREEN_W`
  / `lv_pct(100)`. Fonts are `lv_font_montserrat_NN`; only the sizes enabled in `sdkconfig.defaults` exist (16, 18, 24, 28, 36,
  48): a new size means adding `CONFIG_LV_FONT_MONTSERRAT_NN=y` (see "Add a Kconfig option").
- **Colours**: never write a colour value. Use a `theme::Role` through `theme::bind(obj, Prop::Bg|Text|Border, Role::X, state)` at
  build time (it remembers the binding and re-applies it on a theme switch, and keeps a button's label readable), or
  `theme::color(Role::X)` for colours you recompute in `refresh()`. Anything cached or set without a style (chart series,
  the Devices rows' cached state) must be repainted in `themeChanged()` at the end of `ui.cpp`.
- **Helpers**: `button(parent, text, w, h, Role, cb)`, `row()`, `segmented()`, `labeledSeg()`, `setLabel()` (skips unchanged
  text, which keeps PSRAM traffic down; the 200 ms `refresh()` timer calls it for a dozen labels), `setButtonText()`.
- **A new tab**: write `static void buildFoo(lv_obj_t *tab)` and add `buildFoo(lv_tabview_add_tab(tv, "Foo"));` in `uiBegin()`
  **before** the Settings tab. Tab numbers are hard-coded: `uiShowTab()` clamps to 0..5 and `tools/screenshot/capture.sh`,
  `screenshot_dump.cpp` and the docs treat **5 as Settings** (never captured). If you insert a tab, update all of those; the safe
  way is to keep Settings last and bump the clamp.
- **Locking**: LVGL runs on esp_lvgl_port's task. Code that runs inside an LVGL event or timer callback already holds the
  lock. Any other task (web server, a worker) must wrap every LVGL call, `control::` call and shared-I2C access in
  `UiLock l;` (recursive, `ui.h`). Hardware whose driver was set up on the loop task's core goes through
  `uiCall(fn, arg)` (not while holding `UiLock`).
- **Never call `lv_refr_now` from another task** to force a redraw: invalidate (`lv_obj_invalidate`) under `UiLock` and wait for
  the LVGL task instead, as `onScreenshot` does. `uiShowTab()` and `uiShowUpdating()` no longer render (2.0.0-p4.7); they only
  mark the screen dirty and the LVGL task draws it. A task that needs the picture (or the update message before a flash write)
  waits a few hundred ms after the call, outside the lock.
- **Do not hold `UiLock` across anything slow.** `wlan::loop()` and `battery::loop()` run in `loop()` outside the lock because
  they call the C6 (esp_hosted RPCs, 5 s timeout) and the I2C bus; holding the lock freezes touch, STOP and the 100 ms control
  tick. Their shared state is read through copies (`wlan::info()` copies the problem text, `battery::info()` copies under a
  spinlock). A handler that must wait for hardware (like `/update` waiting for the STOP) waits without the lock.
- **Do not wait on a zero-tick delay.** `CONFIG_FREERTOS_HZ` is 1000 (as on Arduino) and the shim's `delay(ms)` rounds a
  non-zero delay up to one tick; use `delay()`/`vTaskDelay` with a real wait in any loop.
- **One owner for the Wi-Fi scan**: use `wlan::scanStart()` / `scanRunning()` / `scanGeneration()` / `scanCount()` /
  `scanSsid(i)`. Never call `esp_wifi_scan_get_ap_records` elsewhere: it consumes the driver's list.
- **Theme switches** go through `uiSetTheme(idx)`, which defers to `lv_async_call`: never restyle from inside the event of a
  widget the switch restyles.
- Test: build, flash, look. `GET /screenshot?tab=N` (login) or `tools/screenshot/capture.sh`. UI is not covered by host tests.

## Add a toy (Bluetooth driver)
Contract: `main/toys/toys.h` (read the header comment: which functions run on which task and what must never block).
1. New `main/toys/foo.cpp` ending in `const Driver foo = {'F', devices::Kind::..., "Brand", matches, init, level, tick, stop, battery};`
   - `id`: one unused character, **stored in NVS** (`bletoys`), so never change or reuse one (taken: `L K 2 O`).
   - `kind`: `devices::Kind` in `main/devices.h` (`Autoblow, Lovense, Kiiroo, Ossm`); add a value only if the UI needs it (it
     special-cases `Kind::Ossm` in `ui.cpp`).
   - `matches(adv)`: decide from the advertisement only. Existing drivers match the **exact** name (Keon: `KEON`, `Keon R2`;
     Keon 2: `KEON2`, `KEON WIFI`, `Keon Wifi`), a name prefix (Lovense: `LVS-`, `LOVE-`), manufacturer data, or a service UUID
     (OSSM). Names are case-sensitive; do not match a bare common service (Nordic UART counts only for a nameless device).
   - `level(0..100)`, `tick()` and `stop()` run on the `blesvc` task and must not block: write only through `toys::write()` /
     `writeStr()`. `init()` and `battery()` run on the `ble` worker and may wait (`toys::request`, `toys::idle`).
   - Per-toy state goes in the `union` in `struct Ctx` in `toys.h` (add a `FooState` struct and a member).
2. Register it: `extern const Driver ... foo;` in `toys.h` and add `&foo` to `ALL[]` in `main/toys/toys.cpp`. The array order is
   the match order (first match wins); `byId()` uses `id`.
3. Build: `idf.py reconfigure` (new source file; the glob is evaluated at configure time) then `idf.py build`.
4. Host test: add a `testFoo()` to `test/host/toys_test.cpp` (and checks to `testMatch()`), call it in `main()`, **add
   `"$root/main/toys/foo.cpp"` to the g++ line in `test/host/run.sh`** (the file list is explicit), and run `test/host/run.sh`.
   It prints `N checks, 0 failed`. The mock (`test/host/mock/`) provides `NimBLEAdvertisedDevice` with `name`, `mfg` and `svc`
   fields; the test harness fakes `toys::write/subscribe/reply/request/idle` and records what was written. If a driver needs
   more of NimBLE, extend `mock/NimBLEDevice.h`.
5. A real toy on the air is a separate, deliberate test (not verified on hardware for any toy so far). Keep STOP safe: `stop()` must
   work while `battery()` is waiting.
6. Docs: README, matrix row "Bluetooth toys", [architecture.md](architecture.md) module table, and the Arduino repo (the drivers
   are shared source: change both).

## Add a web route (`main/ota.cpp`)
1. Write `static esp_err_t onFoo(httpd_req_t *req)`. Anything that changes state or shows private data starts with
   `if (!login(req)) return ESP_OK;` (basic auth user `ota`, password `OTA_PASSWORD`; an empty password means every login
   fails). Only `GET /` and, in portal mode, `/setup` are open.
2. Anything touching LVGL, `control::`, or the shared I2C bus takes `UiLock l;` in a small scope; long copies or sends happen
   **outside** the lock (see `onScreenshot`: copy under the lock, send after). Never block the server for long.
3. Reply with `sendText(req, "200 OK", text)`; read query values with `queryArg(req, "name")` (a value longer than 47 characters, or a query longer than 200, reads as empty) and form bodies with `formArg`.
4. Register it in the `routes[]` table in `otaBegin()` (`{"/foo", HTTP_GET, onFoo}`). `cfg.max_uri_handlers` is **20** and 11 are
   used; one entry per URI+method pair. Raising the limit costs RAM; a registration beyond it returns `ESP_ERR_HTTPD_HANDLERS_FULL`, which the loop in `otaBegin()` ignores, so the route would just be missing.
5. Document it in the route table in [architecture.md](architecture.md) (auth column!), and in `README.md` if users need it.
6. Test without hardware is not possible; with a panel: `curl -u ota:PASSWORD http://twiddle-p4.local/foo` (the dev box needs a network
   path, [network.md](network.md)).

## Add a Kconfig option or a dependency
- **Kconfig**: add the line to `sdkconfig.defaults` (never edit `sdkconfig`, it is generated and gitignored), then
  `rm sdkconfig && idf.py set-target esp32p4 && idf.py build`. **Then check the result**: `grep CONFIG_YOUR_OPTION sdkconfig`. A
  value that does not exist in this configuration is silently dropped (the PSRAM 200 MHz trap: `CONFIG_SPIRAM_SPEED_200M`
  needs `CONFIG_IDF_EXPERIMENTAL_FEATURES=y`). Explain non-obvious lines with a comment in the file. Do not change the PSRAM
  speed or the esp-hosted settings.
- **Dependency from the component manager**: add it to `main/idf_component.yml` with a version constraint (see the existing
  `~2.6.0`/`^7.4.0`/`^2.0.0`), run `idf.py reconfigure`, and **commit the updated `dependencies.lock`** (it is tracked and pins
  the resolved versions; `managed_components/` is not committed). Pin with `~` when a newer minor needs a newer IDF (that is why
  `esp_lvgl_port` is `~2.6.0`). Check the lock's `idf` entry still says 5.5.1.
- **IDF component** (like `esp_http_server`): add it to `REQUIRES` in `main/CMakeLists.txt`.
- **New source file** under `main/`: `idf.py reconfigure`.
- Re-read the build summary: `twiddle.bin binary size ... (N%) free` for the 3 MB slot; and update the docs that quote versions
  ([dev-setup.md](dev-setup.md), [c6-firmware.md](c6-firmware.md) when esp_hosted changes: host and C6 versions must match).

## Bump the version and cut a build for OTA
1. `main/version.h`: `#define FW_VERSION "2.0.0-p4.N"`. Bump `N` for every flash that matters (shown on the Settings tab, at `GET /`,
   on the setup page). A release drops the suffix.
2. `idf.py build` (a changed `FW_VERSION` is enough to change the image; `id:` on `GET /` is the ELF hash and differs per build).
3. `./ota.sh` (uses `build/twiddle.bin`; host from the first argument, `$PANEL_HOST`, `OTA_HOST` in `main/secrets.h`, or
   `twiddle-p4.local`; password from `OTA_PASSWORD` in `secrets.h`). The running session is stopped by the panel. It prints `OK: new
   firmware confirmed` after the 30 s probation, `ROLLED BACK` if the old image is back, or times out after about 4 minutes.
4. The first install on a blank board is over USB: `idf.py -p /dev/ttyACM0 flash`. Details and rollback rules: [ota-and-rollback.md](ota-and-rollback.md).

## Release checklist
Do not paste private material (tokens, `secrets.h`, Wi-Fi passwords, Settings screenshots) into issues, PRs or docs.
- [ ] Build from a clean checkout: `rm -rf build sdkconfig && idf.py set-target esp32p4 && idf.py build` succeeds, no new warnings
      you introduced; the size line shows the slot headroom.
- [ ] `test/host/run.sh` prints `... 0 failed`; `bash -n ota.sh tools/screenshot/capture.sh test/host/run.sh`.
- [ ] `FW_VERSION` bumped (release: suffix dropped); same number appears in the notes.
- [ ] Release notes: add the user-visible changes (what, why, anything needing a C6 or sdkconfig reset) to [CHANGELOG.md](../CHANGELOG.md); put the same notes in the PR description.
- [ ] `README.md` updated: status section and its "Not verified yet" list, build steps, anything user-visible.
- [ ] [flashing.md](flashing.md) updated if anything about building, flashing, identifying, recovering or per-unit setup changed (commands, per-board sections, your unit inventory).
- [ ] [feature-matrix.md](feature-matrix.md) updated: P4 column, and the other targets' status/backport branch for anything new.
- [ ] Docs updated: the page that owns the changed behaviour (architecture route/NVS tables, hardware, ota-and-rollback, troubleshooting
      for any new symptom), and [howto.md](howto.md) if a recipe changed.
- [ ] Tested as far as the hardware allows, and the rest labelled "(not verified on hardware)" in the docs. Never test against the real
      VacuGlide: emulator only ([testing.md](testing.md)).
- [ ] Changes submitted as a PR / patch against the main development branch; nothing force-pushed; `secrets.h`,
      `sdkconfig`, `build/` and `managed_components/` not committed.
- [ ] OTA'd onto a panel (or flashed) and confirmed (`firmware: confirmed`), if a panel is available.
