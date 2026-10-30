// The panel's web server: firmware updates over Wi-Fi (POST the .bin to
// http://<panel>/update with basic auth ota:OTA_PASSWORD, see ota.sh; the
// VacuGlide is stopped first), status, screenshot and diagnostics. Runs on its
// own task, so a slow request never stalls the UI or the control timer; it
// takes UiLock for anything that touches the UI, control:: or shared hardware.
#pragma once

void otaBegin();  // call last in setup(), after uiBegin()
