// Routing decision for GET /: on the setup access point the root goes to the setup form, on the LAN it stays the
// status page. Pure logic (no ESP headers), tested in test/host.
#pragma once
#include <cstring>

namespace portalroute {

// host: the request's Host header (may be null/empty); apIp: the AP address ("192.168.4.1").
inline bool rootGoesToSetup(bool portalUp, const char *host, const char *apIp) {
  if (!portalUp || !host || !apIp) return false;
  size_t n = strlen(apIp);
  if (strncmp(host, apIp, n) != 0) return false;
  return host[n] == '\0' || host[n] == ':';  // optional port
}

}  // namespace portalroute
