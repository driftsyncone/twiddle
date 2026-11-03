// Which build-time defaults the firmware gets: the builder's include/secrets.h (a dev build) or the empty
// include/secrets.release.h (a release image, or a checkout with no secrets.h). Every source file includes this
// instead of secrets.h. A release image carries no Wi-Fi name, password, device token, web password or host:
// they come from the Twiddle-Setup portal and live in NVS only.
#pragma once

#if defined(TWIDDLE_RELEASE)
#include "secrets.release.h"
#elif __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.release.h"
#endif

#if defined(TWIDDLE_RELEASE) && (defined(OTA_PASSWORD) || defined(OTA_PASS))
#error "A release build must not have a fixed web password"
#endif

// Dev builds: a fixed web password from secrets.h (as before). A release build, or a dev build without one, makes a
// random per-device password at first boot (otapw.h).
#if !defined(TWIDDLE_RELEASE) && defined(OTA_PASSWORD)
#define TWIDDLE_DEV_WEB_PASSWORD OTA_PASSWORD
#endif
