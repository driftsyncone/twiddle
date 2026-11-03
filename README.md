# Twiddle

Standalone touchscreen remotes for the **Autoblow VacuGlide** (and a few Bluetooth toys). Each panel talks to
Autoblow's public device API with your device token, and to Bluetooth toys directly, so no phone, PC or
home-automation hub is needed once it is set up. One firmware family, two hardware platforms, the same behaviour,
look (six colour themes, Midnight by default) and safety rules on both.

> **Unofficial community project.** Not made, endorsed or supported by Autoblow, Lovense or Kiiroo. "Autoblow",
> "VacuGlide", "VacuPump", "Lovense", "Kiiroo" and "Keon" are trademarks of their owners. Use it at your own risk.

| Folder | Board | Stack |
|---|---|---|
| [`p4/`](p4/) | Guition **JC1060P470C**: ESP32-P4, 7" 1024x600, ESP32-C6 for Wi-Fi and Bluetooth | ESP-IDF 5.5 |
| [`s3/`](s3/) | Guition **JC8048W550C**: ESP32-S3, 5" 800x480 (also builds for the Waveshare ESP32-S3-Touch-LCD-5) | Arduino / PlatformIO |

Start with the README of the board you have. How the two compare feature by feature, and the rule that keeps them
consistent: [`p4/docs/feature-matrix.md`](p4/docs/feature-matrix.md). Flashing each board:
[`p4/docs/flashing.md`](p4/docs/flashing.md).

## Install

The easiest way is the **web flasher**: <https://driftsyncone.github.io/twiddle-flasher/>. It flashes the board over USB from
your browser, with no tools to install.

1. Use **desktop Chrome or Edge** and a USB cable that carries data (some charge-only cables do not).
   On Windows the Guition S3 board may need the WCH CH340 driver.
2. Plug the board in, open the flasher, pick your board (Guition **JC1060P470C** or **JC8048W550C**) and the latest version.
3. Choose **New install** for a fresh board, or **Update** for a board that already runs Twiddle.
   - In the installer window **leave "Erase device" unticked for an Update**: ticking it wipes your settings and, because an Update
     does not rewrite the boot files, the board stays dead until you run **New install** again.
   - For a New install, tick "Erase device" only if you accept losing everything on the board.
4. When it says it is done, close the flasher tab and **power-cycle the board** (unplug and replug USB). The ESP32-P4 stays in
   download mode until you do.
5. A new board shows **Connect to Wi-Fi** on its screen: tap **Scan**, pick your network, enter the password, **Save & restart**.
   The Autoblow token and Bluetooth toys are set up afterwards on the **Settings** and **Devices** tabs. If the screen is black or
   garbled, or you prefer a phone, join the board's own Wi-Fi **Twiddle-Setup** (password `twiddle-setup`) and open
   <http://192.168.4.1/setup>.
6. The board's own web login (user `ota`, used for updates over Wi-Fi and the admin pages) is a 6-character password shown **only**
   on the board's Settings tab, under "Web login". Five wrong tries lock the login for 60 seconds.

Prefer to build it yourself? Each folder has its own build and flashing instructions: [`p4/docs/flashing.md`](p4/docs/flashing.md)
and the [`s3/`](s3/) README. The prebuilt images are on the [releases page](https://github.com/driftsyncone/twiddle/releases);
the ESP32-S3 image contains LGPL-2.1 parts, and its source package and rebuild instructions are attached to each release.

**Everything here is unofficial and comes with no warranty or guarantee of any kind.** Flashing a board can fail and can stop it
from starting; back up first if the board matters to you. Known issue in 2.0.0: on the S3 the on-screen keyboard and the
Settings list are slow (about a third of a second per redraw).

## History
Twiddle started as `vacuglide-panel`, an Arduino firmware for the ESP32-S3 panel, and kept that name until it grew
support for other toys and a second board. The old repository,
[`driftsyncone/vacuglide-panel`](https://github.com/driftsyncone/vacuglide-panel), stays up as the 1.x release
history and points here.

## License
[Creative Commons Attribution-NonCommercial-ShareAlike 4.0](LICENSE) (the same for both folders). Third-party
components keep their own licenses: see `THIRD_PARTY_NOTICES.md` in each folder.
