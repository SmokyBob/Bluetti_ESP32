#include "Arduino.h"
#include "config.h"
#include <Preferences.h>

bool _rebootDevice = false;
bool _resetWifiConfig = false;

ESPBluettiSettings wifiConfig;

Preferences prf_config;

void readConfigs()
{

#if DEBUG < 5
  Serial.println(F("Loading Values from Preferences"));
#endif

  prf_config.begin("bluetti_esp32", false);

  wifiConfig.salt = prf_config.getInt("salt", EEPROM_SALT);
  wifiConfig.bluetti_device_id = prf_config.getString("bluetti_id", "");

  wifiConfig.APMode = prf_config.getBool("APMode", true);
  wifiConfig.ssid = prf_config.getString("ssid", "");
  wifiConfig.password = prf_config.getString("password", "");

#ifdef IFTTT
  wifiConfig.useIFTT = prf_config.getBool("useIFTT", false);
  wifiConfig.IFTT_Key = prf_config.getString("IFTT_Key", "");
  wifiConfig.IFTT_Event_low = prf_config.getString("IFTT_Event_low", "");
  wifiConfig.IFTT_low_bl = prf_config.getShort("IFTT_low_bl", 0);
  wifiConfig.IFTT_Event_high = prf_config.getString("IFTT_Event_high", "");
  wifiConfig.IFTT_high_bl = prf_config.getShort("IFTT_high_bl", 0);
#endif

  wifiConfig.showDebugInfos = prf_config.getBool("showDebugInfos", false);
  wifiConfig.useDbgFilelog = prf_config.getBool("useDbgFilelog", false);

  wifiConfig.BtLogTime_Start = prf_config.getString("BtLogTime_Start", "");
  wifiConfig.BtLogTime_Stop = prf_config.getString("BtLogTime_Stop", "");
  wifiConfig.clrSpiffOnRst = prf_config.getBool("clrSpiffOnRst", false);

  prf_config.end();
}

void saveConfig()
{

#if DEBUG < 5
  Serial.println(F("Saving Values to Preferences"));
#endif
  prf_config.begin("bluetti_esp32", false);

  prf_config.putInt("salt", wifiConfig.salt);
  prf_config.putString("bluetti_id", wifiConfig.bluetti_device_id);
  prf_config.putBool("APMode", wifiConfig.APMode);
  prf_config.putString("ssid", wifiConfig.ssid);
  prf_config.putString("password", wifiConfig.password);

#ifdef IFTTT
  prf_config.putBool("useIFTT", wifiConfig.useIFTT);
  prf_config.putString("IFTT_Key", wifiConfig.IFTT_Key);
  prf_config.putString("IFTT_Event_low", wifiConfig.IFTT_Event_low);
  prf_config.putShort("IFTT_low_bl", wifiConfig.IFTT_low_bl);
  prf_config.putString("IFTT_Event_high", wifiConfig.IFTT_Event_high);
  prf_config.putShort("IFTT_high_bl", wifiConfig.IFTT_high_bl);
#endif

  prf_config.putBool("showDebugInfos", wifiConfig.showDebugInfos);
  prf_config.putBool("useDbgFilelog", wifiConfig.useDbgFilelog);

  prf_config.putString("BtLogTime_Start", wifiConfig.BtLogTime_Start);
  prf_config.putString("BtLogTime_Stop", wifiConfig.BtLogTime_Stop);
  prf_config.putBool("clrSpiffOnRst", wifiConfig.clrSpiffOnRst);

  prf_config.end();
}