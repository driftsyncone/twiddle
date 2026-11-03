// Adafruit seesaw rotary encoder breakout (product 5880, ATtiny-based seesaw): the pure-logic half.
// Frame building / parsing, product check, button debounce + short/hold detection, encoder delta accumulation and the
// NeoPixel state colours. No Arduino, no I2C: the transport is encoder.cpp, and test/host/seesaw_test.cpp tests this file.
//
// Protocol (Adafruit_Seesaw library source, checked 2026-11-03): every register is a (module base, function) byte pair.
// A read = write the 2 bytes, wait ~125-250 us, read N bytes. A write = the 2 bytes followed by the payload.
// Multi-byte values are big-endian. Default address 0x36, jumpers A0/A1/A2 add 1/2/4 (0x36..0x3D).
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace seesaw {

// Module bases and function registers
static const uint8_t STATUS_BASE = 0x00, GPIO_BASE = 0x01, NEOPIXEL_BASE = 0x0E, ENCODER_BASE = 0x11;
static const uint8_t STATUS_HW_ID = 0x01, STATUS_VERSION = 0x02, STATUS_SWRST = 0x7F;
static const uint8_t GPIO_DIRCLR_BULK = 0x03, GPIO_BULK = 0x04, GPIO_BULK_SET = 0x05, GPIO_PULLENSET = 0x0B;
static const uint8_t NEOPIXEL_PIN = 0x01, NEOPIXEL_SPEED = 0x02, NEOPIXEL_BUF_LENGTH = 0x03, NEOPIXEL_BUF = 0x04, NEOPIXEL_SHOW = 0x05;
static const uint8_t ENCODER_STATUS = 0x00, ENCODER_POSITION = 0x30, ENCODER_DELTA = 0x40;

// Pins on the 5880 board (seesaw pin numbers)
static const uint8_t PIN_BUTTON = 24, PIN_NEOPIXEL = 6;

static const uint8_t ADDR_DEFAULT = 0x36, ADDR_LAST = 0x3D;
inline bool validAddress(uint8_t a) { return a >= ADDR_DEFAULT && a <= ADDR_LAST; }

// ---------- probe: tell a seesaw from a MAX17048 fuel gauge (both default to 0x36) ----------
// HW_ID is one byte; the library accepts exactly these chip codes. A MAX17048 answers the same 2-byte-write + 1-byte-read with
// the high byte of VCELL (0xB4..0xD2 for 3.6..4.2 V), which is none of them; the product id then closes the gap.
bool validHwId(uint8_t id);
// VERSION is 4 bytes big-endian: (product id << 16) | date code. Adafruit's product number is the id.
uint16_t productFromVersion(const uint8_t v[4]);
bool knownProduct(uint16_t pid);  // 5880 (this breakout), 4991 (the earlier NeoPixel encoder board, same seesaw map)
// True when both probe answers say "seesaw encoder board".
bool isEncoderBoard(uint8_t hwId, const uint8_t version[4]);

// ---------- frames ----------
// Each builder writes into out (room for the maximum, 8 bytes) and returns the length.
size_t readPrefix(uint8_t base, uint8_t reg, uint8_t *out);                       // 2 bytes: write, then read
size_t gpioMaskWrite(uint8_t reg, uint32_t mask, uint8_t *out);                   // 6 bytes
size_t neopixelWrite(uint8_t reg, const uint8_t *data, size_t n, uint8_t *out);   // 2 + n (n <= 6)
// NeoPixel setup/show frames (one GRB pixel on pin 6, 800 kHz)
size_t npSpeed(uint8_t *out);                      // SPEED = 1
size_t npLength(uint8_t *out);                     // BUF_LENGTH = 3 (bytes)
size_t npPin(uint8_t *out);                        // PIN = 6
size_t npColor(uint8_t r, uint8_t g, uint8_t b, uint8_t *out);  // BUF: offset 0, then G R B
size_t npShow(uint8_t *out);                       // SHOW, no payload
// Pull-up on the button: DIRCLR, PULLENSET, BULK_SET with mask 1<<24
void buttonSetupFrames(uint8_t out[3][6]);

// ---------- parsing ----------
int32_t i32be(const uint8_t *b);
bool buttonPressed(const uint8_t gpioBulk[4]);  // pin 24 low = pressed (pull-up)

// ---------- encoder position -> clicks ----------
// Works on POSITION (idempotent: a missed poll loses nothing) rather than DELTA (which is consumed by every read). The first sample
// after connect only primes it, so a stale position (or a seesaw that kept counting while unplugged) never turns into a step.
class Clicks {
 public:
  void reset() { primed_ = false; }
  // Returns the clicks since the last sample (0 on the first). A jump beyond maxJump is treated as a glitch and swallowed.
  int feed(int32_t position);
  static const int maxJump = 40;
 private:
  bool primed_ = false;
  int32_t last_ = 0;
};

// ---------- button: debounce, short press, hold ----------
enum class Press : uint8_t { None, Short, Hold };
class Button {
 public:
  static const uint32_t debounceMs = 30, holdMs = 1000;
  void reset() { *this = Button(); }
  // raw = pin currently low. Short fires on release of a press shorter than holdMs; Hold fires once, while still held, at holdMs
  // (and the release after it fires nothing). The first sample after reset() only primes: a button already down at connect is
  // ignored until it is let go.
  Press update(bool raw, uint32_t nowMs);
  bool held() const { return down_; }
 private:
  bool primed_ = false, raw_ = false, down_ = false, holdFired_ = false, blocked_ = false;
  uint32_t rawSince_ = 0, downSince_ = 0;
};

// ---------- NeoPixel state colour ----------
enum class Light : uint8_t { Off, Paused, Running, Hold };
struct Rgb { uint8_t r, g, b; };
Rgb lightColor(Light l);  // low brightness already applied (max channel 12 of 255)

}  // namespace seesaw
