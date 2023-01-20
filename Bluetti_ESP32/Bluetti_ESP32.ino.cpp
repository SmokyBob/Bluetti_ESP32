# 1 "C:\\Users\\mauro\\AppData\\Local\\Temp\\tmp491uhy4r"
#include <Arduino.h>
# 1 "C:/Progetti/Bluetti_ESP32/Bluetti_ESP32/Bluetti_ESP32.ino"
#include "BWifi.h"
#include "BTooth.h"
#include "config.h"
#include "BluettiConfig.h"
#include <esp_task_wdt.h>
void setup();
void loop();
#line 7 "C:/Progetti/Bluetti_ESP32/Bluetti_ESP32/Bluetti_ESP32.ino"
void setup()
{
  Serial.begin(115200);
#if RELAISMODE == 1
  pinMode(RELAIS_PIN, OUTPUT);
#if DEBUG <= 5
  Serial.println("deactivate relais contact");
#endif
  digitalWrite(RELAIS_PIN, RELAIS_LOW);
#endif


  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer.c_str());


  bluetti_data_queue = xQueueCreate(1, sizeof(bluetti_device_state));


  initLog();
  writeLog("---------------------------");
  writeLog("Starting");

  initBWifi(false);
  initBluetooth();

  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL);
}

#if logHeap > 0
unsigned long heapMillis = 0;
bool printHeap = false;
#define heapPrintSeconds 5
#endif

unsigned long serialTick = 0;

void loop()
{
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
    Serial.println("------------------------ heap loop start        " + String(ESP.getHeapSize() - ESP.getFreeHeap()));
  }
#endif

  handleBluetooth();
#if logHeap > 0
  if (printHeap)
  {
    Serial.println("------------------------ after BT handle        " + String(ESP.getHeapSize() - ESP.getFreeHeap()));
  }
#endif

  handleWebserver();
#if logHeap > 0
  if (printHeap)
  {
    Serial.println("------------------------ after WebServer handle " + String(ESP.getHeapSize() - ESP.getFreeHeap()));
  }
#endif

  if ((millis() - serialTick) > (5 * 1000))
  {
    esp_task_wdt_reset();

    serialTick = millis();
  }

#ifdef FORCED_REBOOT_HRS
  if (millis() > (FORCED_REBOOT_HRS * 60 * 60 * 1000))
  {
    ESP.restart();
  }
#endif

  delay(50);
}