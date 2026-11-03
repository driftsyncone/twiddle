// Build-time defaults for a RELEASE image: nothing of the builder's is compiled in.
// build_config.h includes this instead of secrets.h when the build is configured with -DTWIDDLE_RELEASE=1
// (env:panel-release), and also when no include/secrets.h exists at all. Wi-Fi, device token and the web password then
// come from the Twiddle-Setup portal and are kept in NVS only.
//
// Rules for this file: no real values, ever. tools/check-release-image.sh scans the built image for home-network strings.
#pragma once
#define WIFI_SSID ""
#define WIFI_PASS ""
#define DEVICE_TOKEN ""
// No OTA_PASSWORD (the panel makes its own random one at first boot, see otapw.h), no OTA_HOST / OTA_HOST_WS (only ota.sh
// reads those) and no PANEL_HOSTNAME (the per-board default).
