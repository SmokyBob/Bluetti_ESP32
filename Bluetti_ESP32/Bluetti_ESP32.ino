#include "BWifi.h"
#include "BTooth.h"
#include "config.h"
#include "BluettiConfig.h"
#include <esp_task_wdt.h>
#include "WiFi.h"

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

  // Init time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer.c_str());

  // initialize the queue used to share device state between processes
  bluetti_data_queue = xQueueCreate(1, sizeof(bluetti_device_state));

  initLog(); // Init debug log to file

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

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Wifi Not connected! Reconnecting");
    writeLog("Wifi Not connected! Reconnecting");
    esp_task_wdt_delete(NULL); // Remove watchdog
    delay(10000);              // Wait 10 secs before trying again
    wifiConnect(false);        // REinit Wifi
  }
  else
  {
    if ((millis() - serialTick) > (5 * 1000))
    {
      if (esp_task_wdt_status(NULL) == ESP_ERR_NOT_FOUND)
      {
        esp_task_wdt_add(NULL); // add current thread to WDT watch
      }
      else
      {
        esp_task_wdt_reset(); // Reset WDT every 5 secs in
      }

      serialTick = millis();
    }
  }

  if (wifiConfig.forcedResetHRS != 0)
  {

    if (millis() > (wifiConfig.forcedResetHRS * 60 * 60 * 1000))
    {
      writeLog("Forced restart after " + String(wifiConfig.forcedResetHRS) + " Hrs");
      delay(500);
      ESP.restart();
    }
  }

  delay(50); // Delay to give time to other internal tasks to run
}
