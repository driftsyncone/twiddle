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
  int pclkMhz = 0;  // display pixel clock override in MHz, 0 = the variant default (52, 48 on the "_Y" panel)
  int graceS = 20;  // 10, 20 or 30
  // Bluetooth toys: -1 = auto (on if a toy is remembered), 0 = off, 1 = on.
  // Off lets Wi-Fi run without modem sleep. Read at boot only.
  int bluetooth = -1;
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
void settingsSavePanel();  // the caller restarts after
void settingsSaveTheme();      // applied live by the caller
void settingsSaveBluetooth();  // the caller restarts after
