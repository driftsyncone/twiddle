#!/bin/sh
# Upload firmware over Wi-Fi to the panel's /update endpoint (ESP32-P4 build).
#   ./ota.sh [build/twiddle.bin] [host]
#   ./ota.sh --check [host]     only verify the panel is reachable and runs this project (no upload)
# Host: argument, else $PANEL_HOST, else OTA_HOST from main/secrets.h, else
# twiddle-p4.local (this firmware's default mDNS name; PANEL_HOSTNAME in secrets.h changes it). The panel must be reachable from this machine.
#
# After the upload it waits for the new firmware to pass probation: it must get
# onto Wi-Fi and run for 30 s, or the panel goes back to the previous firmware.
set -e
cd "$(dirname "$0")"
bin=${1:-build/twiddle.bin}
define() { sed -n "s/^#define $1 *\"\(.*\)\".*/\1/p" main/secrets.h; }
host=${2:-${PANEL_HOST:-$(define OTA_HOST)}}
host=${host:-twiddle-p4.local}
pass=$(define OTA_PASSWORD)
built() { curl -fsS --max-time 5 "http://$host/" 2>/dev/null | sed -n 's/^id: //p'; }  # the firmware's own hash: changes with every build
state() { curl -fsS --max-time 5 "http://$host/" 2>/dev/null | sed -n 's/^firmware: //p'; }

# Refuse to touch a board that is not running this project on this model. GET / starts with
# "<project> <version>" and has a "board:" line (BOARD_NAME in main/board.h). Several projects can share the same P4
# hardware, so the model alone is not enough.
expect_board=$(sed -n 's/^#define BOARD_NAME *"\(.*\)".*/\1/p' main/board.h)
status=$(curl -fsS --max-time 5 "http://$host/" 2>/dev/null || true)
project=$(printf '%s\n' "$status" | sed -n '1s/ .*//p')
board=$(printf '%s\n' "$status" | sed -n 's/^board: //p')
if [ -z "$status" ]; then
  echo "Cannot reach $host (powered off, wrong network, or no route: see docs/network.md)." >&2; exit 1
fi
# TRANSITIONAL: boards still running firmware from before the rename to Twiddle identify as "vacuglide-panel"; accept
# both names so they can take one update to the new name. Any other project is refused.
case "$project" in
  twiddle|vacuglide-panel) ;;
  *)
  echo "Refusing: $host runs '$(printf '%s\n' "$status" | sed -n 1p)', not twiddle (another project's firmware owns that board)." >&2; exit 1 ;;
esac
if [ "$board" != "$expect_board" ]; then
  echo "Refusing: $host is a '$board', but this is a '$expect_board' build." >&2; exit 1
fi
if [ "$1" = "--check" ]; then echo "OK: $host runs $project on a $board"; exit 0; fi

before=$(built || true)
echo "Uploading $bin to $host ..."
curl -fsS --max-time 120 -u "ota:$pass" -F "firmware=@$bin" "http://$host/update"

echo "Waiting for the new firmware to confirm itself (up to 4 min) ..."
sleep 10
i=0
while [ $i -lt 46 ]; do
  now=$(built || true)
  st=$(state || true)
  if [ -n "$now" ] && [ -n "$before" ] && [ "$now" = "$before" ]; then
    echo "ROLLED BACK (or the same image was uploaded): the panel still runs firmware id $now." >&2
    exit 1
  fi
  if [ "$st" = "confirmed" ]; then
    echo "OK: new firmware confirmed (id $now)."
    exit 0
  fi
  sleep 5
  i=$((i + 1))
done
echo "No confirmation within 4 min (last state: ${st:-unreachable}). Check the panel." >&2
exit 1
