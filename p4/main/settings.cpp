// Panel settings stored in NVS namespace "vgpanel" (see settings.h). The name is a legacy one from before the
// project was renamed to Twiddle; keep it, or every user's stored settings are lost.
#include "settings.h"

#include <Preferences.h>

#include "log.h"
#include "panelfb.h"

#include "build_config.h"

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
  // Display variant with its trial state (panelfb.h). A unit from before the trial existed has only "panel": no trial.
  panelfb::State ps;
  ps.cur = p.getInt("panel", 0);
  ps.prev = p.getInt("pprev", ps.cur);
  ps.trial = p.getBool("ptrial", false);
  ps.boots = p.getInt("pboots", 0);
  panelfb::sanitize(ps);
  int triedVariant = ps.cur;
  panelfb::Boot pb = panelfb::onBoot(ps);  // counts this boot, or goes back to the confirmed variant
  settings.panelVariant = ps.cur;
  settings.panelPrev = ps.prev;
  settings.panelTrial = ps.trial;
  settings.panelBoots = ps.boots;
  settings.panelRevertedFrom = pb == panelfb::Boot::Reverted ? triedVariant : -1;
  if (pb == panelfb::Boot::Trial)
    plog("panel: variant %s on trial (boot %d of %d, last confirmed %s)\n", panelfb::name(ps.cur), ps.boots, panelfb::TRIAL_BOOTS, panelfb::name(ps.prev));
  else if (pb == panelfb::Boot::Reverted)
    plog("panel: not confirmed after %d boots, reverted from %s to %s\n", panelfb::TRIAL_BOOTS, panelfb::name(triedVariant), panelfb::name(ps.cur));
  else
    plog("panel: variant %s confirmed\n", panelfb::name(ps.cur));
  settings.pclkMhz = p.getInt("pclk", 0);
  if (settings.pclkMhz != 0 && (settings.pclkMhz < 20 || settings.pclkMhz > 60)) settings.pclkMhz = 0;
  if (pb == panelfb::Boot::Reverted) settings.pclkMhz = 0;  // an override made for the failed variant does not belong to the old one
  settings.theme = p.getInt("theme", 1);  // default Midnight; out-of-range values are mapped to the default by theme::begin
  settings.hrEdging = p.getBool("hre", false);
  settings.hrResting = constrain(p.getInt("hrrest", 70), 40, 150);
  settings.hrEdge = constrain(p.getInt("hredge", 120), 50, 200);
  settings.hrDrop = constrain(p.getInt("hrdrop", 15), 3, 60);
  settings.hrEdges = constrain(p.getInt("hrcount", 5), 0, 50);
  settings.hrStep = constrain(p.getInt("hrstep", 2), 0, 10);
  settings.hrRise = p.getInt("hrrise", 20) < 8 ? 0 : constrain(p.getInt("hrrise", 20), 8, 60);
  for (int i = 0; i < Settings::FAVOURITES; i++)
    p.getBytes(("fav" + String(i)).c_str(), settings.favPeaks[i], sizeof settings.favPeaks[i]);
  for (int i = 0; i < SCRIPT_SLOTS; i++)
    settings.scriptNames[i] = p.getString(nameKey(i).c_str(), "");
  p.end();
  if (pb != panelfb::Boot::Plain) settingsSavePanel();  // the boot counter / the revert must survive a power cut
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
  p.putInt("pprev", settings.panelPrev);
  p.putBool("ptrial", settings.panelTrial);
  p.putInt("pboots", settings.panelBoots);
  p.putInt("pclk", settings.pclkMhz);
  p.end();
}

static panelfb::State panelState() {
  panelfb::State ps;
  ps.cur = settings.panelVariant;
  ps.prev = settings.panelPrev;
  ps.trial = settings.panelTrial;
  ps.boots = settings.panelBoots;
  return ps;
}

static void setPanelState(const panelfb::State &ps) {
  settings.panelVariant = ps.cur;
  settings.panelPrev = ps.prev;
  settings.panelTrial = ps.trial;
  settings.panelBoots = ps.boots;
}

void settingsSelectPanel(int variant) {
  panelfb::State ps = panelState();
  panelfb::select(ps, variant);
  setPanelState(ps);
  if (ps.trial) plog("panel: variant %s selected (trial, previous %s)\n", panelfb::name(ps.cur), panelfb::name(ps.prev));
  else plog("panel: variant %s selected (already confirmed, no trial)\n", panelfb::name(ps.cur));
  settings.pclkMhz = 0;  // a pixel clock tuned for the old variant is not the new variant's
  settingsSavePanel();
}

void settingsConfirmPanel() {
  panelfb::State ps = panelState();
  panelfb::confirm(ps);
  setPanelState(ps);
  plog("panel: variant %s kept\n", panelfb::name(ps.cur));
  settings.panelRevertedFrom = -1;
  settingsSavePanel();
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

void settingsSaveHr() {
  Preferences p;
  p.begin(NS, false);
  p.putBool("hre", settings.hrEdging);
  p.putInt("hrrest", settings.hrResting);
  p.putInt("hredge", settings.hrEdge);
  p.putInt("hrdrop", settings.hrDrop);
  p.putInt("hrcount", settings.hrEdges);
  p.putInt("hrstep", settings.hrStep);
  p.putInt("hrrise", settings.hrRise);
  p.end();
}
