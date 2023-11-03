#ifndef CONFIG_H
#define CONFIG_H
#include "Arduino.h"
#include "logger.h"

// Info = 5
// Log = 4
#define DEBUG 100 // For relase set to 100, only setup infos will be printed
#define logHeap 0 // 1=log heap value in loop

#define EEPROM_SALT 13374

#define DEVICE_NAME "BLUETTI-ESP32"

#define BLUETOOTH_QUERY_MESSAGE_DELAY 2500
#define DEVICE_STATE_UPDATE 5

#define RELAISMODE 0
#define RELAIS_PIN 23
#define RELAIS_LOW LOW
#define RELAIS_HIGH HIGH

#define USE_TEMPERATURE_SENSOR 1 // Set to 0 for no STH* / HUT21 temp sensor
#define TEMPERATURE_SDA_PIN 21
#define TEMPERATURE_SCL_PIN 22

#define USE_EXT_BAT 1 // Set to 0 for External battery
#define VOLT_PIN 36   // SP o VP
#define PWM_SWITCH_PIN 18

#define MAX_DISCONNECTED_TIME_UNTIL_REBOOT 2 // device will reboot when wlan/BT is not connectet within x Minutes

#ifndef BLUETTI_TYPE
#define BLUETTI_TYPE EB3A
#endif

// Watch Dog Timeout in seconds (7 minutes for OTA web updates)
#define WDT_TIMEOUT 420

// NTP configs
static String ntpServer = "pool.ntp.org";
static long gmtOffset_sec = 3600 * (+1);
static int daylightOffset_sec = 3600;

#define IFTTT // Decomment for IFTTT support and configuration

// N.B. if changed, update the function config_HTML to edit the fields
// readConfigs and saveConfig needs to be updated too
typedef struct
{
  int salt = EEPROM_SALT;

  String bluetti_device_id = "";
  bool APMode = false; // Start in AP Mode
  String ssid = "";
  String password = "";
#ifdef IFTTT
  // IFTT Parameters
  bool useIFTT = false;       // Following parameters used (and shown) only if this is true
  String IFTT_Key = "";       // https://ifttt.com/maker_webhooks then click Documentation
  String IFTT_Event_low = ""; // If "" no event triggered
  uint8_t IFTT_low_bl = 0;
  String IFTT_Event_high = ""; // If "" no event triggered
  uint8_t IFTT_high_bl = 0;
#endif

#if USE_EXT_BAT == 1
  float volt_Switch_off = 12.0;
  float volt_Switch_ON = 12.6;
  uint8_t volt_MAX_BLUETT_PERC = 80;
  float volt_calibration = 1.1074;
#endif
  // Root Page configs
  bool showDebugInfos = false; // shows FreeHeap, debugLog Link, etc...
  // Logging
  bool useDbgFilelog = false;
  bool _useBTFilelog = false;
  String BtLogTime_Start = "";
  String BtLogTime_Stop = "";

  // Just in case the board misbehave
  uint8_t forcedResetHRS = 0;
  bool clrSpiffOnRst = false;

} ESPBluettiSettings;

extern ESPBluettiSettings wifiConfig;

extern void readConfigs();

extern void saveConfig();

// Vars to handle commands received from the web that require delays
extern bool _rebootDevice;
extern bool _resetWifiConfig;

#if USE_TEMPERATURE_SENSOR == 1
extern float temperature;
extern float humidity;
#endif

#if USE_EXT_BAT == 1
extern float vref;
extern float curr_EXT_Voltage;
extern bool _pwm_switch_status;
extern void calculateVoltage();
extern float getVoltage();
extern void setSwitch(bool bON);
#endif

#endif
