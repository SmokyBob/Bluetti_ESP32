#include "IFTTT.h"
#include "HTTPClient.h"

const char* IFTTT_Server = "http://maker.ifttt.com";

// Replace with your unique IFTTT URL resource
// How your resource variable should look like, but with your own API KEY (that API KEY below is just an example):
//const char* resource = "/trigger/bme280_readings/with/key/nAZjOphL3d-ZO4N3k64-1A7gTlNSrxMJdmqy3";

const char* resource_low = "/trigger/YOUR_COMMAND_low/json/with/key/c40OcIijjeHv38vpg8OtCb";
const char* resource_high = "/trigger/YOUR_COMMAND_75/json/with/key/c40OcIijjeHv38vpg8OtCb";

unsigned long millis_low = 0;
const int min_wait_low = 5;
unsigned long millis_high = 0;
const int min_wait_high = 2;

// Make an HTTP request to the IFTTT web service
void makeIFTTTRequest(String event) {
  long curr_wait;
  unsigned long curr_millis;

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
    
    HTTPClient reqClient;
    String serverPath = IFTTT_Server ;

    if (event=="low"){
      Serial.println(resource_low);
      serverPath = serverPath + resource_low;
    }else{
      Serial.println(resource_high);
      serverPath = serverPath + resource_high;
    }
    // Your Domain name with URL path or IP address with path
    reqClient.begin(serverPath.c_str());
    
    // If you need Node-RED/server authentication, insert user and password below
    //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");
    
    // Send HTTP GET request
    int httpResponseCode = reqClient.GET();
    
    if (httpResponseCode>0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = reqClient.getString();
      Serial.println(payload);
    }
    else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    // Free resources
    reqClient.end();

    if (event=="low"){
      millis_low = millis();
    }else{
      millis_high = millis();
    }
  }
}