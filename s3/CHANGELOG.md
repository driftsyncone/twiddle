# Changelog

Release notes for each version come from this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and versions follow [semantic versioning](https://semver.org/).

## [Unreleased]

## [2.0.0] - 2026-11-03

First public release of the Twiddle firmware for the Guition JC8048W550C (ESP32-S3, 5 inch). Unofficial project, provided as is, with no
warranty or guarantee of any kind.

### What is in
- **Autopilot** with a graph window (the last 10 s and the next 30 s, "now" marked, tap to jump, Draw to edit the whole session), **Scripts**,
  **themes** (Midnight by default), **Bluetooth toys** with Connect and Forget on the Devices tab, and the Autoblow VacuGlide, VacuPump and Ultra.
- **Heart-rate edging** (work in progress, not yet tested with real sessions): Autopilot can be driven by a Bluetooth heart rate monitor. Host tests
  only. Do not rely on it; it is not a medical function.
- **Setup portal** at `http://192.168.4.1/setup` (join `Twiddle-Setup`) with Wi-Fi, token and Display pixel clock. The form never shows stored secrets.
- **On-screen Wi-Fi setup**: a **Connect to Wi-Fi** screen (Scan, network, password, Save & restart, Skip) appears when no Wi-Fi is saved, or a saved network has not joined 45 s after boot for a reason the user can fix; Wi-Fi only (token and toys stay on Settings and Devices). A **Save & restart** bar stays in view on every tab while Wi-Fi or token have unsaved changes. The phone page `/setup` is the fallback.
- **Known issue**: the on-screen keyboard and Settings scrolling are slow on this board (about 0.3 s per redraw, about 1.4 million pixels per second). Measured with `pio run -e panel-uitrace`: not the display copy, draw-buffer size or compiler speed; likely PSRAM bandwidth shared with the screen refresh. Open for 2.0.1.
- **Per-device web password** (6 characters, no look-alikes), shown only on the panel's Settings tab; admin endpoints answer 401 without login, and 5 wrong passwords in a row lock the web login for 60 s (429).
- **Display pixel clock with trial/Keep**: a new value is tried for 3 boots and reverts unless you tap Keep (default 16 MHz).
- **Optional rotary encoder** (Adafruit 5880 on the Guition connector; work in progress, not tested on hardware): detected at boot when present (Auto), or set Off.
- **Web flasher** parts and a secrets-free release build; the LGPL source and relink package is published with the release.

### Identity
- The firmware identifies as Twiddle everywhere: `GET /` starts with `twiddle 2.0.0`, mDNS `twiddle-s3` (Waveshare `twiddle-ws`), Bluetooth name `twiddle`,
  setup network `Twiddle-Setup`, web login realm `twiddle`. Settings (NVS namespace `vgpanel`) are unchanged. `ota.sh` accepts a board that still
  answers as `vacuglide-panel` (old firmware) and refuses other projects. Still the first public release.

### Fixed
- The encoder's Autopilot scale (a lowered speed percentage) is now reset to 100 % whenever an Autopilot session starts or you come back from another
  mode. Only resuming the same paused session keeps it, so a resume never jumps up. Host-tested (`encscale.h`).

### Known limitations
- Tested on one JC8048W550C unit; the resistive (R) and no-touch (N) versions are not supported, and the Waveshare 5 inch build is not part of this release.
- If the screen shows coloured bars, change the Display pixel clock (for example to 16 MHz) in the setup portal.
- The rotary encoder and heart-rate edging have not been tested with real hardware or sessions.
- The CH340 USB-serial chip may need a driver on Windows; if the flasher cannot connect, hold BOOT, tap RESET and release BOOT.
- No warranty; flashing can fail; keep a way to stop the device that you can reach by hand.

### Development history (folded into 2.0.0)

The panel is now a remote for more than the VacuGlide: the Autoblow VacuPump
and Autoblow Ultra over Autoblow's cloud, and Lovense and Kiiroo Keon toys over
Bluetooth, straight from the panel. One output drives all of them at once.

### Rotary encoder merged into the release line (2.0.0-dev.24)

- `encoder-s3` merged into `release-prep-s3` (Devices-tab, graph-window, per-device password and HR-edging work all kept). The encoder
  README section is marked work in progress / untested on hardware; new README section for feature requests and bug reports.

### Rotary encoder speed control (2.0.0-dev.22, branch `encoder-s3`)

- New: an Adafruit I2C STEMMA QT rotary encoder (product 5880, seesaw) on the Guition P4 connector (GPIO17 SDA / GPIO18 SCL). Turn =
  active speed (Remote: manual speed; Autopilot / HR edging: live scale 20 to 100 % of the plan, never above the screen's Intensity;
  Scripts: ignored). Short press = pause / resume, resuming ramps up over about 1.5 s. Hold 1 s = hard STOP (`control::stopAll`).
  The encoder's NeoPixel shows off / paused / running at low brightness.
- Settings: Encoder Auto / Off / On (restarts), step 1 / 2 / 5 %, direction. Off unless found at boot (Auto) or set On. The loss
  handshake and the low-battery stop are untouched; a missing or unplugged encoder is ignored (10 ms bounded I2C timeouts, re-probe
  every 2 s when it was lost or mode is On). The first reading after connect never turns into a step.
- The probe reads the seesaw hardware id and product number, so a MAX17048 gauge at the same default address 0x36 is not taken for an
  encoder and the encoder is not taken for a gauge. With the encoder on the P4 connector, GPIO17 is not used as the battery ADC.
- Waveshare 5": compiled out (GPIO17/18 drive the display; no free I2C connector).
- Host tests: `test/host/seesaw_test.cpp` (frames, probe, click accumulation, debounce, short / hold). Not yet tested on hardware.
- `GET /i2c` reports the encoder on the P4 pins.

### Licensing notices (docs only, branch `release-prep-s3`)

- Licence decision: Twiddle's own code stays under CC BY-NC-SA 4.0 (`LICENSE` unchanged). New `LICENSE-NOTES.md` carries the draft LGPL
  additional-permission paragraph; `tools/lgpl-package.sh` writes it to `WRITTEN-OFFER.txt`, `ADDITIONAL-PERMISSION.txt` and the source archive,
  with the contact from `OFFER_CONTACT` (default: the public issues page; an empty value is refused).
- Buttplug attribution corrected to the upstream line "Copyright (c) 2016-2023, Nonpolynomial Labs, LLC" (was 2016-2026). The SIL OFL 1.1 text for
  LVGL's Montserrat fonts and the NimBLE-Arduino NOTICE are now in `THIRD_PARTY_NOTICES.md`.
- README: disclaimer and safety section (no warranty, adults only, not a medical device, keep a hand-reachable way to stop, no affiliation).

### Autopilot graph: a moving window (2.0.0-dev.23, branch `release-prep-s3`)

- The graph is now a zoomed window on the session: the last 10 s and the next 30 s, with "now" (the orange line) at 25 % from the left and an axis in
  seconds relative to now (-10 s, now, +30 s). It is clamped at the start and end of the session and redrawn twice a second at most (no extra memory).
- HR edging: the future depends on the heart rate and is not planned, so the window shows the actual speed and the red heart rate line for the last 10 s
  and, ahead, only the current target speed as a dashed flat line, with the note "HR-driven: depends on heart rate".
- Tapping the graph still jumps to the tapped moment (now measured in the window). **Draw** (Session tab) switches the graph to the whole session
  while it is on, so every wave's peak can still be dragged; the axis then reads 0:00 to the session length.
- The window arithmetic is `apwindow.h`, host-tested.

### Devices tab: Connect for remembered devices, heart rate row (2.0.0-dev.22, branch `release-prep-s3`)

- A remembered Bluetooth device that is not connected now has **Connect** next to **Forget** (it stays remembered; Connect tries at once and restarts the
  retry backoff). While it connects the button reads "Connecting" and is disabled; once up the status says "Connected" and only **Forget** shows.
- A heart rate monitor's row never shows Follow or the 25-100 % buttons, however early the row was built, and tapping the row never selects it (decided
  every refresh by `devrow.h`, host-tested; the monitor is recognised as an input from the advert or NVS, before any link).

### Web-flasher release preparation (2.0.0-dev.21, branch `release-prep-s3`)

- LGPL package: `tools/lgpl-package.sh <tag>` builds the corresponding-source archive (with `REBUILD.md`, a written offer, the pinned
  versions) and the licence page for a release; `docs/lgpl-compliance.md` is the draft compliance statement (a lawyer's read of the LGPL question is still open, see its
  last section). `env:panel-release` pins the libraries to exact versions. `THIRD_PARTY_NOTICES.md` now says which Arduino-ESP32 parts are
  statically linked (found from the link map).
- README: **Flash the firmware** describes the web flasher and the manual path.

### Display pixel clock: trial, Keep, automatic revert (2.0.0-dev.20)

- A pixel clock chosen on the setup page or with `GET /pclk?mhz=N` is now **on trial**. After the restart the panel shows a banner
  with a **Keep** button; only a tap makes the clock permanent. A clock that garbles the picture (nobody can tap) goes back to the last
  confirmed one after 3 unconfirmed boots (switch the panel off and on three times). Logged as `display: pixel clock N MHz on trial
  / kept / not confirmed ... reverted`; the setup page and `GET /pclk` show the trial state. Guition only.
- Saved values keep working: a unit that has only the old `pclk` key (for example the 14 MHz one) boots plain at that value, with no
  trial. New NVS keys `pclkp` (confirmed), `pclkt`, `pclkb`.
- The setup page's clock list also offers a saved value outside 12-18 (before, a browser preselected 12 and a save changed it).

### Secrets-free release build and web-flasher parts (2.0.0-dev.19)

- New PlatformIO env `panel-release` (`-DTWIDDLE_RELEASE=1`): `include/secrets.h` is not read at all, so the image has no Wi-Fi
  name or password, device token, web password, host or hostname override; everything comes from the Twiddle-Setup portal and lives
  in NVS. Builder directories are mapped out of the image (`-ffile-prefix-map`).
- `tools/check-release-image.sh` scans the image, bootloader and partition table for values from `secrets.h`, a private hash
  denylist, and home-network patterns (private IPs, lab names, home paths, e-mail, MAC). Keep it out of any public tree.
- `tools/dist.sh` writes `dist/twiddle-s3-<ver>.bin` (+ `.sha256`), `-bootloader.bin` (0x0), `-partition-table.bin` (0x8000),
  `-boot_app0.bin` (0xe000) and `twiddle-s3-<ver>.parts.json` for the twiddle-flasher. Offsets come from PlatformIO's own
  `FLASH_EXTRA_IMAGES` (`tools/pio_flash_layout.py`) and are cross-checked against the partition table in the build.

### Per-device web password (2.0.0-dev.19, branch `release-prep-s3`)

- The web login (user `ota`, used by `/update`, `/setup` on the LAN, `/log`, `/screenshot`, `/restart` ...) is now a random
  12-character password per panel (an alphabet without look-alikes), made at first boot and kept in NVS. It is shown **only on the
  Settings tab** ("Web login"; tap to reset, with a confirmation), never on a web page, in the serial log or at `GET /`. Until it
  exists the admin addresses answer 403, a wrong one 401 (an empty password never matches). It is made after Wi-Fi starts, because the
  ESP32-S3 random number generator is only a true one while the radio runs.
- Dev builds with `OTA_PASSWORD` in `include/secrets.h` keep that fixed password (the tab says so; it cannot be reset there).
- `ota.sh` reads `$OTA_PASSWORD`, else `secrets.h`, else asks; the password goes to curl on stdin, not the command line. An upload
  with a wrong password now gets a 401/403 instead of a dropped connection.
- Source files include `build_config.h` instead of `secrets.h`: a checkout without `secrets.h` builds with empty defaults.

### Setup portal no longer echoes secrets (2.0.0-dev.18, branch `release-prep-s3`)

Audit of the setup form against the flaws found on the P4 (all four were present):
- The stored Wi-Fi password and Autoblow token were written into the HTML (`value=`). The setup access point's password is public,
  so anyone in range could read them. The form now shows no stored secret; an empty field keeps the stored value, a different
  network clears the stored password, a typed value replaces it.
- The README, the serial log and the panel's status text pointed at `http://192.168.4.1/`; the form is at `/setup`. Fixed.
- `GET /` on the setup access point showed the plain-text status page; it now redirects to `/setup`.
- The browser could refill old values: the form, password and token now have `autocomplete=off`, and the page is `no-store`.
- The setup network is now **Twiddle-Setup** (password `twiddle-setup`), as the web flasher says (was VacuGlide-Panel).

### Supported hardware, pixel clock default 16 MHz (2.0.0-dev.17, branch `variants-s3`)

- The Guition pixel clock now defaults to **16 MHz** (was 14): two of the three sources (vendor demo, ESPHome, the second unit) use
  16. A clock saved in NVS (`pclk`) is kept, `GET /pclk` is unchanged. A panel that was never given a value changes from 14 to 16;
  the unit that needs 14 has it saved.
- The setup page (`/setup`, also the Wi-Fi portal) has a **Display pixel clock** choice (12-18) with a short display-test hint
  (colour pattern or shaking: pick another value, most units 16, a few 14). Guition only; the Waveshare keeps its fixed clock.
- README: new **Supported hardware** section (supported, probably working, unsupported SKUs, how to identify a unit).
### Faster heart rate monitor reconnect (2.0.0-dev.16, branch `hr-edging-s3`)

- A remembered heart rate monitor that is not found is retried after 15 s, 30 s and then every 30 s (other remembered toys still back
  off to 120 s). Measured with a fake monitor advertising once a second: a 4 s try at the 25 % connect duty finds it on ~13 % of
  tries (mean reconnect ~74 s with a flat 6 s retry; the old 120 s cap makes it minutes). Wi-Fi was unaffected (RSSI and status
  sampled every 2 s, same as without BLE tries, even at 75 % scan duty).

### Session tab no longer scrolls by default (2.0.0-dev.15, branch `hr-edging-s3`)

- The HR edging rows made the Session tab taller than the screen, so it scrolled for the first time, and a scroll repaints the whole
  tab (about 0.25 s a frame, 3.4 fps measured; the Settings tab scrolls at 3.8 fps for the same reason). The HR edging row is now one
  line (switch plus a button) and the six numbers, the Learn button and the live line sit in a section that is collapsed by default;
  the Session rows are 42 px (were 48) so seven rows fit and the tab does not scroll (measured: nothing to scroll, as before HR edging).
  Open the section with the arrow button; it scrolls as the Settings tab does.

### HR edging fixes (2.0.0-dev.14, branch `hr-edging-s3`)

- Lowering **Edges** in the settings mid-session, to or below the number of edges already done, now ends the session at
  once (it used to run on and could go past the target).
- Lowering the Intensity floor mid-session now applies to Lost / Rest / HoldOff immediately (they stayed at the old floor).
- Host test `testLowerEdgesMidSession` covers both (56 checks).

### HR edging: a heart rate monitor drives Autopilot (2.0.0-dev.13, branch `hr-edging-s3`)

Backport of the ESP32-P4 firmware's feature (same behaviour and defaults, same controller code).

- **Heart rate monitor as a Bluetooth client** (`toys/hrm.cpp`, `hrm.cpp`, `hrmparse.cpp`): Heart Rate Service 0x180D /
  Measurement 0x2A37 notify, 8- and 16-bit bpm and the sensor-contact flag decoded per the spec; readings outside 40..220 bpm
  or without contact are dropped. `hrm::bpm()`, `hrm::ageMs()`, `hrm::connected()`. New device kind `Hrm` (cap `CAP_HR`):
  scanned, connected and remembered in NVS like a toy and shown in the Devices tab with the live bpm, but it never gets
  output, has no Follow / scale, is never the selected device, and its dropping out is not a "lost device" stop. It uses one
  of the 3 NimBLE links (unchanged limit, first come first served, nothing preempted); with none free the status reads
  "No free Bluetooth link: disconnect a toy first" (was "Too many toys connected (max 3)").
- **Edge controller** (`edge.h/.cpp`, pure C++): Climb / HoldOff (the edge) / Rest / Lost / Done from bpm samples and time.
  Edge at bpm >= edge bpm or a rise of N bpm within 10 s; rest until bpm <= edge - recovery drop and a minimum rest (maximum
  2 min); edge bpm +step per finished cycle; ends after N edges or the Length; signal lost > 5 s goes to the floor, then
  stops; jumps of > 30 bpm need a second agreeing reading. Never above the Intensity level's ceiling.
- **Autopilot option "HR edging"** (Session tab, off by default; with it off the Autopilot behaves exactly as before):
  resting bpm, edge bpm (with Learn), recovery drop, edges, raise per edge, fast rise, stored in the `vgpanel` namespace
  (`hre`, `hrrest`, `hredge`, `hrdrop`, `hrcount`, `hrstep`, `hrrise`). Autopilot tab: live bpm, phase, edge count, and a red
  HR line on the graph. STOP, the loss handshake and the low-battery rules are untouched.
- Host tests: `test/host/edge_test.cpp` (52 checks), run with `test/host/run.sh` (g++ only).
- **Verified by build (Guition and Waveshare) and host tests only.** No board was flashed and no real monitor was used.
  Internal RAM cost is about 0.9 KB of static data; no new task or large allocation.

### Renamed to Twiddle (2.0.0-dev.13)

Renamed to Twiddle; mDNS name, setup-portal AP, Bluetooth name, auth realm and `GET /` identity changed; settings kept; boards on the old firmware need one update.

The project is now called **Twiddle** (was "vacuglide-panel"); this repository is `twiddle-s3`, the ESP32-P4 sibling
is `twiddle-p4`. "VacuGlide" remains wherever it means the Autoblow device. What changed on the panel:

- mDNS / DHCP name is `twiddle-s3` (Waveshare `twiddle-ws`), was `vgremote` / `vgremote-ws`. `PANEL_HOSTNAME` in
  `secrets.h` still overrides it. The Bluetooth name is `twiddle` too.
- The setup-portal Wi-Fi is `Twiddle-Setup` (password `twiddle-setup`), was `VacuGlide-Panel` / `vacuglide`.
- The HTTP Basic auth realm is `twiddle` (was the library default "Login Required"), same as the P4 firmware.
- `GET /` starts with `twiddle <version>` (was `vacuglide-panel <version>`).
- Settings are kept: the NVS namespace is unchanged on purpose.
- A board on the old firmware needs one more update, found under its old name (`vgremote.local`); after that it
  answers as `twiddle-s3.local`. Update your `OTA_HOST` in `include/secrets.h`.

### Display pixel clock is a per-unit setting (2.0.0-dev.12)

Guition JC8048W550C units are not all the same: the first one needs a 14 MHz pixel clock (16 MHz garbles it under
PSRAM load, 12 MHz gives the panel's own colour self-test), while a later unit (MAC ending 87:08) shows the colour
self-test at 14 MHz and needs 16 MHz, the value the ESPHome config uses. The clock is now stored in NVS (`pclk`,
default 14, read at boot). A unit that cannot show its own Settings tab is fixed over the web: `GET /pclk` shows it,
`GET /pclk?mhz=16` saves it (login required), then restart (`POST /restart`). The Waveshare keeps its fixed clock.

<!-- TESTING STATUS: fill in before release -->
At the time of writing this version is compiled and running on both panels
(Guition and Waveshare). **Bluetooth toys, the VacuPump and the Ultra have not
been tested on real hardware yet.**

### Added

- **Colour themes** (Settings, Theme row; 2.0.0-dev.11): Daylight (default),
  Midnight, Ocean, Forest, Ember and Blush. A switch applies live and is kept
  across restarts. Same six palettes as the ESP32-P4 panel firmware, so all
  panels look alike. Every colour in the UI is now a named theme role.
  `GET /theme` (login) lists them; `/theme?n=N` switches live, `&save=1` keeps it.
  Note: Daylight is a light look; the previous fixed look was dark, closest to
  Midnight.
- **Devices tab** (tabs are now Remote, Autopilot, Session, Scripts, Devices,
  Settings): Scan for Bluetooth toys, one row per device with a **Follow**
  switch, a **25 / 50 / 75 / 100 %** scale, and Connect / Forget.
- **One output, every device:** the Remote speed slider or Autopilot drives
  every device that follows, each at its own scale. If any following device
  drops out mid-run, the panel stops everything, as it always did for the
  VacuGlide.
- **Autoblow VacuPump** (cloud): the panel detects the device type behind the
  token by itself. Speed sets the vacuum depth in Manual mode (100 % = 40 kPa).
  STOP vents to 0 kPa first, because the pump's own stop keeps the vacuum.
- **Autoblow Ultra** (cloud): speed sets the oscillation speed over the full
  stroke.
- **Lovense toys over Bluetooth**, no app or PC needed: the models the Buttplug
  project knows. Vibration (0-20) follows the level; the Max's air pump (0-3)
  and rotating heads follow it too, and the Solace oscillates.
- **Kiiroo Keon (gen 1), Keon 2 and Keon WiFi over Bluetooth:** continuous full
  strokes, stroke speed follows the level.
- **OSSM over Bluetooth:** the panel puts it in Stroke Engine and drives its
  speed; stroke, depth, sensation and pattern stay as set on the machine, and
  the machine's knob still caps the speed. Turn the RADR off first: the OSSM
  takes one controller at a time.
- **Safety for Bluetooth toys:** STOP goes out on its own high-priority task,
  so it never waits behind a slow toy; a toy that reconnects stays at 0 until
  the panel's level has been 0; any following toy that drops out mid-run stops
  the session.
- Up to **3 Bluetooth toys** connected at once. Remembered toys reconnect by
  themselves when they're in range.
- **Autopilot graph:** the planned session as a graph with the played part
  greyed out, a playhead, "peak in" and time left. Tap the graph to jump.
- **Back off / More** steer Autopilot live: drop to a rest now (or rest
  longer), go to the peak now (or hold it longer and a bit higher).
- **Session tab:** Intensity, Edge, Vacuum, Length (auto or 5/10/20/30 min),
  Peaks (rising, shuffled, wild), three favourites (tap loads, hold saves),
  Draw (drag on the graph to set each wave's peak) and Reset. Changes
  mid-session blend in without losing the place.
- **Brightness slider** and auto-dim to a quarter after a minute without a
  touch (Guition; the Waveshare backlight is on/off only).
- **Wi-Fi setup portal:** a panel with no Wi-Fi configured opens its own
  network, **VacuGlide-Panel** (since 2.0.0-dev.13: **Twiddle-Setup**), with a setup page for Wi-Fi and token.

### Changed

- **Weak Wi-Fi is tolerated:** a device only counts as lost after a gap without
  an answer (it used to be the moment Wi-Fi blinked). The panel picks the
  strongest access point, transmits at full power and waits longer before
  giving up on a quiet access point. The top bar says "Wi-Fi dropped,
  reconnecting..." meanwhile.
- **Much steadier Wi-Fi (2.0.0-dev.9):**
  - A remembered Bluetooth toy that is switched off no longer keeps the radio
    busy. The panel used to look for it 3 s out of every 5 s, listening all
    the time, and Wi-Fi starved. It now tries after 15 s, 30 s, 60 s, then
    every 2 minutes, and its connects and scans only listen part of the time.
    On the Guition this took ping loss from 1.1 % to 0 and the worst answer
    from 1.6 s to 0.17 s.
  - **Bluetooth on/off** in Settings. Bluetooth only starts when it's on, or
    (default) when a toy is remembered. Without it, Wi-Fi runs without modem
    sleep: answers come about 6x faster (12 ms instead of 77 ms on average),
    and about 50 KB more memory stays free for the cloud connection.
  - **Wi-Fi gaps** in Settings: 10, 20 or 30 s (default 20 s, was a fixed
    10 s) without an answer before the cloud device counts as lost and the
    session stops. The device keeps its last speed during the gap, and commands
    that didn't land are sent again once the link is back.
  - The top bar shows "No answer / Wi-Fi dropped, reconnecting... N s (stops
    at 20 s)" with an orange dot during a gap, instead of nothing until the
    loss.
  - After a loss the panel stays on the same Autoblow server instead of
    starting over with discovery (one TLS handshake instead of two on a weak
    link). Request timeout is 6 s (was 8 s). The Wi-Fi reconnect watchdog
    restarts a stuck attempt after 10 s (was 20 s).
  - `GET /` shows signal, channel, access point, power save, Wi-Fi drops,
    cloud losses and the grace period; `/log` logs every Wi-Fi drop (reason,
    signal) and how long the link took to come back.
- The status line, the lost banner and the speed label cover every device and
  name the one that dropped out. VacuGlide-only controls (Looser/Tighter, vacuum,
  scripts) grey out when no VacuGlide follows.
- `GET /screenshot?tab=` now takes 0-5; Settings is tab 5.
- Under the hood: TLS and the Bluetooth host live in PSRAM and the display uses
  one draw buffer, so Bluetooth fits next to the cloud connection. Wi-Fi modem
  sleep is now on (minimum), which Bluetooth on the same radio requires.

### Fixed

- **Vacuum maintenance tightens now instead of loosening.** On the real
  VacuGlide 2 the Autoblow valve names are the other way round from the
  manual: `stroke-plus` lets air out (tighter, more suction) and `stroke-minus`
  lets air in (looser). Vacuum maintenance on the Remote and Autopilot used
  `stroke-minus`, so every "keep suction" pulse loosened the sleeve; it now
  pulses `stroke-plus`, and pauses while either valve button is held.
- The valve buttons are now labelled **Looser** and **Tighter** (were
  "Stroke -" / "Stroke +"). Each opens the same valve as before (Looser =
  `stroke-minus`, Tighter = `stroke-plus`); the labels now say what you feel.
- After a Wi-Fi drop, the first request no longer waits out an 8 s timeout on
  a dead connection.

### Known limits

- One Autoblow device per panel (one token).
- A Bluetooth toy pairs with one controller at a time: close the Lovense Remote
  app (or any other app) first.
- The VacuPump must be on its Online screen for the cloud to reach it.

## [1.2.0] - 2026-10-19

### Added

- **Waveshare ESP32-S3-Touch-LCD-5** (800x480) as a second supported board,
  tested on hardware.
- Battery % from a two-resistor voltage divider on GPIO17, as an alternative to
  the fuel gauge.
- **Flip screen** for mounting the panel the other way round (touch follows).
- Settings: **Restart**, and **Power off**, which explains the battery button
  (software can't cut the power on this board) and offers Screen off.
- `POST /restart`, `POST /resync` and `GET /adc` for diagnostics.
- Our own case designs in `case/` (fit tests in progress).

### Changed

- The device token shows only its last 3 characters.
- Autopilot plans the whole session up front and runs on a clock, so changing
  settings keeps your place.
- Vacuum pulses are timed by the network task, more exactly.
- The web server runs on its own task, so the screen stays responsive during
  uploads and requests.
- The screen goes dark while an update is written.
- `ota.sh` judges an update by the firmware's embedded build hash.

### Fixed

- The display could garble after flash writes (update confirm, saved settings);
  it now resyncs itself.

## [1.1.0] - 2026-10-17

### Added

- **Battery level** from a MAX17048 / MAX17043 fuel gauge, on either I2C bus,
  with a safety stop at 5 % or less on battery. Not yet tested with a real
  gauge.
- **Update safety net:** a new firmware runs on probation and the panel rolls
  back by itself if it crashes, freezes or never reaches Wi-Fi.
- **Build your own** guide in the README: parts, board checks, battery, case,
  assembly.
- `GET /i2c` diagnostic.

## [1.0.0] - 2026-10-16

First public release.

### Added

- **Remote:** speed slider, Start/Pause, hold-to-open Stroke - / Stroke +,
  vacuum maintenance.
- **Autopilot (edging):** the project's own on-device generator, with
  Intensity, Edge, Vacuum and Finish Me. Not yet tested on a VacuGlide at
  release.
- **Scripts:** play the device's 30 local-script slots; name them on the panel.
- **Settings:** Wi-Fi with a network scan, device token, screen timeout.
- Screen sleep, STOP on every screen, connection problems explained in the top
  bar, and a red banner when the panel loses the device mid-run.
- Firmware updates over Wi-Fi (`ota.sh`) and `GET /screenshot`.

[2.0.0]: https://github.com/driftsyncone/vacuglide-panel/compare/v1.2.0...HEAD
[1.2.0]: https://github.com/driftsyncone/vacuglide-panel/releases/tag/v1.2.0
[1.1.0]: https://github.com/driftsyncone/vacuglide-panel/releases/tag/v1.1.0
[1.0.0]: https://github.com/driftsyncone/vacuglide-panel/releases/tag/v1.0.0
