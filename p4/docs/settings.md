# Settings reference

Every setting the panel has, where it is stored and when it takes effect. Source: `main/settings.cpp`,
`main/settings.h`, `main/devices.cpp`, `main/ui.cpp`. For how to use them see the [user guide](user-guide.md).

Settings live in the ESP32's flash in **NVS** (non-volatile storage). They survive restarts and firmware
updates (an update only rewrites the app partition). "Erase flash" (`idf.py erase-flash`) removes them.

## Settings on the Settings tab

NVS namespace `vgpanel`, unless noted. (`vgpanel` is a legacy name from before the project was called Twiddle; it is kept for settings compatibility, so a renamed firmware keeps every stored setting.)

| Setting (label) | Key | Values | Default | Applies | Notes |
|---|---|---|---|---|---|
| Wi-Fi network (**Wi-Fi**) | `ssid` | text, up to 32 characters | `WIFI_SSID` from `secrets.h` | after restart (**Save & restart**) | Empty means "not set up": the panel starts the [setup portal](user-guide.md#first-boot-and-the-setup-portal) |
| Wi-Fi password (**Password**) | `pass` | text | `WIFI_PASS` from `secrets.h` | after restart | Shown masked |
| Autoblow device token (**Token**) | `token` | text, surrounding spaces are trimmed | `DEVICE_TOKEN` from `secrets.h` | after restart | Shown masked except the last 3 characters. Treat as a password |
| Screen off | `sleep` | Never (0), 1, 5, 15 minutes | 5 | immediately | Only while nothing runs and no device moves |
| Wi-Fi gaps | `grace` | 10, 20, 30 seconds | 20 | immediately | How long the cloud device may go unanswered before it counts as lost. Any other stored value is treated as 20 |
| Bluetooth | `bt` | -1 automatic, 0 off, 1 on | -1 | after restart (the panel restarts when you change it) | Automatic means on if at least one toy is remembered. The Settings buttons show the current state, Off or On, and writing either one makes the choice explicit. Off lets Wi-Fi run without modem power save |
| Flip screen | `flip` | on/off | off | immediately | Not available on the P4: the button is greyed (**Flip: n/a**) and no longer changes the stored value |
| Brightness (eye slider) | `bright` | 10 to 100 % | 100 | immediately; saved when you lift your finger | After 1 minute idle the screen dims to a quarter of this value (minimum 8 %) |
| Theme | `theme` | index into the theme table: 0 Daylight, 1 Midnight, 2 Ocean, 3 Forest, 4 Ember, 5 Blush | 1 (Midnight) | immediately | An out-of-range stored index falls back to Midnight. See [themes.md](themes.md) |

"Save & restart" writes `ssid`, `pass` and `token` together and restarts. If nothing was edited the button is a
plain **Restart** and writes nothing. Both stop all devices before restarting (waiting at most 5 seconds for the
device to confirm it stopped).

## Other stored data

| Data | Namespace and key | Values | Default | Notes |
|---|---|---|---|---|
| Autopilot favourites | `vgpanel`: `fav0`, `fav1`, `fav2` | 8 peaks each, 1 byte per wave | empty (the Session tab shows `-`) | Saved with a long press on a favourite button; a tap loads it |
| Script names | `vgpanel`: `sn0` to `sn29` | text | none | 30 slots; an empty name removes the key |
| Follow and scale per device | `devices`: one entry per device (`ab` for the cloud device, the Bluetooth address without colons for a toy) | Follow on/off; scale 10 to 100 % (the UI offers 25, 50, 75, 100) | cloud device: on, 100 %; a toy: off until you Connect it (then on, 100 %) | Stored as a 2-byte blob |
| Remembered Bluetooth toys | `bletoys`: one entry per toy (address, type) | | none | Remembered toys reconnect by themselves |

## Not saved

These return to their defaults at every restart:

| Value | Default |
|---|---|
| Remote speed | 20 % |
| Remote vacuum | Off |
| Autopilot Intensity / Edge / Vacuum | Medium / Moderate / Off |
| Session Length / Peaks | Auto / Rising |
| Hand-drawn peaks (Draw) | none |
| Which device is selected (status bar) | the first connected device, the Autoblow device first |

## Build-time settings (`main/secrets.h`)

Set before building; not changeable on the panel. The file is ignored by git: copy
`main/secrets.example.h` to `main/secrets.h`.

| Define | Purpose |
|---|---|
| `WIFI_SSID`, `WIFI_PASS` | Default Wi-Fi. Leave `WIFI_SSID` empty to get the setup portal on first boot |
| `DEVICE_TOKEN` | Default Autoblow device token |
| `OTA_PASSWORD` | Password of the panel's web interface, which `ota.sh` uses (user `ota`). If empty, no login succeeds, so updates and the protected pages are unreachable |
| `OTA_HOST` | Where `ota.sh` sends updates (host name or IP); default `twiddle-p4.local` |
| `PANEL_HOSTNAME` (optional) | The panel's network name; default `twiddle-p4` (reachable as `twiddle-p4.local`) |
| `AUTOBLOW_API_BASE` (optional) | Use another server than the Autoblow cloud (plain http allowed), for example an emulator for testing. Leave out for the real cloud |

Values saved on the Settings tab win over `secrets.h`. The values from `secrets.h` are used only while nothing
is stored in NVS for that key.

## Fixed in the firmware

Not settings, but good to know: the Autopilot has eight waves; there are 30 script slots; the setup portal
network is `Twiddle-Setup` (password `twiddle-setup`, address 192.168.4.1); a new firmware is confirmed after 30 s
of Wi-Fi and rolled back after 3 minutes without ([ota-and-rollback.md](ota-and-rollback.md)).
