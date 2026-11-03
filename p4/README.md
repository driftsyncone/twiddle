# Twiddle (ESP32-P4)

A standalone touchscreen remote for the **Autoblow VacuGlide** (and a few Bluetooth toys), running on the
**Guition JC1060P470C**: an ESP32-P4 board with a 7" 1024x600 touch display and an ESP32-C6 coprocessor that
provides Wi-Fi and Bluetooth LE. It talks to Autoblow's public device API with your device token, so no phone, PC
or hub is needed once it is set up.

This is the ESP-IDF 5.5 port of the Arduino/ESP32-S3 Twiddle firmware (in [`../s3`](../s3)). The Arduino firmware for the
smaller ESP32-S3 panels lives in that folder; see [docs/other-panels.md](docs/other-panels.md).

> **Unofficial project, provided as is, with no warranty or guarantee of any kind.** Everything here is unofficial. It is not made, endorsed or supported by Autoblow, Lovense, Kiiroo, Research & Desire, or any product or company named Twiddle, and is not affiliated with them. Names are used only to describe compatibility. Use it at your own risk; see [Disclaimer and safety](#disclaimer-and-safety).

## Supported hardware

| Part | What |
|---|---|
| Board | Guition **JC1060P470C**: ESP32-P4 (chip rev v1.3 tested), 16 MB flash, PSRAM, ESP32-C6 coprocessor |
| Display | 7" 1024x600, MIPI-DSI, JD9165 controller. The board is sold with two panel variants; this firmware is for the **V2** panel (2026 batch). The original variant needs a different init table and timings and is **not supported** |
| Touch | GT911 capacitive |
| Radio | Wi-Fi and Bluetooth LE through the ESP32-C6 over SDIO (esp_hosted) |

Other boards are not supported. Pins and board notes: [docs/hardware.md](docs/hardware.md).

## Features

- **Remote**: speed slider (5 to 100 %), Start/Pause, hold-to-open **Looser** / **Tighter** valve buttons, vacuum
  maintenance Off / Low / Medium / High.
- **Autopilot**: an on-device edging generator (this project's own, not Autoblow's patterns). A session is eight
  waves; each climbs to a peak, holds it, then drops to a rest. Controls for Intensity, Edge, Vacuum, session
  length and peak order, steering buttons (More, Back off, **Finish Me**), a graph you can tap to jump or draw on,
  and three favourite sessions.
- **Scripts**: play any of the device's 30 local-script slots, with names you give them (stored on the panel).
- **Devices**: the Autoblow device plus Bluetooth toys (Lovense, Kiiroo Keon, OSSM drivers) and a standard BLE heart rate monitor (an input, not a toy; see HR edging below). Scan, Connect,
  Follow with a per-device scale; remembered toys reconnect by themselves.
- **STOP** on every screen, and a connection-loss safety behaviour (see below).
- **Settings**: Wi-Fi with network scan, device token, screen sleep, brightness, Wi-Fi gap tolerance, Bluetooth
  on/off, six colour themes (live switching; Midnight is the default).
- **Setup portal**: with no Wi-Fi configured the panel opens its own access point and a web page to enter Wi-Fi and
  token from a phone.
- **Update over Wi-Fi** with automatic rollback if the new firmware does not come up
  ([docs/ota-and-rollback.md](docs/ota-and-rollback.md)).
- Network name `twiddle-p4.local` by default (`PANEL_HOSTNAME` in `secrets.h` changes it; the S3 firmware uses `twiddle-s3`).
- A small web server for status, log, screenshots and diagnostics ([docs/architecture.md](docs/architecture.md)).

The P4 board's Wi-Fi and Bluetooth radio is a separate ESP32-C6 with its own firmware, which the web flasher does not install;
factory C6 firmware can be too old to join Wi-Fi. See [docs/c6-firmware.md](docs/c6-firmware.md).

The user documentation is in [docs/user-guide.md](docs/user-guide.md) and [docs/settings.md](docs/settings.md).

## Status

Working and tested on the board: display and touch, the six tabs, Wi-Fi through the C6, the Autoblow cloud client
(tested against an emulator of the cloud API), Bluetooth scanning, firmware update over Wi-Fi with rollback, themes
(checked by screenshot).

**HR edging** (Session tab, Autopilot option, off by default): pair a standard BLE heart rate monitor (Heart Rate Service
0x180D, Measurement 0x2A37; 8- and 16-bit bpm, sensor contact flag, 40 to 220 bpm accepted) on the Devices tab and Autopilot
edges by your heart rate instead of the planned waves: it climbs like the plain Autopilot, EDGES when the bpm reaches the
edge bpm or rises by the Fast rise amount within 10 s, drops to the level floor and rests until the bpm is down by the
recovery drop (and a minimum rest), then raises the edge bpm a little and climbs again. It ends after N edges or the Length
setting, and stops (floor first) when the signal is lost for more than 5 s. The speed never exceeds the Intensity level's
ceiling. The monitor takes no output. It shares the **3 Bluetooth links** with the toys (first come, first served; if all 3
are in use it reads "No free Bluetooth link: disconnect a toy first" and no toy is dropped). Recommended for testing: Polar H10
(enable "two BLE devices" once in the Polar app). The generator is our own; no pattern numbers from Autoblow's site.

**Not verified yet** (implemented, but not confirmed on hardware):

- use by finger on the real screen over a longer period;
- commanding a real VacuGlide (all testing so far used an emulator);
- **HR edging** and the heart rate monitor client (`edge.h`, `toys/hrm.cpp`, firmware 2.0.0-p4.11): build and host-test verified only (`test/host/run.sh`: 52 edge/decoder checks); no board was flashed and no real monitor (the Polar H10 is the recommended one) has been used;
- any Bluetooth toy (the drivers pass the offline encoder tests in `test/host/`, no real toy has been used);
- the Settings tab visually, and the setup portal from a phone;
- the **release prerequisites** (release build, per-device web password, the portal Display select with the Keep/revert
  fallback, the chip-revision line; branch `release-prep`, 2.0.0-p4.12): build and host-test verified only, no board flashed;
- a battery fuel gauge (the board has none; the code looks for a MAX17048/MAX17043 on the touch I2C bus).

**Known limits:** the **Flip** button is greyed and reads "Flip: n/a" on this board (the DSI panel does not support it in this
firmware). The Autopilot settings (Intensity, Edge and so on) are not saved across restarts; favourites, script
names and the Settings tab values are. The bring-up history and lessons are in [docs/history.md](docs/history.md).

**Rotary encoder (not implemented on the P4 yet).** The S3 firmware has an optional Adafruit 5880 rotary encoder for speed (turn = speed, press = pause/resume, hold = STOP; work in progress there, untested on hardware). It is not on the P4 yet: the 7" board's rear I2C connector pins are not confirmed from the schematic (community reports say it shares the touch bus GPIO7/GPIO8, and one report has SDA/SCL swapped), and the encoder's default address 0x36 is already taken on that bus. See [docs/feature-matrix.md](docs/feature-matrix.md).

## Safety

- Stroke-plus **tightens**, stroke-minus **loosens**; the buttons are labelled Looser and Tighter. Vacuum pulses
  use stroke-plus and are skipped while either valve is held.
- The panel controls the device through Autoblow's cloud. **If the panel loses its connection it cannot stop the
  device.** The panel waits for the configured Wi-Fi gap (10, 20 or 30 s), then treats the device as lost, shows a
  red banner and sends only STOP until the device acknowledges. The device keeps its last speed during the gap.
- With a battery gauge present, a nearly empty battery keeps the device stopped.
- Never rely on the panel as the only way to stop a device: switch the device off at the device itself.

## Quick start

You need a Linux machine, ESP-IDF **v5.5.1**, and the board connected by USB-C.

```sh
git clone https://github.com/driftsyncone/twiddle && cd twiddle/p4
export PATH=/usr/bin:$PATH            # put the system Python first if another Python shadows it
. ~/esp/esp-idf/export.sh             # ESP-IDF v5.5.1
cp main/secrets.example.h main/secrets.h && $EDITOR main/secrets.h
idf.py set-target esp32p4             # first time only
idf.py build
idf.py -p /dev/ttyACM0 flash
```

`main/secrets.h` is ignored by git and holds the defaults baked into your build (Wi-Fi, device token, optionally a fixed
update password). Leave `WIFI_SSID` empty to get the setup portal on first boot. For an image to give to other people
build with `idf.py -DTWIDDLE_RELEASE=ON build`: nothing of yours is compiled in (`tools/check-release-image.sh` verifies it),
Wi-Fi is asked for on the panel's own screen at first start (the token and toys are set on the Settings and Devices tabs); the display type is chosen on the `Twiddle-Setup` portal (the phone page is also a fallback for Wi-Fi and token), and the panel makes its own random web password
(shown only on the panel's Settings tab, never on a web page; user `ota`, used by `ota.sh`). Full steps, including installing
ESP-IDF and reading the serial log: [docs/dev-setup.md](docs/dev-setup.md). Per-board flashing and backup:
[docs/flashing.md](docs/flashing.md). After the first flash, later builds can be sent over Wi-Fi with
`./ota.sh` ([docs/ota-and-rollback.md](docs/ota-and-rollback.md)).

Run the offline tests with `test/host/run.sh` (needs `g++` and, once, a build to download the components).

## Documentation

Everything is indexed in [docs/README.md](docs/README.md).

| For users | For developers |
|---|---|
| [User guide](docs/user-guide.md) | [Dev setup](docs/dev-setup.md) |
| [Settings reference](docs/settings.md) | [Architecture](docs/architecture.md) |
| [Themes](docs/themes.md) | [How-to recipes](docs/howto.md) |
| [Troubleshooting](docs/troubleshooting.md) | [Contributing](CONTRIBUTING.md), [Changelog](CHANGELOG.md) |

## Licence

The project's own code is licensed under **CC BY-NC-SA 4.0** (see [LICENSE](LICENSE)), like the Arduino project
it derives from. Third-party components keep their own licences; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

For the full picture see [LICENSE-NOTES.md](LICENSE-NOTES.md) (DRAFT, not read by a lawyer): Twiddle's own code stays under CC BY-NC-SA 4.0,
the third-party components keep their own licences (the generated notices page for each released version lists them with the full texts),
and the ESP32-P4 image links no GPL or LGPL component, so the LGPL additional permission in that file is not needed for it.

## Credits

- Built on the [Autoblow](https://developers.autoblow.com/) public device API (no Autoblow code or patterns are
  included).
- [ESP-IDF](https://github.com/espressif/esp-idf), [esp-hosted](https://github.com/espressif/esp-hosted-mcu),
  [LVGL](https://lvgl.io/), [esp-nimble-cpp](https://github.com/h2zero/esp-nimble-cpp),
  [ArduinoJson](https://arduinojson.org/) and Espressif's display and touch components.
- Bluetooth toy protocols follow the [Buttplug](https://github.com/buttplugio/buttplug) project's device
  definitions.

## Feature requests and bug reports

Use [GitHub Issues](https://github.com/driftsyncone/twiddle/issues), one issue per request or bug. Say which board and firmware version you have, what you expected and what happened, and attach logs only after removing tokens and Wi-Fi passwords. Requests are welcome, but there is no promise of a reply, a timeline or support, in line with the unofficial, no-warranty notice at the top. Do not put secrets in a report, security-related ones included.

## Disclaimer and safety

- Unofficial hobby project, provided as is, with no warranty. Use it at your own risk; the authors are not liable for damage or injury, to
  the extent the law allows.
- For adults only, and only with the consent of everyone involved.
- It is not a medical device. Any heart-rate or "edging" feature is not a monitoring or medical function; do not rely on it for health.
- Software, Wi-Fi, Bluetooth and cloud links can fail. Always keep a way to stop the device that you can reach by hand (its own power
  switch or stop control), and do not leave a running session unattended.
- You are responsible for how you use it, for the settings you choose and for following the terms of the services and the laws that
  apply to you.
- Vendor and product names (Autoblow, VacuGlide, Guition, Espressif, Lovense, Kiiroo and others) are used only to describe compatibility.
  The authors are not affiliated with them, and they have not endorsed this project.
