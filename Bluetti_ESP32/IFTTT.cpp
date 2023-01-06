#include "IFTTT.h"
#ifdef ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif

const char* IFTTT_Server = "maker.ifttt.com";

// Replace with your unique IFTTT URL resource
// How your resource variable should look like, but with your own API KEY (that API KEY below is just an example):
//const char* resource = "/trigger/bme280_readings/with/key/nAZjOphL3d-ZO4N3k64-1A7gTlNSrxMJdmqy3";

const char* resource_low = "/trigger/YOUR_COMMAND_low/json/with/key/c40OcIijjeHv38vpg8OtCb";
const char* resource_high = "/trigger/YOUR_COMMAND_75/json/with/key/c40OcIijjeHv38vpg8OtCb";

unsigned long millis_low = 0;
const long min_wait_low = 5;
unsigned long millis_high = 0;
const long min_wait_high = 2;



// Make an HTTP request to the IFTTT web service
void makeIFTTTRequest(String event) {
  long curr_wait;
  long curr_millis;

  if (event=="low"){
    curr_wait = min_wait_low * 60 * 1000;
    curr_millis = millis_low;
  }else{
    curr_wait = min_wait_high * 60 * 1000;
    curr_millis = millis_high;
  }

  //Send the message only after some time
  if ((millis() - curr_millis) > curr_wait){ 
    Serial.print("Connecting to "); 
    Serial.print(IFTTT_Server);
    
    WiFiClient client;
    int retries = 5;
    while(!!!client.connect(IFTTT_Server, 80) && (retries-- > 0)) {
      Serial.print(".");
    }
    Serial.println();
    if(!!!client.connected()) {
      Serial.println("Failed to connect...");
    }
    
    Serial.print("Request resource: "); 
    if (event=="low"){
      Serial.println(resource_low);
      client.println(String("GET ") + resource_low + " HTTP/1.1");
    }else{
      Serial.println(resource_high);
      client.println(String("GET ") + resource_high + " HTTP/1.1");
    }
    
    

    // Temperature in Celsius
    // String jsonObject = String("{\"value1\":\"") + bme.readTemperature() + "\",\"value2\":\"" + (bme.readPressure()/100.0F)
    //                     + "\",\"value3\":\"" + bme.readHumidity() + "\"}";
                        
    // Comment the previous line and uncomment the next line to publish temperature readings in Fahrenheit                    
    /*String jsonObject = String("{\"value1\":\"") + (1.8 * bme.readTemperature() + 32) + "\",\"value2\":\"" 
                        + (bme.readPressure()/100.0F) + "\",\"value3\":\"" + bme.readHumidity() + "\"}";*/
                        
    client.println(String("Host: ") + IFTTT_Server); 
    client.println("Connection: close");
    client.println();
    // client.println("Connection: close\r\nContent-Type: application/json");
    // client.print("Content-Length: ");
    // client.println(jsonObject.length());
    
    // client.println(jsonObject);
          
    int timeout = 5 * 10; // 5 seconds             
    while(!!!client.available() && (timeout-- > 0)){
      delay(100);
    }
    if(!!!client.available()) {
      Serial.println("No response...");
    }
    while(client.available()){
      Serial.write(client.read());
    }
    
    Serial.println("\nclosing connection");
    client.stop(); 

    if (event=="low"){
      millis_low = millis();
    }else{
      millis_high = millis();
    }
  }
}