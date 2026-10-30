// Panel settings stored in NVS namespace "vgpanel" (see settings.h). The name is a legacy one from before the
// project was renamed to Twiddle; keep it, or every user's stored settings are lost.
#include "settings.h"

#include <Preferences.h>

#include "secrets.h"

Settings settings;

static const char *NS = "vgpanel";  // legacy name kept for settings compatibility: do NOT rename

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
  settings.bluetooth = constrain(p.getInt("bt", -1), -1, 1);
  settings.panelVariant = p.getInt("panel", 0);
  if (settings.panelVariant < 0 || settings.panelVariant > 4) settings.panelVariant = 0;
  settings.pclkMhz = p.getInt("pclk", 0);
  if (settings.pclkMhz != 0 && (settings.pclkMhz < 20 || settings.pclkMhz > 60)) settings.pclkMhz = 0;
  settings.theme = p.getInt("theme", 1);  // default Midnight; out-of-range values are mapped to the default by theme::begin
  for (int i = 0; i < Settings::FAVOURITES; i++)
    p.getBytes(("fav" + String(i)).c_str(), settings.favPeaks[i], sizeof settings.favPeaks[i]);
  for (int i = 0; i < SCRIPT_SLOTS; i++)
    settings.scriptNames[i] = p.getString(nameKey(i).c_str(), "");
  p.end();
}

void settingsSave() {
  Preferences p;
  p.begin(NS, false);
  p.putString("ssid", settings.wifiSsid.c_str());
  p.putString("pass", settings.wifiPass.c_str());
  p.putString("token", settings.token.c_str());
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
  if (settings.scriptNames[slot].length()) p.putString(key.c_str(), settings.scriptNames[slot].c_str());
  else p.remove(key.c_str());
  p.end();
}

void settingsSavePanel() {
  Preferences p;
  p.begin(NS, false);
  p.putInt("panel", settings.panelVariant);
  p.putInt("pclk", settings.pclkMhz);
  p.end();
}

void settingsSaveGrace() {
  Preferences p;
  p.begin(NS, false);
  p.putInt("grace", settings.graceS);
  p.end();
}

void settingsSaveTheme() {
  Preferences p;
  p.begin(NS, false);
  p.putInt("theme", settings.theme);
  p.end();
}

void settingsSaveBluetooth() {
  Preferences p;
  p.begin(NS, false);
  p.putInt("bt", settings.bluetooth);
  p.end();
}
