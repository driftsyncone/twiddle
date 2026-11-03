#include "hrmparse.h"

#include "edge.h"  // BPM_MIN / BPM_MAX: one range for the whole chain

namespace hrm {

Reading parse(const uint8_t *d, size_t n) {
  Reading r;
  if (!d || n < 2) return r;
  uint8_t flags = d[0];
  bool wide = flags & 0x01;
  r.contactSupported = flags & 0x04;
  r.contact = flags & 0x02;
  if (n < (wide ? 3u : 2u)) return r;
  r.bpm = wide ? (d[1] | (d[2] << 8)) : d[1];
  r.ok = r.bpm >= edge::BPM_MIN && r.bpm <= edge::BPM_MAX && (!r.contactSupported || r.contact);
  return r;
}

}  // namespace hrm
