# Dev setup: build, flash, serial

## What you need
- A Linux dev box with a USB-C data cable to the board's USB port (shows up as `/dev/ttyACM0`, USB ID
  `303a:1001` Espressif USB JTAG/serial). Your user needs access to the device (group `uucp` or `dialout`).
- ESP-IDF **v5.5.1** (not 6.x: the pinned component versions below target 5.5).

## Install ESP-IDF (once)
```sh
mkdir -p ~/esp && cd ~/esp
git clone -b v5.5.1 --depth 1 --recursive --shallow-submodules https://github.com/espressif/esp-idf.git
cd esp-idf && ./install.sh esp32p4        # tools go to ~/.espressif (about 3 GB)
```
**PATH gotcha:** if miniconda (or another Python) comes first on `PATH`, `export.sh` fails with "ESP-IDF
Python virtual environment ... not found". Always put the system Python first:
```sh
export PATH=/usr/bin:$PATH
. ~/esp/esp-idf/export.sh
```
(The IDF Python env here is `~/.espressif/python_env/idf5.5_py3.14_env`; it also has `pyserial`.)

## Configure secrets
```sh
cp main/secrets.example.h main/secrets.h     # gitignored; never commit it
$EDITOR main/secrets.h                       # WIFI_SSID, WIFI_PASS, DEVICE_TOKEN, OTA_PASSWORD, OTA_HOST (see secrets.example.h)
```
`PANEL_HOSTNAME` (optional) changes the mDNS name from `twiddle-p4`. `AUTOBLOW_API_BASE` is optional and points the cloud client at another server (the emulator, see
[testing.md](testing.md)); leave it out for the real Autoblow cloud. Wi-Fi and token can also be changed from the
panel's Settings tab later (stored in NVS, which wins over `secrets.h`).

## Build
```sh
idf.py set-target esp32p4        # first time only (also regenerates sdkconfig from sdkconfig.defaults)
idf.py build                     # about 1.8 MB image (1,868,816 bytes for 2.0.0-p4.4); first build in a clean tree ~6 min including the component downloads (2215 build steps); incremental builds are much faster
```
- The component manager downloads dependencies into `managed_components/` on the first build
  (`dependencies.lock` pins the resolved versions). Constraints in `main/idf_component.yml`: `espressif/esp_lvgl_port ~2.6.0`
  (2.9.x needs a newer IDF API; locked 2.6.3), `bblanchon/arduinojson ^7.4.0` (locked 7.4.3), `h2zero/esp-nimble-cpp ^2.0.0`
  (locked 2.6.0); `espressif/esp_hosted`, `esp_wifi_remote`, `mdns`, `esp_lcd_jd9165`, `esp_lcd_touch_gt911` are `'*'` in
  the manifest and only pinned by the lock file: esp_hosted 3.0.10, esp_wifi_remote 1.6.5, mdns 1.14.1, esp_lcd_jd9165 2.0.2,
  esp_lcd_touch_gt911 1.2.1. LVGL comes in transitively (`lvgl/lvgl` 9.6.0~1), IDF 5.5.1.
- `sdkconfig` is generated and gitignored; **put configuration in `sdkconfig.defaults`**, then
  `rm sdkconfig && idf.py set-target esp32p4` to apply it. Do not hand-edit `sdkconfig`.
- `main/CMakeLists.txt` globs every `.c/.cpp` under `main/`. The glob is evaluated at configure time: after adding
  or removing a source file run `idf.py reconfigure`, or the new file silently is not compiled.

## Flash
```sh
idf.py -p /dev/ttyACM0 flash
```
Flashing resets the board. After a partition-table change (different flash layout, for example after the C6
updater project) run `idf.py -p /dev/ttyACM0 erase-flash flash` so no stale boot data is left.
For updates over Wi-Fi see [ota-and-rollback.md](ota-and-rollback.md).

## Reading the serial log
`idf.py monitor` needs a real terminal and the board's USB console does **not accept input** (writes time out),
so use a small pyserial script. The USB device re-enumerates on every reset, so reconnect in a loop:
```python
import serial, time
buf, t = b"", time.time()
while time.time() - t < 40:
    try:
        s = serial.Serial("/dev/ttyACM0", 115200, timeout=1)
        s.dtr = False; s.rts = True; time.sleep(0.1); s.rts = False   # reset the chip into a fresh boot
        while time.time() - t < 40:
            buf += s.read(65536)
    except Exception:
        time.sleep(0.3)
print(buf.decode("latin1"))
```
Notes: opening the port alone does not reset the board; the DTR/RTS toggle above does. Run it with the IDF python
(`~/.espressif/python_env/idf5.5_py3.14_env/bin/python`) so `serial` imports. A panic prints `Guru Meditation` or
`assert failed`; decode PCs with `riscv32-esp-elf-addr2line -pfiaC -e build/twiddle.elf <addr>...`.

Healthy boot, in order: `rollback: ... state confirmed`, `ble: off ...` (Bluetooth stays off until a toy is
remembered or you tap "Turn on"), `board: display and touch up (1024x600)`, `esp-hosted fw versions: host=3.0.10
coprocessor=3.0.10 (match)`, `mem boot: internal ~284000 free`, then `Wi-Fi up` about 11 s after boot (observed on the board; not verified in this review; the first
one or two association attempts often fail with reasons 203/205; that is normal).

## Typical loop
1. edit -> `idf.py build`
2. `idf.py -p /dev/ttyACM0 flash`, capture the serial log
3. for visual checks use `tools/screenshot/capture.sh` ([testing.md](testing.md))
4. when more than one person or agent uses the board, wrap flash + capture in one `flock` hold (see testing.md)

## Version rule
`main/version.h` `FW_VERSION`: bump the suffix on every flash that matters (`2.0.0-p4.N`); a release drops it.
