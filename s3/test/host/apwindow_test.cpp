// Host test: Autopilot graph window math (apwindow.h).
#include <stdio.h>
#include "apwindow.h"
using namespace apwin;
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)
int main() {
  const int N = 96;
  CHECK(SPAN_MS == 40000);
  // now sits at 25 %
  CHECK(nowIndex(N) == 23);
  CHECK(nowIndex(41) == 10);  // 41 points: exactly 1 s each
  CHECK(timeAt(0, N, 60000) == 50000);
  CHECK(timeAt(N - 1, N, 60000) == 90000);
  CHECK(timeAt(10, 41, 60000) == 60000);
  // clamping at the session start: the first points are before 0
  CHECK(!inSession(timeAt(0, N, 3000), 600000));
  CHECK(inSession(timeAt(N - 1, N, 3000), 600000));
  CHECK(!inSession(timeAt(N - 1, N, 595000), 600000));  // past the end near the finish
  CHECK(inSession(0, 600000) && inSession(600000, 600000) && !inSession(-1, 600000));
  CHECK(clampMs(-500, 1000) == 0 && clampMs(5000, 1000) == 1000 && clampMs(400, 1000) == 400);
  // tap mapping round trip
  for (int i = 0; i < N; i++) CHECK(indexFor(timeAt(i, N, 123456), N, 123456) == i);
  CHECK(indexFor(49999, 41, 60000) == -1);
  CHECK(indexFor(100001, 41, 60000) == -1);
  CHECK(indexFor(60000, 41, 60000) == 10);
  // seconds labels
  CHECK(secondsRel(50000, 60000) == -10);
  CHECK(secondsRel(60000, 60000) == 0);
  CHECK(secondsRel(90000, 60000) == 30);
  CHECK(secondsRel(55400, 60000) == -5);
  CHECK(secondsRel(55600, 60000) == -4);
  // HR history ages
  CHECK(ageSec(60000, 60000) == 0);
  CHECK(ageSec(50000, 60000) == 10);
  CHECK(ageSec(49000, 60000) == -1);
  CHECK(ageSec(60001, 60000) == -1);  // future
  // dashes
  CHECK(dashOn(0) && dashOn(1) && !dashOn(2) && !dashOn(3) && dashOn(4));
  if (!fails) printf("apwindow: all ok\n");
  return fails ? 1 : 0;
}
