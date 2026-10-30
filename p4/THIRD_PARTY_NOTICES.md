# Third-party notices

This project's own code is licensed under CC BY-NC-SA 4.0 (see `LICENSE`). It uses the components below. Those
that the build downloads (`managed_components/`, pinned by `dependencies.lock`) are not stored in this repository
and keep their own licences; their licence files are included in the download. Code and protocol knowledge that
**is** in this tree (`main/toys/`) is acknowledged with full notices in the first section.

## Code derived from third-party sources, in this tree

### Buttplug protocol definitions - BSD-3-Clause
Used in `main/toys/lovense.cpp`, `main/toys/kiiroo.cpp`, `main/toys/ossm.cpp` and described in `main/ble.h`:
device names, service and characteristic UUIDs, command formats and timing, taken from the Buttplug project
(https://github.com/buttplugio/buttplug; device configs under
`crates/buttplug_server_device_config/device-config/protocols/` and protocol implementations under
`crates/buttplug_server/src/device/protocol_impl/`). The OSSM driver is our own client written from the OSSM's
documented BLE API; only its UUIDs and name match Buttplug's device config.

> Copyright (c) 2016-2026 Nonpolynomial Labs, LLC
> All rights reserved.
>
> Redistribution and use in source and binary forms, with or without modification, are permitted provided that
> the following conditions are met:
>
> 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the
>    following disclaimer.
> 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
>    following disclaimer in the documentation and/or other materials provided with the distribution.
> 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
>    products derived from this software without specific prior written permission.
>
> THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
> WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
> PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
> DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
> PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
> CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
> OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
> DAMAGE.

The copyright line is the one given in the source headers of the toy drivers. Check the
LICENSE file in the Buttplug repository for the upstream text.

### FeelRobotics keon-wifi-sdk - MIT
Used in `main/toys/kiiroo.cpp`: Keon 2 / Keon WiFi details (UUIDs, movement mode, the `0x03 0x01` move, pause,
battery) from `src/devices/keon2.ts`, `keonWifi.ts`, `keonProtocol.ts` and `src/managers/BleManager.ts`.

> Copyright (c) 2025 FeelRobotics
>
> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to deal
> in the Software without restriction, including without limitation the rights
> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
> copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all
> copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
> SOFTWARE.

### Pin and init-table data
`main/board_dsi_init.c`: the JD9165 init sequence for the V2 panel was taken from the panel definitions in the
ESPHome project (`esphome/components/mipi_dsi/models/guition.py`). Only register values are used, not code.
ESPHome is licensed under several licences depending on the component (see https://github.com/esphome/esphome);
we have not verified the licence of that particular file, so treat this table as third-party data.

## Components downloaded at build time

Versions are from `dependencies.lock` (a build with `managed_components/` present shows them in each
`idf_component.yml`).

| Component | Version | Licence | Source |
|---|---|---|---|
| ESP-IDF | 5.5.1 | Apache-2.0 | https://github.com/espressif/esp-idf (includes mbedTLS, NimBLE host, FreeRTOS, lwIP and others under their own licences, see the IDF tree) |
| espressif/esp_hosted | 3.0.10 | Apache-2.0 (the Linux kernel-module part is GPL-2.0-only and is not used; a bundled protobuf-c is BSD-3-Clause) | https://github.com/espressif/esp-hosted-mcu |
| espressif/esp_wifi_remote | 1.6.5 | Apache-2.0 | https://github.com/espressif/esp-wifi-remote |
| espressif/wifi_remote_over_eppp | 0.3.3 | Apache-2.0 | same repository (pulled in by esp_wifi_remote) |
| espressif/eppp_link | 1.1.6 | Apache-2.0 | https://github.com/espressif/esp-protocols |
| espressif/esp_serial_slave_link | 1.1.2 | Apache-2.0 | https://github.com/espressif/idf-extra-components |
| espressif/mdns | 1.14.1 | Apache-2.0 | https://github.com/espressif/esp-protocols |
| espressif/esp_lvgl_port | 2.6.3 | Apache-2.0 | https://github.com/espressif/esp-bsp |
| espressif/esp_lcd_jd9165 | 2.0.2 | Apache-2.0 | https://github.com/espressif/esp-bsp |
| espressif/esp_lcd_touch | 1.2.1 | Apache-2.0 | https://github.com/espressif/esp-bsp |
| espressif/esp_lcd_touch_gt911 | 1.2.1 | Apache-2.0 | https://github.com/espressif/esp-bsp |
| espressif/cmake_utilities | 0.5.3 | Apache-2.0 | https://github.com/espressif/esp-iot-solution |
| h2zero/esp-nimble-cpp | 2.6.0 | Apache-2.0 | https://github.com/h2zero/esp-nimble-cpp |
| lvgl/lvgl | 9.6.0~1 | MIT | https://github.com/lvgl/lvgl |
| bblanchon/arduinojson | 7.4.3 | MIT | https://github.com/bblanchon/ArduinoJson |

The Apache-2.0 licence text is in each component's licence file (and in ESP-IDF's `LICENSE`); Apache-2.0 requires
that recipients get a copy of it, so keep those files if you redistribute the sources of these components. LVGL's
bundled Montserrat fonts are distributed under the SIL Open Font License 1.1 as part of LVGL (see LVGL's repository).

### esp-nimble-cpp NOTICE (required by Apache-2.0 section 4(d))

```
esp-nimble-cpp
NimBLE-Arduino
Copyright 2020-2025 Ryan Powell <ryan@nable-embedded.io> and
esp-nimble-cpp, NimBLE-Arduino contributors.

The Initial Developer of some parts of this library, which are copied from,
derived from, or inspired by is, esp32-snippets, Copyright 2017 Neil Kolban.
```

### LVGL - MIT
> Copyright (c) 2025 LVGL Kft
>
> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to deal
> in the Software without restriction, including without limitation the rights
> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
> copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all
> copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
> SOFTWARE.

### ArduinoJson - MIT
> Copyright (c) 2014-2026, Benoit BLANCHON
>
(MIT text as above.)

## Autoblow device API
The panel uses Autoblow's public device API (https://developers.autoblow.com/). No Autoblow code or Autopilot
patterns are included; the Autopilot generator in `main/control.cpp` is this project's own. "Autoblow" and
"VacuGlide" are trademarks of their owners.
