#include "Arduino.h"
#include "config.h"
#include <Preferences.h>

bool _rebootDevice = false;
bool _resetWifiConfig = false;

#if USE_TEMPERATURE_SENSOR == 1
float temperature = 0.00;
float humidity = 0.00;
#endif

#if USE_EXT_BAT == 1
float vref = 1100;
float curr_EXT_Voltage = 18.3;
bool _pwm_switch_status = false;
float _voltArray[5];
bool voltInitComplete = false;
byte voltArrayindex = 0;

void calculateVoltage()
{
  // Calculate the current voltage and add it to the last five values array
  float calibration = wifiConfig.volt_calibration; // Adjust for ultimate accuracy when input is measured using an accurate DVM, if reading too high then use e.g. 0.99, too low use 1.01

  // R1 10k, R2 2.2k, output 3.3v e fatto calcolare input massimo = 18.3, massimo a batteria piena sono 14.6 (lifepo4)
  float voltage = (float)analogRead(VOLT_PIN) / 4095 // Risoluzione ADC
                  * 18.3                             // Voltaggio massimo
                  * (1100 / vref)                    // Offset calibrazione
                  * ((10000 + // R1 or the resistence connected to positive (real value)
                    2200) / // R2 or the resistence connectect to GND (real value)
                    2200) *
                  calibration;

  _voltArray[voltArrayindex] = voltage;
  voltArrayindex = voltArrayindex + 1;

  if (voltArrayindex == 5)
  {
    voltArrayindex = 0;
    if (voltInitComplete == false)
    {
      voltInitComplete = true;
    }
  }
}

float getVoltage()
{
  if (voltInitComplete)
  {

    double sum = 0.00; // sum will be larger than an item, double for safety.
    for (int i = 0; i < 5; i++)
      sum += _voltArray[i];
    return ((float)sum) / 5; // average will be fractional, so float may be appropriate.
  }
  else
  {
    return _voltArray[voltArrayindex];
  }
};

void setSwitch(bool bON)
{
  if (bON)
  {
    digitalWrite(PWM_SWITCH_PIN, HIGH);
  }
  else
  {
    digitalWrite(PWM_SWITCH_PIN, LOW);
  }
  _pwm_switch_status = bON;

  Serial.printf("pwm_switch set to: %s \n", String(_pwm_switch_status));
};
#endif

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

#if USE_EXT_BAT == 1
  wifiConfig.volt_Switch_off = prf_config.getFloat("volt_Switch_off", 12.0);
  wifiConfig.volt_Switch_ON = prf_config.getFloat("volt_Switch_ON", 12.6);
  wifiConfig.volt_MAX_BLUETT_PERC = prf_config.getShort("volt_MAX_PERC", 80);
  wifiConfig.volt_calibration = prf_config.getFloat("volt_calib", 1.1074);
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

#if USE_EXT_BAT == 1
  prf_config.putFloat("volt_Switch_off", wifiConfig.volt_Switch_off);
  prf_config.putFloat("volt_Switch_ON", wifiConfig.volt_Switch_ON);
  prf_config.putShort("volt_MAX_PERC", wifiConfig.volt_MAX_BLUETT_PERC);
  prf_config.putFloat("volt_calib", wifiConfig.volt_calibration);
#endif

  prf_config.putBool("showDebugInfos", wifiConfig.showDebugInfos);
  prf_config.putBool("useDbgFilelog", wifiConfig.useDbgFilelog);

  prf_config.putString("BtLogTime_Start", wifiConfig.BtLogTime_Start);
  prf_config.putString("BtLogTime_Stop", wifiConfig.BtLogTime_Stop);
  prf_config.putBool("clrSpiffOnRst", wifiConfig.clrSpiffOnRst);

  prf_config.end();
}