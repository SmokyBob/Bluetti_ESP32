#ifndef BWIFI_H
#define BWIFI_H
#include "Arduino.h"
#include "config.h"
#include "time.h"

//N.B. if changed, update the function config_HTML to edit the fields
typedef struct{
  int  salt = EEPROM_SALT;
  char bluetti_device_id[40] = "";
  bool APMode = false;//Start in AP Mode


  // bool use_Meross = false;
  // //TODO: add meross parameters
  // bool use_Relay = false;
  // //TODO: add Relay GPIO.. when tested

} ESPBluettiSettings;

extern String runningSince;

extern ESPBluettiSettings get_esp32_bluetti_settings();
extern void initBWifi(bool resetWifi);
extern void handleWebserver();
void handleNotFound();
void handleRoot();

#endif
