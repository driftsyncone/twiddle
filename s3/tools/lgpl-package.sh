#!/bin/bash
# lgpl-package.sh: build the LGPL-2.1 "corresponding source" package for a tagged ESP32-S3 release. Run at release time. PUBLISHES NOTHING:
# it only writes files into a directory; what to upload where is the maintainer's decision (docs/lgpl-compliance.md).
#
#   tools/lgpl-package.sh TAG [--out DIR] [--flasher DIR] [--mirror] [--verify-build] [--allow-dev]
#
# TAG            git tag (or any commit-ish) of the release. Its FW_VERSION (include/version.h) names the files. A "-dev" version is
#                refused unless --allow-dev (dry runs).
# --out DIR      default lgpl-package (gitignored); files go to DIR/<version>/
# --flasher DIR  the twiddle-flasher checkout: also writes the licence page (scripts/gen_notices_pio.py) from the build in this work tree.
#                Needs the work tree to be at TAG and `pio run -e panel-release` to have been run.
# --mirror       also download the pinned Arduino-ESP32 source archives (belt and braces against the upstream URL disappearing)
# --verify-build unpack the source archive into a temporary directory and build env panel-release there (proves the archive is complete)
#
# Writes, in DIR/<version>/:
#   twiddle-s3-<ver>-source.tar.gz   our source at the tag (git archive; no secrets: include/secrets.h is gitignored, the private
#                                    lab-specific check script and unpublished case drawings are left out) + REBUILD.md +
#                                    WRITTEN-OFFER.txt + ADDITIONAL-PERMISSION.txt + PINNED-VERSIONS.txt (from `pio pkg list`)
#   REBUILD.md, WRITTEN-OFFER.txt, ADDITIONAL-PERMISSION.txt, PINNED-VERSIONS.txt    the same four files, loose
#   arduino-esp32-<ver>.tar.xz, ...-libs.tar.xz           (with --mirror) the upstream archives the pinned platform downloads
#   licenses-guition-jc8048w550c/                         (with --flasher) the licence page for the flasher's licenses/ folder
#   lgpl-assets.json                 the asset list the licence page shows
#   SHA256SUMS
# Environment: OFFER_CONTACT = where a written-offer request goes (e-mail or issue tracker URL). Default (when unset):
# https://github.com/driftsyncone/twiddle/issues . Set but empty is an error. The user must confirm the contact before a release,
# because the 3-year written offer points there.
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$here"
tag=${1:-}; [ -n "$tag" ] || { sed -n '2,25p' "${BASH_SOURCE[0]}"; exit 2; }
shift
out=lgpl-package; flasher=""; mirror=0; verify=0; allow_dev=0
while [ $# -gt 0 ]; do
  case "$1" in
    --out) out=${2:?}; shift 2 ;;
    --flasher) flasher=${2:?}; shift 2 ;;
    --mirror) mirror=1; shift ;;
    --verify-build) verify=1; shift ;;
    --allow-dev) allow_dev=1; shift ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
  esac
done
PIO=${PIO:-pio}
command -v "$PIO" >/dev/null || PIO=~/.local/share/uv/tools/platformio/bin/pio
commit=$(git rev-parse --verify "$tag^{commit}") || { echo "no such tag/commit: $tag" >&2; exit 1; }
ver=$(git show "$commit:include/version.h" | sed -n 's/^#define FW_VERSION *"\(.*\)".*/\1/p')
[ -n "$ver" ] || { echo "no FW_VERSION in include/version.h at $tag" >&2; exit 1; }
case "$ver" in *-dev*) [ $allow_dev = 1 ] || { echo "$ver is a development version; a release needs a real version (or pass --allow-dev for a dry run)." >&2; exit 1; } ;; esac
dest=$(realpath -m "$out/$ver"); mkdir -p "$dest"
name="twiddle-s3-$ver"
work=$(mktemp -d); trap 'rm -rf "$work"' EXIT
echo "lgpl-package: $tag = $(git rev-parse --short "$commit"), version $ver -> $dest"

# --- the exact versions the pinned platform resolves (needs the packages installed: run `pio run -e panel-release` once) ---
{
  echo "# Versions this release was built with (output of: pio pkg list -e panel-release), from $(git rev-parse --short "$commit")"
  echo "# Rebuild with exactly these; platformio.ini pins the platform by URL and the libraries by exact version in env:panel-release."
  "$PIO" pkg list -e panel-release 2>/dev/null | grep -v '^Resolving' | sed -E "s#(/home|/Users)/[^/]+/#~/#g" || echo "(pio pkg list failed: install the packages with 'pio run -e panel-release' and run this script again)"
} > "$dest/PINNED-VERSIONS.txt"
grep -q "framework-arduinoespressif32 @" "$dest/PINNED-VERSIONS.txt" || { echo "PINNED-VERSIONS.txt has no framework line: packages not installed?" >&2; exit 1; }
fwver=$(sed -n 's/.*framework-arduinoespressif32 @ \([0-9.]*\) .*/\1/p' "$dest/PINNED-VERSIONS.txt" | head -1)
fwurl=$(sed -n 's/.*framework-arduinoespressif32 @ [^ ]* (required: \(.*\))/\1/p' "$dest/PINNED-VERSIONS.txt" | head -1)
libsurl=$(sed -n 's/.*framework-arduinoespressif32-libs @ [^ ]* (required: \(.*\))/\1/p' "$dest/PINNED-VERSIONS.txt" | head -1)

# --- written offer (LGPL-2.1 section 6c) ---
contact=${OFFER_CONTACT-https://github.com/driftsyncone/twiddle/issues}
[ -n "$contact" ] || { echo "OFFER_CONTACT is empty: the written offer needs a contact (unset it to use the default issues URL)." >&2; exit 1; }

# --- additional permission paragraph (DRAFT wording, same text as LICENSE-NOTES.md; not read by a lawyer) ---
perm=$(cat <<'PERM'
DRAFT (not read by a lawyer). Additional permission for the Arduino-ESP32 LGPL components. The Twiddle firmware source is licensed under
CC BY-NC-SA 4.0. Firmware images for the ESP32-S3 statically link the Arduino core for ESP32 and several of its libraries, which are
licensed under the GNU Lesser General Public License, version 2.1 or later; they are not covered by the Twiddle licence and remain under
the LGPL, whose text is included with every release. Notwithstanding the NonCommercial condition, the copyright holder additionally
permits every recipient of a Twiddle firmware image or its source, for the sole purposes of exercising rights under section 6 of the
LGPL-2.1, to modify the combined firmware, to replace the LGPL components with modified versions and relink, to run the result, and to
reverse engineer it for debugging those modifications, for the recipient's own use. This permission does not allow distribution or
commercial exploitation of Twiddle code. The complete corresponding source, the pinned build instructions and a written offer valid for
at least three years are in the release (@NAME@-source.tar.gz, REBUILD.md, WRITTEN-OFFER.txt) and obtainable from @CONTACT@.
PERM
)
perm=${perm//@NAME@/$name}; perm=${perm//@CONTACT@/$contact}
printf '%s\n' "$perm" > "$dest/ADDITIONAL-PERMISSION.txt"
cat > "$dest/WRITTEN-OFFER.txt" <<OFFER
Written offer for source code (DRAFT wording, not reviewed by a lawyer)

The firmware image $name (ESP32-S3, Guition JC8048W550C) contains the Arduino core for the ESP32 and some of its bundled libraries
("Arduino-ESP32" $fwver), which are licensed under the GNU Lesser General Public License, version 2.1 or later (LGPL-2.1+). They are linked
statically into the image. The licence page for this version lists exactly which parts, and carries the licence text.

You may modify those libraries and the Twiddle source code and rebuild and relink the image for your own use, and you may reverse
engineer the firmware to debug such modifications. The complete corresponding source code, the build instructions and the exact versions
of the build environment are published with this release:
  $name-source.tar.gz   (REBUILD.md inside explains how to rebuild and how to relink against a modified Arduino-ESP32)

If you cannot get these files from the place where you got the firmware, we will give you the same materials, on a physical medium or
by download, for a charge no more than the cost of performing the distribution. This offer is valid for at least three years from the
date of this release, and for as long as we offer this firmware version for download. Request them from:
  $contact

Twiddle's own code is licensed under CC BY-NC-SA 4.0 (see LICENSE in the archive). That licence covers only code written for Twiddle.
Nothing in it restricts the rights the LGPL gives you for the LGPL-licensed parts.

OFFER
printf '%s\n' "$perm" >> "$dest/WRITTEN-OFFER.txt"

# --- rebuild instructions ---
cat > "$dest/REBUILD.md" <<REBUILD
# Rebuilding Twiddle $ver (ESP32-S3) and relinking it against a modified Arduino-ESP32

This archive holds the complete source of the Twiddle firmware at release $ver and its build files. The Arduino-ESP32 libraries that the
image links statically (LGPL-2.1+) are not copied into it: PlatformIO downloads the exact version named below, and you can replace it
with your own modified copy.

## 1. Rebuild the released image

1. Install [PlatformIO Core](https://platformio.org/install/cli) (the release was built with PlatformIO Core 6.x).
2. In this directory run:  \`pio run -e panel-release\`
   - The platform is pinned in \`platformio.ini\` (pioarduino platform-espressif32 55.03.311), which pins Arduino-ESP32 $fwver
     ($fwurl), its prebuilt ESP-IDF libraries ($libsurl), ESP-IDF 5.5.5 and the Xtensa toolchain. The libraries from \`lib_deps\`
     are pinned to exact versions in env \`panel-release\`. \`PINNED-VERSIONS.txt\` lists what the release was built with.
   - No \`include/secrets.h\` is needed: the release env does not read it.
3. The images are in \`.pio/build/panel-release/\`: \`bootloader.bin\` (flash at 0x0), \`partitions.bin\` (0x8000), \`firmware.bin\` (0x10000) and
   boot_app0.bin from the platform (0xe000). \`tools/dist.sh\` names them for the web flasher. The build embeds the compile time, so the
   file is not byte-identical to the published one; the code is.

## 2. Relink against a modified Arduino-ESP32

1. Get the Arduino-ESP32 source the release used: the folder PlatformIO installed (\`~/.platformio/packages/framework-arduinoespressif32\`,
   package version $fwver), or unpack $fwurl.
2. Copy that folder somewhere, for example \`/work/arduino-esp32-mod\`, and make your changes in \`cores/\` and \`libraries/\`.
3. Point the build at your copy. **PlatformIO installs a \`file://\` package into its packages folder, replacing the installed package of the same
   name**, so do this in a separate PlatformIO core directory, not in the one you use for other projects:

       export PLATFORMIO_CORE_DIR=/work/pio-core      # everything is downloaded again into here (a few GB)

   Add to \`platformio.ini\`, in \`[env:panel-release]\`:

       platform_packages =
         framework-arduinoespressif32 @ file:///work/arduino-esp32-mod

   then run \`pio run -e panel-release\`. The Arduino core and libraries are compiled from your copy and linked with the Twiddle objects
   into a new \`firmware.bin\`. (Checked by the maintainers: with such an override PlatformIO compiled the modified core into
   \`libFrameworkArduino.a\`. Changes to the prebuilt ESP-IDF libraries, \`framework-arduinoespressif32-libs\`, are a different matter: they are
   Espressif's, mostly Apache-2.0, built from ESP-IDF 5.5.5.)
4. Flash \`firmware.bin\` to your own board (for example \`pio run -e panel-release -t upload\`). On the web flasher's board entry, use New
   install only for a board you are willing to erase.

You may also change the Twiddle sources (\`src/\`) and rebuild; see \`LICENSE\` (CC BY-NC-SA 4.0) for the terms that apply to those.
REBUILD

# --- source archive ---
mkdir -p "$work/$name"
git archive --format=tar "$commit" -- . ':(exclude)case' ':(exclude)tools/check-release-image.sh' ':(exclude).publicignore' | tar -x -C "$work/$name"
cp "$dest/REBUILD.md" "$dest/WRITTEN-OFFER.txt" "$dest/ADDITIONAL-PERMISSION.txt" "$dest/PINNED-VERSIONS.txt" "$work/$name/"
echo "$commit" > "$work/$name/SOURCE-COMMIT"
epoch=$(git show -s --format=%ct "$commit")
( cd "$work" && tar --sort=name --mtime="@$epoch" --owner=0 --group=0 --numeric-owner -cf - "$name" | gzip -n -9 > "$dest/$name-source.tar.gz" )
# nothing secret may be in the archive
if tar -tzf "$dest/$name-source.tar.gz" | grep -E '(^|/)secrets\.h$'; then echo "secrets.h is in the archive" >&2; exit 1; fi

assets='[["'$name'-source.tar.gz","our source at this release, the build files, REBUILD.md and the written offer"],["REBUILD.md","how to rebuild and relink against a modified Arduino-ESP32"],["WRITTEN-OFFER.txt","the written offer (LGPL-2.1 section 6c), with the additional permission"],["ADDITIONAL-PERMISSION.txt","the additional permission for the LGPL components (draft wording)"],["PINNED-VERSIONS.txt","exact versions of the build environment"]'

if [ $mirror = 1 ]; then
  [ -n "$fwurl" ] && [ -n "$libsurl" ] || { echo "cannot read the framework URLs from PINNED-VERSIONS.txt" >&2; exit 1; }
  curl -fL --retry 3 -o "$dest/arduino-esp32-$fwver.tar.xz" "$fwurl"
  curl -fL --retry 3 -o "$dest/arduino-esp32-$fwver-libs.tar.xz" "$libsurl"
  assets="$assets"',["arduino-esp32-'$fwver'.tar.xz","mirror of the Arduino-ESP32 source archive this release was built with"],["arduino-esp32-'$fwver'-libs.tar.xz","mirror of the prebuilt ESP-IDF libraries archive"]'
fi
assets="$assets]"
echo "$assets" > "$dest/lgpl-assets.json"

# --- licence page ---
if [ -n "$flasher" ]; then
  [ "$(git rev-parse HEAD)" = "$commit" ] || { echo "--flasher needs this work tree to be at $tag (it reads the build in .pio/build/panel-release)" >&2; exit 1; }
  [ -f .pio/build/panel-release/firmware.map ] || { echo "no .pio/build/panel-release/firmware.map: run pio run -e panel-release first" >&2; exit 1; }
  "$PIO" run -e panel-release -t compiledb >/dev/null
  mv compile_commands.json "$work/compile_commands.json"
  python3 -I "$flasher/scripts/gen_notices_pio.py" --project "$here" --build .pio/build/panel-release --compdb "$work/compile_commands.json" \
    --version "$ver" --assets "$dest/lgpl-assets.json" --out "$dest/licenses-guition-jc8048w550c" --check
fi

# --- prove the archive is complete ---
if [ $verify = 1 ]; then
  mkdir -p "$work/verify" && tar -xzf "$dest/$name-source.tar.gz" -C "$work/verify"
  ( cd "$work/verify/$name" && "$PIO" run -e panel-release ) >"$work/verify.log" 2>&1 || { tail -30 "$work/verify.log" >&2; echo "the source archive does NOT build" >&2; exit 1; }
  echo "verify-build: the source archive builds env panel-release ($(grep -E 'Flash:' "$work/verify.log" | tr -s ' '))"
fi

( cd "$dest" && find . -maxdepth 1 -type f ! -name SHA256SUMS -printf '%P\n' | sort | xargs sha256sum > SHA256SUMS )
echo "--- release assets to publish next to the images (nothing was uploaded) ---"
( cd "$dest" && ls -l . )
echo "written-offer contact: $contact (set OFFER_CONTACT to change; confirm it before publishing)" >&2
