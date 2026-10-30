# Updating over Wi-Fi, and rollback

## Upload
```sh
./ota.sh [build/twiddle.bin] [host]
```
Host: argument, else `$PANEL_HOST`, else `OTA_HOST` in `main/secrets.h`, else `twiddle-p4.local` (the panel's mDNS
name). The script reads `OTA_PASSWORD` from `secrets.h`, `POST`s the image to `http://<host>/update` with basic
auth `ota:<password>` (`curl -F firmware=@file`), then polls `GET /` until `firmware: confirmed`.
It compares the `id:` line (the firmware's own hash, different for every build) before and after: unchanged after
the reboot prints `ROLLED BACK (or the same image was uploaded)`.
The machine running it must be able to reach the panel: see [network.md](network.md).

## What the panel does with an upload
`/update` first calls `control::stopAll()` (verified in `ota.cpp`: the running session and every device are stopped
before anything is written, like the Arduino firmware), then waits for the STOP to go out (at least 0.8 s and until no
device reports moving, at most 5 s; after 5 s it logs `OTA: stop not confirmed` and continues, the new firmware sends STOP again
when it boots). Only then does it show the "Updating firmware" screen (the LVGL task draws it; the web task never renders) and
waits 0.7 s, then streams the body into the inactive OTA slot (`esp_ota_*`), validates it (`esp_ota_end`), sets it as the boot
partition and reboots after answering `OK, rebooting`. The screen shows a percentage every 10 %. On any failure (empty
upload, stalled or lost connection, flash write error, image rejected) it answers 500 with the reason, aborts the
write and removes the screen; the old firmware keeps running. Note that the devices were already stopped by then.

## Rollback (probation)
`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` makes the bootloader mark a freshly written image "pending verify".
Nothing confirms it except `rollback.cpp`: **30 s of Wi-Fi up** (continuously, with the loop task still running)
confirms. If that has not happened after 3 minutes, a timer sends STOP (via the loop task), 3 s later calls
`esp_ota_mark_app_invalid_rollback_and_reboot()`. A crash or reset before confirmation also makes the bootloader go
back to the previous slot on the next boot. `GET /` reports the state as
`firmware: pending | confirmed | new | invalid | aborted | undefined | factory` (`pending` and `confirmed` are the normal ones;
`factory` is what `rollback::state()` returns when the image state cannot be read, i.e. a firmware that was not
written through the OTA mechanism; what a USB flash reports (not verified on hardware) depends on `otadata`).
Tested on the board (as reported when the module was written; the test hooks exist in the code: `TEST_ROLLBACK_CRASH` aborts 15 s after boot, `TEST_ROLLBACK_NEVER_CONFIRM` lets the deadline fire): a good image confirmed after 30 s, and an image that called `abort()` 15 s after boot was rolled back automatically. (not re-verified here)

Without that Kconfig option an update is never "pending" and rollback does nothing; keep it in `sdkconfig.defaults`.

## Things to know
- First real OTA of the P4: 2026-10-30 (`2.0.0-p4.2` -> `p4.3` -> `p4.4`), each confirmed after 30 s. The dev box needed a firewall rule to reach the panel ([network.md](network.md)).
- A USB flash (`idf.py flash`) writes the first slot (`ota_0` at 0x20000) and resets `otadata`; whether it then starts on probation or reports `factory` has not been checked on hardware.
- The slots are 3 MB (0x300000); the image is 0x1c8410 bytes = 1,868,816 bytes (1.78 MiB), so 41 % of a slot is free (this is the figure `idf.py build` prints: "0x137bf0 bytes (41%) free", checked on a clean build of `2.0.0-p4.4`).
- Updating the **C6** firmware is a separate procedure: [c6-firmware.md](c6-firmware.md).
- `ota.sh` waits up to about 4 minutes (10 s, then 46 polls 5 s apart) for `firmware: confirmed`; a new `id:` that is not yet confirmed keeps it waiting, an unchanged `id:` after the reboot prints `ROLLED BACK`.
