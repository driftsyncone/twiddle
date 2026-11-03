# User guide

How to use the panel once it is flashed. Every label and range below is taken from the firmware source
(`main/ui.cpp`, `main/control.cpp`, `main/settings.cpp`); the full list of settings is in
[settings.md](settings.md), and the colour themes are in [themes.md](themes.md).

> **Safety first.** The panel controls a device through Autoblow's cloud. If the panel loses its connection it
> **cannot stop the device**. Keep a way to switch the device off at the device itself. See
> [Connection loss](#connection-loss-and-the-banners).

Parts of this guide describe features that have not yet been confirmed on real hardware (see the list in the
[README](../README.md#status)). Where that matters it says so.

## Contents
1. [First boot and the setup portal](#first-boot-and-the-setup-portal)
2. [The screen: status bar, STOP and tabs](#the-screen)
3. [Remote](#remote-tab)
4. [Autopilot and Session](#autopilot-tab)
5. [Scripts](#scripts-tab)
6. [Devices and Bluetooth toys](#devices-tab)
7. [Settings](#settings-tab)
8. [Screen sleep, brightness, flip](#screen-sleep-brightness-and-flip)
9. [Status bar and banners](#status-bar-and-banners)
10. [Updates and rollback](#updates-and-rollback)

## First boot and the setup portal

The panel needs two things from you: a Wi-Fi network (name and password) and your Autoblow **device token**.
You can build them into the firmware as defaults (`main/secrets.h`, see [dev-setup.md](dev-setup.md)) or enter
them on the panel.

Once on your network the panel answers to `twiddle-p4.local` (its network name; two P4 panels on one network need
different `PANEL_HOSTNAME` values in `secrets.h`).

If no Wi-Fi network name is stored (a build with an empty `WIFI_SSID` and nothing saved yet), the panel starts in
**setup portal mode**:

(The panel's own screen also asks for Wi-Fi on first start: Scan, pick the network, password, Save & restart. The steps below are the phone route, the fallback for a black or garbled screen.)

1. The status bar at the top shows: `setup: join Wi-Fi Twiddle-Setup (pw twiddle-setup), open 192.168.4.1/setup`.
2. On a phone or laptop, join the Wi-Fi network **Twiddle-Setup** (password **twiddle-setup**; WPA2, up to four
   devices at once).
3. Open `http://192.168.4.1/setup` in a browser (`http://192.168.4.1/` redirects there). Most phones open it by themselves
   as a "sign in to network" page, because the panel redirects every address to the setup page.
   **Check the form before saving:** a phone browser may refill old Wi-Fi or token values from an earlier setup; the panel
   never shows stored passwords or tokens, so anything already typed in a field is the browser's, not the panel's.
4. On the **Twiddle setup** page:
   - **Wi-Fi network**: pick one from the list (shown with signal strength) or choose *Other / hidden* and type
     the name in the box below. The list is scanned in the background: use the *refresh* link if it is empty.
   - **Wi-Fi password**.
   - **Autoblow device token**: the page says "From the Autoblow web app's device settings." The panel talks to
     Autoblow's public device API with this token. Nothing else about where to find it is documented here; check
     Autoblow's own documentation ([developers.autoblow.com](https://developers.autoblow.com/)).
   - Tap **Save and restart**.
5. The panel saves, restarts and joins your network. The status bar shows how that goes.

You can also enter the same three values later on the panel itself ([Settings tab](#settings-tab)). Treat the
token like a password: anyone with it can control your device.

Joining Wi-Fi usually takes 10 to 20 seconds after boot. The first one or two attempts often fail; the panel
retries by itself.

## The screen

The screen is 1024 x 600 and touch only. It has three parts:

- **Status bar** (top): the selected device and its state on the left, Wi-Fi signal (and battery, if the panel
  has a battery gauge), the current speed, and the red **STOP** button on the right.
- **Tabs** (below the bar): **Remote**, **Autopilot**, **Session**, **Scripts**, **Devices**, **Settings** (gear
  symbol). Tap a tab name to switch.
- **Banners**: a red banner under the status bar when a connection is lost, and one at the bottom when the
  battery is nearly empty.

### STOP
**STOP** is on every screen. It stops whatever is running (Remote, Autopilot, a script), releases both valves and
tells every connected device to stop. It is the same action the panel takes by itself on connection loss. It
only works if the panel can reach the device (see [Connection loss](#connection-loss-and-the-banners)).

### Which device you are controlling
The top-left of the status bar shows the **selected device** (name, a down-arrow symbol and its state, for
example `VacuGlide v  Ready`). The controls on the Remote tab act on the panel's output; every device that has
**Follow** switched on receives that output, but the selected device decides which controls are shown:

- **VacuGlide**: all controls, including Looser/Tighter, Vacuum and the Scripts tab.
- **Other devices** (VacuPump, Autoblow Ultra, Bluetooth toys): the Looser/Tighter and Vacuum rows and the
  Scripts tab are hidden. A small line under the slider says what the slider does on that device (for example
  "vibration", "stroke speed", "vacuum depth", "speed (knob caps it)").

Tap the top-left area to switch to the next connected device. If there is no other connected device, the tap
opens the **Devices** tab. You can also tap a row on the Devices tab.

## Remote tab

The manual remote.

| Control | What it does |
|---|---|
| Vertical **slider** (left) and its **%** label | Speed, 5 to 100 %. Dragging while Remote is running changes the speed in steps of 5 %; letting go sends the exact value, and **starts** Remote if it was not running |
| **Start** / **Pause** (large button) | Start Remote at the slider speed, or pause it. The button changes to Pause (orange) while Remote runs |
| **Looser** and **Tighter** | Hold-to-open valve buttons (VacuGlide only): the valve is open only while your finger is on the button, and closes when you let go (the button turns red while held). **Tighter** = the API's stroke-plus valve (air out, more suction); **Looser** = stroke-minus (air in) |
| **Vacuum**: Off, Low, Medium, High | Automatic vacuum maintenance (VacuGlide only). While Remote runs, the panel briefly opens the Tighter valve at intervals: Low 0.3 s every 12 s, Medium 0.4 s every 9 s, High 0.5 s every 6 s. No pulse is sent while you hold either valve button, and a full interval passes after you let go |

The slider keeps its value if you pause, and **Pause** stops the device. The speed shown at the top right of the
status bar is the speed the device reports.

## Autopilot tab

Autopilot is this project's **own** wave generator for edging sessions. It is not Autoblow's pattern engine and
uses none of their numbers.

### How a session works
A session is **eight waves**. Each wave has three phases:

1. **Climb**: the speed rises in 4 to 7 steps from a floor level to the wave's peak.
2. **Hold**: the speed stays at the peak, with small random dips (the *wobble*).
3. **Rest**: the speed drops back to the floor and stays there.

Peaks rise as the session goes on (the first wave peaks at 60 % of the intensity range, the last at 100 %). Each
wave's timings are random within limits set by the **Edge** setting. When the eighth wave finishes, a fresh random
session starts automatically.

### Controls
| Control | What it does |
|---|---|
| **Start** / **Pause** | Start the session, or pause it. Resuming continues where it paused |
| Status text next to it | While running: current speed and `peak in m:ss` (or `at the peak`), and time left. When stopped: `Standby` and the planned length in minutes |
| **Graph** | A moving window on the planned speed: the last 10 s (grey-blue) and the next 30 s (lighter), with a marker for *now* at 25 % from the left and an axis in seconds relative to now. **Tap** it to jump to that moment. While *Draw* is on (Session tab) it shows the whole session instead, and dragging on it sets the peak of the wave under your finger |
| **Back off** | Drop to rest now (during Climb or Hold), or make the current rest 5 s longer |
| **More** | During Climb: go to the peak now. During Hold: hold 5 s longer and 5 % higher. During Rest: skip the rest and start the next wave |
| **Finish Me** | Switches to the Remote path at **100 %** with no vacuum pulses and both valves released. Press **Pause** (Remote tab) or STOP to end it |
| **Looser** / **Tighter** | Same hold-to-open valve buttons as on the Remote tab (VacuGlide only) |

### Session tab: shaping what Autopilot plays
Changing any of these re-plans the waves but keeps your position in the session, so the change blends in.

| Row | Choices | Meaning |
|---|---|---|
| **Intensity** | Warmup, Low, Medium, High | The speed range. Floor to ceiling in %: Warmup 8 to 25, Low 10 to 40, Medium 20 to 70, High 35 to 100 |
| **Edge** | Gentle, Moderate, Intense | How long peaks and rests last. Gentle: peaks 5 to 9 s, rests 15 to 25 s, larger wobble. Moderate: peaks 10 to 16 s, rests 10 to 15 s. Intense: peaks 15 to 25 s, rests 5 to 9 s, small wobble |
| **Vacuum** | Off, Low, High | Vacuum pulses during Autopilot (VacuGlide only): Low 0.25 s every 4 s, High 0.4 s every 2.5 s (a little shorter at high speed) |
| **Length** | Auto, 5 min, 10 min, 20 min, 30 min | Auto keeps the natural length of the planned waves; the others stretch or squeeze all phases to fit |
| **Peaks** | Rising, Shuffled, Wild | Rising: peaks climb wave by wave. Shuffled: the same peaks in random order. Wild: random peaks and durations from half to double |
| **Favourites** | 1, 2, 3 | **Tap** a number to load a saved set of peaks. **Hold** it (long press) to save the current peaks into it. A `-` after the number means the slot is empty. Favourites survive restarts |
| **Draw** | on/off | While on, dragging on the Autopilot graph sets a wave's peak (5 to 100 %) |
| **Reset** | | Clears all hand-drawn peaks and leaves Draw mode; the planner chooses again |

The Autopilot choices (Intensity, Edge and so on) return to their defaults (Medium, Moderate, Vacuum Off,
Length Auto, Peaks Rising) when the panel restarts. Favourites are saved.

## Scripts tab

Shown only when the selected device is a VacuGlide. It has 30 slots, numbered 1 to 30; they are the device's own
local-script slots (the cloud API only knows slot numbers).

- **Tap** a slot to play that script on the device.
- **Hold** a slot to give it a name (a text box and keyboard open; empty clears the name). Names are stored on
  the panel, not on the device, and show as `3  My name`.
- The big button at the bottom is **Pause** / **Resume** for the running script, or `Tap a slot to play` when
  none is running. The playing slot is highlighted.

## Devices tab

One card per device. Card 1 is always the Autoblow device (cloud); below it come Bluetooth toys, both remembered
ones and whatever the last scan found.

Top row:

- **Turn on**: shown while Bluetooth is off. Bluetooth only starts at boot, so tapping it saves the setting
  "Bluetooth on" and restarts the panel (it stops everything first). After the restart the button reads **Scan**.
- **Scan**: look for Bluetooth toys for 8 seconds (the button reads `Scanning...`). Switch the toy on first.
  Found toys appear as cards.
- A hint text: "Tap a device to show its controls. New toy: switch it on, Scan, Connect. Follow: it takes the
  panel's speed, at the % next to it."

Each card shows, left to right:

| Element | Meaning |
|---|---|
| Coloured dot | Green: connected. Orange: connecting. Grey: not connected |
| Name and status line | For example `Ready`, `Connecting...`, `Out of range`, or an error. A running device adds `at 40%`; a toy adds its battery and signal strength (`... 82%`, `... -60 dBm`). The cloud device says **Set the token in Settings** if no token is stored |
| **Follow** switch | On: this device receives the panel's output. The Autoblow device starts with Follow on |
| **25% / 50% / 75% / 100%** | The scale: this device gets that percentage of the panel's speed (the Autoblow device never goes below 5 %) |
| **Connect** / **Forget** (toys only) | Connect remembers the toy: it then reconnects by itself, now and after every restart, retrying after 15, 30, 60, then every 120 seconds while it is away. A remembered toy that is not connected shows **Connect** (tries at once, restarts the retry timer) next to **Forget**; while connecting the button reads "Connecting"; once up only **Forget** shows. Forget disconnects and removes it. A heart rate monitor's row has no Follow switch or 25-100 % buttons (it is an input), and tapping it does not select it |

A toy you connect on purpose starts with Follow **on** at 100 %. A selected device gets a thick coloured border.
Tap a card (outside its switch, scale or button) to make it the selected device.

Supported toys are those matched by the built-in drivers: Lovense toys, Kiiroo Keon strokers and OSSM. No real
toy has been tested on this hardware yet.

While a VacuGlide **script** is playing, Bluetooth toys rest (a script has no speed to share).

### HR edging (heart rate drives Autopilot)
**Build and unit-test verified only: no board test with a real heart rate monitor yet.** On the Devices tab, wear a standard
BLE heart rate monitor (chest strap or watch broadcasting the Heart Rate Service; the Polar H10 is the recommended one, with
"two BLE devices" switched on once in the Polar app), Scan and Connect. It shows its bpm on its card and takes no output (no
Follow, no scale). It uses one of the **3 Bluetooth links** shared with the toys: if all 3 are in use the card says
"No free Bluetooth link: disconnect a toy first" (Forget a toy, then Connect); no toy is ever dropped for it.

On the Session tab set **HR edging** to On. Autopilot then climbs like normal, **edges** when the bpm reaches the edge bpm (or
rises by Fast rise bpm within 10 s), drops to the floor of the Intensity level, rests until the bpm is down by the Recovery
drop and a minimum rest has passed, raises the edge bpm by Raise per edge and climbs again. It ends after the set number of
Edges or the Length, and if the heart rate signal is lost for more than 5 s it goes to the floor and then stops.
The speed never goes above the Intensity ceiling. **Back off** rests now; **More** and tapping the graph do nothing in this mode;
every Start begins a new HR session. STOP, the lost-device handshake and the low-battery stop work as always.

| Setting | Meaning |
|---|---|
| **Resting bpm** | Your resting rate (the edge is kept at least 10 above it, recovery at least 3 above it) |
| **Edge bpm** / **Learn** | The trigger. **Learn** copies the current bpm into it (needs a live reading) |
| **Recovery drop** | Rest until bpm <= edge bpm minus this |
| **Edges** | Session ends after this many edges (no limit = 0) |
| **Raise per edge** | Added to the edge bpm after each finished cycle |
| **Fast rise** | A second trigger: this many bpm gained within 10 s (off below 8) |

The Autopilot tab shows the bpm, the phase and the edge count; the graph shows the last 10 s of actual speed and a red heart rate line
(40 to 220 bpm over the 0 to 100 axis). The future depends on your heart rate, so ahead it only shows the current target speed as a dashed flat line ("HR-driven: depends on heart rate").

## Settings tab

Do **not** photograph or screenshot this tab to share: it shows the last three characters of your device token
and your Wi-Fi name. The password is always masked.

| Row | What it does |
|---|---|
| **Wi-Fi** + **Scan** | Tap the name box to type the network name. **Scan** lists nearby networks; tapping one fills the name and asks for the password |
| **Password** | Opens a keyboard (masked, with Show/Hide) |
| **Token** | Your Autoblow device token. Shown masked except for the last three characters |
| **Screen off** | Never, 1 min, 5 min, 15 min |
| **Wi-Fi gaps** | 10 s, 20 s, 30 s: how long the cloud device may go unanswered before the panel treats it as lost |
| **Bluetooth** | Off / On. Changing it restarts the panel |
| **Display** | **Flip: n/a** (greyed: not available on this board) and the brightness slider (eye symbol) |
| **Theme** | Daylight, Midnight (the default on a fresh panel), Ocean, Forest, Ember, Blush. Applies at once; a theme you saved is kept |
| Info text | Panel IP and signal strength, battery line, firmware version and build date. If Wi-Fi is down it shows why instead |
| **Save & restart** | Appears when you changed Wi-Fi name, password or token. Saves them and restarts. If nothing changed it reads **Restart** and just restarts (stopping everything first) |
| **Power off** | See below |

Wi-Fi name, password and token are **staged** while you edit and only take effect after **Save & restart**. Screen
off, Wi-Fi gaps, display, and theme apply immediately and are saved at once. See [settings.md](settings.md).

**Power off**: software cannot switch the panel off. The button opens a box that says "To power off, hold the
battery button on the panel for 2 seconds. Press it briefly to turn back on." That text comes from the
battery-powered ESP32-S3 panels; whether the Guition P4 board has such a button was not checked, and it has no
battery gauge. The box also offers **Screen off**, which stops the devices and blanks the screen (the next touch
only wakes it), and **Cancel**.

## Screen sleep, brightness and flip

- **Dim**: after one minute without a touch the backlight drops to a quarter of your brightness (never below 8 %)
  so STOP stays readable. The next touch restores it.
- **Screen off**: after the idle time you chose (default 5 minutes) the backlight turns off, but **only while
  nothing is running** and no device is moving, so STOP is always one tap away. The touch that wakes the screen
  does nothing else: it is swallowed until you lift your finger.
- **Brightness**: the slider in Settings (10 to 100 %). It changes live and is saved when you lift your finger.
- **Flip screen**: on boards where it works, shows the picture upside down for an upside-down mount. **It is not
  available on the Guition P4** in this firmware: the Settings button is greyed and reads **Flip: n/a**.

## Status bar and banners

### Left: selected device and state
`<device> v  <state>`, followed by `+N` if N other connected devices also follow, or `, Follow off` if this device
is switched off. The dot before it:

| Colour | Meaning |
|---|---|
| Green | The device is connected and answering |
| Orange | Connecting, or the link is silent for a moment (within the Wi-Fi gap) |
| Red (Wi-Fi) | No Wi-Fi |
| Grey (Bluetooth toy) | Not connected |

State texts for the Autoblow device:

| Text | Meaning |
|---|---|
| `Ready`, `Running`, `Paused`, `Script playing`, `Script paused`, `Updating firmware` | The device's own mode |
| `Connecting...` | Looking for the device in the cloud |
| `No Wi-Fi: <reason>` | The panel is not on Wi-Fi. Reasons: `network not found`, `wrong password?`, `signal lost`, `router dropped the panel`, `not set up`, `disconnected (code N)`. "Check Settings" is added when you can fix it there (network not found, wrong password, not set up) |
| `No answer, reconnecting... N s (stops at M s)` or `Wi-Fi dropped, reconnecting...` | The link is down but still inside the Wi-Fi gap. The session carries on; if the link is not back at M seconds the device counts as lost |
| `<mode> - weak Wi-Fi` | Signal is in the weakest band |
| `<mode> - <error>` | The device or the cloud reported an error |
| `Set the token in Settings` | No device token stored |

### Wi-Fi and speed
Next to the device: a Wi-Fi symbol and the signal strength in dBm (`--` when down). The colour is green at -65 dBm
or stronger, orange from -66 to -75 dBm, and red below that or when down. If a battery gauge is fitted, the
battery level appears under it. At the right: the current speed in %.

### Banners
- **Connection lost** (red, under the status bar): "Connection lost while <device> was running. It may still be
  moving: switch it off on the device. The panel sends STOP as soon as it reconnects." This appears when a device
  that was moving becomes unreachable.
- **Battery** (red, at the bottom): "Battery at N%: control is paused until you plug in USB." Only with a fuel
  gauge, when the charge is 5 % or less and not charging. The panel stops everything and stays stopped.

### Connection loss
"Lost" means no answer from the cloud for the Wi-Fi gap (10, 20 or 30 s). From that moment the panel stops the
session and sends **only STOP** until the device acknowledges it. During the gap the device keeps running at its
last speed, because the panel cannot reach it. A Bluetooth toy that disconnects while running also stops the
session.

## Updates and rollback

You will usually update the panel over Wi-Fi from a computer (`./ota.sh`, see
[ota-and-rollback.md](ota-and-rollback.md)). What you see on the panel:

1. Everything stops, and the screen shows **Updating firmware** with a percentage while the new image is written.
2. The panel restarts into the new firmware. For the first minutes the new firmware is "on probation".
3. After 30 seconds of working Wi-Fi the update is confirmed and nothing more happens.
4. If Wi-Fi does not come up within 3 minutes, or the new firmware crashes before confirming, the panel goes
   back to the previous firmware by itself. You see a restart and the old version number on the Settings tab.

If an update does not start (wrong password, wrong network), the old firmware keeps running unchanged, although
the devices will already have been stopped.
