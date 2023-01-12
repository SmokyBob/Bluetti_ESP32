#ifndef CONFIG_H
#define CONFIG_H
#include "Arduino.h"

// Info = 5
// Log = 4
#define DEBUG 100 //For relase set to 100, only setup infos will be printed
#define logHeap 0 //1=log heap value in loop
//#define RESET_WIFI_SETTINGS   1

#define EEPROM_SALT 13374

#define DEVICE_NAME "BLUETTI-ESP32"

#define BLUETOOTH_QUERY_MESSAGE_DELAY 10000
#define DEVICE_STATE_UPDATE  5

#define RELAISMODE 0
#define RELAIS_PIN 22
#define RELAIS_LOW LOW
#define RELAIS_HIGH HIGH

#define MAX_DISCONNECTED_TIME_UNTIL_REBOOT 2 //device will reboot when wlan/BT is not connectet within x Minutes

#ifndef BLUETTI_TYPE
  #define BLUETTI_TYPE BLUETTI_EB3A
#endif

//10 seconds WDT
#define WDT_TIMEOUT 10

//NTP configs
static char* ntpServer = "pool.ntp.org";
static long  gmtOffset_sec = 3600*(+1);
static int   daylightOffset_sec = 3600;

#endif
