#include "IFTTT.h"
#include "config.h"
#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

const char *IFTTT_Server = "maker.ifttt.com";

// BASE IFTT address (parameters replaced)
const String base_address = "/trigger/{event}/json/with/key/{key}";

unsigned long millis_low = 0;
const long min_wait_low = 5;
unsigned long millis_high = 0;
const long min_wait_high = 2;

// Make an HTTP request to the IFTTT web service
void makeIFTTTRequest(String event)
{
  long curr_wait;
  long curr_millis;

  // handle requests only if parameters are set
  if (strlen(wifiConfig.IFTT_Key) > 0 &&
      (strlen(wifiConfig.IFTT_Event_low) > 0 || strlen(wifiConfig.IFTT_Event_high) > 0))
  {

    if (event == "low")
    {
      curr_wait = min_wait_low * 60 * 1000;
      curr_millis = millis_low;
    }
    else
    {
      curr_wait = min_wait_high * 60 * 1000;
      curr_millis = millis_high;
    }

    // Send the message only after some time
    if (curr_millis == 0 || ((millis() - curr_millis) > curr_wait))
    {
      Serial.print("Connecting to ");
      Serial.print(IFTTT_Server);

      WiFiClient reqClient;
      int retries = 5;

      while (!!!reqClient.connect(IFTTT_Server, 80) && (retries-- > 0))
      {
#if DEBUG <= 5
        Serial.print(".");
#endif
        retries--;
      }

      if (!!!reqClient.connected())
      {
        Serial.println("Failed to connect...");
      }
      else
      {
        String currAddr = base_address;
        currAddr.replace("{key}", wifiConfig.IFTT_Key);
        
        if (event == "low")
        {
          if (strlen(wifiConfig.IFTT_Event_low) > 0)
          {
            currAddr.replace("{event}", wifiConfig.IFTT_Event_low);
          }
          else
          {
            currAddr = "";
          }
        }
        else
        {
          if (strlen(wifiConfig.IFTT_Event_high) > 0)
          {
            currAddr.replace("{event}", wifiConfig.IFTT_Event_high);
          }
          else
          {
            currAddr = "";
          }
        }
#if DEBUG <= 5
        Serial.print("Request resource: ");
        Serial.println(currAddr);
#endif
        if (currAddr.length() > 0)
        {
          reqClient.println(String("GET ") + currAddr + " HTTP/1.1");

          // Temperature in Celsius
          // String jsonObject = String("{\"value1\":\"") + bme.readTemperature() + "\",\"value2\":\"" + (bme.readPressure()/100.0F)
          //                     + "\",\"value3\":\"" + bme.readHumidity() + "\"}";

          // Comment the previous line and uncomment the next line to publish temperature readings in Fahrenheit
          /*String jsonObject = String("{\"value1\":\"") + (1.8 * bme.readTemperature() + 32) + "\",\"value2\":\""
                              + (bme.readPressure()/100.0F) + "\",\"value3\":\"" + bme.readHumidity() + "\"}";*/

          reqClient.println(String("Host: ") + IFTTT_Server);
          reqClient.println("Connection: close");
          reqClient.println();
          // reqClient.println("Connection: close\r\nContent-Type: application/json");
          // reqClient.print("Content-Length: ");
          // reqClient.println(jsonObject.length());

          // reqClient.println(jsonObject);

          int timeout = 5; // 5 seconds
          while (!!!reqClient.available() && (timeout-- > 0))
          {
            delay(1000);
            timeout--;
          }

          if (!!!reqClient.available())
          {
            Serial.println("No response...");
          }

#if DEBUG <= 5
          while (reqClient.available())
          {
            Serial.write(reqClient.read());
          }
          Serial.println("\nClosing connection");
#endif

          reqClient.stop();

          if (event == "low")
          {
            millis_low = millis();
          }
          else
          {
            millis_high = millis();
          }
        }
      }
    }
  }
}