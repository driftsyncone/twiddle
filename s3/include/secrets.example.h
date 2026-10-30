// Copy to secrets.h and fill in. Baked in at build time as defaults; the
// panel's Settings tab can change Wi-Fi and the token later (stored in NVS).
#pragma once
#define WIFI_SSID    "your-ssid"
#define WIFI_PASS    "your-password"
#define DEVICE_TOKEN "your-autoblow-device-token"
#define OTA_PASSWORD "pick-a-password"  // for ./ota.sh uploads over Wi-Fi
#define OTA_HOST     "twiddle-s3.local"   // where ./ota.sh sends updates (hostname or IP)
// #define OTA_HOST_WS  "twiddle-ws.local"  // same, for a Waveshare panel (env waveshare5-ota)
// #define PANEL_HOSTNAME "my-panel"  // overrides the network name (defaults: Guition "twiddle-s3", Waveshare "twiddle-ws")
