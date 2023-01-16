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
#define DEVICE_STATE_UPDATE  5 //NOT USED

#define RELAISMODE 0
#define RELAIS_PIN 22
#define RELAIS_LOW LOW
#define RELAIS_HIGH HIGH

#define MAX_DISCONNECTED_TIME_UNTIL_REBOOT 2 //device will reboot when wlan/BT is not connectet within x Minutes

#ifndef BLUETTI_TYPE
  #define BLUETTI_TYPE EB3A
#endif

//15 seconds WDT
#define WDT_TIMEOUT 15

//NTP configs
static String ntpServer = "pool.ntp.org";
static long  gmtOffset_sec = 3600*(+1);
static int   daylightOffset_sec = 3600;

//If defined, the device will be forcefully rebooted every X hours
#define FORCED_REBOOT_HRS 12

//N.B. if changed, update the function config_HTML to edit the fields
typedef struct{
  int  salt = EEPROM_SALT;
  char bluetti_device_id[40] = "";
  bool APMode = false;//Start in AP Mode
  //IFTT Parameters
  bool useIFTT = false; //Following parameters used (and shown) only if this is true
  String IFTT_Key = ""; //https://ifttt.com/maker_webhooks then click Documentation
  String IFTT_Event_low = ""; //If "" no event triggered
  int IFTT_low_batteryLevel = 0;
  String IFTT_Event_high = ""; //If "" no event triggered
  int IFTT_high_batteryLevel = 0;

  // bool use_Meross = false;
  // //TODO: add meross parameters
  // bool use_Relay = false;
  // //TODO: add Relay GPIO.. when tested

} ESPBluettiSettings;

extern ESPBluettiSettings wifiConfig;

extern ESPBluettiSettings get_esp32_bluetti_settings();

extern void readConfigs();

extern void saveConfig();

#endif
