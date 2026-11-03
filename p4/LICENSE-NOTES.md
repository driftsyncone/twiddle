# Licence notes (DRAFT, not read by a lawyer)

This page summarises the licensing of this repository and its firmware images. It does not replace or change the text of
[LICENSE](LICENSE). It is documentation, not legal advice.

## Twiddle's own code

The code written for Twiddle (everything in this repository except the third-party material named below) is licensed under
**CC BY-NC-SA 4.0**, unchanged: see [LICENSE](LICENSE).

## Third-party components

A firmware image also contains third-party software that keeps its own licence (ESP-IDF, esp-hosted, LVGL, esp-nimble-cpp, ArduinoJson,
mbedTLS, lwIP, FreeRTOS, newlib, the toolchain runtime, the Montserrat fonts and others). Nothing in CC BY-NC-SA 4.0 limits the rights
those licences give you.

- [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md): code and protocol knowledge that is in this source tree and the components downloaded at
  build time, with the licence texts.
- The generated notices page for each released version, published with the web flasher at `licenses/<board>/<version>/`: the exact list
  of components linked into that image (from the linker map), with the full licence texts.

## Additional permission (LGPL components)

The ESP32-P4 image built from this repository links no LGPL component (checked for 2.0.0-p4.13 by `scripts/gen_notices.py --check` in the
twiddle-flasher repository, which fails when a GPL or LGPL component is linked). The additional permission below is therefore not needed for
the P4 image. It is repeated here only so that this repository and the ESP32-S3 repository carry the same text; it applies to the
ESP32-S3 firmware, which statically links the Arduino-ESP32 LGPL components:

> DRAFT (not read by a lawyer). Additional permission for the Arduino-ESP32 LGPL components. The Twiddle firmware source is licensed under CC BY-NC-SA 4.0. Firmware images for the ESP32-S3 statically link the Arduino core for ESP32 and several of its libraries, which are licensed under the GNU Lesser General Public License, version 2.1 or later; they are not covered by the Twiddle licence and remain under the LGPL, whose text is included with every release. Notwithstanding the NonCommercial condition, the copyright holder additionally permits every recipient of a Twiddle firmware image or its source, for the sole purposes of exercising rights under section 6 of the LGPL-2.1, to modify the combined firmware, to replace the LGPL components with modified versions and relink, to run the result, and to reverse engineer it for debugging those modifications, for the recipient's own use. This permission does not allow distribution or commercial exploitation of Twiddle code. The complete corresponding source, the pinned build instructions and a written offer valid for at least three years are in the release (`twiddle-s3-<ver>-source.tar.gz`, REBUILD.md, WRITTEN-OFFER.txt) and obtainable from https://github.com/driftsyncone/twiddle/issues.

## Disclaimer

Provided as is, without warranty; see [README.md](README.md#disclaimer-and-safety). Vendor and product names are used only to describe
compatibility; there is no affiliation.
