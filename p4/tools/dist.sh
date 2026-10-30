#!/bin/sh
# Copy the built image to dist/ under a name that says which board and version it is for, with its checksum.
#   tools/dist.sh            -> dist/twiddle-p4-<FW_VERSION>.bin (+ .sha256)
# Artefacts for different boards must never share a name: see docs/flashing.md.
set -e
cd "$(dirname "$0")/.."
ver=$(sed -n 's/^#define FW_VERSION *"\(.*\)".*/\1/p' main/version.h)
src=build/twiddle.bin
[ -f "$src" ] || { echo "No $src: run idf.py build first." >&2; exit 1; }
mkdir -p dist
out="dist/twiddle-p4-$ver.bin"
cp "$src" "$out"
( cd dist && sha256sum "$(basename "$out")" > "$(basename "$out").sha256" )
echo "$out"; cat "$out.sha256"
