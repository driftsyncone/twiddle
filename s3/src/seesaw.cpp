#include "seesaw.h"

namespace seesaw {

bool validHwId(uint8_t id) {
  switch (id) {
    case 0x55:                                  // SAMD09
    case 0x46:                                  // ATtiny416
    case 0x84: case 0x85: case 0x86: case 0x87: // ATtiny806/807/816/817
    case 0x88: case 0x89:                       // ATtiny1616/1617
    case 0x90: case 0x91: case 0x92: case 0x93: // STM32C011/C031/C051/G071
      return true;
  }
  return false;
}

uint16_t productFromVersion(const uint8_t v[4]) { return (uint16_t)((v[0] << 8) | v[1]); }

bool knownProduct(uint16_t pid) { return pid == 5880 || pid == 4991; }

bool isEncoderBoard(uint8_t hwId, const uint8_t version[4]) {
  return validHwId(hwId) && knownProduct(productFromVersion(version));
}

size_t readPrefix(uint8_t base, uint8_t reg, uint8_t *out) {
  out[0] = base;
  out[1] = reg;
  return 2;
}

size_t gpioMaskWrite(uint8_t reg, uint32_t mask, uint8_t *out) {
  out[0] = GPIO_BASE;
  out[1] = reg;
  for (int i = 0; i < 4; i++) out[2 + i] = (uint8_t)(mask >> (24 - 8 * i));
  return 6;
}

size_t neopixelWrite(uint8_t reg, const uint8_t *data, size_t n, uint8_t *out) {
  out[0] = NEOPIXEL_BASE;
  out[1] = reg;
  for (size_t i = 0; i < n; i++) out[2 + i] = data[i];
  return 2 + n;
}

size_t npSpeed(uint8_t *out) { const uint8_t d[] = {1}; return neopixelWrite(NEOPIXEL_SPEED, d, 1, out); }
size_t npLength(uint8_t *out) { const uint8_t d[] = {0, 3}; return neopixelWrite(NEOPIXEL_BUF_LENGTH, d, 2, out); }
size_t npPin(uint8_t *out) { const uint8_t d[] = {PIN_NEOPIXEL}; return neopixelWrite(NEOPIXEL_PIN, d, 1, out); }
size_t npColor(uint8_t r, uint8_t g, uint8_t b, uint8_t *out) {
  const uint8_t d[] = {0, 0, g, r, b};  // offset hi, lo, then GRB (NEO_GRB)
  return neopixelWrite(NEOPIXEL_BUF, d, 5, out);
}
size_t npShow(uint8_t *out) { return neopixelWrite(NEOPIXEL_SHOW, nullptr, 0, out); }

void buttonSetupFrames(uint8_t out[3][6]) {
  const uint32_t mask = 1u << PIN_BUTTON;
  gpioMaskWrite(GPIO_DIRCLR_BULK, mask, out[0]);
  gpioMaskWrite(GPIO_PULLENSET, mask, out[1]);
  gpioMaskWrite(GPIO_BULK_SET, mask, out[2]);
}

int32_t i32be(const uint8_t *b) {
  return (int32_t)(((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3]);
}

bool buttonPressed(const uint8_t g[4]) { return !((((uint32_t)g[0] << 24) | ((uint32_t)g[1] << 16) | ((uint32_t)g[2] << 8) | g[3]) & (1u << PIN_BUTTON)); }

int Clicks::feed(int32_t position) {
  if (!primed_) {
    primed_ = true;
    last_ = position;
    return 0;
  }
  int32_t d = (int32_t)((uint32_t)position - (uint32_t)last_);  // wrap-safe
  last_ = position;
  if (d > maxJump || d < -maxJump) return 0;
  return (int)d;
}

Press Button::update(bool raw, uint32_t now) {
  if (!primed_) {
    primed_ = true;
    raw_ = raw;
    rawSince_ = now;
    blocked_ = raw;  // already down at connect: wait for release
    return Press::None;
  }
  if (raw != raw_) {
    raw_ = raw;
    rawSince_ = now;
  }
  bool stable = (uint32_t)(now - rawSince_) >= debounceMs;
  if (stable && raw_ != down_) {
    bool wasDown = down_;
    down_ = raw_;
    if (down_) {
      downSince_ = now;
      holdFired_ = false;
    } else {
      bool wasBlocked = blocked_;
      blocked_ = false;
      if (wasDown && !holdFired_ && !wasBlocked) return Press::Short;
      return Press::None;
    }
  }
  if (down_ && !holdFired_ && !blocked_ && (uint32_t)(now - downSince_) >= holdMs) {
    holdFired_ = true;
    return Press::Hold;
  }
  return Press::None;
}

Rgb lightColor(Light l) {
  switch (l) {
    case Light::Running: return {0, 12, 0};
    case Light::Paused: return {12, 6, 0};
    case Light::Hold: return {12, 0, 0};
    default: return {0, 0, 0};
  }
}

}  // namespace seesaw
