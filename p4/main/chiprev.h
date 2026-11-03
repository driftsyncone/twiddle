// ESP32-P4 silicon revision, as esp_chip_info() reports it (major * 100 + minor, so v1.3 = 103). Pure logic (host-tested).
//
// This image is built for REV_MIN v0.1 .. REV_MAX v1.99 (ESP-IDF 5.5.1; the maximum is fixed by IDF and cannot be
// raised in sdkconfig). The ROM-started second-stage bootloader refuses an app whose max revision is below the chip's,
// so a v3.0 chip never gets as far as this code with this IDF version: the warning below matters for revisions that
// boot (v1.x) but were never tested. Tested: v1.3 only.
#pragma once

namespace chiprev {

static const unsigned TESTED = 103;      // v1.3
static const unsigned IDF_MAX = 199;     // CONFIG_ESP32P4_REV_MAX_FULL of this build

// nullptr when the revision is the tested one; otherwise a short sentence for the log and the status page.
inline const char *warning(unsigned rev) {
  if (rev == TESTED) return nullptr;
  if (rev > IDF_MAX) return "chip revision is newer than this firmware supports: it may misbehave (tested on v1.3 only)";
  return "chip revision not tested with Twiddle (tested on v1.3 only)";
}

}  // namespace chiprev
