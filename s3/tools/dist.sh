#!/bin/sh
# Copy the built release image to dist/ under names that say which board and version it is for, with checksums, and emit the
# parts a web flasher needs plus a parts manifest.
#   tools/dist.sh     -> dist/twiddle-s3-<FW_VERSION>.bin (+ .sha256)         the app, also what ota.sh uploads
#                        dist/twiddle-s3-<FW_VERSION>-bootloader.bin           flashed at 0x0 (ESP32-S3)
#                        dist/twiddle-s3-<FW_VERSION>-partition-table.bin      0x8000
#                        dist/twiddle-s3-<FW_VERSION>-boot_app0.bin            the otadata image (0xe000): selects ota_0
#                        dist/twiddle-s3-<FW_VERSION>.parts.json               offsets (decimal), sizes, sha256, flash settings
# Offsets and files are NOT guessed: they come from BUILD_DIR/flash-layout.json, which tools/pio_flash_layout.py writes from
# PlatformIO's own FLASH_EXTRA_IMAGES / ESP32_APP_OFFSET at build time, and they are cross-checked against the partition table
# in the build (otadata offset == boot_app0 offset, first ota app == app offset, the app fits its slot).
# The twiddle-flasher repo's gen_site.py reads the .parts.json; the offsets are checked there against its boards.json.
# Refuses an image that fails tools/check-release-image.sh and one that does not carry this FW_VERSION.
# Build first:  pio run -e panel-release
# Environment: BUILD_DIR (default .pio/build/panel-release), DIST_DIR (default dist), BOARD_ID (default guition-jc8048w550c),
#              DIST_ALLOW_DEV=1 (skip the release-image check: a private dev copy, never for the web flasher).
set -e
cd "$(dirname "$0")/.."
ver=$(sed -n 's/^#define FW_VERSION *"\(.*\)".*/\1/p' include/version.h)
[ -n "$ver" ] || { echo "No FW_VERSION in include/version.h." >&2; exit 1; }
build=${BUILD_DIR:-.pio/build/panel-release}
dist=${DIST_DIR:-dist}
board=${BOARD_ID:-guition-jc8048w550c}
fa="$build/flash-layout.json"
[ -f "$build/firmware.bin" ] || { echo "No $build/firmware.bin: run pio run -e panel-release first." >&2; exit 1; }
[ -f "$fa" ] || { echo "No $fa: build with env panel-release (it runs tools/pio_flash_layout.py)." >&2; exit 1; }
command -v python3 >/dev/null || { echo "python3 is needed to read $fa." >&2; exit 1; }
grep -qa -F "$ver" "$build/firmware.bin" || { echo "$build/firmware.bin does not contain version $ver: rebuild (pio run -e panel-release)." >&2; exit 1; }
# A published image must be a release build: no Wi-Fi, token, web password or home-network string inside.
if [ "${DIST_ALLOW_DEV:-0}" != 1 ] && [ -x tools/check-release-image.sh ]; then
  tools/check-release-image.sh --build-dir "$build" || { echo "Not a release image: build with pio run -e panel-release (see tools/check-release-image.sh)." >&2; exit 1; }
fi
mkdir -p "$dist"
python3 -I - "$build" "$dist" "$ver" "$board" <<'PY'
import hashlib, json, os, shutil, struct, sys
build, dist, ver, board = sys.argv[1:5]
fa = json.load(open(os.path.join(build, "flash-layout.json")))
chips = {"esp32s3": "ESP32-S3", "esp32p4": "ESP32-P4", "esp32": "ESP32", "esp32c3": "ESP32-C3"}
chip = chips.get(fa["mcu"])
if chip is None:
    sys.exit("unknown mcu %r in flash-layout.json" % fa["mcu"])
# PlatformIO's image names -> the part names the flasher's boards.json uses
NAMES = {"bootloader.bin": "bootloader", "partitions.bin": "partition-table", "boot_app0.bin": "boot_app0", "firmware.bin": "app"}
parts = []
for im in fa["images"]:
    base = os.path.basename(im["file"])
    if base not in NAMES:
        sys.exit("flash-layout.json has an image this script does not know: %s (a new image must not be dropped silently)" % base)
    name = NAMES[base]
    if not os.path.isfile(im["file"]):
        sys.exit("missing build output: " + im["file"])
    fn = "twiddle-s3-%s.bin" % ver if name == "app" else "twiddle-s3-%s-%s.bin" % (ver, name)
    shutil.copyfile(im["file"], os.path.join(dist, fn))
    data = open(os.path.join(dist, fn), "rb").read()
    parts.append({"name": name, "offset": int(im["offset"], 16), "file": fn, "size": len(data), "sha256": hashlib.sha256(data).hexdigest()})
want = sorted(NAMES.values())
if sorted(p["name"] for p in parts) != want:
    sys.exit("expected parts %s, got %s" % (want, sorted(p["name"] for p in parts)))
by = {p["name"]: p for p in parts}

# cross-check against the partition table that is in the build (gen_esp32part.py format: 32-byte entries, magic AA 50)
pt = open(os.path.join(dist, by["partition-table"]["file"]), "rb").read()
ents = []
for i in range(0, min(len(pt), 0xC00), 32):
    e = pt[i:i + 32]
    if e[:2] != b"\xaa\x50":
        break
    typ, sub, off, size, label, flags = struct.unpack("<BBII16sI", e[2:])
    ents.append((typ, sub, off, size, label.split(b"\0")[0].decode()))
otadata = [e for e in ents if e[0] == 1 and e[1] == 0]
ota0 = [e for e in ents if e[0] == 0 and e[1] == 0x10]
if len(otadata) != 1 or len(ota0) != 1:
    sys.exit("partition table: expected one otadata and one ota_0 entry, found %d and %d" % (len(otadata), len(ota0)))
if otadata[0][2] != by["boot_app0"]["offset"]:
    sys.exit("boot_app0 is at 0x%x but the partition table puts otadata at 0x%x" % (by["boot_app0"]["offset"], otadata[0][2]))
if ota0[0][2] != by["app"]["offset"]:
    sys.exit("app is at 0x%x but the partition table puts ota_0 at 0x%x" % (by["app"]["offset"], ota0[0][2]))
if by["app"]["size"] > ota0[0][3]:
    sys.exit("app is %d bytes, larger than its %d-byte slot" % (by["app"]["size"], ota0[0][3]))
if by["boot_app0"]["size"] > otadata[0][3]:
    sys.exit("boot_app0 image is larger than the otadata partition")
# the image header (byte 2 = flash mode, byte 3 = size nibble + frequency nibble) is what the chip uses; record it too
hdr = open(os.path.join(dist, by["bootloader"]["file"]), "rb").read(4)
modes = {0: "qio", 1: "qout", 2: "dio", 3: "dout"}
sizes = {0: "1MB", 1: "2MB", 2: "4MB", 3: "8MB", 4: "16MB", 5: "32MB"}
freqs = {0xF: "80m", 0: "40m", 1: "26m", 2: "20m"}
flash = {"flash_mode": modes.get(hdr[2], "?"), "flash_size": sizes.get(hdr[3] >> 4, "?"), "flash_freq": freqs.get(hdr[3] & 0xF, "?")}
if "?" in flash.values():
    sys.exit("cannot read the flash settings from the bootloader header %s" % hdr.hex())
if fa.get("flash_size") and fa["flash_size"] != flash["flash_size"]:
    sys.exit("flash size: PlatformIO says %s, the bootloader header says %s" % (fa["flash_size"], flash["flash_size"]))
parts.sort(key=lambda p: p["offset"])
out = os.path.join(dist, "twiddle-s3-%s.parts.json" % ver)
with open(out, "w") as f:
    json.dump({"schema": 1, "board": board, "version": ver, "chipFamily": chip, "flash": flash, "parts": parts}, f, indent=2)
    f.write("\n")
PY
app="$dist/twiddle-s3-$ver.bin"
( cd "$dist" && sha256sum "$(basename "$app")" > "$(basename "$app").sha256" )
echo "$app"; cat "$app.sha256"
echo "$dist/twiddle-s3-$ver.parts.json"
