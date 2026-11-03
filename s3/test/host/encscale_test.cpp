// Host test: the encoder's Autopilot scale is reset whenever a session starts (encscale.h).
#include <stdio.h>
#include "encscale.h"
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)
int main() {
  CHECK(encscale::onStart(40, false) == 100);  // new session or return from another mode: back to full
  CHECK(encscale::onStart(100, false) == 100);
  CHECK(encscale::onStart(40, true) == 40);    // resume after a pause: the lowered scale stays (no jump up)
  CHECK(encscale::onStart(20, true) == 20);
  printf(fails ? "encscale: %d FAILED\n" : "encscale: all ok\n", fails);
  return fails != 0;
}
