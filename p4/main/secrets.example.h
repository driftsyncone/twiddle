// Copy to secrets.h (gitignored) and fill in. Baked in at build time as defaults; the
// panel's Settings tab can change Wi-Fi and the token later (stored in NVS).
#pragma once
#define WIFI_SSID    "your-ssid"
#define WIFI_PASS    "your-password"
#define DEVICE_TOKEN "your-autoblow-device-token"
#define OTA_PASSWORD "pick-a-password"  // for ./ota.sh uploads over Wi-Fi
#define OTA_HOST     "twiddle-p4.local"   // where ./ota.sh sends updates (hostname or IP)
// Optional: this panel's network name (default "twiddle-p4"). Set it when you run two P4 panels on one network.
// #define PANEL_HOSTNAME "twiddle-p4-2"
// Optional: talk to another server instead of the Autoblow cloud, e.g. the Autoblow emulator
// (plain http is only allowed through this override). Leave it out for the real cloud.
// #define AUTOBLOW_API_BASE "http://your-host.example.com:8792"
