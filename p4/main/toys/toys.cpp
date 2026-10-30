// The driver list: which protocol a toy speaks.
#include "toys.h"

namespace toys {

static const Driver *const ALL[] = {&lovense, &keon, &keon2, &ossm};

const Driver *match(const NimBLEAdvertisedDevice &adv) {
  for (const Driver *d : ALL)
    if (d->matches(adv)) return d;
  return nullptr;
}

const Driver *byId(char id) {
  for (const Driver *d : ALL)
    if (d->id == id) return d;
  return nullptr;
}

}  // namespace toys
