// Panel settings in NVS. Wi-Fi and token default to secrets.h until the
// Settings tab saves its own.
#pragma once
#include <Arduino.h>

static const int SCRIPT_SLOTS = 30;

struct Settings {
  String wifiSsid;
  String wifiPass;
  String token;
  int sleepMin = 5;  // screen off after this many idle minutes, 0 = never
  bool flip = false;  // show the picture upside down (panel mounted the other way round)
  static const int FAVOURITES = 3, WAVES = 8;
  int8_t favPeaks[FAVOURITES][WAVES] = {};  // Autopilot favourite sessions: a peak per wave, 0 = unused slot
  int brightness = 100;  // backlight, 10..100 % (PWM on GPIO23)
  String scriptNames[SCRIPT_SLOTS];  // "" = unnamed
  // Wi-Fi gaps: how long the cloud device may go unanswered before it counts
  // as lost (and the session stops). It keeps its last speed meanwhile.
  static const int GRACE_CHOICES = 3;
  // Display panel variant behind the JD9165 driver (P4): 0 = V2 (2026 batch, default), 1 = original. A wrong
  // choice leaves the screen black or washed out; read at boot, set over the web (GET /panel).
  int panelVariant = 0;
  // A variant chosen on the setup portal (or /panel) is on TRIAL until "Keep" is tapped on the panel's screen; after
  // panelfb::TRIAL_BOOTS unconfirmed boots the next boot goes back to panelPrev (see panelfb.h). Read at boot.
  int panelPrev = 0;            // last confirmed variant
  bool panelTrial = false;      // the current variant is not confirmed yet
  int panelBoots = 0;           // boots spent in this trial
  int panelRevertedFrom = -1;   // >= 0: this boot just went back from that unconfirmed variant (the UI says so once)
  int pclkMhz = 0;  // display pixel clock override in MHz, 0 = the variant default (52, 48 on the "_Y" panel)
  int graceS = 20;  // 10, 20 or 30
  // Bluetooth toys: -1 = auto (on if a toy is remembered), 0 = off, 1 = on.
  // Off lets Wi-Fi run without modem sleep. Read at boot only.
  int bluetooth = -1;
  // Heart-rate edging (Autopilot option, edge.h). Off by default.
  bool hrEdging = false;
  int hrResting = 70;   // resting bpm, 40..150
  int hrEdge = 120;     // edge trigger bpm, resting+10 .. 200
  int hrDrop = 15;      // rest until bpm <= edge - this, 3..60
  int hrEdges = 5;      // session ends after this many edges, 0 = no limit
  int hrStep = 2;       // edge bpm goes up by this after each finished cycle, 0..10
  int hrRise = 20;      // fast-rise trigger: bpm gained within 10 s, 0 = off
  int theme = 1;  // colour theme index (theme.cpp): 1 = Midnight (default), 0 = Daylight
};

extern Settings settings;

void settingsLoad();
// Wi-Fi + token. They are only read at boot, so the caller restarts after.
void settingsSave();
void settingsSaveSleep();
void settingsSaveFlip();
void settingsSaveBrightness();
void settingsSaveFavourite(int slot);
void settingsSaveScriptName(int slot);
void settingsSaveGrace();
void settingsSavePanel();  // panel state + pclk; the caller restarts after
// The user chose a display variant (setup portal, /panel): starts a trial (or ends it when it is the confirmed one),
// resets the pixel clock to the variant default and saves. The caller restarts after.
void settingsSelectPanel(int variant);
void settingsConfirmPanel();  // "Keep": the current variant becomes the confirmed one
void settingsSaveTheme();      // applied live by the caller
void settingsSaveHr();         // all the hr* fields
void settingsSaveBluetooth();  // the caller restarts after
