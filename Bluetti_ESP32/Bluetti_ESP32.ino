#include "BWifi.h"
#include "BTooth.h"
#include "config.h"
#include "BluettiConfig.h"
#include <esp_task_wdt.h>

void setup() {
  Serial.begin(115200);
  #if RELAISMODE == 1
    pinMode(RELAIS_PIN, OUTPUT);
    #if DEBUG <= 5
      Serial.println("deactivate relais contact");
    #endif
    digitalWrite(RELAIS_PIN, RELAIS_LOW);
  #endif

  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch

  //Init time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  //initialize the queue used to share device state between processes
  bluetti_data_queue = xQueueCreate(1,sizeof(bluetti_device_state));

  initBWifi(false);//Init Wifi and WebServer
  initBluetooth();
}

#if logHeap > 0
unsigned long heapMillis = 0;
bool printHeap = false;
#define heapPrintSeconds 5
#endif

unsigned long serialTick = 0;

void loop() {
  #if logHeap > 0
  if ((millis()-heapMillis)>(heapPrintSeconds*1000)){
    printHeap = true;
    heapMillis = millis();
  }else{
    printHeap = false;
  }

  if (printHeap){
    Serial.println("------------------------ heap loop start        " + String(ESP.getHeapSize()-ESP.getFreeHeap()));
  }
  #endif
  
  handleBluetooth();
  #if logHeap > 0
  if (printHeap){
    Serial.println("------------------------ after BT handle        " + String(ESP.getHeapSize()-ESP.getFreeHeap()));
  }
  #endif
  
  handleWebserver();
  #if logHeap > 0
  if (printHeap){
    Serial.println("------------------------ after WebServer handle " + String(ESP.getHeapSize()-ESP.getFreeHeap()));
  }
  #endif

  if ((millis()-serialTick)>(5*1000)){
    esp_task_wdt_reset();//Reset WDT every 5 secs in
    Serial.println("Loop Running");
    serialTick = millis();
  }

  delay(50);//Delay to give time to other internal tasks to run
  
}
