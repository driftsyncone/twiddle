<div align="center">

# Twiddle

**A standalone touchscreen remote for the Autoblow VacuGlide and Bluetooth toys.**
No phone, no PC, no hub. Flash an ESP32 touch panel, put it on the desk, done.

[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/license-CC%20BY--NC--SA%204.0-lightgrey.svg)](LICENSE)
![ESP32-P4](https://img.shields.io/badge/ESP32--P4-ESP--IDF%205.5-red)
![ESP32-S3](https://img.shields.io/badge/ESP32--S3-Arduino%20%2F%20PlatformIO-orange)
![Status](https://img.shields.io/badge/status-hobby%20project%20%C2%B7%20source%20release-blue)

</div>

> **Unofficial community project.** Not made, endorsed or supported by Autoblow, Lovense or Kiiroo. "Autoblow",
> "VacuGlide", "VacuPump", "Lovense", "Kiiroo" and "Keon" are trademarks of their owners. Use it at your own risk,
> and read [Safety](#safety) first.

<!-- Screenshots go here: capture them from a running panel (GET /screenshot?tab=N, see p4/docs/howto.md), put them in
docs/images/ (remote, autopilot, devices) and add a three-column table: Remote, Autopilot, Devices. -->

## Why Twiddle

- **Self-contained.** The panel talks to Autoblow's public device API with your device token, and to Bluetooth toys
  directly. Once it is set up nothing else needs to be on.
- **STOP on every screen**, and a defined behaviour when the connection drops (see [Safety](#safety)).
- **Autopilot.** An on-device edging generator: eight waves that climb, hold and rest, with steering buttons
  (More, Back off, Finish Me), a graph you can tap or draw on, and three favourite sessions.
- **One output, many devices.** The Remote slider or Autopilot drives every device that follows it, each at its own scale.
- **Update over Wi-Fi, with a safety net.** A new firmware that does not come up is rolled back automatically.
- **Six colour themes**, switched live. Midnight is the default.
- **Two hardware platforms, one behaviour.** The same features, look and safety rules on both boards.

## Supported hardware

| | **P4 panel** (recommended) | **S3 panel** |
|---|---|---|
| Board | Guition **JC1060P470C** | Guition **JC8048W550C** |
| Chip | ESP32-P4 + ESP32-C6 (Wi-Fi, BLE) | ESP32-S3 |
| Display | 7", 1024x600, MIPI-DSI | 5", 800x480, RGB |
| Stack | ESP-IDF 5.5 | Arduino / PlatformIO |
| Folder | [`p4/`](p4/) | [`s3/`](s3/) |
| Docs | [P4 README](p4/README.md) | [S3 README](s3/README.md) |

The S3 firmware also builds for the **Waveshare ESP32-S3-Touch-LCD-5** (compile-checked; little recent hardware testing).
Other boards are not supported. The feature-by-feature comparison and the rule that keeps the two firmwares
consistent: [`p4/docs/feature-matrix.md`](p4/docs/feature-matrix.md).

## Supported devices

| Device | Connection | Tested on hardware |
|---|---|---|
| Autoblow **VacuGlide** | Autoblow cloud (token) | yes (S3); P4 against a cloud emulator |
| Autoblow **VacuPump**, **Ultra** | Autoblow cloud (token) | no |
| **Lovense** (models known to the Buttplug project) | Bluetooth LE | no |
| **Kiiroo Keon** (gen 1), **Keon 2**, **Keon WiFi** | Bluetooth LE | no |
| **OSSM** (Research and Desire) | Bluetooth LE | no |

The untested drivers pass offline encoder tests (`p4/test/host/`) but have not been tried on a real toy. Reports welcome.

## Flashing

There is no prebuilt release yet: you build it and flash it over USB-C. Pick your board.

### P4 (Guition JC1060P470C)

You need a Linux machine with [ESP-IDF **v5.5.1**](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32p4/get-started/)
and the board on USB-C (it shows up as `/dev/ttyACM0`).

```sh
git clone https://github.com/driftsyncone/twiddle && cd twiddle/p4
. ~/esp/esp-idf/export.sh                        # ESP-IDF v5.5.1
cp main/secrets.example.h main/secrets.h         # then edit it (Wi-Fi, token, update password)
idf.py set-target esp32p4                        # first time only
idf.py build
idf.py -p /dev/ttyACM0 flash
```

Leave `WIFI_SSID` empty and the panel opens a **`Twiddle-Setup`** Wi-Fi network and a web page on first boot, so you
can enter Wi-Fi and your device token from a phone.

- If the screen stays black but `http://twiddle-p4.local/screenshot` shows a picture, your unit is a different
  display batch: set the panel variant ([hardware.md](p4/docs/hardware.md)).
- If the firmware ignores USB: **hold BOOT, tap RESET, release BOOT** for download mode.
- Back up the original firmware first if you may want it again ([flashing.md](p4/docs/flashing.md)).

### S3 (Guition JC8048W550C)

You need [PlatformIO](https://platformio.org/).

```sh
git clone https://github.com/driftsyncone/twiddle && cd twiddle/s3
cp include/secrets.example.h include/secrets.h   # then edit it
pio run -e panel -t upload && pio device monitor
```

### After the first flash

Later builds go over Wi-Fi: `./ota.sh` in the board's folder. The new image is written to the other slot and must
get onto Wi-Fi and run for 30 s, or the panel returns to the previous firmware by itself
([OTA and rollback](p4/docs/ota-and-rollback.md)). USB always works as the last resort.

Full per-board procedure (identify, back up, flash, recover): [`p4/docs/flashing.md`](p4/docs/flashing.md).

## Safety

- **The panel controls the VacuGlide through Autoblow's cloud. If the panel loses its connection, it cannot stop
  the device.** After the configured Wi-Fi gap (10, 20 or 30 s) it shows a red banner and sends only STOP until the
  device acknowledges. Until then the device keeps its last speed.
- Saving settings or installing an update stops the device first.
- With a battery gauge fitted (S3, optional), a nearly empty battery keeps the device stopped.
- Valves: stroke-plus **tightens**, stroke-minus **loosens**. The buttons are labelled Tighter and Looser.
- **Keep the device's own controls within reach.** Never rely on the panel as the only way to stop it.

## Documentation

| Start here | Go deeper |
|---|---|
| [P4 user guide](p4/docs/user-guide.md) | [Architecture](p4/docs/architecture.md) |
| [Settings reference](p4/docs/settings.md) | [Dev setup](p4/docs/dev-setup.md) |
| [Themes](p4/docs/themes.md) | [How-to recipes](p4/docs/howto.md) (add a theme, setting or toy) |
| [Troubleshooting](p4/docs/troubleshooting.md) | [Testing](p4/docs/testing.md), [Contributing](p4/CONTRIBUTING.md) |

Changelogs: [P4](p4/CHANGELOG.md), [S3](s3/CHANGELOG.md).

## Project status

Twiddle is a hobby project. 2.0.0 is built and running on both panels; the P4 is verified for display, touch,
Wi-Fi through the C6, the Autoblow client (against an emulator), Bluetooth scanning and update with rollback. Not yet
verified on real hardware: a real VacuGlide from the P4, any Bluetooth toy, the VacuPump and Ultra. Details are in
the [P4 README](p4/README.md#status).

## History

Twiddle started as `vacuglide-panel`, an Arduino firmware for the ESP32-S3 panel, and kept that name until it grew
support for other toys and a second board. The old repository,
[`driftsyncone/vacuglide-panel`](https://github.com/driftsyncone/vacuglide-panel), stays up as the 1.x release history.

## License and credits

[Creative Commons Attribution-NonCommercial-ShareAlike 4.0](LICENSE), the same for both folders. Third-party
components keep their own licenses: see `THIRD_PARTY_NOTICES.md` in each folder.

Built on the [Autoblow](https://developers.autoblow.com/) public device API (no Autoblow code or patterns included),
[ESP-IDF](https://github.com/espressif/esp-idf), [esp-hosted](https://github.com/espressif/esp-hosted-mcu),
[LVGL](https://lvgl.io/), [esp-nimble-cpp](https://github.com/h2zero/esp-nimble-cpp) and
[ArduinoJson](https://arduinojson.org/). Bluetooth toy protocols follow the
[Buttplug](https://github.com/buttplugio/buttplug) project's device definitions.

This is an unofficial hobby project provided as is, with no warranty. You are responsible for how you use it, for the
safety of whoever uses the device, and for complying with the terms of the services and laws that apply to you.
