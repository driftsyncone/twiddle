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

## Arduino core for ESP32 and ESP-IDF (via pioarduino)

- Arduino-ESP32: LGPL-2.1, https://github.com/espressif/arduino-esp32
- ESP-IDF: Apache-2.0, https://github.com/espressif/esp-idf

These are linked into the firmware you build. This repository ships source
only; if you distribute built firmware images, follow the LGPL-2.1 terms for
the Arduino core.

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

Copyright (c) 2016-2026, Nonpolynomial Labs, LLC
https://github.com/buttplugio/buttplug

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
