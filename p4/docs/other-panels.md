# The older ESP32-S3 panels

The ESP32-P4 panel is the third device in the family. The earlier ones run Arduino firmware and have their own
docs and release flow:

| Panel | Hardware | Firmware / docs |
|---|---|---|
| Guition JC8048W550C | ESP32-S3, 5" 800x480 RGB, GT911, optional battery gauge (MAX17048) | Arduino/PlatformIO folder [`../../s3`](../../s3) (its older public release is the GitHub project `driftsyncone/vacuglide-panel`, see [history.md](history.md)). The README there covers building, the BOM, the 3D-printed case and settings |
| Waveshare ESP32-S3-Touch-LCD-5 | ESP32-S3, 5" 800x480 | an earlier VacuGlide remote target in the Arduino repo |

The Arduino repo's own versioning rule (`X.Y.Z-dev.N` bumped on every flash) is followed here as `2.0.0-p4.N`. The
Arduino panels are updated with PlatformIO (`pio run -e ota -t upload`) or `ota.sh`.
