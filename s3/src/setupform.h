// What the setup form does with a stored secret. Pure logic (no Arduino headers), tested in test/host.
//
// The form never shows a stored Wi-Fi password or device token (the setup access point's own password is public, so
// anything rendered into the page could be read by anyone in range). An empty field therefore means "keep what is stored".
#pragma once
#include <string>

namespace setupform {

// Wi-Fi password to store after a submit. A typed value replaces the stored one. An empty field keeps the stored one
// only while the network is unchanged; a new network clears it (the old password does not belong to it).
inline std::string wifiPass(const std::string &stored, const std::string &typed, bool sameNetwork) {
  if (!typed.empty()) return typed;
  return sameNetwork ? stored : std::string();
}

// Device token to store after a submit: a typed value (trimmed) replaces it, an empty field keeps it.
inline std::string token(const std::string &stored, const std::string &typed) {
  size_t a = typed.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return stored;
  size_t b = typed.find_last_not_of(" \t\r\n");
  return typed.substr(a, b - a + 1);
}

}  // namespace setupform
