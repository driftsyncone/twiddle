// Battery level. The Guition JC1060P470C has no known fuel gauge, so this reports "not present"
// unless a MAX17048 / MAX17043 (address 0x36) answers on the touch I2C bus (GPIO7/8, I2C0, owned
// by board.cpp). The bus is only borrowed through the IDF master driver (it serialises its users);
// nothing else is configured here. No voltage-divider ADC fallback: the board has no divider.
#include "battery.h"

#include <Arduino.h>

#include "freertos/FreeRTOS.h"

#include "driver/i2c_master.h"
#include "esp_log.h"

namespace battery {

static const uint8_t ADDR = 0x36;
static const uint8_t REG_VCELL = 0x02, REG_SOC = 0x04, REG_VERSION = 0x08, REG_CRATE = 0x16;

static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t dev;
static Info cur;  // working copy, touched only by begin() and loop() (the loop task)
static Info pub;  // published copy for info(), swapped under pubMux (readers: UI, control, any task)
static portMUX_TYPE pubMux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t lastRead, lastProbe;

static void publish() {
  portENTER_CRITICAL(&pubMux);
  pub = cur;
  portEXIT_CRITICAL(&pubMux);
}

static bool read16(uint16_t &out, uint8_t reg) {
  uint8_t b[2];
  if (i2c_master_transmit_receive(dev, &reg, 1, b, 2, 50) != ESP_OK) return false;
  out = (b[0] << 8) | b[1];
  return true;
}

static bool rejected = false;  // something answers at ADDR but it isn't a gauge: stop probing

static void forget() {
  if (dev) i2c_master_bus_rm_device(dev);
  dev = nullptr;
  cur = Info();
}

static void probe() {
  lastProbe = millis();
  if (!bus && i2c_master_get_bus_handle(I2C_NUM_0, &bus) != ESP_OK) {
    bus = nullptr;  // the touch bus isn't up (yet)
    return;
  }
  if (i2c_master_probe(bus, ADDR, 50) != ESP_OK) return;
  i2c_device_config_t dc = {};
  dc.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  dc.device_address = ADDR;
  dc.scl_speed_hz = 100000;
  if (i2c_master_bus_add_device(bus, &dc, &dev) != ESP_OK) return;
  uint16_t ver = 0;
  // MAX17048/49 report 0x001x; the older MAX17043/44 report 0x000x.
  if (!read16(ver, REG_VERSION)) {
    forget();
    return;
  }
  // Only a real MAX1704x counts: a known version value AND a plausible cell voltage. Other chips share
  // address 0x36 (this board has one that answers any read with zeros), so don't trust a bare ACK.
  uint16_t vcell = 0;
  bool knownVersion = (ver & 0xFFF0) == 0x0010 || (ver & 0xFFF0) == 0x0000;
  bool plausible = read16(vcell, REG_VCELL) && vcell * 78.125e-6f >= 2.5f && vcell * 78.125e-6f <= 4.6f;
  if (!knownVersion || !plausible) {
    printf("battery: device at 0x%02x is not a fuel gauge (version 0x%04x, vcell 0x%04x), ignoring it\n", ADDR, ver, vcell);
    forget();
    rejected = true;
    return;
  }
  cur.chip = (ver & 0xFFF0) == 0x0010 ? "MAX17048" : "MAX17043";
  printf("battery: %s (version 0x%04x) on the touch bus\n", cur.chip, ver);
}

static void read() {
  uint16_t v, soc;
  if (!read16(v, REG_VCELL) || !read16(soc, REG_SOC)) {
    forget();  // unplugged or bus error: probe again later
    return;
  }
  cur.present = true;
  cur.volts = v * 78.125e-6f;  // both chips: 78.125 uV per LSB
  cur.percent = constrain(soc / 256.0f, 0.0f, 100.0f);
  cur.charging = false;
  uint16_t crate;
  if (strcmp(cur.chip, "MAX17048") == 0 && read16(crate, REG_CRATE))
    cur.charging = (int16_t)crate * 0.208f > 0.5f;  // %/hour; above +0.5 %/h = charging
}

void begin() {
  probe();
  publish();
}

void loop() {
#ifdef BATTERY_FAKE_PERCENT  // UI test builds only: -DBATTERY_FAKE_PERCENT=57
  cur.present = true;
  cur.percent = BATTERY_FAKE_PERCENT;
  cur.volts = 3.5f + BATTERY_FAKE_PERCENT * 0.007f;
  cur.chip = "fake";
  publish();
  return;
#endif
  uint32_t now = millis();
  if (!dev) {
    if (!rejected && now - lastProbe > 30000) {
      probe();
      publish();
    }
    return;
  }
  if (now - lastRead > 5000 || !cur.present) {
    lastRead = now;
    read();
    publish();
  }
}

Info info() {
  portENTER_CRITICAL(&pubMux);
  Info i = pub;
  portEXIT_CRITICAL(&pubMux);
  return i;
}


}  // namespace battery
