# Hardware: Guition JC1060P470C (ESP32-P4, 7" 1024x600)

## The board
| Part | Detail |
|---|---|
| SoC | ESP32-P4, chip revision v1.3 (printed at boot; not verified here), dual-core RISC-V, 360 MHz (`CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=360`) |
| Flash | 16 MB (the boot log says 16384k; the bootloader header must be 16 MB, set in `sdkconfig.defaults`) |
| PSRAM | 32 MB hex PSRAM; **must run at 200 MHz** (see below) |
| Coprocessor | ESP32-C6-MINI-1U (external antenna connector) for Wi-Fi 6 and Bluetooth 5 LE, connected over SDIO (slot 1, 4-bit, 40 MHz: `CONFIG_ESP_HOSTED_HOST_SDIO_CLK_KHZ=40000`) |
| Display | 7" IPS 1024x600, JD9165 driver, MIPI-DSI 2 lanes |
| Touch | GT911 capacitive, I2C |
| USB | USB-C, native USB-Serial/JTAG (`/dev/ttyACM0`); used for flashing and the log |
| Battery | no fuel gauge known. The battery code only accepts a real MAX1704x (see "I2C" below) |

## Three panel variants: check yours
The same board name ships with different panels behind the JD9165, and a wrong choice gives a black, washed-out
or shifting picture. The variant is a **per-unit setting** (NVS `panel`, `GET /panel`, applies after a restart), default 0.
Guition's own variant naming: `JC1060P470C_I_W` (original and V2) and `JC1060P470C_I_W_Y` (the "new panel").

| `panel` | Name | How to recognise it |
|---|---|---|
| **0** | V2 (default) | rear label shows **V2** after the material number; the first 7" unit we tested. Also verified on two `JC1060P470C_I_W_Y` units (1024x600, normal colours) |
| **1** | original | older batch; the V2 table on it is washed out and shifts. Not seen on hardware here; values from ESPHome |
| **4** | **`_Y` new panel** (alternative RGB565 table) | model `JC1060P470C_I_W_Y`. An earlier test unit stayed black with a faint white glow with V2 settings; on a fresh/erased unit the V2 default has since driven two `_I_W_Y` units correctly, so try 0 first and 4 only if the screen stays black or the colours are wrong. Other batches may differ. The Display select flow (trial, Keep, revert) is untested on hardware |
| 2, 3 | test combinations | table and reset pin crossed; they gave nothing on the early `_Y` unit; kept for experiments |

`panel 4` differs from V2 in exactly two ways (found from Guition's `JC1060P470C_I_W_Y_New_Panel` Arduino demo and
verified on hardware): LCD reset on **GPIO5** and **16-bit RGB565** pixels, with the panel told so by COLMOD `0x3A = 0x55`
placed *before* sleep-out in the init table (the driver never sets COLMOD itself). Same V2 init table and timings, but the
pixel clock default is **48 MHz** (the picture shakes at 52). LVGL runs RGB565 on that variant and `/screenshot` converts
it to the same 24-bit BMP. Do **not** read a DCS register back (`esp_lcd_panel_io_rx_param`) on this panel: the read blanks it.
The `_Y` demo drives the touch without the reset/interrupt pins; the GT911 works here with both (verified by tapping).

**Choosing the variant without a working screen:** on a fresh image join the `Twiddle-Setup` Wi-Fi from a phone, open
`http://192.168.4.1/setup` and use **My screen looks wrong** (a select with the variants above). The panel restarts with the
choice **on trial**: a **Keep** banner appears at the bottom of its screen, and only a tap on it makes the choice permanent.
If the screen stays black nobody can tap, so after 3 unconfirmed boots the next boot goes back to the last confirmed variant
(the pixel clock override is cleared too) and the panel says so once on its screen. Boot counting is in `panelfb.h`
(NVS `panel`, `pprev`, `ptrial`, `pboots`). The firmware never reads the display controller's ID registers to guess the
variant: a DCS read blanks the `_Y` panel. The same select is on the LAN `/setup` page (behind the web login) and as `GET /panel?v=N`.

If a unit stays black: set `panel` 0, 4, 1 in turn (`curl -u ota:PW "http://<ip>/panel?v=4"`, then `POST /restart`); a black
screen with the backlight glowing and `GET /screenshot` showing the UI means the panel setup, not the firmware.

| | Original panel | V2 panel (default) |
|---|---|---|
| LCD reset GPIO | 5 | **0** |
| Pixel clock | 54 MHz | **52 MHz** |
| hsync pulse / back / front | 40 / 160 / 160 | **24 / 136 / 160** |
| vsync pulse / back / front | 10 / 23 / 12 | **2 / 21 / 12** |
| Init sequence | original table | V2 table (`main/board_dsi_init.c`) |
The V2 column is what `board_dsi_init.c` sets (checked). The original-panel column and the 750 Mbps lane rate come from
ESPHome's `mipi_dsi` models (`JC1060P470` and `JC1060P470-V2`) and are not in this repo's code (not verified here); RGB
element order is set in `board_dsi_init.c`. The DSI PHY is powered from LDO channel 3 at 2.5 V (checked).

## Chip revision
The boot log prints `chip: ESP32-P4 rev vX.Y` and `GET /` has the same `chip:` line; anything but **v1.3 (the only
revision tested)** adds a warning. The image is built with ESP-IDF 5.5.1 for revisions **v0.1 to v1.99**
(`CONFIG_ESP32P4_REV_MIN_FULL=1`, `CONFIG_ESP32P4_REV_MAX_FULL=199`). Research for a v3.0 chip (2026 silicon, "changes power rails"):
- `ESP32P4_REV_MAX_FULL` is a hidden Kconfig symbol in IDF 5.5.1 ("can not be changed by user, only Espressif can change it when a new version is supported in IDF"), so it cannot be raised from `sdkconfig.defaults`.
- The check is in the ROM-started second-stage bootloader (`bootloader_common_check_chip_revision_validity`): an app image whose
  max revision (v1.99) is below the chip's is refused with `chip revision check failed. Required <= v1.99, found v3.0.`, unless the
  eFuse `disable_wafer_version_major` is burnt (irreversible, not an option). So a v3.0 board would not boot this image at all.
  The warning above therefore only helps for revisions that boot (v1.0, v1.1 ...), not for v3.0.
- The fix is a newer ESP-IDF that supports v3.x (it also carries the chip fixes), a rebuild and a hardware test on a v3.0 unit.
  Nothing was changed here: lowering the minimum or editing the maximum without that would only trade a clear bootloader error for undefined behaviour.
  The web flasher page says "tested on chip revision v1.3 only".

## Pin map
| Function | Pins |
|---|---|
| Backlight (LEDC PWM, 8 bit, 2 kHz) | GPIO23 |
| LCD reset | GPIO0 (V2), GPIO5 (original, `_Y`) |
| MIPI-DSI | dedicated DSI pins, PHY powered from LDO channel 3 at 2.5 V |
| Touch / I2C0 | SDA GPIO7, SCL GPIO8; GT911 at 0x5D, reset GPIO22, interrupt GPIO21 |
| C6 over SDIO (slot 1, 4-bit, 40 MHz) | CLK 18, CMD 19, D0 14, D1 15, D2 16, D3 17 |
| C6 reset | GPIO54 (`CONFIG_ESP_HOSTED_HOST_RESET_GPIO=54`; polarity not verified here) |

## I2C bus (touch bus, I2C0)
Devices seen on a scan: `0x14`, `0x18`, `0x32`, `0x36`, `0x5D`. The GT911 is `0x5D`. The others are not
identified. **`0x36` is the address of the MAX17048 fuel gauge, but the chip there is not one**: it answers every
read with zeros. `battery.cpp` therefore requires a known version value and a plausible cell voltage before it
believes a gauge exists; without that check the panel showed "Battery at 0%: control is paused".

## Display pipeline
esp_lvgl_port with a MIPI-DSI display: **RGB888 (24-bit), two full frame buffers in PSRAM**, `avoid_tearing` and
`direct_mode` on, so LVGL draws straight into the buffers being scanned out. LVGL's own heap is in PSRAM
(`CONFIG_LV_USE_CUSTOM_MALLOC`, `lv_mem_psram.c`). No LVGL rotation (the DSI panel rejects `swap_xy`; the harmless
log line `esp_lcd_panel_swap_xy ... not supported` comes from the port). The UI was laid out for 800x480 and is
scaled with `PX()` / `PY()` helpers in `ui.cpp`.

## PSRAM must run at 200 MHz
`CONFIG_SPIRAM_SPEED_200M` only exists with `CONFIG_IDF_EXPERIMENTAL_FEATURES=y`. Without it the option is ignored
and PSRAM silently runs at **20 MHz** ("100M" is also not an option and falls back to 20M). Symptoms of a slow
PSRAM: white screen, `lcd.dsi.dpi: can't fetch data from external memory fast enough, underrun happens`, blue
flashes while the UI redraws, a wobbling picture. 80 MHz is enough for a static image only. The boot log must say
`esp_psram: Speed: 200MHz`. Without PSRAM at all, creating the display fails with `ESP_ERR_NO_MEM` (one 24-bit
frame buffer is 1.8 MB).

## Memory (after boot, Wi-Fi + mDNS + web server up, Bluetooth off)
Measured on the board when this was written (not re-measured in this review): about 284 KB internal RAM free (largest block about 200 KB) and 29.7 MB PSRAM free. BLE costs about 21 KB internal. (Earlier notes quoted ~197 KB free with Wi-Fi + BLE both on; the two numbers describe different states.) The firmware prints live values in the `mem boot:` log line and in `GET /` (`heap:`).
Large buffers (LVGL, mbedTLS via `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC`, NimBLE host pools via
`CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL`) are in PSRAM; tasks keep internal stacks.

## Flash layout (`partitions.csv`)
| Name | Type | Offset | Size |
|---|---|---|---|
| nvs | data | 0x9000 | 24 KB |
| otadata | data | 0xF000 | 8 KB |
| phy_init | data | 0x11000 | 4 KB |
| ota_0 | app | 0x20000 | 3 MB |
| ota_1 | app | 0x320000 | 3 MB |
There is no factory slot: updates alternate between ota_0 and ota_1, and `otadata` decides which one boots (see
[ota-and-rollback.md](ota-and-rollback.md)). Other fixed offsets (from the `idf.py build` flash command): bootloader 0x2000, partition table 0x8000.
The app is 1,868,816 bytes (0x1c8410, version 2.0.0-p4.4) and the build tool reports "0x137bf0 bytes (41%) free" in the
smallest slot. That headroom shrinks as features are added: read it from the build output, not from here.
The 16 MB flash is mostly unused (~9.6 MB after 0x620000). Do not reuse the C6-updater project's table (it has different offsets).

## Power and USB
Powered over the USB-C port (5 V). The same port flashes and carries the log. The panel needs a decent cable and
port: a weak port shows up as brownouts/boot loops.

## Troubleshooting: Flip screen

Flip sends MADCTL GS+SS to the JD9165 (`boardSetFlip`, `GET /flip?n=1` to test); the log line `board: flip N: mirror=ESP_OK` shows the call worked. Verified by eye on V2 and on the `_Y` panel (no register readback: it blanks the `_Y` panel). If the picture does not turn although it reads 0x03, the V2 panel ignores MADCTL in DSI video mode; then use the fallback of a PPA 180-degree copy into the DPI buffer (not implemented). If the picture turns but touch is mirrored wrong, check `touchCb` in `main/board.cpp`.
