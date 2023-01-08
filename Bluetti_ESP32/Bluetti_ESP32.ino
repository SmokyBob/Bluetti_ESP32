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

void loop() {
  handleBluetooth();
  handleWebserver();
}
