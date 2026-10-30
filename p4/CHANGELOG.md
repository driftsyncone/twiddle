# Changelog

All notable changes to the ESP32-P4 panel firmware. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions are `FW_VERSION` in `main/version.h`
(`2.0.0-p4.N` is the P4 port of the Arduino panel firmware 2.0.0; the suffix is bumped on every flash that
matters, and a release drops it). Nothing has been released without the `-p4.N` suffix yet.

## [Unreleased]

### Added (2.0.0-p4.10: the `_Y` "new panel", per-unit panel settings)
- Support for the **`JC1060P470C_I_W_Y`** unit (plastic shell, no rear label), which stayed black with the V2 settings:
  panel variant 4 = V2 table and timings with LCD reset on GPIO5 and 16-bit colour (COLMOD 0x3A=0x55 sent before sleep-out), LVGL in
  RGB565, default pixel clock 48 MHz (shakes at 52). Verified on hardware: picture, touch and flip.
- Per-unit settings over the web, saved in NVS: `GET /panel?v=0..4` (display panel variant) and `GET /pclk?mhz=N` (display
  pixel clock), both applied after a restart. `/screenshot` converts RGB565 to the usual 24-bit BMP.
- Flip screen works again (merged from the flip branch) and was checked by eye on both panels.

### Fixed
- Reading a panel register back (DCS read after the mirror command) blanked the `_Y` panel; the readback is gone (flip still checked by eye).

### Changed (per-board network names)
- The default mDNS/DHCP hostname of this firmware is now `twiddle-p4` (`twiddle-p4.local`; was `twiddle`), so it no
  longer clashes with the other boards. The sibling defaults are `twiddle-s3` (Guition S3) and `twiddle-ws` (Waveshare).
  `PANEL_HOSTNAME` in `secrets.h` still overrides it. `ota.sh` falls back to `twiddle-p4.local`. A board that was
  flashed with `p4.6` answers to `twiddle.local` until its next update; use that name (or its IP) for that one update.
- Two boards of the same model still need different `PANEL_HOSTNAME` values.

### Changed (default theme)
- The default colour theme is now **Midnight** (stored `theme` defaults to 1 when nothing is saved). A theme a user
  saved is kept; Daylight stays available. A stored value that is out of range now falls back to Midnight too (same as the
  S3 firmware). The Settings theme chip reads `theme::current()`, so it shows Midnight on
  a fresh NVS. The S3 firmware gets the same default.

### Fixed (independent review of the port, 2.0.0-p4.7)
Verified by build and code review only; no board was available. See the hardware checks in the release notes of the PR.
- **H1** `/update` and `/screenshot?tab=` no longer call `lv_refr_now` from the web-server task (possible heap corruption).
  `uiShowUpdating`/`uiShowTab` only invalidate and the LVGL task draws. `/update` now stops the devices and waits for the
  STOP to go out (0.8 s minimum, until nothing moves, 5 s maximum, then logs and continues) before the update screen and the
  flash write. A failed upload still leaves the old firmware running.
- **M1** `CONFIG_FREERTOS_HZ=1000` (was 100): `delay(5)` in `loop()` was a zero-tick yield, so the loop task spun at 100 % on
  core 1. The Arduino shim's `delay()` also rounds a non-zero delay up to one tick. Nothing else relied on 100 Hz.
- **M2** `wlan::loop()` and `battery::loop()` run outside `UiLock`: their C6 calls (5 s RPC timeout) no longer freeze LVGL,
  touch, the control tick and STOP. Shared state is copied under a spinlock (`battery::info()`, link sample).
- **M3** The Settings Wi-Fi scan uses `wlan::scanStart()/scanRunning()/scanGeneration()/scanCount()/scanSsid()`; the extra
  scan task is gone, so the results are consumed once. A scan that never completes is given up after 20 s (15 s in the UI).
- **L1** Wi-Fi reconnect backs off after failed attempts while down: 5 s, 10 s ... 30 s for a wrong password, 1/2/4/8 s for a
  missing network; a drop of an established link is still retried at once, and our own 10 s restart no longer causes a
  doubled connect.
- **L2** `wlan::info()` copies the problem text into the returned struct (no shared static buffer); `wlan::up()` added for
  the autoblow and rollback tasks.
- **L3** Not changed: console output stays unbuffered (a UART driver change cannot be tested without a board). Documented in
  `docs/troubleshooting.md` with the suggested fix.
- **L4** The Settings **Flip** button is greyed and reads "Flip: n/a" on this board and no longer changes the stored value.
  The unverified `flip` branch is not merged.
- `FW_VERSION` is `2.0.0-p4.7`.

### Added
- User documentation: `docs/user-guide.md`, `docs/settings.md`, `docs/themes.md`, `docs/history.md`.
- `CHANGELOG.md`, `CONTRIBUTING.md`, `THIRD_PARTY_NOTICES.md`, and a rewritten `README.md`.

### Changed
- Header comment on every source file; compiler warnings fixed (LVGL per-flag setters, mixed enum selectors);
  the unknown Kconfig symbol `ESP_HOSTED_SDIO_HOST_INTERFACE` removed from `sdkconfig.defaults`.
- `.gitignore` completed (`dist/`, `sdkconfig*`, scratch and editor files).

### Removed
- Unused code: `boardSimTouch`, `boardFrameBuffer`, `boardIsFlipped`, `boardBacklight`, `battery::usesP4`,
  `control::autopilotWaves`, `control::autopilotProgress`, `devices::vacuGlideEnabled`, unused `BOARD_*` macros.
- `legacy/` (the bring-up test code that was never built; still in git history, see `docs/history.md`).

## [2.0.0-p4.6] - 2026-10-30

### Changed
- Renamed the project to **Twiddle** (repos `twiddle-p4` and `twiddle-s3`). "VacuGlide" still names the Autoblow device.
- `GET /` now starts with `twiddle <version>` (was `vacuglide-panel <version>`); the CMake project and image are
  `twiddle` / `build/twiddle.bin`; `tools/dist.sh` writes `dist/twiddle-p4-<version>.bin`.
- The default mDNS name is `twiddle` (`twiddle.local`, was `vgremote`; `PANEL_HOSTNAME` in `secrets.h` still overrides it),
  and so is the Bluetooth device name. The setup-portal Wi-Fi network is `Twiddle-Setup` with password `twiddle-setup`
  (was `VacuGlide-Panel` / `vacuglide`).
- Settings are kept: the NVS namespace stays `vgpanel` (legacy name, kept for settings compatibility).
- Boards running an older firmware need one update to get the new name; `ota.sh` accepts both `twiddle` and the old
  `vacuglide-panel` as the project name meanwhile (other projects are still refused).

## [2.0.0-p4.4] - 2026-10-30

### Added
- `GET /screenshot[?tab=N]` returns a working 24-bit BMP of the P4 frame buffer.
- `GET /theme[?n=N[&save=1]]` lists and switches themes over the web.
- Documentation: hardware, dev setup, architecture, C6 firmware, OTA and rollback, testing, network,
  troubleshooting, how-to recipes, per-board flashing, the consistency rule and feature matrix.
- `tools/dist.sh` names release artefacts by board and version, with a checksum.

### Changed
- `ota.sh` refuses to upload to a board that runs another project's firmware or another board model.

## [2.0.0-p4.3] - 2026-10-30

### Added
- Colour themes: Daylight (default, the original look), Midnight, Ocean, Forest, Ember, Blush. Switching is live
  and saved. The UI names colour roles only; a theme is one row in `main/theme.cpp`.

## [2.0.0-p4.2] - 2026-10-30

### Added
- `README.md`, `LICENSE` (CC BY-NC-SA 4.0), `ota.sh` (update over Wi-Fi, waits for the rollback confirmation),
  `tools/screenshot/capture.sh` (screenshots over the serial output), `main/secrets.example.h`.

### Changed
- The CMake project is named `vacuglide-panel` (image `build/vacuglide-panel.bin`).

## [2.0.0-p4.1] - 2026-10-29

First complete port: the Arduino panel firmware running natively on ESP-IDF 5.5.1.

### Added
- Display (JD9165 over MIPI-DSI, V2 panel), GT911 touch, PWM backlight, LVGL through esp_lvgl_port; the UI scaled
  from 800x480 to 1024x600.
- Settings, control (Remote, Autopilot, scripts, valves, vacuum pulses) and the log ring buffer.
- Autoblow cloud client (discovery, state polling, speed/valve/stop/pulse/script commands, loss handshake).
- Wi-Fi through the ESP32-C6 (esp_hosted over SDIO), mDNS, setup portal, web server with firmware update.
- Firmware probation and automatic rollback (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`).
- Bluetooth LE through the C6 (esp-nimble-cpp over hosted HCI): Lovense, Kiiroo Keon and OSSM drivers, device
  registry with Follow and scale.
- Offline toy-encoder tests (`test/host/run.sh`).
- A compatibility shim (`main/compat/`) so the Arduino-style module code ports as written.

### Fixed
- The battery probe no longer accepts a non-gauge chip at I2C address 0x36 (it caused a false "Battery at 0%"
  banner); a real MAX1704x needs a known version and a plausible cell voltage.

### Before p4.1 (bring-up, not versioned)
- Display and touch bring-up, LVGL test screen at 200 MHz PSRAM with 24-bit colour, Wi-Fi through the C6, BLE
  scan-only, read-only Autoblow client (see `docs/history.md`).
