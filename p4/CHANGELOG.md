# Changelog

All notable changes to the ESP32-P4 panel firmware. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions are `FW_VERSION` in `main/version.h`
(`2.0.0-p4.N` is the P4 port of the Arduino panel firmware 2.0.0; the suffix is bumped on every flash that
matters, and a release drops it). 2.0.0 is the first public release; the `-p4.N` entries below it are the development history that led to it.

## [Unreleased]

## [2.0.0] - 2026-11-03

First public release of the Twiddle firmware for the Guition JC1060P470C (ESP32-P4, 7 inch). Unofficial project, provided as is, with no warranty
or guarantee of any kind.

### What is in
- **Autopilot**: timeline-driven sessions with a graph window (the last 10 s and the next 30 s, "now" marked, tap to jump, Draw to edit the whole session).
- **Scripts**: play funscript-style motion scripts.
- **Themes**: Midnight (default) and Daylight.
- **Bluetooth toys**: connect, remember and forget devices on the Devices tab (Connect and Forget on a remembered device that is not connected).
- **Heart-rate edging** (work in progress, not yet tested with real sessions): Autopilot can be driven by a Bluetooth heart rate monitor. It has host tests
  only. Do not rely on it, and it is not a medical function.
- **Setup portal** at `http://192.168.4.1/setup` (join `Twiddle-Setup`): Wi-Fi, token and Display select. The form never shows stored Wi-Fi passwords or tokens.
- **On-screen Wi-Fi setup**: a **Connect to Wi-Fi** screen (Scan, network, password, Save & restart, Skip) appears when no Wi-Fi is saved, or a saved network has not joined 45 s after boot for a reason the user can fix; Wi-Fi only (token and toys stay on Settings and Devices). A **Save & restart** bar stays in view while Wi-Fi or token have unsaved changes. The phone page `/setup` is the fallback. UNTESTED on the P4 hardware until the 2.0.0 hands-on pass.
- **Per-device web password**: generated on the device and shown only on the panel's Settings tab; admin endpoints answer 401 without login.
- **Display select with trial/Keep**: a new panel variant is tried for 3 boots and reverts unless you tap Keep.
- **Web flasher** parts (bootloader, partition table, otadata, app) and the secrets-free release build.

### Known limitations
- The V2 panel path is verified on only two `JC1060P470C_I_W_Y` units (both 1024x600 with normal colours); other batches may differ.
- ESP32-P4 chip revision 3.0 is unsupported; tested on revision v1.3 only.
- The rotary encoder is not implemented on the P4 (it exists on the S3 firmware).
- The ESP32-C6 radio firmware is not part of this install; very old factory C6 firmware may not join Wi-Fi.
- No warranty; flashing can fail; keep a way to stop the device that you can reach by hand.

### Development history (folded into 2.0.0)
### Docs (2.0.0-p4.17, branch `release-prep`)
- README: notes that the S3 rotary encoder is not implemented on the P4 yet and why (connector pins and shared touch bus unconfirmed); new "Feature requests and bug reports" section; feature-matrix row merged from `encoder-matrix`.

### Changed (Autopilot graph window, 2.0.0-p4.17, branch `release-prep`)
- The graph is now a zoomed window on the session: the last 10 s and the next 30 s, with "now" (the orange line) at 25 % from the left and an axis in
  seconds relative to now (-10 s, now, +30 s). It is clamped at the start and end of the session and redrawn twice a second at most (no extra memory).
- HR edging: the future depends on the heart rate and is not planned, so the window shows the actual speed and the red heart rate line for the last 10 s
  and, ahead, only the current target speed as a dashed flat line, with the note "HR-driven: depends on heart rate".
- Tapping the graph still jumps to the tapped moment (now measured in the window). **Draw** (Session tab) switches the graph to the whole session
  while it is on, so every wave's peak can still be dragged; the axis then reads 0:00 to the session length.
- The window arithmetic is `apwindow.h`, host-tested.

### Changed (Devices tab, 2.0.0-p4.16, branch `release-prep`)
- A remembered Bluetooth device that is not connected now has **Connect** next to **Forget** (it stays remembered; Connect tries at once and restarts the
  retry backoff). While it connects the button reads "Connecting" and is disabled; once up the status says "Connected" and only **Forget** shows.
- A heart rate monitor's row never shows Follow or the 25-100 % buttons, however early the row was built, and tapping the row never selects it (decided
  every refresh by `devrow.h`, host-tested; the monitor is recognised as an input from the advert or NVS, before any link).

### Changed (licensing notices, docs only, branch `release-prep`)
- Licence decision: Twiddle's own code stays under CC BY-NC-SA 4.0 (`LICENSE` unchanged). New `LICENSE-NOTES.md` (draft, not read by a lawyer) states it,
  points to the third-party notices and carries the LGPL additional-permission paragraph (not needed for the P4 image, which links no LGPL component).
- Buttplug attribution corrected to the upstream line "Copyright (c) 2016-2023, Nonpolynomial Labs, LLC" (was 2016-2026) in `THIRD_PARTY_NOTICES.md` and the toy driver headers. The SIL OFL 1.1 text for LVGL's Montserrat fonts is now in `THIRD_PARTY_NOTICES.md`.
- README: disclaimer and safety section (no warranty, adults only, not a medical device, keep a hand-reachable way to stop, no affiliation).

### Changed (display trial logging, setup URL in log, 2.0.0-p4.15, branch `release-prep`)
- The serial log now shows the display-variant trial: `panel: variant X on trial (boot n of 3, last confirmed Y)`, `panel: variant X confirmed`,
  `panel: not confirmed after 3 boots, reverted from X to Y`, `panel: variant X kept` (Keep tapped) and `panel: variant X selected (trial, previous Y)`
  (POST /display or /panel). Variant names only, nothing secret.
- The setup-portal log line now says `http://192.168.4.1/setup`. No other user-visible text pointed to the bare address.
- Verified by build and host tests only, nothing flashed.

### Changed (setup address, form hygiene, softer `_Y` wording, 2.0.0-p4.14, branch `release-prep`)
- `GET /` on the setup AP (Host `192.168.4.1`) now answers `302` to `/setup`; on the LAN it is still the status page. The docs, the
  status-bar text and the flasher now say `http://192.168.4.1/setup`. Routing decision: `portalroute.h`, host-tested.
- The setup form no longer renders the stored Wi-Fi password or token (it used to put them in `value=`), and has `autocomplete=off`.
  An empty password/token field now means "keep the stored value" (a new network name does clear the password). Tip in the docs: a phone
  browser may still refill old values, so check the fields before saving.
- Wording: panel variant 4 is an alternative RGB565 table, not a requirement for `_Y`. The V2 default was verified on a fresh/erased unit
  driving two `JC1060P470C_I_W_Y` units (1024x600, normal colours); other batches may differ. The Display select flow (trial, Keep, revert)
  is untested on hardware.
- Verified by build and host tests only, nothing flashed: dev and release builds pass, `test/host/run.sh` passes, `tools/check-release-image.sh` 0 findings.

### Changed (setup portal no longer shows the web password, 2.0.0-p4.13, branch `release-prep`)
- The `Twiddle-Setup` AP password is public (it is in the docs), so anyone in range could read the web password off the portal
  page. The portal page and its Saved page now only say "read the password on the panel: Settings tab, Web login"; the password is
  shown nowhere but the panel's own Settings tab. Checked: `otapw::get()` is called only by the Settings tab (`ui.cpp`); the
  serial log never prints it (`otapw.cpp` logs events only); `GET /` does not include it. 403 until set and 401 after 0.5 s are unchanged.
  `test/host/run.sh` now fails if any file but `ui.cpp` reads the password with `otapw::get()`.
- README: draft paragraph "Third-party components and additional permissions" (later moved to LICENSE-NOTES.md). The licence text is unchanged.
- Verified by build and host tests only: no board was flashed. Dev and release (`-DTWIDDLE_RELEASE=ON`, `build-release`) builds pass, `test/host/run.sh` passes, `tools/check-release-image.sh` reports 0 findings.

### Added (release prerequisites for the web flasher, 2.0.0-p4.12, branch `release-prep`)
- **Panel variant 4 and the p4.10 line** merged from the `integration` line (commit 48b0cb5) onto `flasher-dist` (`hr-edging` + dist parts):
  the `_I_W_Y` "new panel", per-unit `GET /panel` and `GET /pclk`, flip. Nothing else from `integration` (publish tooling) was taken.
- **Secrets-free release build**: `idf.py -DTWIDDLE_RELEASE=ON build` compiles `main/secrets.release.h` (empty Wi-Fi, empty token,
  no web password, no host, no custom cloud URL) and ignores any `main/secrets.h`; without a `secrets.h` the same defaults apply.
  A fresh image starts the `Twiddle-Setup` portal and keeps Wi-Fi, token and everything else in NVS only.
  `tools/check-release-image.sh` scans the built images (secrets.h values, an optional private hash denylist, home-network
  patterns) without printing any value and fails the release; `tools/dist.sh` runs it first (`DIST_ALLOW_DEV=1` skips it).
- **Per-device web password**: made at first boot (6 characters from a 27-letter alphabet without look-alikes, from the
  hardware RNG), stored in NVS `otapw`, shown **only** on the Settings tab (**Web login**, tap to reset after a confirmation). It is the password of user `ota` for `/update` and every admin endpoint. While no
  password exists every admin endpoint answers **403**; a wrong one answers 401 after a 0.5 s pause, and **5 wrong passwords in a row lock the web login for 60 s** (429, even the right password is refused meanwhile). A dev build with
  `OTA_PASSWORD` in `secrets.h` keeps using that fixed password (no reset). `ota.sh` takes `$OTA_PASSWORD`, then `secrets.h`,
  then asks, and hands the password to curl on stdin.
- **Setup-portal "My screen looks wrong"**: a Display select on `/setup` (V2 default, new panel `_Y`, original, two advanced
  reset-pin variants) posting to `POST /display`, open on the `Twiddle-Setup` AP, behind the web login on the LAN. No display ID is
  read (a register read blanks the `_Y` panel). The chosen variant is on **trial**: the panel shows a **Keep** banner and only a
  tap makes it permanent; after 3 unconfirmed boots the next boot returns to the last confirmed variant (`panelfb.h`, NVS
  `pprev`, `ptrial`, `pboots`) and says so. `GET /panel?v=N` uses the same trial (`&keep=1` skips it); a variant change also resets the pixel clock to the default.
- **Chip revision**: the boot log and `GET /` show `chip: ESP32-P4 rev vX.Y` with a warning for anything but the tested v1.3.
  The min/max revision settings are unchanged (see [docs/hardware.md](docs/hardware.md): IDF 5.5.1 fixes the maximum at v1.99 and a v3.0 chip is refused by the bootloader).
- Host tests: `test/host/release_test.cpp` (password alphabet, length, uniformity and rejection sampling; fallback sequences; revision warning).
- Verified by build and host tests only: **no board was flashed**.

### Added (web flasher parts, branch `flasher-dist`)
- `tools/dist.sh` also writes the bootloader, partition-table and otadata parts and a `twiddle-p4-<version>.parts.json` (decimal offsets, sizes, sha256, taken from `build/flasher_args.json`), which the web flasher's generator reads. The app file keeps its name, so `ota.sh` is unaffected.

### Changed (HR monitor reconnect, 2.0.0-p4.11, branch `hr-edging`)
- A remembered heart rate monitor that is not found is retried at most every 30 s (was backing off to 120 s), as in the S3 firmware.

### Changed (Session tab layout, 2.0.0-p4.10, branch `hr-edging`)
- Same layout change as the S3 firmware: the HR edging row is one line (switch plus a button that opens the numbers, collapsed by
  default) and the Session rows are slightly shorter, so the tab does not need to scroll (a scroll repaints the whole tab).
  Layout checked by build only on this board.

### Fixed (HR edging, 2.0.0-p4.9, branch `hr-edging`)
- Lowering **Edges** mid-session, to or below the number of edges already done, now ends the session at once.
- Lowering the Intensity floor mid-session now applies to Lost / Rest / HoldOff immediately.
- The HR line on the Autopilot graph follows the theme (it kept the colour from the theme that was active at start-up).
- Host test `testLowerEdgesMidSession` covers the first two.

### Added (HR edging, 2.0.0-p4.8, branch `hr-edging`)
- **Heart rate monitor as a Bluetooth client** (`toys/hrm.cpp`, `hrm.h`, `hrmparse.h`): Heart Rate Service 0x180D / Measurement
  0x2A37 notify, 8- and 16-bit bpm and the sensor-contact flag decoded per the spec; readings outside 40..220 bpm or
  without contact are dropped. `hrm::bpm()`, `hrm::ageMs()`, `hrm::connected()`. New device kind `Hrm` (cap `CAP_HR`): scanned,
  connected, remembered in NVS like a toy and shown in the Devices tab with the live bpm, but it never gets output, has no
  Follow / share, is never the "selected device", and its dropping out is not a "lost device" stop. It uses one of the 3 NimBLE
  links (unchanged limit, first come first served); with none free the status reads "No free Bluetooth link: disconnect a toy first".
- **Edge controller** (`edge.h/.cpp`, pure C++): Climb / HoldOff (the edge) / Rest / Lost / Done from bpm samples and time. Edge at
  bpm >= edge bpm or a rise of N bpm within 10 s; rest until bpm <= edge - recovery drop and a minimum rest (maximum 2 min);
  edge bpm +step per finished cycle; ends after N edges or the Length; signal lost > 5 s goes to the floor, then stops;
  jumps of > 30 bpm need a second agreeing reading. Never above the Intensity level's ceiling.
- **Autopilot option "HR edging"** (Session tab, off by default; with it off the Autopilot behaves exactly as before): settings
  resting bpm, edge bpm (with "Learn" from the current bpm), recovery drop, edge count, raise per edge, fast rise, stored in the
  `vgpanel` namespace (`hre`, `hrrest`, `hredge`, `hrdrop`, `hrcount`, `hrstep`, `hrrise`). Autopilot tab: live bpm, phase, edge
  count, and a red HR line on the graph. STOP, the loss handshake and the low-battery rules are untouched.
- Host tests: `test/host/edge_test.cpp` (52 checks: normal cycle, fast rise, signal loss, artefacts, threshold, session end,
  limits, decoder); `test/host/run.sh` runs them with the toy tests.
- **Verified by build and host tests only.** No board was flashed and no real monitor was used.

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
