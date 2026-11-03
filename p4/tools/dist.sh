#!/bin/sh
# Copy the built image to dist/ under a name that says which board and version it is for, with its checksum,
# and also emit the bootloader and partition-table parts a web flasher needs, plus a parts manifest.
#   tools/dist.sh            -> dist/twiddle-p4-<FW_VERSION>.bin (+ .sha256)          the app, also what ota.sh uploads
#                               dist/twiddle-p4-<FW_VERSION>-bootloader.bin
#                               dist/twiddle-p4-<FW_VERSION>-partition-table.bin
#                               dist/twiddle-p4-<FW_VERSION>-otadata.bin
#                               dist/twiddle-p4-<FW_VERSION>.parts.json               offsets (decimal), sizes, sha256
# Offsets and file names come from build/flasher_args.json (what idf.py flash uses), not from constants.
# The twiddle-flasher repo's gen_site.py reads the .parts.json; the offsets are checked there against its boards.json.
# Artefacts for different boards must never share a name: see docs/flashing.md.
# Refuses an image that fails tools/check-release-image.sh (build with -DTWIDDLE_RELEASE=ON).
# Environment: BUILD_DIR (default build), DIST_DIR (default dist), BOARD_ID (default guition-jc1060p470c), DIST_ALLOW_DEV=1.
set -e
cd "$(dirname "$0")/.."
ver=$(sed -n 's/^#define FW_VERSION *"\(.*\)".*/\1/p' main/version.h)
[ -n "$ver" ] || { echo "No FW_VERSION in main/version.h." >&2; exit 1; }
build=${BUILD_DIR:-build}
dist=${DIST_DIR:-dist}
board=${BOARD_ID:-guition-jc1060p470c}
src="$build/twiddle.bin"
fa="$build/flasher_args.json"
[ -f "$src" ] || { echo "No $src: run idf.py build first." >&2; exit 1; }
[ -f "$fa" ] || { echo "No $fa: run idf.py build first." >&2; exit 1; }
command -v python3 >/dev/null || { echo "python3 is needed to read $fa." >&2; exit 1; }
# A published image must be a release build: no Wi-Fi, token, web password or home-network string inside.
# (DIST_ALLOW_DEV=1 skips this for a private dev copy; never for anything that goes to the web flasher.)
if [ "${DIST_ALLOW_DEV:-0}" != 1 ] && [ -x tools/check-release-image.sh ]; then
  tools/check-release-image.sh --build-dir "$build" || { echo "Not a release image: build with idf.py -DTWIDDLE_RELEASE=ON (see tools/check-release-image.sh)." >&2; exit 1; }
fi
mkdir -p "$dist"
out="$dist/twiddle-p4-$ver.bin"
cp "$src" "$out"
( cd "$dist" && sha256sum "$(basename "$out")" > "$(basename "$out").sha256" )
python3 - "$build" "$dist" "$ver" "$board" <<'PY'
import hashlib, json, os, shutil, sys
build, dist, ver, board = sys.argv[1:5]
fa = json.load(open(os.path.join(build, "flasher_args.json")))
chips = {"esp32p4": "ESP32-P4", "esp32s3": "ESP32-S3"}
chip = chips.get(fa["extra_esptool_args"]["chip"])
if chip is None:
    sys.exit("unknown chip %r in flasher_args.json" % fa["extra_esptool_args"]["chip"])
parts = []
for name in ("bootloader", "partition-table", "otadata", "app"):
    if name not in fa:
        sys.exit("flasher_args.json has no %r entry" % name)
    offset = int(fa[name]["offset"], 16)
    srcfile = os.path.join(build, fa[name]["file"])
    if not os.path.isfile(srcfile):
        sys.exit("missing build output: " + srcfile)
    # the app keeps its OTA name (twiddle-p4-<ver>.bin); the other parts get a -<part> suffix
    fn = "twiddle-p4-%s.bin" % ver if name == "app" else "twiddle-p4-%s-%s.bin" % (ver, name)
    dst = os.path.join(dist, fn)
    if name != "app":
        shutil.copyfile(srcfile, dst)
    data = open(dst, "rb").read()
    parts.append({"name": name, "offset": offset, "file": fn, "size": len(data), "sha256": hashlib.sha256(data).hexdigest()})
# every flash_files entry must be one of the parts we emit: a new image in the build must not be dropped silently
listed = {int(k, 16) for k in fa["flash_files"]}
if listed != {p["offset"] for p in parts}:
    sys.exit("flasher_args.json flash_files %s differ from emitted parts %s" % (sorted(listed), sorted(p["offset"] for p in parts)))
json.dump({"schema": 1, "board": board, "version": ver, "chipFamily": chip, "flash": fa["flash_settings"], "parts": parts},
          open(os.path.join(dist, "twiddle-p4-%s.parts.json" % ver), "w"), indent=2)
open(os.path.join(dist, "twiddle-p4-%s.parts.json" % ver), "a").write("\n")
PY
echo "$out"; cat "$out.sha256"
echo "$dist/twiddle-p4-$ver.parts.json"
