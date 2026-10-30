# Testing without the real device

**Hard rule: never send a speed, valve or stop command to the real VacuGlide while testing.** The real device may
be in use or switched off. Test cloud and control code against the emulator.

The panel is reachable as `twiddle-p4.local` unless `PANEL_HOSTNAME` is set; use its IP if mDNS does not resolve.

## The Autoblow emulator
A fake Autoblow cloud API with a live page (public as `driftsyncone/autoblow-emulator` on GitHub). Clone it
anywhere on the machine that will host the test server.
```sh
cd <your clone of autoblow-emulator>
node src/server.ts --host 0.0.0.0        # Node 23.6+ strips types; or `node dist/server.js` (Node 18+) when dist/ exists
```
- Listens on port 8792 (`--host 0.0.0.0` so the panel can reach it; it is plain http).
- The device token is **random per run**: read it from `~/.config/autoblow-emulator/devices.json` (it is an
  emulator token, not a secret). The page is `http://127.0.0.1:8792/`; the live call log is at
  `/emu/api/snapshot`; flip a device offline/online with
  `PATCH /emu/api/devices/<token>` and body `{"online": false}`.
- Point the panel at it in your `secrets.h` (never commit this):
  ```c
  #define DEVICE_TOKEN "<the emulator's token>"
  #define AUTOBLOW_API_BASE "http://<your computer's hostname>:8792"
  ```
  Restore the real values before any real-cloud build (keep a copy of the real `secrets.h` aside).
- Known difference: the emulator reports `operationalMode` `TARGET_SPEED_ACTIVE`, the Arduino firmware (which
  worked with the real device) expects `TARGET_SPEED_PLAYING`. The emulator is probably the inaccurate one; check
  the Autoblow docs before changing the firmware.
- If the panel is on another network than your computer it needs a network path, see [network.md](network.md). Stop the
  emulator when you are done (it listens on all interfaces).

What was verified against it: discovery, state polling, speed commands, valve and pulse commands, STOP, and the
loss handshake (emulator offline -> `lost`, STOP set first on return, speed only after the acknowledgement).

## Screenshots without touching the screen
```sh
tools/screenshot/capture.sh [OUTDIR] [TABS] [THEME] [STEP]
# OUTDIR default build/shots; TABS like "1,2", default 0,1,2,3,4; THEME = theme index switched to live before the shots
# (not saved), default as saved; STEP = every STEP-th pixel, default 2
```
It builds a variant with `tools/screenshot/screenshot_dump.cpp` copied into `main/` (removed afterwards), flashes
it, reads the serial log and writes `tab<N>.png` (512x300 at STEP 2, about 100 s per tab; STEP 4 gives 256x150 in about 25 s, enough for theme checks; timings not re-measured here) plus `raw.log`. **Never
capture the Settings tab (5)**: it shows the token's tail; the script silently strips a `5` from the TABS list and the dump code skips tab 5 as well. Flash the normal build
afterwards. How it works and why: the USB console takes no input, so the dump variant shows each tab itself and
prints it as base64 rows; it forces two full redraws first (invalidating the screen under `UiLock`, 1.2 s apart) because in direct mode the buffer you read can be one frame behind;
the file is force-linked (`-Wl,-u,screenshot_dump_keep`, see `main/CMakeLists.txt`) because nothing references it.
When the panel is reachable over the network, `GET /screenshot?tab=N` (basic auth) returns a full-resolution BMP with no USB cable, and `GET /theme?n=N` switches theme first: that is how all six themes were checked. Tab 5 (Settings) is never captured.

## Host tests for the toy drivers
`test/host/run.sh` compiles the real `main/toys/*.cpp` on the host (g++, no IDF) against a mock of the NimBLE
classes (`test/host/mock/`) and runs 67 checks (run in this review: `67 checks, 0 failed`). It needs
`managed_components/bblanchon__arduinojson` (created by the first `idf.py reconfigure`/`build`) and the output binary goes to `$TMPDIR` (default `/tmp`). Checks: name/manufacturer-data matching, Lovense scaling and rate limit, Keon packets, OSSM
arming. Not covered: any real toy on the air (untested).

## Sharing the one board
There is one board and one USB port. If several people/agents use it, serialise with a lock file and hold it for the
whole flash **and** the capture that follows (another run can flash between your steps otherwise):
```sh
flock -w 600 /path/to/p4-board.lock bash -c 'idf.py -p /dev/ttyACM0 flash && <capture the log>'
```
A script that waits forever while holding the lock blocks everyone. Find the holder with `fuser -v <lockfile>` or
`lsof <lockfile>`, check whether it is stuck, then stop it. Give every long capture a `timeout`.

## Checklist before calling a change done
- builds clean; flashed; boot log healthy (see [dev-setup.md](dev-setup.md)); no `Guru Meditation`/`assert failed`
  over a few minutes; `esp-hosted fw versions ... (match)`.
- UI change: screenshots of the affected tabs (not Settings).
- cloud/control change: tested against the emulator, including the offline and recovery path.
- BLE change: scan and encoder tests; a real toy only when one is deliberately powered on and in range.
