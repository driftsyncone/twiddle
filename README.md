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

## History
Twiddle started as `vacuglide-panel`, an Arduino firmware for the ESP32-S3 panel, and kept that name until it grew
support for other toys and a second board. The old repository,
[`driftsyncone/vacuglide-panel`](https://github.com/driftsyncone/vacuglide-panel), stays up as the 1.x release
history and points here.

## License
[Creative Commons Attribution-NonCommercial-ShareAlike 4.0](LICENSE) (the same for both folders). Third-party
components keep their own licenses: see `THIRD_PARTY_NOTICES.md` in each folder.
