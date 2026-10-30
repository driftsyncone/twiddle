# History: how Twiddle was ported to the ESP32-P4

A short account for people who wonder why the code looks the way it does.

## Where it came from
The panel started as an Arduino (PlatformIO) firmware for ESP32-S3 touchscreens: a standalone remote for the
Autoblow VacuGlide that talks to Autoblow's cloud API. Its logic is split into modules (`ui`, `control`,
`autoblow`, `ble`, `devices`, `wlan`, `ota`, `rollback`, `settings`, `battery`, `log`, `mem`), each with a header
that is the contract between modules.

In October 2026 it was ported to the **Guition JC1060P470C** (ESP32-P4, 7" 1024x600, ESP32-C6 coprocessor) on
**ESP-IDF 5.5.1**. The aim was the same behaviour, not a redesign.

## The name
The Arduino project was called `vacuglide-panel`. In October 2026 (`2.0.0-p4.6`) the project was renamed **Twiddle**; its
folders are `p4/` (this one) and `s3/` (the Arduino/ESP32-S3 boards) of one repository. The older GitHub project
`vacuglide-panel` stays as the historical 1.x release and points here. "VacuGlide" remains the name of the Autoblow device.
The NVS namespace `vgpanel` keeps its old name so that stored settings survive the rename.

## How the port was done
1. **Bring-up first.** A throw-away test project proved each piece of hardware in turn: the display (JD9165 over
   MIPI-DSI), touch (GT911), LVGL on 24-bit colour, Wi-Fi through the C6, Bluetooth scanning through the C6, and a
   read-only Autoblow client. That code was kept in `legacy/` (never part of the build). Everything in it was later
   re-done properly in `main/board.cpp`, `main/board_dsi_init.c`, `main/wlan.cpp`, `main/ble.cpp` and
   `main/autoblow.cpp`, so `legacy/` was deleted in the clean-up for publication; `git log` still has it.
2. **Skeleton.** The original module headers were kept as contracts. A tiny Arduino shim (`main/compat/`:
   `millis`, `delay`, `String` on `std::string`, `Serial`, `Preferences` on NVS, and a task that runs `setup()` and
   `loop()`) let the module code compile with few edits.
3. **Three parallel branches** ported disjoint sets of modules against those contracts: core (board, UI scaled
   from 800x480 to 1024x600, settings, control), net (Autoblow client, Wi-Fi and setup portal, web server and
   OTA, rollback, battery) and Bluetooth (BLE, devices, toy drivers on esp-nimble-cpp). They were merged into one
   `integration` line, which built and linked first time without stubs and became `2.0.0-p4.1`.
4. **Hardening and extras**: battery probe fix, README and tools (p4.2), themes (p4.3), working web screenshots
   and the theme endpoint (p4.4), then documentation and clean-up. See [CHANGELOG.md](../CHANGELOG.md).

Rules carried over from the Arduino project and kept through the port: Looser/Tighter naming, Autopilot is our own
generator, the loss handshake and low-battery behaviour, secrets never in git.


## Bring-up lessons
- **Two panel variants** exist behind the same board name. The V2 panel (2026 batch) needs LCD reset on GPIO0, a
  52 MHz pixel clock and its own init table. The original variant's table gives washed-out colours and a shifting
  image on a V2 board. The init sequence must end with sleep-out (0x11, 120 ms) and display-on (0x29, 20 ms).
- **PSRAM must run at 200 MHz.** The 200 MHz option only exists with `CONFIG_IDF_EXPERIMENTAL_FEATURES=y`;
  without it the setting is silently ignored and PSRAM falls back to 20 MHz, giving DPI underruns (white screen,
  blue flashes). 80 MHz is enough for a static image but not for LVGL activity. Underruns only show in the log
  during activity, so a quiet log proves nothing.
- **No PSRAM means no display**: one 1024x600 frame buffer is 1.2 MB (RGB565) or 1.8 MB (RGB888).
- **esp_lvgl_port must be pinned to 2.6.x**; 2.9 needs a newer IDF API than 5.5.1. LVGL runs in RGB888 with two
  DPI frame buffers (`avoid_tearing`, `direct_mode`). RGB565 wobbled in the early tests.
- **GT911 touch** needs `.driver_data` with the I2C address in the touch config; without it the driver skips the
  reset/address sequence and reads NACK.
- **Wi-Fi through the C6**: esp_hosted 3.0.10 with esp_wifi_remote over SDIO. The C6 ships with an older
  esp-hosted slave (2.3.2). Updating it needs a 2.x host to push the image (a 3.x host fails at the first chunk);
  see [c6-firmware.md](c6-firmware.md).
- **Partitions**: two 3 MB OTA slots; the default 1 MB is too small once Wi-Fi is in.
- **Bluetooth**: esp-nimble-cpp needs `NIMBLE_CPP_ARDUINO_STRING_AVAILABLE=0` because the shim defines `String`;
  the NimBLE host runs over the C6's hosted HCI.
- **Rollback** only works with `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`.
- **Threading**: LVGL runs on its own task; anything else takes `UiLock`. Calling `lv_refr_now` from another task
  corrupts the heap. In direct mode the active LVGL buffer lags one frame, which matters for screenshots.
- **USB console**: the USB-JTAG console accepts no input here and re-enumerates on every reset, so tooling uses a
  reconnecting serial reader.
- **CMake**: the source glob is evaluated at configure time; run `idf.py reconfigure` after adding files.
- **Shared board**: when several people or agents use one board, serialise flash and serial capture, or they
  overwrite each other's firmware.
