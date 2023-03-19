#ifndef BWIFI_H
#define BWIFI_H
#include "Arduino.h"
#include "config.h"
#include "time.h"

extern String runningSince;

extern void initBWifi(bool resetWifi);
extern void handleWebserver();
extern void wifiConnect(bool resetWifi);
extern void handleBTCommand(String topic, String payloadData);

#endif
