#ifndef CONFIG_H
#define CONFIG_H
#include "Arduino.h"
#include "logger.h"

// Info = 5
// Log = 4
#define DEBUG 100 // For relase set to 100, only setup infos will be printed
#define logHeap 0 // 1=log heap value in loop
// #define RESET_WIFI_SETTINGS   1

#define EEPROM_SALT 13374

#define DEVICE_NAME "BLUETTI-ESP32"

#define BLUETOOTH_QUERY_MESSAGE_DELAY 5000
#define DEVICE_STATE_UPDATE 5 // NOT USED

#define RELAISMODE 0
#define RELAIS_PIN 22
#define RELAIS_LOW LOW
#define RELAIS_HIGH HIGH

#define MAX_DISCONNECTED_TIME_UNTIL_REBOOT 2 // device will reboot when wlan/BT is not connectet within x Minutes

#ifndef BLUETTI_TYPE
#define BLUETTI_TYPE EB3A
#endif

// 15 seconds WDT
#define WDT_TIMEOUT 15

// NTP configs
static String ntpServer = "pool.ntp.org";
static long gmtOffset_sec = 3600 * (+1);
static int daylightOffset_sec = 3600;

// N.B. if changed, update the function config_HTML to edit the fields
// readConfigs and saveConfig needs to be updated too
typedef struct
{
  int salt = EEPROM_SALT;
  String bluetti_device_id = "";
  bool APMode = false; // Start in AP Mode
  // IFTT Parameters
  bool useIFTT = false;       // Following parameters used (and shown) only if this is true
  String IFTT_Key = "";       // https://ifttt.com/maker_webhooks then click Documentation
  String IFTT_Event_low = ""; // If "" no event triggered
  uint8_t IFTT_low_bl = 0;
  String IFTT_Event_high = ""; // If "" no event triggered
  uint8_t IFTT_high_bl = 0;

  // Root Page configs
  uint8_t homeRefreshS = 30;   // 0 disables autorefresh
  bool showDebugInfos = false; // shows FreeHeap, debugLog Link, etc...
  // Logging
  bool useDbgFilelog = false;
  bool _useBTFilelog = false;
  String BtLogTime_Start = "";
  String BtLogTime_Stop = "";

  // Just in case the board misbehave
  uint8_t forcedResetHRS = 0;

} ESPBluettiSettings;

extern ESPBluettiSettings wifiConfig;

// Vars to handle commands received from the web that require delays
extern bool _rebootDevice;
extern bool _resetWifiConfig;

extern void readConfigs();

extern void saveConfig();

#endif
