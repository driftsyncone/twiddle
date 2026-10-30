# Changelog

Release notes for each version come from this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and versions follow [semantic versioning](https://semver.org/).

## [2.0.0] - unreleased

The panel is now a remote for more than the VacuGlide: the Autoblow VacuPump
and Autoblow Ultra over Autoblow's cloud, and Lovense and Kiiroo Keon toys over
Bluetooth, straight from the panel. One output drives all of them at once.

### Per-board network names and Midnight default (2.0.0-dev.14)

Guition S3 answers as `twiddle-s3` (was `twiddle`), and the default theme is Midnight (was Daylight).

- mDNS / DHCP name per board: Guition S3 `twiddle-s3`, Waveshare `twiddle-ws`; the P4 sibling uses `twiddle-p4`.
  `PANEL_HOSTNAME` in `secrets.h` still overrides it. `ota.sh` and `secrets.example.h` default to `twiddle-s3.local`;
  update `OTA_HOST` if you used `twiddle.local`. Boards still on the pre-rename firmware answer only as
  `vgremote` / `vgremote-ws`, and boards on dev.13 answer as `twiddle`.
- Default theme is **Midnight** when nothing is saved (also for out-of-range stored values); the Settings chip shows it
  selected on a fresh panel. Panels that saved a theme keep it. Daylight stays available. This corrects the dev.11 note
  below: Daylight was then the default and turned the S3 light; Midnight restores the original dark look. Palettes are
  unchanged and identical to the P4's.

### Renamed to Twiddle (2.0.0-dev.13)

Renamed to Twiddle; mDNS name, setup-portal AP, Bluetooth name, auth realm and `GET /` identity changed; settings kept; boards on the old firmware need one update.

The project is now called **Twiddle** (was "vacuglide-panel"); this folder is the ESP32-S3 firmware, the ESP32-P4 sibling
is in `../p4`. "VacuGlide" remains wherever it means the Autoblow device. What changed on the panel:

- mDNS / DHCP name is `twiddle` (Waveshare `twiddle-ws`), was `vgremote` / `vgremote-ws`. `PANEL_HOSTNAME` in
  `secrets.h` still overrides it. The Bluetooth name is `twiddle` too.
- The setup-portal Wi-Fi is `Twiddle-Setup` (password `twiddle-setup`), was `VacuGlide-Panel` / `vacuglide`.
- The HTTP Basic auth realm is `twiddle` (was the library default "Login Required"), same as the P4 firmware.
- `GET /` starts with `twiddle <version>` (was `vacuglide-panel <version>`).
- Settings are kept: the NVS namespace is unchanged on purpose.
- A board on the old firmware needs one more update, found under its old name (`vgremote.local`); after that it
  answers as `twiddle.local`. Update your `OTA_HOST` in `include/secrets.h`.

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
