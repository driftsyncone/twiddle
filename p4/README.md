# Twiddle (ESP32-P4)

A standalone touchscreen remote for the **Autoblow VacuGlide** (and a few Bluetooth toys), running on the
**Guition JC1060P470C**: an ESP32-P4 board with a 7" 1024x600 touch display and an ESP32-C6 coprocessor that
provides Wi-Fi and Bluetooth LE. It talks to Autoblow's public device API with your device token, so no phone, PC
or hub is needed once it is set up.

This is the ESP-IDF 5.5 port of the Arduino/ESP32-S3 Twiddle firmware (in [`../s3`](../s3)). The Arduino firmware for the
smaller ESP32-S3 panels lives in that folder; see [docs/other-panels.md](docs/other-panels.md).

> **Unofficial community project.** Not made, endorsed or supported by Autoblow. "Autoblow" and "VacuGlide" are
> trademarks of their owners. Use it at your own risk (see [Disclaimer](#disclaimer)).

## Supported hardware

| Part | What |
|---|---|
| Board | Guition **JC1060P470C**: ESP32-P4 (chip rev v1.3 tested), 16 MB flash, PSRAM, ESP32-C6 coprocessor |
| Display | 7" 1024x600, MIPI-DSI, JD9165 controller. The board is sold with different panel batches: **V2** (default) and the newer **`_Y`** ("new panel", model `JC1060P470C_I_W_Y`) are supported and verified; pick yours with the per-unit `panel` setting ([docs/hardware.md](docs/hardware.md)). The original batch is implemented from ESPHome's values but untested |
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
- **Devices**: the Autoblow device plus Bluetooth toys (Lovense, Kiiroo Keon, OSSM drivers). Scan, Connect,
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

The user documentation is in [docs/user-guide.md](docs/user-guide.md) and [docs/settings.md](docs/settings.md).

## Status

Working and tested on the board: display and touch, the six tabs, Wi-Fi through the C6, the Autoblow cloud client
(tested against an emulator of the cloud API), Bluetooth scanning, firmware update over Wi-Fi with rollback, themes
(checked by screenshot).

**Not verified yet** (implemented, but not confirmed on hardware):

- use by finger on the real screen over a longer period;
- commanding a real VacuGlide (all testing so far used an emulator);
- any Bluetooth toy (the drivers pass the offline encoder tests in `test/host/`, no real toy has been used);
- the Settings tab visually, and the setup portal from a phone;
- a battery fuel gauge (the board has none; the code looks for a MAX17048/MAX17043 on the touch I2C bus).

**Known limits:** the **Flip** button is greyed and reads "Flip: n/a" on this board (the DSI panel does not support it in this
firmware). The Autopilot settings (Intensity, Edge and so on) are not saved across restarts; favourites, script
names and the Settings tab values are. The bring-up history and lessons are in [docs/history.md](docs/history.md).

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

`main/secrets.h` is ignored by git and holds the defaults baked into your build (Wi-Fi, device token, update
password). Leave `WIFI_SSID` empty to get the setup portal on first boot. Full steps, including installing
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

## Credits

- Built on the [Autoblow](https://developers.autoblow.com/) public device API (no Autoblow code or patterns are
  included).
- [ESP-IDF](https://github.com/espressif/esp-idf), [esp-hosted](https://github.com/espressif/esp-hosted-mcu),
  [LVGL](https://lvgl.io/), [esp-nimble-cpp](https://github.com/h2zero/esp-nimble-cpp),
  [ArduinoJson](https://arduinojson.org/) and Espressif's display and touch components.
- Bluetooth toy protocols follow the [Buttplug](https://github.com/buttplugio/buttplug) project's device
  definitions.

## Disclaimer

This is an unofficial hobby project provided as is, with no warranty. It controls a connected device over the internet:
you are responsible for how you use it, for the safety of whoever uses the device, and for complying with the
terms of the services and the laws that apply to you. The authors are not affiliated with Autoblow, Guition,
Espressif, Lovense, Kiiroo or any other brand named here.
