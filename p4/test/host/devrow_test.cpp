// Host test: which buttons / controls a Devices row shows for (known, connected, connecting, hr).
#include <stdio.h>
#include "devrow.h"
using namespace devrow;
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)
int main() {
  // layout(hasAddr, known, connected, connecting, hr)
  Layout l = layout(false, true, true, false, false);  // cloud device
  CHECK(l.primary == Primary::None && !l.forget && l.follow && l.selectable);
  l = layout(true, true, true, false, false);  // connected toy
  CHECK(l.primary == Primary::None && l.forget && l.follow && l.selectable);
  l = layout(true, true, false, false, false);  // remembered toy that dropped: Connect + Forget
  CHECK(l.primary == Primary::Connect && l.forget);
  l = layout(true, true, false, true, false);  // connecting: disabled progress button, Forget stays
  CHECK(l.primary == Primary::Connecting && l.forget);
  l = layout(true, false, false, false, false);  // new scan result
  CHECK(l.primary == Primary::Connect && !l.forget);
  l = layout(true, false, false, true, false);
  CHECK(l.primary == Primary::Connecting && !l.forget);
  // Heart rate monitor: never Follow/scale, never selectable, in every state.
  for (int k = 0; k < 2; k++) for (int c = 0; c < 2; c++) for (int g = 0; g < 2; g++) {
    l = layout(true, k, c, g, true);
    CHECK(!l.follow && !l.selectable);
  }
  l = layout(true, true, false, false, true);
  CHECK(l.primary == Primary::Connect && l.forget);
  // key() differs between distinct layouts
  CHECK(layout(true, true, true, false, false).key() != layout(true, true, false, false, false).key());
  CHECK(layout(true, true, false, false, true).key() != layout(true, true, false, false, false).key());
  if (!fails) printf("devrow: all ok\n");
  return fails ? 1 : 0;
}
