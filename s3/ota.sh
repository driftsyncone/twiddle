#!/bin/sh
# Upload firmware over Wi-Fi. Used by `pio run -e ota -t upload`, or by hand:
#   ./ota.sh .pio/build/panel/firmware.bin [host]
# Host: argument, else $PANEL_HOST, else from include/secrets.h: OTA_HOST
# (Guition) or OTA_HOST_WS (Waveshare, when $OTA_BOARD=waveshare5), else the
# panel's mDNS name: twiddle-s3.local / twiddle-ws.local.
#
# After the upload it waits for the new firmware to pass probation: it must get
# onto Wi-Fi and run for 30 s, or the panel goes back to the previous firmware.
set -e
cd "$(dirname "$0")"
bin=${1:-.pio/build/${OTA_BOARD:-panel}/firmware.bin}
define() { sed -n "s/^#define $1 *\"\(.*\)\".*/\1/p" include/secrets.h; }
if [ "$OTA_BOARD" = waveshare5 ]; then
  host=${2:-${PANEL_HOST:-$(define OTA_HOST_WS)}}
  host=${host:-twiddle-ws.local}
else
  host=${2:-${PANEL_HOST:-$(define OTA_HOST)}}
  host=${host:-twiddle-s3.local}
fi
pass=$(define OTA_PASSWORD)
# The image carries its ELF hash (esp_app_desc_t, 176 bytes in); the panel shows
# the same hash as "id:" on GET /, so we can tell our image from any other.
want=$(xxd -s 176 -l 8 -p "$bin")
status() { curl -fsS --max-time 5 "http://$host/" 2>/dev/null; }

# Refuse the wrong board's image: a Guition build on the Waveshare (or the other
# way round) boots with a dead screen, and its Wi-Fi alone would confirm it.
# Full model names (BOARD_NAME in src/board.h): "Guition" alone would also accept the ESP32-P4 Guition board.
expect_board="Guition JC8048W550C"
[ "$OTA_BOARD" = waveshare5 ] && expect_board="Waveshare ESP32-S3-Touch-LCD-5"
board=$(status | sed -n 's/^board: //p' || true)
case "$board" in
  "") echo "Warning: $host didn't say which board it is (old firmware?); sending anyway." ;;
  *"$expect_board"*) ;;
  *) echo "Refusing: $host is a $board, but this is a $expect_board build (set OTA_BOARD)." >&2; exit 1 ;;
esac

echo "Uploading $bin ($want) to $host ..."
curl -fsS --max-time 120 -u "ota:$pass" -F "firmware=@$bin" "http://$host/update"

echo "Waiting for the new firmware to confirm itself (up to 4 min) ..."
sleep 10
i=0
while [ $i -lt 46 ]; do
  st=$(status || true)
  id=$(printf '%s' "$st" | sed -n 's/^id: //p')
  fw=$(printf '%s' "$st" | sed -n 's/^firmware: //p')
  if [ -n "$id" ]; then
    case "$want" in
      "$id"*) [ "$fw" = confirmed ] && { echo "OK: new firmware confirmed ($id)."; exit 0; } ;;
      *) [ $i -ge 2 ] && { echo "ROLLED BACK: the panel is running other firmware ($id, state $fw)." >&2; exit 1; } ;;
    esac
  fi
  sleep 5
  i=$((i + 1))
done
echo "No confirmation within 4 min (last state: ${fw:-unreachable}). Check the panel." >&2
exit 1
