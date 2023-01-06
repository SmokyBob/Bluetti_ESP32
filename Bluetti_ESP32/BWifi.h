#ifndef BWIFI_H
#define BWIFI_H
#include "Arduino.h"
#include "config.h"

typedef struct{
  int  salt = EEPROM_SALT;
  char bluetti_device_id[40] = "Bluetti Blutetooth Id";
  char use_Meross[6] = "false";//true o false
  //TODO: add meross parameters
  char use_Relay[6] = "false";//true o false
  //TODO: add Relay GPIO.. when tested
} ESPBluettiSettings;

extern ESPBluettiSettings get_esp32_bluetti_settings();
extern void initBWifi(bool resetWifi);
extern void handleWebserver();
void handleNotFound();
void handleRoot();

#endif
