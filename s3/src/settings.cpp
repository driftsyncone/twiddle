#include "settings.h"

#include <Preferences.h>

#include "build_config.h"
#include "log.h"
#include "pclkfb.h"

Settings settings;

// NVS namespace: legacy name (from before the rename to Twiddle), kept so existing settings survive. Do not rename.
static const char *NS = "vgpanel";

static void settingsSavePclkState();

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
  // Display pixel clock with its trial state (pclkfb.h). A unit from before the trial existed has only "pclk": no trial.
  pclkfb::State ps;
  ps.cur = p.getInt("pclk", pclkfb::DEFAULT_MHZ);
  ps.prev = p.getInt("pclkp", ps.cur);
  ps.trial = p.getBool("pclkt", false);
  ps.boots = p.getInt("pclkb", 0);
  pclkfb::sanitize(ps);
  int triedMHz = ps.cur;
  pclkfb::Boot pb = pclkfb::onBoot(ps);  // counts this boot, or goes back to the confirmed clock
  settings.pclkMHz = ps.cur;
  settings.pclkPrev = ps.prev;
  settings.pclkTrial = ps.trial;
  settings.pclkBoots = ps.boots;
  settings.pclkRevertedFrom = pb == pclkfb::Boot::Reverted ? triedMHz : -1;
  settings.bluetooth = constrain(p.getInt("bt", -1), -1, 1);
  settings.encoder = constrain(p.getInt("enc", -1), -1, 1);
  settings.encStep = p.getInt("encs", 2);
  if (settings.encStep != 1 && settings.encStep != 5) settings.encStep = 2;
  settings.encInvert = p.getBool("enci", false);
  settings.theme = p.getInt("theme", 0);  // out-of-range values are mapped to Daylight by theme::begin
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
#if !defined(BOARD_WAVESHARE_LCD5)  // the Waveshare's clock is fixed in the build
  if (pb == pclkfb::Boot::Trial)
    plog("display: pixel clock %d MHz on trial (boot %d of %d, last confirmed %d MHz)\n", ps.cur, ps.boots, pclkfb::TRIAL_BOOTS, ps.prev);
  else if (pb == pclkfb::Boot::Reverted)
    plog("display: pixel clock not confirmed after %d boots, reverted from %d MHz to %d MHz\n", pclkfb::TRIAL_BOOTS, triedMHz, ps.cur);
  else
    plog("display: pixel clock %d MHz confirmed\n", ps.cur);
  if (pb != pclkfb::Boot::Plain) settingsSavePclkState();  // the boot counter / the revert must survive a power cut
#endif
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

static void settingsSavePclkState() {
  Preferences p;
  p.begin(NS, false);
  p.putInt("pclk", settings.pclkMHz);
  p.putInt("pclkp", settings.pclkPrev);
  p.putBool("pclkt", settings.pclkTrial);
  p.putInt("pclkb", settings.pclkBoots);
  p.end();
}

static pclkfb::State pclkState() {
  pclkfb::State ps;
  ps.cur = settings.pclkMHz;
  ps.prev = settings.pclkPrev;
  ps.trial = settings.pclkTrial;
  ps.boots = settings.pclkBoots;
  return ps;
}

static void setPclkState(const pclkfb::State &ps) {
  settings.pclkMHz = ps.cur;
  settings.pclkPrev = ps.prev;
  settings.pclkTrial = ps.trial;
  settings.pclkBoots = ps.boots;
}

void settingsSelectPclk(int mhz) {
  pclkfb::State ps = pclkState();
  pclkfb::select(ps, mhz);
  setPclkState(ps);
  settingsSavePclkState();
  if (ps.trial) plog("display: pixel clock %d MHz selected (trial, previous %d MHz)\n", ps.cur, ps.prev);
  else plog("display: pixel clock %d MHz selected (already confirmed, no trial)\n", ps.cur);
}

void settingsConfirmPclk() {
  pclkfb::State ps = pclkState();
  pclkfb::confirm(ps);
  setPclkState(ps);
  settingsSavePclkState();
  plog("display: pixel clock %d MHz kept\n", ps.cur);
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

void settingsSaveEncoder() {
  Preferences p;
  p.begin(NS, false);
  p.putInt("enc", settings.encoder);
  p.putInt("encs", settings.encStep);
  p.putBool("enci", settings.encInvert);
  p.end();
}
