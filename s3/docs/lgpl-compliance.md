# LGPL compliance for the ESP32-S3 image (DRAFT)

> **Draft, written by the maintainers' tooling, not read by a lawyer, not legal advice.** It records what is in the image and what we
> publish. Decision (2026-11-02): Twiddle's own code stays under CC BY-NC-SA 4.0 (the `LICENSE` text is not edited), and an explicit LGPL
> additional permission is added in [LICENSE-NOTES.md](../LICENSE-NOTES.md) and in the written offer. Whether that wording satisfies
> LGPL-2.1 section 6 is still the lawyer's question (last section).

## What is statically linked

Found from the linker map of `pio run -e panel-release` (2.0.0-dev.20), not from what is installed: only objects that put bytes into the
image count (`scripts/gen_notices_pio.py` in the twiddle-flasher repository does this and fails on anything new).

| Group | Licence of the linked source | Bytes in the image |
|---|---|---|
| Arduino-ESP32 core 3.3.11 (`libFrameworkArduino`): 29 objects, 13 under LGPL-2.1+ headers or the package default, 15 Apache-2.0 | LGPL-2.1-or-later (package.json `license`) | 66,713 |
| Arduino-ESP32 library `WiFi` (6 of 8 files LGPL, 2 Apache-2.0) | LGPL-2.1+ | 17,496 |
| `Network` (2 of 5 files LGPL) | LGPL-2.1+ | 21,798 |
| `WebServer` (4 of 4; two without a header, counted as LGPL) | LGPL-2.1+ | 30,983 |
| `FS` (1 of 2 LGPL, the other Apache-2.0) | LGPL-2.1+ | 158,148 (includes the Apache-2.0 vfs code) |
| `SPI`, `Wire` | LGPL-2.1+ | 1,048 / 6,649 |
| `AsyncUDP`, `DNSServer` (no header; counted as LGPL, the conservative reading) | package default LGPL-2.1+ | 3,547 / 2,722 |

So **nine groups** of Arduino-ESP32 are statically linked under the LGPL. Other linked Arduino-ESP32 libraries are not LGPL by their
headers: `Preferences`, `Update`, `SD`, `LittleFS`, `Hash` (Apache-2.0) and `ESPmDNS` (MIT).

Not LGPL, also in the image: LVGL 9.5.0 (MIT), NimBLE-Arduino 2.5.2 (Apache-2.0), ArduinoJson (header-only, MIT), Espressif's prebuilt
ESP-IDF 5.5.5 libraries (Apache-2.0, 71 archives with bytes in the image; the check found no GPL or LGPL file in the 58 whose component
sources are in the ESP-IDF package; the `wpa_supplicant` files carry the old BSD/GPL-2 dual header and are used under BSD), the Wi-Fi/PHY/coexistence/Bluetooth radio
binaries (Espressif, "Apache-2.0" per the READMEs of their folders, no sources), and the toolchain runtime (libgcc/libstdc++:
GPL-3.0-or-later with the GCC Runtime Library Exception; newlib). **No GPL-only component is linked.**

## What LGPL-2.1 needs for a statically linked image (our reading)

- Section 6: the combined work may be distributed on terms of our choice **provided** those terms permit modification of the work for the
  recipient's own use and reverse engineering for debugging such modifications, and we **carry the Library's notice and licence text**, and
  give the recipient either (a) the Library's complete corresponding source **and** what is needed to relink (our source or objects), or (c) a
  written offer valid three years. We do (a) and (c): the source archive and the offer.
- Sections 4/5: the Library may be linked into a work; it stays under the LGPL. The licence page lists the groups above.

## What we publish for a release (nothing is published by the tooling)

`tools/lgpl-package.sh <tag>` writes these files; they go next to the images (the same release page / `releases/<ver>/` folder of the flasher):

| File | What |
|---|---|
| `twiddle-s3-<ver>-source.tar.gz` | our source at the tag (git archive; no secrets), the build files, `REBUILD.md`, `WRITTEN-OFFER.txt`, `PINNED-VERSIONS.txt` |
| `ADDITIONAL-PERMISSION.txt` | the additional-permission paragraph (also appended to `WRITTEN-OFFER.txt`, also in the source archive; same text as `LICENSE-NOTES.md`) |
| `REBUILD.md`, `WRITTEN-OFFER.txt`, `PINNED-VERSIONS.txt` | the same three, loose |
| `arduino-esp32-<fw>.tar.xz`, `...-libs.tar.xz` (`--mirror`) | mirror of the upstream archives the pinned platform downloads, in case the upstream URL goes away |
| `licenses-guition-jc8048w550c/` | the licence page with the full texts (LGPL-2.1, Apache-2.0, MIT, GCC exception ...) for the flasher's `licenses/guition-jc8048w550c/<ver>/` |
| `SHA256SUMS`, `lgpl-assets.json` | checksums; the list the licence page shows |

Rebuild and relink: `pio run -e panel-release` from the archive reproduces the image (checked: the unpacked archive builds with the same flash
size). Relinking against a modified Arduino-ESP32 is `REBUILD.md` section 2; a `file://` framework override compiled our modified core
into `libFrameworkArduino.a` in a test. The end-to-end flash of such an image was not tried. A separate `PLATFORMIO_CORE_DIR` is advised
because PlatformIO replaces the installed package of that name (this overwrote the shared copy once during testing).

Written offer: valid at least three years from each release and while the version is offered. The contact is `OFFER_CONTACT`; unset, it
defaults to the public issues page https://github.com/driftsyncone/twiddle/issues (the maintainer must confirm this is the contact they
want a three-year offer to point at). The script refuses an empty `OFFER_CONTACT`.

## Release checklist (S3)

1. Real `FW_VERSION` (no `-dev`), tag it, `pio run -e panel-release`, `tools/dist.sh`.
2. `OFFER_CONTACT=<confirmed contact> tools/lgpl-package.sh <tag> --mirror --verify-build --flasher ../twiddle-flasher`
3. Flash the tested image on a board; write `tested.json` (the flasher refuses an untested one).
4. Put the images, the LGPL files and `licenses-guition-jc8048w550c/` into the flasher's `releases/<ver>/`; `scripts/assemble.sh`.
5. Re-read the licence page against `components.json`; if `gen_notices_pio.py --check` printed anything new, a human decides first.

## Open legal questions (need a light legal read)

1. **LGPL-2.1 section 6 vs CC BY-NC-SA 4.0 on our code.** Section 6 asks that the terms for the combined work "permit modification of the
   work for the customer's own use and reverse engineering for debugging such modifications". NC-SA 4.0 permits modification and sharing
   (non-commercially) and does not forbid reverse engineering, and our source is public, but the NonCommercial condition might be argued
   to restrict a recipient whose "own use" is commercial, and CC's own FAQ discourages CC licences for software. Chosen approach: keep
   CC BY-NC-SA 4.0 and add the explicit additional permission (modify, relink, run and reverse engineer for the recipient's own use;
   no distribution or commercial exploitation of Twiddle code). Still for a lawyer: whether this wording is enough, and whether a CC
   licence can carry such an additional permission.
2. Whether the statements "Apache-2.0" in the folders of Espressif's prebuilt radio libraries cover those binaries (no sources exist to
   check); verified only that the LICENSE files are the plain Apache-2.0 text and the READMEs say so.
3. The 58 scanned ESP-IDF components were checked by source header (SPDX/text) over the whole component folder, not per linked object; the
   9 radio/Bluetooth/coexistence binaries have the statement above; the 4 Espressif registry components (mdns, network_provisioning,
   esp_diagnostics, cjson) have declared, not locally verified, licences (their sources are not in the ESP-IDF package).
4. Whether the "no header, counted as LGPL" files (`AsyncUDP`, `DNSServer`, parts of `WebServer`) are LGPL: the package says so; each file
   does not.
