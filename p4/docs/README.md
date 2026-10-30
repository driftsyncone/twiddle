# Documentation index

Documentation for the ESP32-P4 panel: using it, building and flashing it, and extending it.

| Doc | Read it when |
|---|---|
| [user-guide.md](user-guide.md) | you want to use the panel: first boot, every tab, status bar and banners, updates |
| [settings.md](settings.md) | you need every setting with range, default, storage and when it applies |
| [themes.md](themes.md) | colour themes, their palettes, and how to add one |
| [flashing.md](flashing.md) | flashing ANY board: the pre-flash checklist, per-board USB/OTA steps, backup and restore |
| [dev-setup.md](dev-setup.md) | setting up a machine, building, flashing, reading the serial log |
| [hardware.md](hardware.md) | you need pins, the panel variants, memory, flash layout |
| [architecture.md](architecture.md) | changing the firmware: modules, tasks, boot order, storage, web endpoints, cloud API |
| [c6-firmware.md](c6-firmware.md) | Wi-Fi/Bluetooth coprocessor problems, or the C6 firmware needs updating |
| [ota-and-rollback.md](ota-and-rollback.md) | updating the panel over Wi-Fi, how rollback works |
| [testing.md](testing.md) | testing without the real device: emulator, screenshots, host tests, sharing the board |
| [network.md](network.md) | the panel cannot reach something (VLAN, firewall, DNS, mDNS) |
| [troubleshooting.md](troubleshooting.md) | something looks wrong: symptom -> cause -> fix |
| [howto.md](howto.md) | you are about to add a theme, setting, tab, toy driver, web route or dependency, bump the version or cut a release |
| [feature-matrix.md](feature-matrix.md) | adding or changing a feature: the consistency rule and what each hardware target supports |
| [history.md](history.md) | you want to know how the Arduino firmware was ported and the bring-up lessons |
| [other-panels.md](other-panels.md) | the older ESP32-S3 panels and where their docs live |

Conventions used across the docs: "the dev box" is the machine with the USB cable to the board; `secrets.h` means `main/secrets.h` (never committed); "the real device" is the Autoblow VacuGlide and
must never be commanded during tests.
