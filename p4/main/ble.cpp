// Bluetooth LE toys: the toy table, the BLE tasks and the helpers the drivers
// in toys/ use. See ble.h for the contract.
//
// Tasks (all on core 0):
// - callers (loop task): only take the mutex, change what they want and wake
//   a task. Nothing blocks.
// - "blesvc" (priority 6): STOP, levels and ticks for every toy that's up.
//   Only non-blocking work: toys::write() queues writes with the raw NimBLE
//   host API, so STOP goes out within one pass (~10 ms) even while the worker
//   sits in a blocking GATT call that can take up to the 30 s ATT timeout.
// - "ble" worker (priority 5): everything that blocks: connect, discovery,
//   driver init, battery reads, disconnects, NVS. It only blocks on the toy it
//   is working on.
// - the NimBLE host task: scan results, link events and notifications. These
//   update the table (mutex) or a reply buffer (spinlock) and wake the worker.
//
// Waking uses binary semaphores, never task notifications: NimBLE-Arduino's
// blocking calls wait on the calling task's notification (NimBLEUtils::
// taskWait), and a stray notify would end such a wait early.
//
// Locks: svcMtx is held for a whole blesvc pass; the worker takes it when it
// tears a link down or stops a toy itself, so no pass ever uses a
// characteristic that is being deleted. Order: svcMtx before mtx. Nobody holds
// mtx across a NimBLE call.
//
// NimBLE allows 3 links (CONFIG_BT_NIMBLE_MAX_CONNECTIONS). The 3 client
// objects are made once and lent to whichever toy connects; they're never
// deleted. A link is only handed back once its client is disconnected, and
// its services (with the notify callbacks pointing at the old toy's Ctx) are
// deleted then, so a notification can't land in another toy's Ctx.
#include "ble.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>

#include <algorithm>

#include "esp_heap_caps.h"
#include "esp_hosted_bt_host_stack.h"

#include "log.h"
#include "mem.h"
#include "settings.h"
#include "toys/toys.h"

using std::max;
using std::min;

namespace ble {

static const int MAX_TOYS = 8;
static const int MAX_LINKS = 3;
static const uint32_t CONNECT_TIMEOUT_MS = 4000;
// A remembered toy that isn't there (switched off, away): try again after 15 s,
// then 30, 60 and every 120 s. Each try listens for it for CONNECT_TIMEOUT_MS,
// and that listening takes the shared radio away from Wi-Fi.
static const uint32_t RETRY_MS = 15000;
static const uint32_t RETRY_MAX_MS = 120000;
// Radio time while connecting / scanning (0.625 ms units for the connect,
// ms for the scan): a quarter / half of the time, so Wi-Fi keeps the rest.
// NimBLE's defaults listen 100 % of the time.
static const uint16_t CONNECT_SCAN_ITVL = 64, CONNECT_SCAN_WINDOW = 16;  // 40 ms / 10 ms
static const uint16_t SCAN_ITVL_MS = 100, SCAN_WINDOW_MS = 50;
static const uint32_t BATTERY_MS = 60000;
static const uint32_t BATTERY_RETRY_MS = 10000;
static const uint32_t CLOSE_TIMEOUT_MS = 5000;  // > connect timeout: a cancelled attempt is over by then
static const uint32_t DISCONNECT_RETRY_MS = 1000;
static const char *NS = "bletoys";

enum class St : uint8_t {
  Idle,        // no link
  Connecting,  // async connect running
  Linked,      // link up, driver init still to run
  Up,          // initialised, taking levels
  Closing,     // going down: the link is handed back once the client is disconnected
};

struct Slot {
  bool used;
  bool listed;   // in order[] (false once forget() has taken it off)
  bool known;    // remembered in NVS
  const toys::Driver *drv;
  char addr[18];
  uint8_t addrType;
  char name[24];
  uint8_t caps;
  int rssi;
  char status[48];
  // from callers
  bool connectReq, forgetReq;
  int desired;
  // from the NimBLE host task
  bool evConnected;
  bool linkDown;     // connect failed or link dropped: no connection left
  bool dropped;      // an initialised link dropped (vs. a connect that failed)
  // shared (mutex)
  St st;
  int link;          // links[] index, -1 none
  bool established;  // initialised link: a drop counts
  bool busy;         // the worker is in this toy's battery(): blesvc only stops it
  uint32_t drops;
  // blesvc task
  int applied;       // level last given to the driver, -1 none yet
  bool armed;        // seen desired == 0 since reaching Up
  bool heldLogged;
  bool stopOwed;     // STOP came while a write with response was in flight: sent when it's answered
  // worker
  bool unsupported;
  uint32_t lastTry, lastBattery, closingSince, lastDisconnect;
  uint32_t retryMs;  // wait before the next automatic connect (backs off)
  uint8_t fails;     // automatic connects in a row that found nothing
  toys::Ctx ctx;
};

class LinkCb : public NimBLEClientCallbacks {
 public:
  int link = 0;
  void onConnect(NimBLEClient *) override;
  void onConnectFail(NimBLEClient *, int reason) override;
  void onDisconnect(NimBLEClient *, int reason) override;
};

struct Link {
  NimBLEClient *client;
  LinkCb cb;
  int slot;  // -1 free
};

class ScanCb : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice *adv) override;
  void onScanEnd(const NimBLEScanResults &, int reason) override;
};

static SemaphoreHandle_t mtx, svcMtx, workSem, svcSem;
static TaskHandle_t svcHandle;
static Slot slots[MAX_TOYS];
static int order[MAX_TOYS];  // public index -> slot
static int nOrder;
static Link links[MAX_LINKS];
static ScanCb scanCb;
static bool scanWanted, scanRunning, stopReq;
static bool on;  // the stack and the tasks are running (see begin())
static uint32_t scanMs;

struct Lock {
  SemaphoreHandle_t m;
  explicit Lock(SemaphoreHandle_t h = mtx) : m(h) { xSemaphoreTake(m, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(m); }
};

static void wakeWorker() {
  if (workSem) xSemaphoreGive(workSem);
}
static void wakeSvc() {
  if (svcSem) xSemaphoreGive(svcSem);
}

static void setSt(Slot &s, St st, const char *status = nullptr) {
  Lock l;
  s.st = st;
  if (status) strlcpy(s.status, status, sizeof s.status);
}

// ---------- NVS: remembered toys ----------

struct Stored {
  char addr[18];
  uint8_t type;
  char drv;
  uint8_t caps;
  char name[24];
  char adv[32];
};

static void nvsKey(int k, char *out) { snprintf(out, 8, "t%d", k & 0xf); }

static void nvsSave(const Slot &s) {
  Stored st = {};
  strlcpy(st.addr, s.addr, sizeof st.addr);
  st.type = s.addrType;
  st.drv = s.drv->id;
  st.caps = s.caps;
  strlcpy(st.name, s.name, sizeof st.name);
  strlcpy(st.adv, s.ctx.advName, sizeof st.adv);
  Preferences p;
  p.begin(NS, false);
  int slot = -1;
  char key[8];
  for (int k = 0; k < MAX_TOYS; k++) {
    nvsKey(k, key);
    Stored old;
    if (!p.isKey(key) || p.getBytes(key, &old, sizeof old) != sizeof old) {
      if (slot < 0) slot = k;
      continue;
    }
    if (!strcmp(old.addr, st.addr)) {
      if (!memcmp(&old, &st, sizeof st)) slot = -2;  // unchanged
      else slot = k;
      break;
    }
  }
  if (slot >= 0) {
    nvsKey(slot, key);
    p.putBytes(key, &st, sizeof st);
  }
  p.end();
}

static void nvsRemove(const char *addr) {
  Preferences p;
  p.begin(NS, false);
  char key[8];
  for (int k = 0; k < MAX_TOYS; k++) {
    nvsKey(k, key);
    Stored old;
    if (p.isKey(key) && p.getBytes(key, &old, sizeof old) == sizeof old && !strcmp(old.addr, addr))
      p.remove(key);
  }
  p.end();
}

static void initSlot(Slot &s, const toys::Driver *drv, const char *addr, uint8_t type) {
  s = Slot{};
  s.used = true;
  s.drv = drv;
  strlcpy(s.addr, addr, sizeof s.addr);
  s.addrType = type;
  s.link = -1;
  s.applied = -1;
  s.ctx.battery = -1;
  s.retryMs = RETRY_MS;
}

static void nvsLoad() {
  Preferences p;
  p.begin(NS, true);
  char key[8];
  for (int k = 0; k < MAX_TOYS; k++) {
    nvsKey(k, key);
    Stored st;
    if (!p.isKey(key) || p.getBytes(key, &st, sizeof st) != sizeof st) continue;
    const toys::Driver *drv = toys::byId(st.drv);
    if (!drv || nOrder >= MAX_TOYS) continue;
    Slot &s = slots[nOrder];
    initSlot(s, drv, st.addr, st.type);
    s.known = true;
    s.listed = true;
    s.caps = st.caps;
    strlcpy(s.name, st.name, sizeof s.name);
    strlcpy(s.ctx.advName, st.adv, sizeof s.ctx.advName);
    strcpy(s.status, "Not connected");
    order[nOrder] = nOrder;
    nOrder++;
    plog("ble: remembered %s %s (%s)\n", s.name, s.addr, drv->brand);
  }
  p.end();
}

// ---------- NimBLE host task callbacks ----------

static void linkEvent(int link, int kind, int reason) {
  char name[24] = "";
  uint32_t drops = 0;
  bool dropped = false;
  {
    Lock l;
    int i = links[link].slot;
    if (i < 0) return;
    Slot &s = slots[i];
    if (kind == 0) {
      s.evConnected = true;
    } else {
      s.linkDown = true;
      // Counted here, not in the worker, so drops is current at once even while
      // the worker is busy with another toy.
      if (kind == 2 && s.established) {
        s.dropped = true;
        s.drops++;
        dropped = true;
        drops = s.drops;
      }
      s.established = false;
      if (s.st != St::Idle && s.st != St::Closing) {
        s.st = St::Closing;
        if (!s.unsupported) strcpy(s.status, "Out of range");
      }
    }
    strlcpy(name, s.name, sizeof name);
  }
  if (dropped) plog("ble: %s DROPPED (reason %d, drop #%u)\n", name, reason, (unsigned)drops);
  else if (kind == 1) plog("ble: %s connect failed (%d)\n", name, reason);
  else if (kind == 2) plog("ble: %s disconnected (reason %d)\n", name, reason);
  wakeWorker();
}

void LinkCb::onConnect(NimBLEClient *) { linkEvent(link, 0, 0); }
void LinkCb::onConnectFail(NimBLEClient *, int reason) { linkEvent(link, 1, reason); }
void LinkCb::onDisconnect(NimBLEClient *, int reason) { linkEvent(link, 2, reason); }

static uint32_t advSeen, advOurs;  // this scan, for the log line when it ends

void ScanCb::onResult(const NimBLEAdvertisedDevice *adv) {
  advSeen++;
  const toys::Driver *drv = toys::match(*adv);
  if (!drv) return;
  advOurs++;
  std::string a = adv->getAddress().toString();
  std::string n = adv->getName();
  int rssi = adv->getRSSI();
  bool added = false;
  {
    Lock l;
    for (Slot &s : slots) {
      if (!s.used || strcmp(s.addr, a.c_str())) continue;
      if (!s.forgetReq) s.rssi = rssi;
      if (s.known && s.st == St::Idle) {  // it's here after all: connect after the scan
        s.fails = 0;
        s.retryMs = RETRY_MS;
        s.lastTry = millis() - RETRY_MS;
      }
      return;
    }
    if (nOrder >= MAX_TOYS) return;
    for (int i = 0; i < MAX_TOYS; i++) {
      Slot &s = slots[i];
      if (s.used) continue;
      initSlot(s, drv, a.c_str(), adv->getAddress().getType());
      s.listed = true;
      s.rssi = rssi;
      strlcpy(s.ctx.advName, n.c_str(), sizeof s.ctx.advName);
      strlcpy(s.name, n.empty() ? drv->brand : n.c_str(), sizeof s.name);
      strcpy(s.status, "Not connected");
      order[nOrder++] = i;
      added = true;
      break;
    }
  }
  if (added) plog("ble: found %s %s rssi %d (%s)\n", n.c_str(), a.c_str(), rssi, drv->brand);
}

void ScanCb::onScanEnd(const NimBLEScanResults &, int reason) {
  {
    Lock l;
    scanRunning = false;
  }
  plog("ble: scan done (%d), %u adverts, %u from toys\n", reason, (unsigned)advSeen, (unsigned)advOurs);
  advSeen = advOurs = 0;
  wakeWorker();
}

// ---------- blesvc task: STOP, levels, ticks ----------

static int slotIndex(const Slot &s) { return &s - slots; }

static bool servicePass() {
  Lock sv(svcMtx);
  bool stop, up[MAX_TOYS], busy[MAX_TOYS], any = false;
  int want[MAX_TOYS];
  {
    Lock l;
    stop = stopReq;
    stopReq = false;
    for (int i = 0; i < MAX_TOYS; i++) {
      const Slot &s = slots[i];
      up[i] = s.used && s.st == St::Up;
      busy[i] = s.busy;
      want[i] = s.desired;
      any |= up[i];
    }
  }
  uint32_t now = millis();
  // 1. STOP to every toy first, before any level or tick goes out. A toy whose
  // write with response is still in flight gets it the moment the reply comes
  // (the reply wakes this task), ahead of its levels.
  for (int i = 0; i < MAX_TOYS; i++) {
    if (!up[i]) continue;
    Slot &s = slots[i];
    bool blocked = !s.ctx.txNoResponse && s.ctx.wrPending;
    if (stop) {
      s.applied = 0;
      if (blocked) {
        s.stopOwed = true;
        continue;
      }
      s.stopOwed = false;
      s.drv->stop(s.ctx);
    } else if (s.stopOwed && !blocked) {
      s.stopOwed = false;
      s.drv->stop(s.ctx);
    }
  }
  // 2. Levels and ticks.
  for (int i = 0; i < MAX_TOYS; i++) {
    if (!up[i]) continue;
    Slot &s = slots[i];
    if (busy[i] || s.stopOwed) continue;  // levels once its battery() / its STOP is done
    // After (re)connecting, stay at 0 until the panel has asked for 0 once, so
    // a toy coming back doesn't start up by itself.
    int w = want[i];
    if (!s.armed) {
      if (w == 0) {
        s.armed = true;
      } else {
        if (!s.heldLogged) plog("ble: %s held at 0 until the panel's level is 0\n", s.name);
        s.heldLogged = true;
        w = 0;
      }
    }
    if (w != s.applied) {
      s.drv->level(s.ctx, w, now);
      s.applied = w;
    }
    s.drv->tick(s.ctx, now);
  }
  if (stop) plog("ble: stop all\n");
  return any;
}

static void svcTask(void *) {
  for (;;) {
    bool any = servicePass();
    xSemaphoreTake(svcSem, pdMS_TO_TICKS(any ? 10 : 100));
  }
}

// ---------- worker task ----------

// Hand the link back. Only with the client disconnected: then no callback or
// notification can come from it, and its old services can go.
static void releaseLink(Slot &s) {
  NimBLEClient *c = nullptr;
  {
    Lock sv(svcMtx);
    Lock l;
    if (s.link >= 0) {
      c = links[s.link].client;
      links[s.link].slot = -1;
    }
    s.link = -1;
    s.established = false;
    s.ctx.client = nullptr;
    s.ctx.tx = s.ctx.rx = s.ctx.aux = nullptr;
    s.ctx.wrPending = false;
  }
  if (c) c->deleteServices();
}

static void freeSlot(Slot &s) {
  releaseLink(s);
  Lock l;
  s.used = false;
}

static void startClosing(Slot &s, const char *status = nullptr) {
  NimBLEClient *c = s.link >= 0 ? links[s.link].client : nullptr;
  bool connecting = s.st == St::Connecting;
  {
    Lock l;
    s.st = St::Closing;
    if (status) strlcpy(s.status, status, sizeof s.status);
  }
  s.closingSince = s.lastDisconnect = millis();
  if (!c) return;
  if (connecting) c->cancelConnect();
  else c->disconnect();
}

static void handleEvents() {
  for (Slot &s : slots) {
    bool con;
    St st;
    {
      Lock l;
      if (!s.used) continue;
      con = s.evConnected;
      s.evConnected = false;
      st = s.st;
    }
    if (!con) continue;
    if (st == St::Connecting) {
      plog("ble: %s connected\n", s.name);
      Lock l;
      if (s.st == St::Connecting) {  // not dropped meanwhile
        s.st = St::Linked;
        strcpy(s.status, "Connected, identifying...");
      }
    } else if (st == St::Closing && s.link >= 0) {
      // The connect won the race against cancelConnect(): take it down again.
      plog("ble: %s connected while closing, disconnecting\n", s.name);
      links[s.link].client->disconnect();
      s.lastDisconnect = millis();
    }
  }
}

// Closing toys: hand the link back once the disconnect (or connect-failed)
// event has arrived. Without one, only once the client has no connection
// handle left and the close has timed out; while it has one, keep asking it to
// disconnect. (isConnected() turns false as soon as disconnect() is called,
// while the link may still be up and its notifications still walk the
// services, so it can't be used for this.)
static void handleClosing() {
  uint32_t now = millis();
  for (Slot &s : slots) {
    bool down, forget;
    {
      Lock l;
      if (!s.used || s.st != St::Closing) continue;
      down = s.linkDown;
      forget = s.forgetReq;
    }
    if (s.link >= 0 && !down) {
      NimBLEClient *c = links[s.link].client;
      if (c->getConnHandle() != BLE_HS_CONN_HANDLE_NONE) {
        if (now - s.lastDisconnect >= DISCONNECT_RETRY_MS) {
          if (now - s.closingSince > CLOSE_TIMEOUT_MS)
            plog("ble: %s still connected, asking it to disconnect again\n", s.name);
          c->disconnect();  // BLE_HS_EALREADY while one is under way: fine
          s.lastDisconnect = now;
        }
        continue;
      }
      if (now - s.closingSince < CLOSE_TIMEOUT_MS) continue;
      plog("ble: %s: no link event, letting the link go\n", s.name);
    }
    if (forget) {
      // Forgotten while it was going down: handleForgets() skipped it.
      if (s.known) nvsRemove(s.addr);
      {
        Lock l;
        s.known = false;
      }
      freeSlot(s);
      continue;
    }
    releaseLink(s);
    setSt(s, St::Idle, s.unsupported ? "Not supported" : "Out of range");
    s.lastTry = now;
    if (s.dropped) {  // it was just here: try again in 2 s
      s.dropped = false;
      s.fails = 0;
      s.retryMs = 2000;
    } else {  // nothing answered: wait longer each time
      if (s.fails < 8) s.fails++;
      s.retryMs = min<uint32_t>(RETRY_MS << min<int>(s.fails - 1, 3), RETRY_MAX_MS);
      if (s.known) plog("ble: %s not found, next try in %u s\n", s.name, (unsigned)(s.retryMs / 1000));
    }
  }
}

static void handleForgets() {
  for (Slot &s : slots) {
    St st;
    {
      Lock l;
      if (!s.used || !s.forgetReq || s.st == St::Closing) continue;
      st = s.st;
    }
    plog("ble: forgetting %s %s\n", s.name, s.addr);
    if (st == St::Up) {
      Lock sv(svcMtx);  // not while a blesvc pass is driving it
      s.drv->stop(s.ctx);
    }
    if (s.known) nvsRemove(s.addr);
    {
      Lock l;
      s.known = false;
    }
    if (s.link < 0) freeSlot(s);
    else startClosing(s);
  }
}

static void startScan() {
  uint32_t ms;
  {
    Lock l;
    if (!scanWanted || scanRunning) return;
    for (Slot &s : slots)
      if (s.used && s.st == St::Connecting) return;  // after the connect attempt
    scanWanted = false;
    scanRunning = true;
    ms = scanMs;
  }
  bool ok = NimBLEDevice::getScan()->start(ms, false, true);
  plog("ble: scan %u ms %s\n", (unsigned)ms, ok ? "started" : "FAILED");
  if (!ok) {
    Lock l;
    scanRunning = false;
  }
}

static void startConnects() {
  uint32_t now = millis();
  Slot *pick = nullptr;
  int link = -1;
  {
    Lock l;
    if (scanRunning || scanWanted) return;
    for (Slot &s : slots)
      if (s.used && s.st == St::Connecting) return;  // one at a time
    for (int i = 0; i < MAX_LINKS; i++)
      if (links[i].slot < 0) link = i;
    for (Slot &s : slots)
      if (s.used && s.connectReq && !s.forgetReq) {
        s.connectReq = false;
        if (s.st != St::Idle) continue;
        if (link < 0) {
          strcpy(s.status, "Too many toys connected (max 3)");
          continue;
        }
        pick = &s;
        break;
      }
    if (!pick && link >= 0)
      for (Slot &s : slots)
        if (s.used && s.known && !s.forgetReq && s.st == St::Idle && now - s.lastTry >= s.retryMs) {
          pick = &s;
          break;
        }
    if (!pick) return;
    pick->lastTry = now;
    pick->unsupported = false;
    pick->evConnected = pick->linkDown = false;
    pick->link = link;
    pick->st = St::Connecting;
    strcpy(pick->status, "Connecting...");
    links[link].slot = slotIndex(*pick);
  }
  Slot &s = *pick;
  NimBLEClient *c = links[link].client;
  s.ctx.client = c;
  plog("ble: connecting %s %s\n", s.name, s.addr);
  if (!c->connect(NimBLEAddress(std::string(s.addr), s.addrType), true, true, true)) {
    plog("ble: %s connect didn't start (%d)\n", s.name, c->getLastError());
    releaseLink(s);
    setSt(s, St::Idle, "Out of range");
  }
}

static void handleInits() {
  for (Slot &s : slots) {
    {
      Lock l;
      if (!s.used || s.st != St::Linked || s.forgetReq) continue;
    }
    toys::Ctx &c = s.ctx;
    c.client = links[s.link].client;
    c.tx = c.rx = c.aux = nullptr;
    c.rxSeq = 0;
    c.rxBuf[0] = 0;
    c.wrPending = c.wrRefused = false;
    if (!c.advName[0]) strlcpy(c.advName, s.name, sizeof c.advName);
    bool ok = s.drv->init(c);
    bool published = false, linked;
    {
      Lock l;
      linked = s.st == St::Linked;
      if (linked && !s.forgetReq && ok) {
        strlcpy(s.name, c.name, sizeof s.name);
        s.caps = c.caps;
        s.known = true;
        s.established = true;
        s.applied = -1;
        s.armed = false;
        s.heldLogged = false;
        s.stopOwed = false;
        s.ctx.battery = -1;  // a fresh reading per connect, not the last link's
        s.fails = 0;
        s.retryMs = RETRY_MS;
        s.st = St::Up;  // published last: blesvc picks it up from here
        strcpy(s.status, "Ready");
        published = true;
      }
    }
    if (published) {
      s.lastBattery = millis() - BATTERY_MS;  // read it right away
      nvsSave(s);
      plog("ble: %s ready (%s, caps 0x%02x, writes %s)\n", s.name, s.addr, s.caps,
           c.txNoResponse ? "without response" : "WITH response, one at a time");
      wakeSvc();
    } else if (!ok && linked) {
      plog("ble: %s not supported, disconnecting\n", c.advName);
      s.unsupported = true;
      startClosing(s, "Not supported");
    }
    // Dropped during init (now Closing) or forgotten: handleClosing / handleForgets.
  }
}

static void pollBattery() {
  uint32_t now = millis();
  for (Slot &s : slots) {
    {
      Lock l;
      // No reading yet (or a Lush's "0;" just after switching on): ask again sooner.
      uint32_t every = s.ctx.battery < 0 ? BATTERY_RETRY_MS : BATTERY_MS;
      if (!s.used || s.st != St::Up || now - s.lastBattery < every) continue;
      s.busy = true;
    }
    s.lastBattery = now;
    s.drv->battery(s.ctx);
    {
      Lock l;
      s.busy = false;
    }
    return;  // one per pass
  }
}

static void workTask(void *) {
  bool stacksLogged = false;
  for (;;) {
    if (!stacksLogged && millis() > 60000) {
      stacksLogged = true;
      plog("ble: stack never used: blesvc %u bytes, ble %u bytes\n",
           (unsigned)uxTaskGetStackHighWaterMark(svcHandle), (unsigned)uxTaskGetStackHighWaterMark(nullptr));
    }
    handleEvents();
    handleForgets();
    handleClosing();
    handleInits();
    pollBattery();
    startScan();
    startConnects();
    xSemaphoreTake(workSem, pdMS_TO_TICKS(100));
  }
}

// ---------- API ----------

void begin() {
  mtx = xSemaphoreCreateMutex();
  svcMtx = xSemaphoreCreateMutex();
  workSem = xSemaphoreCreateBinary();
  svcSem = xSemaphoreCreateBinary();
  nvsLoad();
  // Bluetooth shares the radio with Wi-Fi, and with it on, Wi-Fi has to use
  // modem sleep (slower, misses more on a weak signal). So only when wanted:
  // switched on in Settings, or (auto) when a toy is remembered.
  on = settings.bluetooth == 1 || (settings.bluetooth < 0 && nOrder > 0);
  if (!on) {
    for (int i = 0; i < nOrder; i++) strcpy(slots[order[i]].status, "Bluetooth is off (Settings)");
    plog("ble: off, Wi-Fi without modem sleep\n");
    return;
  }
  memReport("before BLE");
  // The controller lives on the C6: bring it up over esp_hosted and bind NimBLE's
  // HCI to it, then esp-nimble-cpp's init() only starts the host (IDF >= 5).
  esp_hosted_bt_host_stack_cfg_t bt = ESP_HOSTED_BT_HOST_STACK_CONFIG_DEFAULT();
  esp_err_t err = esp_hosted_bt_host_stack_setup(&bt);
  if (err != ESP_OK || !NimBLEDevice::init("twiddle")) {
    plog("ble: start failed (%s), Bluetooth stays off\n", err != ESP_OK ? esp_err_to_name(err) : "NimBLE init");
    on = false;
    for (int i = 0; i < nOrder; i++) strcpy(slots[order[i]].status, "Bluetooth failed to start");
    return;
  }
  for (int i = 0; i < MAX_LINKS; i++) {
    Link &k = links[i];
    k.slot = -1;
    k.cb.link = i;
    k.client = NimBLEDevice::createClient();
    k.client->setClientCallbacks(&k.cb, false);
    k.client->setConnectTimeout(CONNECT_TIMEOUT_MS);
    k.client->setConnectionParams(BLE_GAP_INITIAL_CONN_ITVL_MIN, BLE_GAP_INITIAL_CONN_ITVL_MAX,
                                  BLE_GAP_INITIAL_CONN_LATENCY, BLE_GAP_INITIAL_SUPERVISION_TIMEOUT,
                                  CONNECT_SCAN_ITVL, CONNECT_SCAN_WINDOW);
  }
  NimBLEScan *sc = NimBLEDevice::getScan();
  sc->setScanCallbacks(&scanCb, false);
  sc->setActiveScan(true);  // names often come in the scan response
  sc->setMaxResults(0);     // callbacks only, keep nothing
  sc->setInterval(SCAN_ITVL_MS);
  sc->setWindow(SCAN_WINDOW_MS);
  memReport("after BLE");
  xTaskCreatePinnedToCore(svcTask, "blesvc", 4096, nullptr, 6, &svcHandle, 0);
  xTaskCreatePinnedToCore(workTask, "ble", 6144, nullptr, 5, nullptr, 0);
}

bool enabled() { return on; }

int count() {
  Lock l;
  return nOrder;
}

Toy toy(int i) {
  Lock l;
  Toy t = {};
  if (i < 0 || i >= nOrder) return t;
  const Slot &s = slots[order[i]];
  t.kind = s.drv->kind;
  strlcpy(t.name, s.name, sizeof t.name);
  strlcpy(t.addr, s.addr, sizeof t.addr);
  t.caps = s.caps;
  t.known = s.known;
  t.connected = s.st == St::Up;
  t.connecting = s.st == St::Connecting || s.st == St::Linked || s.connectReq;
  t.battery = s.ctx.battery;
  t.rssi = s.rssi;
  t.level = t.connected && s.applied > 0 ? s.applied : 0;
  t.drops = s.drops;
  t.held = t.connected && !s.armed;
  if (t.connected) strcpy(t.status, t.level ? "Running" : "Ready");
  else strlcpy(t.status, s.status, sizeof t.status);
  return t;
}

void scan(uint32_t ms) {
  if (!on) return;
  {
    Lock l;
    if (scanWanted || scanRunning) return;
    // Drop what the last scan found and nobody connected; remembered toys first.
    int n = 0, keep[MAX_TOYS];
    for (int pass = 0; pass < 2; pass++)
      for (int j = 0; j < nOrder; j++) {
        Slot &s = slots[order[j]];
        bool stays = s.known || s.st != St::Idle || s.connectReq || s.link >= 0;
        if (pass == 0 && s.known) keep[n++] = order[j];
        if (pass == 1 && !s.known) {
          if (stays) keep[n++] = order[j];
          else s.used = false;
        }
      }
    memcpy(order, keep, n * sizeof keep[0]);
    nOrder = n;
    scanWanted = true;
    scanMs = ms;
  }
  wakeWorker();
}

bool scanning() {
  Lock l;
  return scanWanted || scanRunning;
}

void connect(int i) {
  if (!on) return;
  {
    Lock l;
    if (i < 0 || i >= nOrder) return;
    Slot &s = slots[order[i]];
    if (s.st != St::Idle) return;
    s.connectReq = true;
    strcpy(s.status, "Connecting...");
  }
  wakeWorker();
}

void forget(int i) {
  if (!on) {  // no worker: forget it right here (it never connected this boot)
    char addr[18];
    {
      Lock l;
      if (i < 0 || i >= nOrder) return;
      Slot &s = slots[order[i]];
      strlcpy(addr, s.addr, sizeof addr);
      s.used = false;
      memmove(order + i, order + i + 1, (nOrder - i - 1) * sizeof order[0]);
      nOrder--;
    }
    nvsRemove(addr);
    return;
  }
  {
    Lock l;
    if (i < 0 || i >= nOrder) return;
    Slot &s = slots[order[i]];
    s.forgetReq = true;
    s.listed = false;
    s.desired = 0;
    s.connectReq = false;
    memmove(order + i, order + i + 1, (nOrder - i - 1) * sizeof order[0]);
    nOrder--;
  }
  wakeWorker();
  wakeSvc();
}

void setLevel(int i, int level) {
  level = constrain(level, 0, 100);
  {
    Lock l;
    if (i < 0 || i >= nOrder) return;
    Slot &s = slots[order[i]];
    if (s.desired == level) return;
    s.desired = level;
  }
  wakeSvc();
}

void stopAll() {
  {
    Lock l;
    for (Slot &s : slots) s.desired = 0;
    stopReq = true;
  }
  wakeSvc();
}

}  // namespace ble

// ---------- helpers for the drivers ----------

namespace toys {

static portMUX_TYPE rxMux = portMUX_INITIALIZER_UNLOCKED;

// Host task: the reply to a write with response (or its failure / timeout).
static int writeDone(uint16_t, const ble_gatt_error *, ble_gatt_attr *, void *arg) {
  static_cast<Ctx *>(arg)->wrPending = false;
  ble::wakeSvc();  // a STOP may be waiting for this
  return 0;
}

// Never waits: the raw host API queues the write. A write with response takes
// one of NimBLE's few GATT procedures (CONFIG_BT_NIMBLE_GATT_MAX_PROCS) and
// requests on a link are served one at a time, so each toy gets at most one in
// flight; that keeps one hung toy from using up the procedures STOP needs for
// the others. Safe from any task.
bool write(Ctx &c, const uint8_t *data, size_t len) {
  if (!c.tx || !c.client || !c.client->isConnected()) return false;
  uint16_t conn = c.client->getConnHandle(), h = c.tx->getHandle();
  if (c.txNoResponse) {
    c.wrRefused = false;
    return ble_gattc_write_no_rsp_flat(conn, h, data, len) == 0;
  }
  bool busy;
  portENTER_CRITICAL(&rxMux);  // blesvc and the worker may both write
  busy = c.wrPending;
  if (!busy) c.wrPending = true;
  portEXIT_CRITICAL(&rxMux);
  c.wrRefused = busy;
  if (busy) return false;
  if (ble_gattc_write_flat(conn, h, data, len, writeDone, &c) == 0) return true;
  c.wrPending = false;
  return false;
}

bool writeStr(Ctx &c, const char *s) { return write(c, (const uint8_t *)s, strlen(s)); }

bool subscribe(Ctx &c, NimBLERemoteCharacteristic *chr) {
  if (!chr) return false;
  Ctx *pc = &c;
  return chr->subscribe(chr->canNotify(), [pc](NimBLERemoteCharacteristic *, uint8_t *d, size_t n, bool) {
    portENTER_CRITICAL(&rxMux);
    size_t m = n < sizeof pc->rxBuf - 1 ? n : sizeof pc->rxBuf - 1;
    memcpy(pc->rxBuf, d, m);
    pc->rxBuf[m] = 0;
    pc->rxSeq = pc->rxSeq + 1;
    portEXIT_CRITICAL(&rxMux);
  });
}

bool reply(Ctx &c, uint32_t &seq, char *out, size_t n) {
  bool got = false;
  portENTER_CRITICAL(&rxMux);
  if (c.rxSeq != seq) {
    seq = c.rxSeq;
    strlcpy(out, c.rxBuf, n);
    got = true;
  }
  portEXIT_CRITICAL(&rxMux);
  return got;
}

bool request(Ctx &c, const char *cmd, char *out, size_t n, uint32_t ms) {
  uint32_t seq = c.rxSeq;
  if (!writeStr(c, cmd)) return false;
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    idle(10);
    if (reply(c, seq, out, n)) return true;
  }
  return false;
}

// The other toys run on blesvc meanwhile; no task notifications involved.
void idle(uint32_t ms) {
  TickType_t t = pdMS_TO_TICKS(ms);
  vTaskDelay(t ? t : 1);
}

}  // namespace toys
