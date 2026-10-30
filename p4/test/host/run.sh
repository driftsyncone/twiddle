#!/bin/bash
# Build and run the offline toy-encoder test on the host (g++, no IDF needed).
# Usage: test/host/run.sh   (needs managed_components/ from one idf.py build, for ArduinoJson;
#        $TMPDIR, default /tmp, receives the test binary)
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${TMPDIR:-/tmp}/toys_test"
g++ -std=c++17 -O0 -g -Wall -Wno-unused-function -Wno-unused-variable \
  -I"$here/mock" -I"$root/main" -I"$root/managed_components/bblanchon__arduinojson/src" \
  "$here/toys_test.cpp" "$root/main/toys/toys.cpp" "$root/main/toys/lovense.cpp" \
  "$root/main/toys/kiiroo.cpp" "$root/main/toys/ossm.cpp" -o "$out"
"$out"
