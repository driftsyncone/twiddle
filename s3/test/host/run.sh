#!/bin/bash
# Host tests (g++ only, no ESP / Arduino / LVGL): the heart-rate edge controller and the 0x2A37 decoder, then the release logic.
# Usage: test/host/run.sh   ($TMPDIR, default /tmp, receives the test binary)
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
tmp="${TMPDIR:-/tmp}"
g++ -std=c++17 -O0 -g -Wall -Wextra -I"$root/src" \
  "$here/edge_test.cpp" "$root/src/edge.cpp" "$root/src/hrmparse.cpp" -o "$tmp/edge_test"
"$tmp/edge_test"
# seesaw.cpp: the rotary-encoder protocol frames, probe, click accumulation and button debounce / short / hold logic.
g++ -std=c++17 -O0 -g -Wall -Wextra -I"$root/src" "$here/seesaw_test.cpp" "$root/src/seesaw.cpp" -o "$tmp/seesaw_test"
"$tmp/seesaw_test"
# setupform.h: what an empty / typed / new-network field does to a stored secret.
g++ -std=c++17 -O0 -g -Wall -Wextra -I"$root/src" "$here/release_test.cpp" -o "$tmp/release_test"
"$tmp/release_test"
# devrow.h: which buttons a Devices row shows (header-only, pure logic).
g++ -std=c++17 -O0 -g -Wall -Wextra -I"$root/src" "$here/devrow_test.cpp" -o "$tmp/devrow_test"
"$tmp/devrow_test"
# apwindow.h: the Autopilot graph window math (header-only, pure logic).
g++ -std=c++17 -O0 -g -Wall -Wextra -I"$root/src" "$here/apwindow_test.cpp" -o "$tmp/apwindow_test"
"$tmp/apwindow_test"
# encscale.h: the encoder's Autopilot scale resets on every new session (header-only, pure logic).
g++ -std=c++17 -O0 -g -Wall -Wextra -I"$root/src" "$here/encscale_test.cpp" -o "$tmp/encscale_test"
"$tmp/encscale_test"
# The web password must never reach a web page or a log: only the Settings tab (ui.cpp) and the one login comparison
# (authenticate("ota", otapw::get()) in ota.cpp) may read it.
if grep -n 'otapw::get()' "$root"/src/*.cpp "$root"/src/toys/*.cpp | grep -v '/ui.cpp:' \
   | grep -v 'ota.cpp:[0-9]*:.*server.authenticate("ota", otapw::get())' | grep -v '^[^:]*:[0-9]*:\s*//'; then
  echo "FAIL: otapw::get() used outside ui.cpp and the login check" >&2; exit 1
fi
echo "otapw::get() only in ui.cpp and the login check: ok"
