#!/bin/bash
# Build and run the offline host tests (g++, no IDF needed): the toy encoders, then the heart-rate
# edge controller + 0x2A37 decoder, then the password generator and display-variant fallback.
# Usage: test/host/run.sh   (needs managed_components/ from one idf.py build, for ArduinoJson;
#        $TMPDIR, default /tmp, receives the test binaries)
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
tmp="${TMPDIR:-/tmp}"
out="$tmp/toys_test"
g++ -std=c++17 -O0 -g -Wall -Wno-unused-function -Wno-unused-variable \
  -I"$here/mock" -I"$root/main" -I"$root/managed_components/bblanchon__arduinojson/src" \
  "$here/toys_test.cpp" "$root/main/toys/toys.cpp" "$root/main/toys/lovense.cpp" \
  "$root/main/toys/kiiroo.cpp" "$root/main/toys/ossm.cpp" "$root/main/toys/hrm.cpp" -o "$out"
"$out"
# edge.cpp / hrmparse.cpp include nothing from ESP, LVGL or Arduino: no mocks needed.
g++ -std=c++17 -O0 -g -Wall -Wextra -I"$root/main" \
  "$here/edge_test.cpp" "$root/main/edge.cpp" "$root/main/hrmparse.cpp" -o "$tmp/edge_test"
"$tmp/edge_test"
# pwgen.h / panelfb.h are header-only and include nothing from ESP, LVGL or Arduino.
g++ -std=c++17 -O0 -g -Wall -Wextra -I"$root/main" "$here/release_test.cpp" -o "$tmp/release_test"
"$tmp/release_test"
# devrow.h: which buttons a Devices row shows (header-only, pure logic).
g++ -std=c++17 -O0 -g -Wall -Wextra -I"$root/main" "$here/devrow_test.cpp" -o "$tmp/devrow_test"
"$tmp/devrow_test"
# apwindow.h: the Autopilot graph window math (header-only, pure logic).
g++ -std=c++17 -O0 -g -Wall -Wextra -I"$root/main" "$here/apwindow_test.cpp" -o "$tmp/apwindow_test"
"$tmp/apwindow_test"
# The web password must never reach a web page or a log: only the Settings tab (ui.cpp) may read it.
if grep -n 'otapw::get()' "$root"/main/*.cpp "$root"/main/toys/*.cpp | grep -v '/ui.cpp:' | grep -v '^[^:]*:[0-9]*:\s*//'; then
  echo "FAIL: otapw::get() used outside ui.cpp" >&2; exit 1
fi
echo "otapw::get() only in ui.cpp: ok"
