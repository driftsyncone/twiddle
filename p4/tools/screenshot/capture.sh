#!/bin/bash
# Screenshots of the panel's tabs, without touching the screen: see screenshot_dump.cpp.
#   tools/screenshot/capture.sh [OUTDIR] [TABS] [THEME] [STEP]
#     TABS like "1,2" (default 0,1,2,3,4; never 5 = Settings)
#     THEME: colour theme index to switch to live before the shots (0 Daylight ... see theme.cpp); default: as saved
#     STEP: take every STEP-th pixel (default 2 = 512x300, ~100 s per tab; 4 = ~25 s, enough for theme checks)
# Builds with the dump file added, flashes over USB, captures the serial output (about 100 s per tab),
# decodes the PNGs, then takes the dump file out again and rebuilds/flashes NOTHING more: reflash the
# normal build afterwards. Needs the ESP-IDF environment (see README) and the board on /dev/ttyACM0.
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
out=${1:-$root/build/shots}
tabs=${2:-0,1,2,3,4}
tabs=${tabs//5/}  # never Settings
theme=${3:--1}
step=${4:-2}
mkdir -p "$out"
cd "$root"
cp tools/screenshot/screenshot_dump.cpp main/zz_screenshot_dump.cpp
trap 'rm -f "$root/main/zz_screenshot_dump.cpp"' EXIT
sed -i "s/#define SHOT_TAB_LIST .*/#define SHOT_TAB_LIST ${tabs//,/, }/" main/zz_screenshot_dump.cpp
sed -i "s/#define SHOT_THEME .*/#define SHOT_THEME $theme/; s/#define SHOT_STEP .*/#define SHOT_STEP $step/" main/zz_screenshot_dump.cpp
idf.py reconfigure build >/dev/null
idf.py -p /dev/ttyACM0 flash 2>&1 | tail -1
count=$(echo "$tabs" | tr ',' '\n' | grep -c .)
timeout $((60 + count * 130 + 30)) "${IDF_PYTHON_ENV_PATH:-$HOME/.espressif/python_env/idf5.5_py3.14_env}/bin/python" - "$out" "$count" <<'PY'
import serial, sys, time
out, count = sys.argv[1], int(sys.argv[2])
for _ in range(60):  # the USB port re-enumerates after the flash reset
    try:
        s = serial.Serial("/dev/ttyACM0", 115200, timeout=0.5)
        break
    except serial.SerialException:
        time.sleep(0.5)
else:
    raise SystemExit("no /dev/ttyACM0")
s.dtr = False; s.rts = True; time.sleep(0.1); s.rts = False  # reset into the boot
buf, t = b"", time.time()
while time.time() - t < 60 + count * 130 and buf.count(b"SHOT_END") < count:
    buf += s.read(65536)
open(out + "/raw.log", "wb").write(buf)
print("captured", len(buf), "bytes,", buf.count(b"SHOT_END"), "of", count, "shots")
PY
python3 tools/screenshot/decode.py "$out/raw.log" "$out"
echo "Done. The board still runs the dump build: flash the normal build (idf.py flash)."
