#include "BWifi.h"
#include "BTooth.h"
#include "config.h"
#include "BluettiConfig.h"
#include <esp_task_wdt.h>
#include "WiFi.h"
#if USE_TEMPERATURE_SENSOR == 1
#include "SHT2x.h"
#endif
#if USE_EXT_BAT == 1
#include "esp_adc_cal.h"
#endif

#if USE_TEMPERATURE_SENSOR == 1
SHT2x tempSensor;
#endif

void setup()
{
  Serial.begin(115200);
#if RELAISMODE == 1
  pinMode(RELAIS_PIN, OUTPUT);
#if DEBUG <= 5
  Serial.println(F("deactivate relais contact"));
#endif
  digitalWrite(RELAIS_PIN, RELAIS_LOW);
#endif

#if USE_EXT_BAT == 1
  pinMode(PWM_SWITCH_PIN, OUTPUT);
  // Voltage measurements calibration
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  vref = adc_chars.vref; // Obtain the device ADC reference voltage
#endif

#if USE_TEMPERATURE_SENSOR == 1
  tempSensor.begin(TEMPERATURE_SDA_PIN, TEMPERATURE_SCL_PIN);
#endif

  // Init time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer.c_str());
  readConfigs();

  initLog(); // Init SPIFFS and debug log to file

  if (wifiConfig.clrSpiffOnRst)
  {
    clearSpiffLog();
    wifiConfig.clrSpiffOnRst = false;
    saveConfig();
  }

  initBWifi(false); // Init Wifi and WebServer
  writeLog("---------------------------");
  writeLog("Starting");
  initBluetooth();

  esp_task_wdt_init(WDT_TIMEOUT, true); // enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);               // add current thread to WDT watch
}

#if logHeap > 0
unsigned long heapMillis = 0;
bool printHeap = false;
#define heapPrintSeconds 5
#endif

unsigned long serialTick = 0;

void loop()
{
  if (_rebootDevice)
  {
    // Device Reboot requested from the UI
    delay(2000);
    ESP.restart();
  }

  if (_resetWifiConfig)
  {
    // Reset Wifi config from the UI
    delay(2000);
    initBWifi(true);
  }

  if (!wifiConfig.APMode && wifiConfig.ssid.length() > 0 && WiFi.status() != WL_CONNECTED)
  {
    Serial.println(F("Wifi Not connected! Reconnecting"));
    writeLog("Wifi Not connected! Reconnecting");
    esp_task_wdt_delete(NULL); // Remove watchdog
    delay(10000);              // Wait 10 secs before trying again
    wifiConnect(false);        // REinit Wifi
  }
  else
  {
    if ((millis() - serialTick) > (DEVICE_STATE_UPDATE * 1000))
    {
      if (esp_task_wdt_status(NULL) == ESP_ERR_NOT_FOUND)
      {
        esp_task_wdt_add(NULL); // add current thread to WDT watch
      }
      else
      {
        esp_task_wdt_reset(); // Reset WDT every 5 secs in
      }
#if USE_TEMPERATURE_SENSOR == 1
      tempSensor.read();
      temperature = tempSensor.getTemperature();
      humidity = tempSensor.getHumidity();
#endif
#if USE_EXT_BAT == 1
      curr_EXT_Voltage = getVoltage();
      Serial.printf("Voltage %.2f \n", curr_EXT_Voltage);
#endif
      serialTick = millis();
    }
  }

#if logHeap > 0
  if ((millis() - heapMillis) > (heapPrintSeconds * 1000))
  {
    printHeap = true;
    heapMillis = millis();
  }
  else
  {
    printHeap = false;
  }

  if (printHeap)
  {
    Serial.println(F("------------------------ heap loop start        " + String(ESP.getHeapSize() - ESP.getFreeHeap())));
  }
#endif

  handleBluetooth();
#if logHeap > 0
  if (printHeap)
  {
    Serial.println(F("------------------------ after BT handle        " + String(ESP.getHeapSize() - ESP.getFreeHeap())));
  }
#endif

  handleWebserver();
#if logHeap > 0
  if (printHeap)
  {
    Serial.println(F("------------------------ after WebServer handle " + String(ESP.getHeapSize() - ESP.getFreeHeap())));
  }
#endif

  if (wifiConfig.forcedResetHRS != 0)
  {

    if (millis() > (wifiConfig.forcedResetHRS * 60 * 60 * 1000))
    {
      writeLog("Forced restart after " + String(wifiConfig.forcedResetHRS) + " Hrs");
      delay(500);
      ESP.restart();
    }
  }
  // Get Current Time to write before the log
  // Full param list
  // https://cplusplus.com/reference/ctime/strftime/

  char buffer[80];
  time_t tt = time(0);
  tm *currTime = localtime(&tt);
  bool enableLog = false;

  if (wifiConfig.BtLogTime_Start.length() > 0)
  {
    uint8_t hh = -1;
    u_int8_t mm = -1;

    try
    {
      sscanf(wifiConfig.BtLogTime_Start.c_str(), "%d:%d", &hh, &mm);
    }
    catch (const std::exception &e)
    {
      // Do mothing
      hh = -1;
      mm = -1;
    }

    if (currTime->tm_hour > hh || (currTime->tm_hour == hh && currTime->tm_min >= mm))
    {
      enableLog = true;
    }
  }

  if (wifiConfig.BtLogTime_Stop.length() > 0)
  {
    uint8_t hh = -1;
    u_int8_t mm = -1;

    try
    {
      sscanf(wifiConfig.BtLogTime_Stop.c_str(), "%d:%d", &hh, &mm);
    }
    catch (const std::exception &e)
    {
      // Do mothing
      hh = -1;
      mm = -1;
    }

    if (currTime->tm_hour > hh || (currTime->tm_hour == hh && currTime->tm_min >= mm))
    {
      enableLog = false;
    }
  }

  if (enableLog != wifiConfig._useBTFilelog)
  {
    wifiConfig._useBTFilelog = enableLog;
    if (enableLog)
    {
      writeLog("Auto Start Bluetti Data Log");
    }
    else
    {
      writeLog("Auto STOP Bluetti Data Log");
    }
  }

  delay(50); // Delay to give time to other internal tasks to run
}
