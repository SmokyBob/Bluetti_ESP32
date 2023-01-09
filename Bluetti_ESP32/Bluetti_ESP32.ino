#include "BWifi.h"
#include "BTooth.h"
#include "config.h"
#include "BluettiConfig.h"

void setup() {
  Serial.begin(115200);
  #if RELAISMODE == 1
    pinMode(RELAIS_PIN, OUTPUT);
    #if DEBUG <= 5
      Serial.println("deactivate relais contact");
    #endif
    digitalWrite(RELAIS_PIN, RELAIS_LOW);
  #endif

  //initialize the queue used to share device state between processes
  bluetti_data_queue = xQueueCreate(1,sizeof(bluetti_device_state));

  initBWifi(false);//AsyncWebServer configured here
  initBluetooth();
}

#if logHeap > 0
int heapMillis = 0;
bool printHeap = false;
#define heapPrintSeconds 5
#endif

void loop() {
  #if logHeap > 0
  if ((millis()-heapMillis)>(heapPrintSeconds*1000)){
    printHeap = true;
    heapMillis = millis();
  }else{
    printHeap = false;
  }

  if (printHeap){
    Serial.println("------------------------ heap loop start        " + String(ESP.getFreeHeap()));
  }
  #endif
  
  handleBluetooth();
  #if logHeap > 0
  if (printHeap){
    Serial.println("------------------------ after BT handle        " + String(ESP.getFreeHeap()));
  }
  #endif
  
  handleWebserver();
  #if logHeap > 0
  if (printHeap){
    Serial.println("------------------------ after WebServer handle " + String(ESP.getFreeHeap()));
  }
  #endif
  
}
