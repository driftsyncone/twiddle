# Architecture

The firmware is a port of the Arduino panel firmware (ESP32-S3) to ESP-IDF. The module split, the headers and most
of the logic are unchanged from the Arduino project (`main/*.h` are the contracts), so the Arduino
sources and [history.md](history.md) are the reference for behaviour.

## Modules (`main/`)
| File | Role |
|---|---|
| `main.cpp` | `setup()` / `loop()` as in the Arduino project; boot order below |
| `board.cpp`, `board_dsi_init.c`, `board.h`, `board_dsi.h` | display (JD9165 over DSI), GT911 touch, backlight PWM, LVGL via esp_lvgl_port, screen sleep, brightness, frame-buffer access |
| `ui.cpp`, `ui.h` | the LVGL UI: status bar + STOP, tabs Remote / Autopilot / Session / Scripts / Devices / Settings; `UiLock`, `uiCall` |
| `control.cpp`, `control.h` | session logic and the Autopilot wave generator (Climb / Hold / Rest), vacuum pulses, safety rules |
| `autoblow.cpp`, `autoblow.h` | Autoblow cloud client: discovery, 3 s state polling, speed / valve / stop / pulse / local-script commands, loss handshake |
| `devices.cpp`, `devices.h` | device registry (the cloud device plus remembered BLE toys), selection, "Follow" and scale per device |
| `ble.cpp`, `ble.h`, `toys/*` | NimBLE (esp-nimble-cpp over the C6's hosted HCI): scan, connect, reconnect backoff, drivers for Lovense, Kiiroo Keon, OSSM |
| `wlan.cpp`, `wlan.h` | Wi-Fi station via the C6, retries, plain-words problem text, mDNS `twiddle-p4.local`, setup portal |
| `ota.cpp`, `ota.h` | web server (status, update, log, screenshot, theme, restart/resync/scan/backlight, setup) |
| `rollback.cpp` | firmware probation after an update |
| `settings.cpp`, `settings.h` | panel settings in NVS |
| `battery.cpp` | MAX1704x gauge if a real one answers (none known on this board) |
| `log.cpp`, `mem.cpp` | log ring buffer in PSRAM, memory report |
| `compat/` | tiny Arduino shim: `millis`, `delay`, `String` on `std::string`, `Serial` on stdout, `Preferences` on NVS; `arduino_main.cpp` runs `setup()` then `loop()` on one task |
| `lv_mem_psram.c` | LVGL's allocator in PSRAM (forced into the link with `-Wl,-u,...` flags in `CMakeLists.txt`) |

## Boot order (`setup()`)
`logBegin` -> `memBegin` (before any TLS) -> `rollback::begin` (arms the deadline for a fresh update) ->
`settingsLoad` -> `autoblow::begin` -> `devices::begin` (Bluetooth first, while internal RAM is in one piece) ->
`boardBegin` (display, touch, LVGL task) -> `battery::begin` -> under `UiLock`: `uiBegin`, flip/brightness,
`control::begin` -> `wlan::begin` -> `otaBegin` -> `memReport("boot")`.

## Tasks and threading
- **loopTask** (core 1, 16 KB stack, priority 1): `setup()` then `loop()`; owns `uiPump`, `rollback::loop`, `wlan::loop`,
  `battery::loop`. `uiPump` and `rollback::loop` run under `UiLock`; `wlan::loop` and `battery::loop` run **outside** it
  (they call the C6 and I2C and can wait seconds; holding the lock would freeze LVGL, touch and STOP). `loop()` ends with
  `delay(5)`; the tick rate is 1000 Hz (`CONFIG_FREERTOS_HZ`), so that is a real 5 ms sleep.
- **LVGL task** (esp_lvgl_port, core 1, 12 KB stack): runs LVGL. `uiLock()/uiUnlock()` map to `lvgl_port_lock/unlock`; anything outside
  the LVGL task that touches LVGL, `control::` or the shared I2C bus takes `UiLock` (recursive). `uiCall(fn,arg)`
  runs a function on the loop task and waits. **Never call `lv_refr_now` from another task** (heap corruption): `uiShowTab` and `uiShowUpdating` only invalidate and the LVGL task draws.
  State shared with other tasks is copied out: `wlan::info()` returns the problem text in the struct (no static buffer), the link sample and `battery::info()` are copied under a spinlock, scan results sit behind a mutex.
- **autoblow** net task (core 0, priority 5, 12 KB stack): all cloud HTTP; speed commands are queued to it.
- **blesvc** (STOP path, core 0, priority 6, 4 KB) and **ble** worker (core 0, priority 5, 6 KB): Bluetooth.
- **web server** (esp_http_server, core 0, 10 KB stack), **dns** (captive portal only, core 0, priority 2, 3 KB),
  **ota-deadline** (esp_timer, only while an
  update is on probation), mDNS, esp-hosted and NimBLE internal tasks. NimBLE's host task stack is 6 KB
  (`CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE=6144`).

## Storage (NVS)
| Namespace | Keys | Notes |
|---|---|---|
| `vgpanel` (legacy name kept for settings compatibility) | `ssid`, `pass`, `token`, `sleep`, `flip`, `bright`, `grace`, `bt`, `theme`, `fav0..fav2` (blobs of 8 bytes), `sn0..sn29` (script names, 30 slots) | Settings (`settings.cpp`). Wi-Fi and token default to `secrets.h` until the Settings tab saves its own. Wi-Fi/token/Bluetooth apply after a restart; sleep, flip, brightness, grace and theme apply at once |
| `devices` | per-device blobs (follow, scale) | `Preferences::isKey` in the shim only knows blobs, so values are stored as blobs |
| `bletoys` | one blob per remembered toy (address, type) | remembered toys auto-reconnect with 15/30/60/120 s backoff |

## Web server (port 80, `ota.cpp`)
Basic auth user `ota`, password per device (`otapw.cpp`, NVS `otapw`; `OTA_PASSWORD` only in a dev build).
| Route | Auth | What |
|---|---|---|
| `GET /` | none | status text, one `key: value` per line: `twiddle <FW_VERSION>`, `built <date time>` (only changes when `ota.cpp` recompiles), `id:` (first 16 hex chars of the ELF SHA-256, different for every build), `board:`, `firmware:` (rollback state), `display fixes:` (always -1 on the P4), `reset:` (reason), `device:` (cloud online/offline + last error), `heap:` (internal free and largest block), the Wi-Fi report (`wifi: ... drops ... last reason`), `bluetooth: on/off`, `cloud: lost N x, silent N ms, grace N s, uptime N s` |
| `POST /update` | yes | multipart (`firmware=@file`, as `ota.sh` sends it) or a raw body. First calls `control::stopAll()` (stops the running session and every device) and waits for the STOP to go out (at least 0.8 s and until no device reports moving, at most 5 s, then logs and continues), shows the updating screen (drawn by the LVGL task) and waits 0.7 s, then streams into the next OTA slot, sets it as boot partition and reboots. Errors answer 500 with a text reason and the updating screen is hidden again |
| `GET /log` | yes | the PSRAM log ring buffer (16 KB, wraps; `plog()` lines with a tenth-of-a-second timestamp) |
| `GET /screenshot[?tab=N]` | yes | full-resolution 24-bit BMP (1024x600); `tab=N` (0..5) switches tab first; the handler waits about 1 s for two redraws (not timed end to end). Never capture or publish tab 5 (Settings shows the token's tail) |
| `GET /theme[?n=N[&save=1]]` | yes | list themes / switch live (`save=1` keeps it after a reboot) |
| `POST /restart`, `/resync`, `/scan`; `GET /backlight?duty=N` | yes | diagnostics: restart (stops everything first), display resync (a no-op on the P4 DSI panel), 8 s Bluetooth scan, raw backlight PWM duty 0..255 (-1 = back to the brightness setting) |
| `GET/POST /setup` | open while in portal mode, else yes | Wi-Fi/token setup page (POST body at most 1500 bytes); saving restarts the panel. In portal mode every unknown path redirects (302) to it; otherwise unknown paths answer 404 |
Auth is checked in each handler by `login()` (basic auth user `ota`, the per-device password from `otapw`; **403** while no password exists, 401 plus a 0.5 s pause on a wrong one; 5 wrong in a row = 429 for 60 s, `loginlimit.h`). `/setup` and `POST /display` skip it only while the setup portal is up. The server has `max_uri_handlers = 20` (15 registered), LRU purge on, 10 s send/receive timeouts.
Portal mode (no SSID stored, i.e. `settings.wifiSsid` empty; a non-empty `WIFI_SSID` in `secrets.h` counts as stored): AP `Twiddle-Setup` / `twiddle-setup` (WPA2, channel 1, 4 clients), address 192.168.4.1, captive DNS, background scan. The mDNS name `twiddle-p4.local` can be changed with `#define PANEL_HOSTNAME` in `secrets.h`.

## Autoblow cloud API as used
Auth header `x-device-token`. `GET https://latency.autoblowapi.com/autoblow/connected` (the generic route, the same
for every device type; retried every 3 s while it fails) returns `connected`, `deviceType` (`vacuglide`, `vacupump`,
`autoblow-ultra`; no `deviceType` is treated as a VacuGlide) and the cluster host, then
`GET https://<cluster>/<type>/state` (`/vacuglide/state` for a VacuGlide), polled every 3 s when no command is waiting
(`operationalMode`, `targetSpeed`, valves, `localScript`). Commands are `PUT`s (VacuGlide): `/vacuglide/target-speed`, `/target-speed/stop`, `/valve/stroke-plus`, `/valve/stroke-minus`,
`/local-script`, `/local-script/start`, `/local-script/stop`; also `/autoblow/oscillate...` and
`/vacupump/manual/...` for the other models. HTTP 502 means the device dropped off the cloud and discovery is
re-run. `AUTOBLOW_API_BASE` in `secrets.h` replaces the cloud with another server (plain http allowed; the cluster the
server reports is ignored and the base is used for everything).

## Safety rules (must survive every change)
- Stroke-plus **tightens**, stroke-minus **loosens**; buttons read Looser / Tighter. Vacuum pulses use stroke-plus
  and are skipped while either valve is held.
- Autopilot is this project's own generator. Never import patterns or numbers from the Autoblow web app.
- Loss = no HTTP response for the Wi-Fi gap setting (10/20/30 s). On loss only STOP is sent until the device
  acknowledges; the device keeps its last speed during the gap; a low battery keeps it stopped.
- A new firmware stays "pending" until Wi-Fi has been up 30 s (3-minute deadline) ([ota-and-rollback.md](ota-and-rollback.md)).

## Differences from the Arduino firmware
Flip is a no-op (the Settings button is greyed and reads "Flip: n/a"); the web `/screenshot` is 24-bit; the backlight is not turned off during an update (no S3
flash/display stall); `/adc` and `/i2c` endpoints are not ported; battery reports "none" on this board; the
backlight PWM and the display driver are new code (`board*.c*`).
