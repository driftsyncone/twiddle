// The one place that decides where the build-time defaults come from. Every file that used to include "secrets.h"
// includes this instead.
//
//  - Release build (cmake -DTWIDDLE_RELEASE=ON, which defines TWIDDLE_RELEASE): ALWAYS secrets.release.h, even if a
//    secrets.h exists on this machine, so a release can never pick up the builder's Wi-Fi or tokens.
//  - Dev build: secrets.h if it exists (gitignored, copied from secrets.example.h), else secrets.release.h.
#pragma once

#if defined(TWIDDLE_RELEASE)
#include "secrets.release.h"
#elif defined(__has_include)
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.release.h"
#endif
#else
#include "secrets.h"
#endif

// Dev builds only: a password compiled into the image (OTA_PASSWORD, or the bring-up name OTA_PASS) overrides the
// per-device random one. A release image defines neither.
#if defined(OTA_PASSWORD)
#define TWIDDLE_DEV_WEB_PASSWORD OTA_PASSWORD
#elif defined(OTA_PASS)
#define TWIDDLE_DEV_WEB_PASSWORD OTA_PASS
#endif
#if defined(TWIDDLE_RELEASE) && defined(TWIDDLE_DEV_WEB_PASSWORD)
#error "A release build must not compile in a web password"
#endif
