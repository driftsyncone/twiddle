# Consistency across hardware, and the feature matrix

## The rule (decided 2026-10-30)
Every user-visible feature that exists on one supported panel should exist, **with the same behaviour and look**,
on every hardware target we have tested. Do not let the firmwares drift apart.

- Add a feature to all targets in the same piece of work, or ship it on one and open the backport straight away
  (a branch per firmware repo, listed in the "Pending" column below).
- If a target genuinely cannot do something, the reason is a **technical** one and it is written down **here**: what
  limits it, and whether a workaround exists. An undocumented gap is a bug.
- Shared rules are identical everywhere: valve naming (Looser = stroke-minus, Tighter = stroke-plus), Autopilot is our own
  generator, the loss/low-battery safety behaviour, the colour roles and the six theme palettes.
- When you change this table, change the code and the docs of **both** firmware repos' READMEs in the same pass.

## Targets
| Target | Hardware | Firmware | Hardware-tested |
|---|---|---|---|
| **P4** | Guition JC1060P470C: ESP32-P4, 7" 1024x600 DSI, C6 coprocessor | ESP-IDF, folder `p4/` (this folder) | yes, 2026-10 |
| **Guition S3** | Guition JC8048W550C: ESP32-S3, 5" 800x480 RGB, GT911 | Arduino/PlatformIO, folder `s3/` (`env:panel`) | yes |
| **Waveshare S3-5** | ESP32-S3-Touch-LCD-5, 800x480 RGB | same Arduino repo (`env:waveshare5`) | built and tested until 2026-10-26; the board was repurposed for another project, so **compile-only since** |

## Matrix
`yes` = same as the reference behaviour, `~` = works with a documented difference, `no` = not available (reason given),
`pending` = being added (branch named).

| Feature | P4 | Guition S3 | Waveshare S3-5 | Notes / technical reason |
|---|---|---|---|---|
| Remote, Autopilot, Session, Scripts, Devices, Settings tabs | yes | yes | yes | P4 layout is scaled 1.28x/1.25x from the 800x480 design (`PX/PY`) |
| Autoblow cloud control (speed, valves, pulses, STOP, loss handshake) | yes | yes | yes | same logic, ported |
| Bluetooth toys (Lovense, Keon, OSSM) | yes (untested with a real toy) | yes | yes | P4: radio is the C6 via esp-hosted; S3: internal radio. Different stacks, same drivers |
| Setup portal, mDNS, web server | yes | yes | yes | Default hostnames differ per board so they do not clash: P4 `twiddle-p4`, Guition S3 `twiddle-s3`, Waveshare `twiddle-ws` (`PANEL_HOSTNAME` overrides). Two boards of the same model still need `PANEL_HOSTNAME` to differ |
| OTA over Wi-Fi + rollback | yes | yes | yes | P4 uses `esp_ota` + bootloader rollback; S3 uses Arduino `Update` + `esp_ota` |
| `/screenshot` over the web | yes (24-bit BMP) | yes (RGB565 BMP) | yes (RGB565 BMP) | colour depth differs because the frame buffers do |
| Brightness slider | yes (PWM, GPIO23) | yes (PWM on the backlight enable) | **no (on/off)** | Waveshare: the backlight is an output of the CH422G IO expander, which cannot PWM |
| Screen flip (upside-down mount) | yes (checked by eye on V2 and `_Y`) | yes | yes | P4: the JD9165 scans both axes the other way (MADCTL GS+SS via `esp_lcd_panel_mirror`, live, no re-init); the GT911 touch is mirrored in `touchCb`. No cost in frame rate or memory. The frame buffer, LVGL and `/screenshot` stay unflipped (Arduino `/screenshot` should be compared: here it is the unflipped buffer). MADCTL reads back 0x03 after flip, 0x00 after unflip; the visual flip and touch were not confirmed by eye. `GET /flip?n=0\|1` flips live for testing |
| Battery gauge | no | optional (MAX17048 on the touch or P4 header, or wired divider) | no | P4 and Waveshare: a non-gauge chip answers at I2C 0x36 / the CH422G occupies 0x30-0x3F; the probe requires a real MAX1704x |
| Display pixel clock per unit | **yes** (NVS `pclk`, MHz, 0 = default 52 / 48 on `_Y`; `GET /pclk`) | **yes** (NVS `pclk`, default 14; `GET /pclk`) | no (fixed) | Guition S3 units differ: #A needs 14 MHz, #B needs 16 (colour self-test at 14). P4: the panel variants differ in init table and timings, not just a clock, so it is a code variant, see [hardware.md](hardware.md) |
| Wi-Fi gap setting, favourites, script names | yes | yes | yes | |
| **Themes** (Daylight, Midnight, Ocean, Forest, Ember, Blush; live, saved) | **yes** | pending (`themes` branch) | pending (same code) | Default is Midnight on every target (the S3 firmware gets the same default so the family looks the same; its original look was dark); saved choices are kept. See [themes.md](themes.md); one palette table row per theme |

Threading and tick rate (`CONFIG_FREERTOS_HZ=1000`, `wlan::`/`battery::` outside the UI lock, no off-task redraws) are P4-only
port details with no S3 counterpart: the S3 firmware runs everything on one Arduino loop task.

## When you add a feature
1. Does it touch the shared UI/logic? Implement it in both repos, same names for settings/NVS keys.
2. Hardware-specific limits? Put the reason in the table above, not only in a code comment.
3. Update both READMEs/CHANGELOGs and this table.
4. Test on each target that can be tested; mark the others compile-only and say so.
5. Did it change how a board is built, flashed, identified, recovered or configured per unit? Update [flashing.md](flashing.md) in the same commit.

## P4 column: what was checked (2026-10-30 review)
Every P4 cell above was compared with the code in this repo. The cells are right as far as the code goes; the differences are
in what has been **verified on hardware** (the matrix's "yes" means "implemented"; see the README "Not verified yet" list):
- Tabs, setup portal: all present in code. Touch by finger on the real screen, the Settings tab visually and the portal from a phone are
  not verified on hardware.
- Autoblow cloud control: tested against the emulator only; the real VacuGlide has not been commanded (not verified on hardware).
- Bluetooth toys: drivers pass the 67 host checks (`test/host/run.sh`); no real toy has been used (the cell already says so).
- Brightness: PWM on GPIO23 in code (`board.cpp`, `BOARD_DIMMABLE 1`), visual check of the slider is part of the unverified Settings tab.
- Flip: `boardSetFlip()` only stores the setting (matches "no (no-op)"). The reason text in the Notes column says LVGL rotation "cannot be
  combined" with direct mode; the code only says the panel has no swap/mirror and rotation is not used. Treat the stronger claim as
  a hypothesis.
- Battery: the probe requires a known MAX1704x version and a plausible voltage (matches "no").
- Themes: six palettes in `PALETTES[]`, live and saved (matches).
