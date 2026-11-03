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
  int brightness = 100;  // backlight, 10..100 % (Guition only: the Waveshare's is on/off)
  String scriptNames[SCRIPT_SLOTS];  // "" = unnamed
  // Wi-Fi gaps: how long the cloud device may go unanswered before it counts
  // as lost (and the session stops). It keeps its last speed meanwhile.
  static const int GRACE_CHOICES = 3;
  int graceS = 20;  // 10, 20 or 30
  // Display pixel clock in MHz (Guition only; 10..20): units differ, see board.cpp. Read at boot, so a change
  // needs a restart.
  int pclkMHz = 16;  // Guition display pixel clock; 14 for the few units that need it (see README)
  // A changed clock is on trial until Keep is tapped on the panel (pclkfb.h); not confirmed after 3 boots, it goes back.
  int pclkPrev = 16;           // last confirmed clock
  bool pclkTrial = false;      // pclkMHz is on trial
  int pclkBoots = 0;           // boots spent in this trial, counting this one
  int pclkRevertedFrom = -1;   // this boot went back from this clock (-1 = no revert); shown once
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
  // Rotary encoder on the P4 connector (encoder.h, Guition only): -1 = auto (on if one answers at boot), 0 = off, 1 = on
  // (also finds one plugged in later). Read at boot only: it decides who owns GPIO17/18.
  int encoder = -1;
  int encStep = 2;        // percent per click: 1, 2 or 5
  bool encInvert = false; // swap the turning direction
  int theme = 0;  // colour theme index (theme.cpp): 0 = Daylight
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
// Display pixel clock (Guition): select = start a trial of this clock (a no-op change ends one); the caller restarts after.
// confirm = the Keep button. Both save and log.
void settingsSelectPclk(int mhz);
void settingsConfirmPclk();
void settingsSaveTheme();      // applied live by the caller
void settingsSaveHr();         // all the hr* fields
void settingsSaveEncoder();    // encoder mode (the caller restarts after), step and direction
void settingsSaveBluetooth();  // the caller restarts after
