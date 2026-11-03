// Per-device web password: random, 6 characters from an alphabet without look-alikes (no 0/O, 1/I/L, 5/S, 2/Z).
// Pure logic (no ESP headers) so test/host can check it. The caller passes the random source
// (esp_random on the panel) and gets one 32-bit word per call.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace pwgen {

static const char ALPHABET[] = "ABCDEFGHJKMNPQRTUVWXY346789";  // 27 characters
static const size_t ALPHABET_LEN = sizeof(ALPHABET) - 1;
static const size_t LENGTH = 6;  // 6 * log2(27) = 28.5 bits: short on purpose (typed from the panel), so ota login is rate-limited (loginlimit.h)

// Writes LENGTH characters plus a NUL into out (out_size must be >= LENGTH + 1). Rejection sampling, so every
// character is uniform. Returns false if out is too small.
template <typename Rng>
bool generate(char *out, size_t out_size, Rng rng) {
  if (!out || out_size < LENGTH + 1) return false;
  const uint32_t limit = (uint32_t)(0xFFFFFFFFu - (0xFFFFFFFFu % ALPHABET_LEN));  // largest multiple of 27 below 2^32
  for (size_t i = 0; i < LENGTH; i++) {
    uint32_t r;
    do { r = (uint32_t)rng(); } while (r >= limit);
    out[i] = ALPHABET[r % ALPHABET_LEN];
  }
  out[LENGTH] = 0;
  return true;
}

// True if s is a password this module could have made (right length, only alphabet characters).
inline bool valid(const char *s) {
  if (!s) return false;
  size_t n = 0;
  for (; s[n]; n++) {
    bool ok = false;
    for (size_t k = 0; k < ALPHABET_LEN; k++) ok |= s[n] == ALPHABET[k];
    if (!ok) return false;
  }
  return n == LENGTH;
}

// Compare without an early exit, so the time does not reveal how many leading characters matched.
inline bool constantTimeEqual(const char *a, size_t an, const char *b, size_t bn) {
  uint8_t diff = (uint8_t)(an != bn);
  size_t n = an < bn ? an : bn;
  for (size_t i = 0; i < n; i++) diff |= (uint8_t)(a[i] ^ b[i]);
  return diff == 0;
}

}  // namespace pwgen
