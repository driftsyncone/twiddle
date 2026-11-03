# Flashing every board: the process

One page for the whole panel family. **Keep it current:** if a change alters how a board is built, flashed,
identified, recovered or configured per unit (new env, partition, setting, endpoint a tool uses, hardware revision),
update this page in the same commit. The checklist in [feature-matrix.md](feature-matrix.md) and the release checklist
in [howto.md](howto.md) both include it.

Several projects and several firmwares can share this hardware, and the same *model* can be different *units*
(display batches, firmware owners). Identify the unit before you flash it. Keep your own inventory of units (identity,
what runs on each, per-unit settings such as the display pixel clock); it is deliberately not part of this repo.

## The six steps before flashing anything
1. **Identify what is on it.** Over the network: `curl http://<ip>/ | head -3` (first line is `<project> <version>`, then
   `board: <model>`). The P4 repo's `./ota.sh --check <ip>` does this and refuses a board that is not running
   `twiddle` (or the old `vacuglide-panel`, see below) on the right model. Over USB: read the boot log.
2. **If another project's firmware is on it, stop and ask** its owner (and read that project's notes). Never overwrite it
   because the model matches.
3. **Back it up** if you cannot rebuild what is on it from source (anything with settings, tokens or a config you do not
   have). Full flash dump and verify (works for any ESP32 on its USB port, ~5 min for 16 MB):
   ```sh
   export PATH=/usr/bin:$PATH; . ~/esp/esp-idf/export.sh
   python -m esptool -p /dev/ttyUSB0 -b 921600 read_flash 0x0 0x1000000 ~/backups/<board>-<mac>-<date>.bin
   sha256sum ~/backups/<board>-<mac>-<date>.bin | tee ~/backups/<board>-<mac>-<date>.bin.sha256
   python -m esptool -p /dev/ttyUSB0 -b 921600 verify_flash 0x0 ~/backups/<board>-<mac>-<date>.bin   # must say "verify OK"
   ```
   Restore (this puts back everything, including settings): `python -m esptool -p /dev/ttyUSB0 -b 921600 write_flash 0x0 <file>`.
   A backup holds Wi-Fi passwords and tokens: keep it private.
4. **Share the board safely.** One USB port, one board: if more than one person or process is around, hold a lock for the
   whole flash **and** the capture after it ([testing.md](testing.md)).
5. **Use the right secrets.** `main/secrets.h` (P4) / `include/secrets.h` (Arduino) are gitignored. Real-cloud config for
   real use; the emulator's token only for emulator tests, restored afterwards ([testing.md](testing.md)).
6. **Know the way back and bump the version.** Bump `FW_VERSION` (`2.0.0-p4.N` on the P4, `2.0.0-dev.N` on the S3). Name the
   artefact with the board in it: `tools/dist.sh` writes `dist/twiddle-p4-<version>.bin` (+ `.sha256`), plus the bootloader, partition-table and otadata parts and `twiddle-p4-<version>.parts.json` for the web flasher.

## P4 (Guition JC1060P470C), folder `p4/`
Environment: `export PATH=/usr/bin:$PATH; . ~/esp/esp-idf/export.sh` ([dev-setup.md](dev-setup.md)). Build: `idf.py build`.
- **By USB** (board on `/dev/ttyACM0`): `idf.py -p /dev/ttyACM0 flash`. After a partition-table change, or when the board ran
  another project's firmware (it may have its own layout), use `idf.py -p /dev/ttyACM0 erase-flash flash`; erase loses the
  stored Wi-Fi/token/settings, so `main/secrets.h` provides them again at boot.
- **Over Wi-Fi:** `./ota.sh --check <ip>` first, then `PANEL_HOST=<ip> ./ota.sh build/twiddle.bin`. Your machine must be
  able to reach the panel ([network.md](network.md)). The new image must confirm itself (30 s on Wi-Fi) or the
  bootloader goes back to the old one ([ota-and-rollback.md](ota-and-rollback.md)).
- **Boards still on the pre-rename firmware:** before Twiddle (`2.0.0-p4.6`) the firmware identified itself as `vacuglide-panel` on `GET /` and answered to `vgremote.local`. `ota.sh` accepts both `twiddle` and `vacuglide-panel` as the project name for now (any other project is still refused), so such a board takes one update to the new name: use its IP or `vgremote.local` as the host that once; afterwards it is `twiddle-p4.local`. A `p4.6` board still answers to `twiddle.local` until it takes this update. Settings are kept.
- **C6 coprocessor:** a separate firmware on a separate chip, updated through the P4 ([c6-firmware.md](c6-firmware.md)). Needed
  only when host and slave major versions differ.
- **Switching a board between projects:** back up first (step 3), flash the other project
  with `erase-flash`, and update your inventory. Only do this with the board owner's agreement.
- **Release image (for the web flasher) and the web password:** `idf.py -DTWIDDLE_RELEASE=ON build` (after changing the option run
  `idf.py reconfigure`; use a separate build directory with `-B build-release` to keep your dev build) compiles no Wi-Fi, token,
  web password or host: `main/secrets.h` is ignored. Then `tools/check-release-image.sh --build-dir build-release` must print
  `0 finding(s)`, and `BUILD_DIR=build-release tools/dist.sh` runs it before it writes `dist/`. A fresh image has no Wi-Fi:
  the panel opens `Twiddle-Setup` (password `twiddle-setup`); join it, open `http://192.168.4.1/setup`, check the form fields (a phone browser may refill old Wi-Fi or token values), choose the display if the
  screen is wrong, pick the Wi-Fi, enter the Autoblow device token, **write down the web login**: the page tells you to read it on the panel
  (Settings tab, "Web login", user `ota`; it is never shown on a web page, because the portal Wi-Fi password is public; tapping it resets it). `ota.sh` reads it from `$OTA_PASSWORD`, then
  `main/secrets.h`, then asks. A dev build with `OTA_PASSWORD` in `secrets.h` keeps that fixed password. While a panel has no
  password at all (should not happen) every admin endpoint answers 403. Erasing the flash gives the unit a new password.
- **Per-unit variant:** the panel behind the display driver differs between batches ([hardware.md](hardware.md)): `panel` 0
  (V2, default; verified on two `_I_W_Y` units), 4 (RGB565 alternative table for `_Y`, if V2 stays black or shows wrong colours), 1 (original). Set it over the web and restart: `curl -u ota:PASSWORD
  "http://<ip>/panel?v=4"`, then `curl -u ota:PASSWORD -X POST http://<ip>/restart`; check the log line
  `board: display panel variant ...`. A unit that stays black (backlight on, maybe a faint white edge glow) while
  `GET /screenshot` shows the UI wants another variant. If the screen shakes, lower the pixel clock: `GET /pclk?mhz=N`
  (20 to 60, 0 = variant default: 52, 48 on `_Y`), then restart. A fresh or erased unit has no Wi-Fi settings and no `panel`
  yet: it boots with variant 0 and joins Wi-Fi from `secrets.h`, so set `panel` over the web after the first flash. A
  release image has no `secrets.h` Wi-Fi: use **My screen looks wrong** on the setup portal instead ([hardware.md](hardware.md));
  the choice is on trial until **Keep** is tapped on the panel (3 unconfirmed boots return to the previous one).
- **BOOT and RESET buttons** (rear view, the two small buttons together at the top-left; the third one near USB-C is the battery
  button): **hold BOOT, tap RESET, release BOOT** = ROM download mode for `esptool`. Use `--before no_reset` then, and press RESET
  alone afterwards (a stuck BOOT leaves the chip in download mode). Needed when the running firmware ignores USB (for example
  Guition's demo makes `esptool` time out with `Write timeout`).
- **Factory state:** Guition's published demo is `JC1060P470_P4.bin` on the vendor page (`ESP32-P4-7inch` files); it is a full 16 MB
  image written at 0x0 after `erase_flash`. It is for the EK79007-type board and did not show a picture on our first `_Y` unit.
  Back up a unit before overwriting it: the original factory firmware of the `_Y` unit was lost.
- **Verify:** boot log (`board: display and touch up`, `esp-hosted fw versions ... (match)`, `Wi-Fi up`), `GET /` (`firmware:
  confirmed`), a screenshot ([testing.md](testing.md)).

## Guition S3 (JC8048W550C), folder `s3/`, `env:panel`
Environment: PlatformIO (`pio`). Build: `pio run -e panel`.
- **By USB:** the board is a CH340 serial adapter, `/dev/ttyUSB0`: `pio run -e panel -t upload --upload-port /dev/ttyUSB0`.
  Not erasing keeps the stored settings (Wi-Fi, token, `pclk`). The partition layout differs from the ESPHome firmware this
  hardware sometimes carries, but a PlatformIO flash writes its own table; to go back to ESPHome restore the backup (step 3).
- **Over Wi-Fi:** `./ota.sh` (host from `OTA_HOST` in `include/secrets.h`, or `PANEL_HOST`); it reads the panel's `board:` and refuses
  another model (full name, e.g. `Guition JC8048W550C`). Or `pio run -e ota -t upload`.
- **Per-unit display clock:** the default is `pclk` 14 MHz. Some units show the panel's colour self-test at 14 and
  need 16. The firmware itself is fine in that case (`GET /screenshot` draws the UI). Fix it over the web and restart:
  `curl -u ota:PASSWORD "http://<ip>/pclk?mhz=16"` then `curl -u ota:PASSWORD -X POST http://<ip>/restart`. Check the log line
  `display: pixel clock 16 MHz`. Record the value per unit in your inventory.
- **Verify:** serial log (`board: Guition JC8048W550C`, `display: pixel clock N MHz`, `GT911 at 0x5D`, `Wi-Fi up`), `GET /`
  (`display fixes: 0`), screenshot.

## Waveshare S3-5, folder `s3/`, `env:waveshare5`
Kept compiling (`pio run -e waveshare5`) so the family stays consistent ([feature-matrix.md](feature-matrix.md)). It has
had little real-hardware testing. If the board runs another firmware, back it up first (step 3) and be ready to restore it.
Flash with `pio run -e waveshare5 -t upload` (native USB, `/dev/ttyACM0`; battery switch off on a weak port).
OTA: `OTA_BOARD=waveshare5 ./ota.sh` (host from `OTA_HOST_WS`).

## After flashing
- Confirm what you think is on the board really is (`GET /` first line and `id:`), and update your inventory if ownership or
  the firmware changed.
- Restore anything you changed temporarily: the emulator token and `AUTOBLOW_API_BASE` in `secrets.h`, firewall rules
  ([network.md](network.md)).
- If the process you just followed differs from this page, fix this page now.
