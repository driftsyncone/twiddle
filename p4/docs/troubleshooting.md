# Troubleshooting: symptom -> cause -> fix

## Build and tooling
| Symptom | Cause | Fix |
|---|---|---|
| `ESP-IDF Python virtual environment ... not found`, or `idf.py: command not found` after sourcing | another Python (miniconda) first on `PATH` | `export PATH=/usr/bin:$PATH` before `. export.sh` |
| New source file is ignored; `undefined reference` to a function in it | CMake glob is evaluated at configure time | `idf.py reconfigure` |
| `esp_lcd_dpi_panel_event_callbacks_t has no member named on_frame_buf_complete` | `esp_lvgl_port` 2.9 targets a newer IDF API | pin `espressif/esp_lvgl_port: ~2.6.0` |
| esp-nimble-cpp includes missing `WString.h` | our `<Arduino.h>` shim makes it think Arduino String exists | `NIMBLE_CPP_ARDUINO_STRING_AVAILABLE=0` (set in `main/CMakeLists.txt`) |
| Config change has no effect | `sdkconfig` is generated | edit `sdkconfig.defaults`, `rm sdkconfig && idf.py set-target esp32p4` |
| `app partition is too small` | partition table | 3 MB OTA slots in `partitions.csv`; do not use the C6 updater's table |
| esptool: `not an ESP32-P4 image` writing the C6 image | it is a C6 image, written as data | `--force` ([c6-firmware.md](c6-firmware.md)) |
| `idf.py monitor` fails, or a serial write times out | the USB console takes no input and monitor needs a tty | pyserial output-only script ([dev-setup.md](dev-setup.md)) |
| Serial port "busy" / capture hangs | another process (or a stuck test) holds the port or the board lock | `fuser -v /dev/ttyACM0` and `fuser -v <lockfile>`, stop the stuck holder |

## Display and touch
| Symptom | Cause | Fix |
|---|---|---|
| Panel dark, log says it reached the UI | wrong LCD reset pin or init table | V2 panel: reset GPIO0, V2 table ([hardware.md](hardware.md)) |
| Washed-out colours **and** the picture shifts | the original-panel table on a V2 panel | `GET /panel?v=0` (V2), restart |
| Screen black, backlight on (maybe a faint white glow at the edges), `/screenshot` shows the UI | a different panel behind the driver (`_Y` "new panel") or a DCS register read | `GET /panel?v=4`, restart ([hardware.md](hardware.md)); never read panel registers back on the `_Y` |
| Picture visible but dim, banded or **shaky** | pixel clock too high for this panel | `GET /pclk?mhz=48` (default on `_Y`), restart; step 4 MHz at a time |
| White screen, `dsi.dpi ... underrun happens` | PSRAM at 20 MHz | `CONFIG_IDF_EXPERIMENTAL_FEATURES=y` + `CONFIG_SPIRAM_SPEED_200M=y`; boot log must show `Speed: 200MHz` |
| Blue flashes / blue strip at the bottom while the UI redraws, wobbling picture | same: DPI refresh starved by slow PSRAM | same fix; underruns only show in the log during activity, an idle log looks clean |
| `ESP_ERR_NO_MEM` creating the DSI panel | PSRAM not enabled | `CONFIG_SPIRAM=y` |
| Crash loop `Stack protection fault`/OOM soon after UI start | LVGL fell back to its 64 KB builtin heap | `CONFIG_LV_USE_CUSTOM_MALLOC=y` and the `-Wl,-u,lv_*` flags in `main/CMakeLists.txt` |
| Touch: `GT911 read error` / init failed | `esp_lcd_touch_config_t.driver_data` not set, so the driver skipped the reset/address sequence | pass `esp_lcd_touch_io_gt911_config_t` with `dev_addr = 0x5D` (see `board_dsi_init.c`) |
| Red banner "Battery at 0%: control is paused" | a non-gauge chip at I2C 0x36 taken for a MAX17048 | fixed: probe needs a known version and a plausible voltage |
| Log line `esp_lcd_panel_swap_xy ... not supported` | esp_lvgl_port rotation setup on DSI | harmless, ignore |
| Flip screen button does nothing | `boardSetFlip()` only remembers the setting on the P4 (DSI panel has no swap/mirror, LVGL rotation is off) | known gap, see [feature-matrix.md](feature-matrix.md) |
| After a theme switch some colours stay old | a colour was set with `lv_obj_set_style_*` and a literal/cached value instead of `theme::bind`/`theme::color` | bind it with a role, or repaint it in `themeChanged()` (`ui.cpp`), see [howto.md](howto.md) |

## Radio and cloud
| Symptom | Cause | Fix |
|---|---|---|
| `major version mismatch — OTA coprocessor from host` | C6 on slave 2.x, host 3.x | update the C6 ([c6-firmware.md](c6-firmware.md)) |
| BLE start-up times out (`CP not ready after 5000ms`) | the old C6 firmware lacks the new BT-init RPC | update the C6 |
| Wi-Fi association fails with reasons 203/205 once or twice | normal on this board | retries succeed; boot to IP takes 7-17 s |
| Label `VacuGlide offline: hold its mode button 2.5 s` | the device is not connected to the Autoblow cloud | switch it on, hold its mode button |
| `no internet` / `request failed ... CONNECT` to the emulator | no network path (host firewall, router rules) | [network.md](network.md) |
| Panel unreachable from the dev box (`ota.sh` timeout) | inter-VLAN block | [network.md](network.md) |
| `discover: HTTP <code>` in the status text / `request failed` right after boot | the discovery call (`GET /autoblow/connected`) failed: wrong token (4xx), no Wi-Fi, or `AUTOBLOW_API_BASE` points at a server that is down | check the token on the Settings tab (without showing it), the Wi-Fi state in `GET /`, and `secrets.h`; it retries every 3 s |
| Status text `<device> went offline` | the cloud answered HTTP 502 to a command or poll: the device dropped off the cloud | switch it on / hold its mode button; the panel rediscovers by itself and resends its state |
| `rate limited` | HTTP 429 from the cloud | the client waits 1 s and retries; avoid hammering the API |
| `unsupported Autoblow device: <type>` | the token belongs to a device type the client does not know (it knows `vacuglide`, `vacupump`, `autoblow-ultra`) | use a token of a supported device |
| A second cloud connection (event stream) makes the first one fail | two TLS sessions do not fit in internal RAM next to each other (tried 2026-10-19, comment in `autoblow.cpp`) | do not add one without freeing RAM first |
| Wi-Fi flaky on a weak signal, answers slow | modem sleep only hears the access point at its beacons, and Bluetooth forces modem sleep on | keep Bluetooth off (Settings, restarts the panel) unless a toy is in use; Wi-Fi power save is off then |

## Updates and rollback
| Symptom | Cause | Fix |
|---|---|---|
| `firmware:` never `pending`/rollback does nothing | `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` off | keep it in `sdkconfig.defaults` |
| Panel returns to the old firmware after an update | new image crashed or had no Wi-Fi for 30 s | read the log of the failed boot; fix; try again |
| `ota.sh` says `ROLLED BACK (or the same image ...)` | rollback, or the same binary was uploaded | check `firmware:` and `id:` on `GET /` |
| `curl` to `/update` returns `login required` (401) | wrong `OTA_PASSWORD`, or none compiled in (then every login fails) | set `OTA_PASSWORD` in `main/secrets.h` and flash that build once over USB |
| `/update` answers 500 `image rejected: ...`, `flash write failed`, `upload stalled`, `connection lost during upload` | bad or truncated image / unreliable Wi-Fi link | upload again; the old firmware keeps running, but the session and devices were stopped when the upload started |
| `/screenshot` answers 500 `no frame buffer` | called before `boardBegin()` finished, or a build that uses the old RGB565 accessor (`boardFrameBuffer()` is always `nullptr` on the P4; fixed in 2.0.0-p4.4) | use `boardFrameBufferRgb888()` and wait for the boot to finish |
| `/theme` answers `no such theme` | index out of range | `GET /theme` lists the valid ones |
| Phone does not open the setup page by itself | captive-portal detection varies | join `Twiddle-Setup` (password `twiddle-setup`) and open `http://192.168.4.1/setup`; portal mode only exists when no Wi-Fi name is stored |

## Debugging aids that bite
- `lv_refr_now` from a non-LVGL task corrupts the heap (`tlsf_free ... block already marked as free`). Since 2.0.0-p4.7 nothing calls it off the LVGL task.
- Console logging is unbuffered: `printf`/`plog` busy-wait on the UART (about 87 us per byte at 115200 once the FIFO is full) and can stall a
  task for tens of ms when a host holds the USB-Serial-JTAG port open without reading. `Serial.setTxBufferSize()` is a no-op in the shim.
  A possible fix (not applied, no board to test it on): `uart_driver_install(UART_NUM_0, 256, 2048, 0, NULL, 0)` plus
  `uart_vfs_dev_use_driver(0)`. Keep log lines on the STOP path (blesvc) short.
- Loop task at 100 % CPU, or IDLE1 task-watchdog warnings: check `CONFIG_FREERTOS_HZ` is 1000 in the generated `sdkconfig` (it is set in `sdkconfig.defaults`; `rm sdkconfig` after changing it).
- Wi-Fi with a wrong password retries after 5 s, 10 s ... 30 s (not back to back); a network that is merely absent after 1, 2, 4, 8 s. A link that was up and dropped is retried at once.
- Reading LVGL's active buffer right after a redraw can show the previous frame (direct mode, 2 buffers).
- A test script that holds the board lock and never exits starves everyone: always use `timeout`.
- Do not capture or publish a screenshot of the Settings tab.
- Touching LVGL, `control::` or the shared touch I2C bus from the web or any non-LVGL task without `UiLock` corrupts state; I2C driver set up on the loop core answers ACKs for absent addresses when used from core 0 (`uiCall` exists for that).
- A new `.cpp` under `main/` or `main/toys/` is not built until `idf.py reconfigure`, and a new toy `.cpp` is not in the host test until it is added to `test/host/run.sh`.
