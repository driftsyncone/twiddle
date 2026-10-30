# The ESP32-C6 coprocessor (Wi-Fi and Bluetooth)

The P4 has no radio. All Wi-Fi and BLE goes through the ESP32-C6 on the board using **esp-hosted** over SDIO. On
the P4 side that is `esp_hosted` + `esp_wifi_remote` (Wi-Fi) and NimBLE with the hosted HCI transport (BLE).
Pins and settings: [hardware.md](hardware.md); the relevant Kconfig lines in `sdkconfig.defaults` are
`CONFIG_ESP_HOSTED_P4_DEV_BOARD_FUNC_BOARD=y`, `CONFIG_ESP_HOSTED_CP_TARGET_ESP32C6=y`,
`CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE=y` (they resolve to slot 1, 4-bit, CLK18/CMD19/D0-3 14-17, reset GPIO54).

## Versions must match (major)
The host (the P4 firmware) uses esp_hosted **3.0.10**. The board ships with the C6 on slave firmware **2.3.2**; the
boot log then complains `major version mismatch`. Wi-Fi still works, but BLE needs a workaround and RPC timeouts
can occur. The C6 on this board has been updated to **3.0.10**; check with the boot line
`esp-hosted fw versions: host=3.0.10 coprocessor=3.0.10 (match)`.

## Updating the C6 firmware (recipe that worked)
The steps use files outside this repo (esp_hosted examples, a separate updater project). They were run twice on hardware:
on the first 7" unit during bring-up, and on 2026-10-30 on the `_Y` unit (C6 2.3.2 to 3.0.10, boot log afterwards:
`esp-hosted fw versions: host=3.0.10 coprocessor=3.0.10 (match)`). The `Failed to write OTA chunk` message comes from the
bring-up work.
A 3.x host cannot push an update to a 2.x slave: its OTA RPC fails at the first chunk
(`Failed to write OTA chunk 0: ESP_FAIL`). The 2.x host library can. Steps, no hardware access to the C6 needed:

1. **Build the new slave image** from the same esp_hosted version as the host. Copy
   `managed_components/espressif__esp_hosted/examples/bluetooth/esp_hosted_nimble/bleprph_wifi_coex/cp`
   (Wi-Fi + BLE with VHCI) to a new folder, pin `espressif/esp_hosted` to `3.0.10` in its
   `main/idf_component.yml`, then `idf.py set-target esp32c6 && idf.py build`. Result:
   `build/eh_cp_bt_wifi_hosted_hci_mcu.bin` (about 1.3 MB; the C6's OTA slots are 1.75 MB). Do not use the
   `coprocessor_ota` example's slave: it has Bluetooth off.
2. **Build the updater** for the P4 from esp_hosted **2.13.1**:
   `idf.py create-project-from-example "espressif/esp_hosted=2.13.1:host_performs_slave_ota"`, pin the
   `espressif/esp_hosted` dependency to `2.13.1`, set `CONFIG_OTA_METHOD_PARTITION=y`, 16 MB flash, PSRAM at
   200 MHz, and the board's hosted pin settings (defaults match). Build for esp32p4.
3. **Flash the updater**, then write the C6 image into its `slave_fw` partition (offset `0x5F0000`; the C6 image
   is for another chip so esptool needs `--force`):
   ```sh
   idf.py -p /dev/ttyACM0 flash
   python -m esptool --chip esp32p4 -p /dev/ttyACM0 --after hard_reset write_flash --force 0x5F0000 eh_cp_bt_wifi_hosted_hci_mcu.bin
   ```
4. The updater streams the image, activates it, prints `Slave firmware version: 3.0.10` and restarts the host.
5. **Restore the real app.** The updater uses a different partition table, so write the app's own bootloader, partition
   table, otadata and app again: `idf.py -p /dev/ttyACM0 flash` from the Twiddle tree (or `esptool write_flash @flash_args` in its
   `build/`). `erase-flash` is not needed: the NVS settings (per-unit `panel`, `pclk`, theme, Wi-Fi) are kept because both layouts
   put `nvs` at 0x9000. Use `erase-flash` only to start from a blank unit.

Notes from the run on the `_Y` unit: the updater prints `Slave firmware version: 2.3.2`, transfers, then restarts itself and
reports `3.0.10` on every later boot (it keeps looping; that is the sign it worked, flash the real app then). The C6 image sits at
0x5F0000, which is the unused tail of the app's second OTA slot (`ota_1` ends at 0x620000), so writing it first, before the updater
is flashed, damages nothing.

The C6 has two firmware slots (A/B). A transfer that dies midway leaves the old firmware bootable.

## Bluetooth start-up order
`devices::begin()` (Bluetooth) runs before `wlan::begin()`: `esp_hosted_bt_host_stack_setup()` then NimBLE init.
Bluetooth is off by default (`settings.bluetooth = -1`: on only when a toy is remembered) so Wi-Fi can run without
modem sleep; turning it on from the Devices tab restarts the panel. While BLE is on, Wi-Fi uses minimum modem sleep.
