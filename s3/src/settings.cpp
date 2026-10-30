#include "settings.h"

#include <Preferences.h>

#include "secrets.h"

Settings settings;

// NVS namespace: legacy name (from before the rename to Twiddle), kept so existing settings survive. Do not rename.
static const char *NS = "vgpanel";

static String nameKey(int slot) { return "sn" + String(slot); }

void settingsLoad() {
  Preferences p;
  p.begin(NS, true);
  settings.wifiSsid = p.getString("ssid", WIFI_SSID);
  settings.wifiPass = p.getString("pass", WIFI_PASS);
  settings.token = p.getString("token", DEVICE_TOKEN);
  settings.sleepMin = p.getInt("sleep", settings.sleepMin);
  settings.flip = p.getBool("flip", settings.flip);
  settings.brightness = constrain(p.getInt("bright", settings.brightness), 10, 100);
  settings.graceS = p.getInt("grace", settings.graceS);
  if (settings.graceS != 10 && settings.graceS != 30) settings.graceS = 20;
  settings.pclkMHz = p.getInt("pclk", settings.pclkMHz);
  if (settings.pclkMHz < 10 || settings.pclkMHz > 20) settings.pclkMHz = 14;
  settings.bluetooth = constrain(p.getInt("bt", -1), -1, 1);
  settings.theme = p.getInt("theme", 1);  // 1 = Midnight when nothing is saved; out-of-range values are mapped to Midnight by theme::begin
  for (int i = 0; i < Settings::FAVOURITES; i++)
    p.getBytes(("fav" + String(i)).c_str(), settings.favPeaks[i], sizeof settings.favPeaks[i]);
  for (int i = 0; i < SCRIPT_SLOTS; i++)
    settings.scriptNames[i] = p.getString(nameKey(i).c_str(), "");
  p.end();
}

void settingsSave() {
  Preferences p;
  p.begin(NS, false);
  p.putString("ssid", settings.wifiSsid);
  p.putString("pass", settings.wifiPass);
  p.putString("token", settings.token);
  p.end();
}

void settingsSaveSleep() {
  Preferences p;
  p.begin(NS, false);
  p.putInt("sleep", settings.sleepMin);
  p.end();
}

void settingsSaveFlip() {
  Preferences p;
  p.begin(NS, false);
  p.putBool("flip", settings.flip);
  p.end();
}

void settingsSaveBrightness() {
  Preferences p;
  p.begin(NS, false);
  p.putInt("bright", settings.brightness);
  p.end();
}

void settingsSaveFavourite(int slot) {
  Preferences p;
  p.begin(NS, false);
  p.putBytes(("fav" + String(slot)).c_str(), settings.favPeaks[slot], sizeof settings.favPeaks[slot]);
  p.end();
}

void settingsSaveScriptName(int slot) {
  Preferences p;
  p.begin(NS, false);
  String key = nameKey(slot);
  if (settings.scriptNames[slot].length()) p.putString(key.c_str(), settings.scriptNames[slot]);
  else p.remove(key.c_str());
  p.end();
}

void settingsSaveTheme() {
  Preferences p;
  p.begin(NS, false);
  p.putInt("theme", settings.theme);
  p.end();
}

void settingsSaveGrace() {
  Preferences p;
  p.begin(NS, false);
  p.putInt("grace", settings.graceS);
  p.end();
}

void settingsSavePclk() {
  Preferences p;
  p.begin(NS, false);
  p.putInt("pclk", settings.pclkMHz);
  p.end();
}

void settingsSaveBluetooth() {
  Preferences p;
  p.begin(NS, false);
  p.putInt("bt", settings.bluetooth);
  p.end();
}
