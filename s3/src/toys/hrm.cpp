// Bluetooth LE heart rate monitors: the standard Heart Rate Service (0x180D) with the Heart Rate
// Measurement characteristic (0x2A37, notify). Any chest strap or watch that broadcasts it works; the
// Bluetooth SIG profile is public, so this is our own client written from that spec.
//
// This is NOT a toy: it has no output, takes no level and is never driven. The driver table is just
// where ble.cpp finds out what is in the room and how to connect. level / tick / stop do nothing, and
// devices::apply() never gives it a level. The notifications go to hrm::feed().
#include <Arduino.h>

#include "../hrm.h"
#include "../log.h"
#include "toys.h"

namespace toys {

static const uint16_t SVC_HR = 0x180D, CHR_HR = 0x2A37, SVC_BATT = 0x180F, CHR_BATT = 0x2A19;

static bool matches(const NimBLEAdvertisedDevice &adv) {
  return adv.isAdvertisingService(NimBLEUUID(SVC_HR));
}

static bool init(Ctx &c) {
  NimBLERemoteService *s = c.client->getService(NimBLEUUID(SVC_HR));
  if (!s) return false;
  NimBLERemoteCharacteristic *m = s->getCharacteristic(NimBLEUUID(CHR_HR));
  if (!m || !m->canNotify()) return false;
  bool sub = m->subscribe(true, [](NimBLERemoteCharacteristic *, uint8_t *d, size_t n, bool) { hrm::feed(d, n); });
  if (!sub) return false;
  NimBLERemoteService *b = c.client->getService(NimBLEUUID(SVC_BATT));
  c.aux = b ? b->getCharacteristic(NimBLEUUID(CHR_BATT)) : nullptr;
  c.caps = devices::CAP_HR;
  strlcpy(c.name, c.advName[0] ? c.advName : "Heart rate monitor", sizeof c.name);
  plog("ble: heart rate monitor %s subscribed, battery char %s\n", c.advName, c.aux ? "yes" : "no");
  return true;
}

static void level(Ctx &, int, uint32_t) {}
static void tick(Ctx &, uint32_t) {}
static void stop(Ctx &) {}

static void battery(Ctx &c) {
  if (!c.aux || !c.aux->canRead()) return;
  NimBLEAttValue v = c.aux->readValue();
  if (v.size() >= 1 && v[0] <= 100) c.battery = v[0];
}

const Driver hrmDriver = {'H', devices::Kind::Hrm, "Heart rate", matches, init, level, tick, stop, battery};

}  // namespace toys
