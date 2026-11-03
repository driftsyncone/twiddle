# Third-party notices

This project's own code is licensed under CC BY-NC-SA 4.0 (see `LICENSE`).
It builds on the components below, which PlatformIO downloads at build time.
They are not part of this repository and keep their own licenses.

## LVGL 9.5.0 — MIT

Copyright (c) 2025 LVGL Kft
https://github.com/lvgl/lvgl

## ArduinoJson 7 — MIT

Copyright © 2014-2026, Benoit BLANCHON
https://github.com/bblanchon/ArduinoJson

MIT License text (applies to both of the above):

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

## NimBLE-Arduino 2.5 — Apache-2.0

Copyright 2020-2026 Ryan Powell and esp-nimble-cpp, NimBLE-Arduino contributors.
Parts derived from esp32-snippets, Copyright 2017 Neil Kolban.
https://github.com/h2zero/NimBLE-Arduino (license and NOTICE in the library).

The library's `NOTICE` file (Apache-2.0 section 4(d)), copied verbatim from NimBLE-Arduino 2.5.2:

```
esp-nimble-cpp
NimBLE-Arduino
Copyright 2020-2026 Ryan Powell <ryan@nable-embedded.io> and
esp-nimble-cpp, NimBLE-Arduino contributors.

The Initial Developer of some parts of this library, which are copied from,
derived from, or inspired by is, esp32-snippets, Copyright 2017 Neil Kolban.

If this library is used for commercial purposes, it is requested that the user consider
making a donation and/or sponsoring this project to support it's ongoing development.
```

## Montserrat fonts in LVGL - SIL Open Font License 1.1
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

## Arduino core for ESP32 and ESP-IDF (via pioarduino)

- Arduino-ESP32 3.3.11: **LGPL-2.1-or-later**, https://github.com/espressif/arduino-esp32. The core and nine of its libraries (WiFi,
  Network, WebServer, FS, SPI, Wire, AsyncUDP, DNSServer, and parts of others) are **statically linked** into the firmware image.
  Other linked Arduino-ESP32 libraries are Apache-2.0 (Preferences, Update, SD, LittleFS, Hash) or MIT (ESPmDNS).
- ESP-IDF 5.5.5 prebuilt libraries (framework-arduinoespressif32-libs): Apache-2.0, with Espressif's Wi-Fi, PHY, coexistence and
  Bluetooth controller binaries (no source) which Espressif states are Apache-2.0; hostap/wpa_supplicant (BSD option of a BSD/GPL dual
  header); GCC runtime (GPL-3.0-or-later with the Runtime Library Exception) and newlib.

This repository ships source only. If you distribute built firmware images, follow the LGPL-2.1 terms for Arduino-ESP32: see
[docs/lgpl-compliance.md](docs/lgpl-compliance.md) (draft) and `tools/lgpl-package.sh`, which builds the corresponding-source package for a
release. The web flasher's licence page for each S3 version is generated from the linker map of that exact image.

## Autoblow device API

The panel uses Autoblow's public device API
(https://developers.autoblow.com/). No Autoblow code or Autopilot patterns are
included; the Autopilot generator in `src/control.cpp` is this project's own.

## Buttplug device protocols — BSD-3-Clause

The Bluetooth toy protocols in `src/toys/lovense.cpp` and `src/toys/kiiroo.cpp`
(service and characteristic UUIDs, command formats, the Lovense model table and
the Keon speed formula) are derived from the Buttplug project's device
configuration and protocol implementations. Attribution only; this project is
not affiliated with or endorsed by Nonpolynomial Labs.

Copyright (c) 2016-2023, Nonpolynomial Labs, LLC
All rights reserved.
https://github.com/buttplugio/buttplug

Text and copyright line copied verbatim from the upstream `LICENSE` file, https://raw.githubusercontent.com/buttplugio/buttplug/master/LICENSE (fetched 2026-11-02).

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

## FeelRobotics keon-wifi-sdk — MIT

The Keon 2 / Keon WiFi details in `src/toys/kiiroo.cpp` (UUIDs, movement mode,
move and pause packets, battery characteristic) follow FeelRobotics' SDK.

Copyright (c) 2025 FeelRobotics
https://github.com/FeelRobotics/keon-wifi-sdk

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
