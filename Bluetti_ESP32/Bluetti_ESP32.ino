#include "BWifi.h"
#include "BTooth.h"
#include "config.h"

void setup() {
  Serial.begin(115200);
  #if RELAISMODE == 1
    pinMode(RELAIS_PIN, OUTPUT);
    #if DEBUG <= 5
      Serial.println("deactivate relais contact");
    #endif
    digitalWrite(RELAIS_PIN, RELAIS_LOW);
  #endif
  initBWifi(false);
  initBluetooth();
}

void loop() {
  handleBluetooth();
  handleWebserver();
}
