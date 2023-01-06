#ifndef CONFIG_H
#define CONFIG_H
#include "Arduino.h"

// Info = 5
// Log = 4
#define DEBUG 5
//#define RESET_WIFI_SETTINGS   1

#define EEPROM_SALT 13374

#define DEVICE_NAME "BLUETTI-MQTT"

#define BLUETOOTH_QUERY_MESSAGE_DELAY 10000
#define DEVICE_STATE_UPDATE  5

#define RELAISMODE 0
#define RELAIS_PIN 22
#define RELAIS_LOW LOW
#define RELAIS_HIGH HIGH

#define MAX_DISCONNECTED_TIME_UNTIL_REBOOT 2 //device will reboot when wlan/BT/MQTT is not connectet within x Minutes

#ifndef BLUETTI_TYPE
  #define BLUETTI_TYPE BLUETTI_EB3A
#endif


#endif
