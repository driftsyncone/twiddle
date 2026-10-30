// Host-side mock of the esp-nimble-cpp classes the drivers name. Compile-only
// except for what the offline test drives (advert matching, Ctx state).
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
struct NimBLEUUID {
  std::string s;
  NimBLEUUID() {}
  NimBLEUUID(uint16_t v) { char b[8]; snprintf(b, sizeof b, "%04x", v); s = b; }
  NimBLEUUID(const char *c) : s(c) {}
  NimBLEUUID(const std::string &c) : s(c) {}
  std::string toString() const { return s; }
  bool operator==(const NimBLEUUID &o) const { return s == o.s; }
};
struct NimBLEAttValue {
  std::string v;
  size_t size() const { return v.size(); }
  const uint8_t *data() const { return (const uint8_t *)v.data(); }
  uint8_t operator[](size_t i) const { return (uint8_t)v[i]; }
};
struct NimBLERemoteCharacteristic {
  bool canWriteNoResponse() { return true; }
  bool canWrite() { return true; }
  bool canNotify() { return true; }
  bool canRead() { return true; }
  bool writeValue(const uint8_t *, size_t, bool) { return true; }
  NimBLEAttValue readValue() { return {}; }
  template <class F> bool subscribe(bool, F) { return true; }
  uint16_t getHandle() { return 1; }
};
struct NimBLERemoteService {
  NimBLEUUID getUUID() { return {}; }
  NimBLERemoteCharacteristic *getCharacteristic(const NimBLEUUID &) { return nullptr; }
};
struct NimBLEConnInfo { bool isEncrypted() { return false; } bool isBonded() { return false; } };
struct NimBLEClient {
  bool isConnected() { return true; }
  uint16_t getConnHandle() { return 1; }
  uint16_t getMTU() { return 23; }
  NimBLEConnInfo getConnInfo() { return {}; }
  std::vector<NimBLERemoteService *> getServices(bool) { return {}; }
  NimBLERemoteService *getService(const NimBLEUUID &) { return nullptr; }
};
struct NimBLEAdvertisedDevice {
  std::string name;
  std::vector<std::string> mfg;
  std::vector<std::string> svc;
  std::string getName() const { return name; }
  uint8_t getManufacturerDataCount() const { return mfg.size(); }
  std::string getManufacturerData(uint8_t i) const { return mfg[i]; }
  uint8_t getServiceUUIDCount() const { return svc.size(); }
  NimBLEUUID getServiceUUID(uint8_t i) const { return NimBLEUUID(svc[i]); }
  bool isAdvertisingService(const NimBLEUUID &u) const {
    for (auto &x : svc) if (x == u.s) return true;
    return false;
  }
};
