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

> Copyright (c) 2016-2023, Nonpolynomial Labs, LLC
> All rights reserved.
>
> Redistribution and use in source and binary forms, with or without
> modification, are permitted provided that the following conditions are met:
>
> * Redistributions of source code must retain the above copyright notice, this
>   list of conditions and the following disclaimer.
>
> * Redistributions in binary form must reproduce the above copyright notice,
>   this list of conditions and the following disclaimer in the documentation
>   and/or other materials provided with the distribution.
>
> * Neither the name of buttplug nor the names of its
>   contributors may be used to endorse or promote products derived from
>   this software without specific prior written permission.
>
> THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
> AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
> IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
> DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
> FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
> DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
> SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
> CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
> OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
> OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Text and copyright line copied verbatim from the upstream `LICENSE` file, https://raw.githubusercontent.com/buttplugio/buttplug/master/LICENSE (fetched 2026-11-02).

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
bundled Montserrat fonts are distributed under the SIL Open Font License 1.1 as part of LVGL (full text below).

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

### Montserrat fonts in LVGL - SIL Open Font License 1.1
LVGL's built-in `lv_font_montserrat_*` fonts are compiled into the firmware image. They are licensed under the SIL Open Font License 1.1; the text below is copied verbatim from LVGL's `scripts/generators/built_in_font/font_license/Montserrat/OFL.txt`. The fonts are used unmodified as supplied by LVGL; they are not sold on their own.

```
Copyright 2011 The Montserrat Project Authors (https://github.com/JulietaUla/Montserrat)

This Font Software is licensed under the SIL Open Font License, Version 1.1.
This license is copied below, and is also available with a FAQ at:
http://scripts.sil.org/OFL


-----------------------------------------------------------
SIL OPEN FONT LICENSE Version 1.1 - 26 February 2007
-----------------------------------------------------------

PREAMBLE
The goals of the Open Font License (OFL) are to stimulate worldwide
development of collaborative font projects, to support the font creation
efforts of academic and linguistic communities, and to provide a free and
open framework in which fonts may be shared and improved in partnership
with others.

The OFL allows the licensed fonts to be used, studied, modified and
redistributed freely as long as they are not sold by themselves. The
fonts, including any derivative works, can be bundled, embedded, 
redistributed and/or sold with any software provided that any reserved
names are not used by derivative works. The fonts and derivatives,
however, cannot be released under any other type of license. The
requirement for fonts to remain under this license does not apply
to any document created using the fonts or their derivatives.

DEFINITIONS
"Font Software" refers to the set of files released by the Copyright
Holder(s) under this license and clearly marked as such. This may
include source files, build scripts and documentation.

"Reserved Font Name" refers to any names specified as such after the
copyright statement(s).

"Original Version" refers to the collection of Font Software components as
distributed by the Copyright Holder(s).

"Modified Version" refers to any derivative made by adding to, deleting,
or substituting -- in part or in whole -- any of the components of the
Original Version, by changing formats or by porting the Font Software to a
new environment.

"Author" refers to any designer, engineer, programmer, technical
writer or other person who contributed to the Font Software.

PERMISSION & CONDITIONS
Permission is hereby granted, free of charge, to any person obtaining
a copy of the Font Software, to use, study, copy, merge, embed, modify,
redistribute, and sell modified and unmodified copies of the Font
Software, subject to the following conditions:

1) Neither the Font Software nor any of its individual components,
in Original or Modified Versions, may be sold by itself.

2) Original or Modified Versions of the Font Software may be bundled,
redistributed and/or sold with any software, provided that each copy
contains the above copyright notice and this license. These can be
included either as stand-alone text files, human-readable headers or
in the appropriate machine-readable metadata fields within text or
binary files as long as those fields can be easily viewed by the user.

3) No Modified Version of the Font Software may use the Reserved Font
Name(s) unless explicit written permission is granted by the corresponding
Copyright Holder. This restriction only applies to the primary font name as
presented to the users.

4) The name(s) of the Copyright Holder(s) or the Author(s) of the Font
Software shall not be used to promote, endorse or advertise any
Modified Version, except to acknowledge the contribution(s) of the
Copyright Holder(s) and the Author(s) or with their explicit written
permission.

5) The Font Software, modified or unmodified, in part or in whole,
must be distributed entirely under this license, and must not be
distributed under any other license. The requirement for fonts to
remain under this license does not apply to any document created
using the Font Software.

TERMINATION
This license becomes null and void if any of the above conditions are
not met.

DISCLAIMER
THE FONT SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
OF COPYRIGHT, PATENT, TRADEMARK, OR OTHER RIGHT. IN NO EVENT SHALL THE
COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
INCLUDING ANY GENERAL, SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL
DAMAGES, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF THE USE OR INABILITY TO USE THE FONT SOFTWARE OR FROM
OTHER DEALINGS IN THE FONT SOFTWARE.
```

## Autoblow device API
The panel uses Autoblow's public device API (https://developers.autoblow.com/). No Autoblow code or Autopilot
patterns are included; the Autopilot generator in `main/control.cpp` is this project's own. "Autoblow" and
"VacuGlide" are trademarks of their owners.
