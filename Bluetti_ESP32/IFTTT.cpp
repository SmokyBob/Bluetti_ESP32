#include "IFTTT.h"
#include "config.h"
#ifdef IFTTT

#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

const char *IFTTT_Server = "maker.ifttt.com";

// BASE IFTT address (parameters replaced)
const String base_address = "/trigger/{event}/with/key/{key}?value1={value1}";

unsigned long millis_low = 0;
const long min_wait_low = 5;
unsigned long millis_high = 0;
const long min_wait_high = 2;

// Make an HTTP request to the IFTTT web service
void makeIFTTTRequest(String event, uint16_t battPerc)
{
  long curr_wait;
  long curr_millis;

  // handle requests only if parameters are set
  if (wifiConfig.IFTT_Key.length() > 0 &&
      (wifiConfig.IFTT_Event_low.length() > 0 || wifiConfig.IFTT_Event_high.length() > 0))
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
      Serial.print(F("Connecting to "));
      Serial.print(IFTTT_Server);

      WiFiClient reqClient;
      int retries = 5;

      while (!!!reqClient.connect(IFTTT_Server, 80) && (retries-- > 0))
      {
#if DEBUG <= 5
        Serial.print(F("."));
#endif
        retries--;
      }

      if (!!!reqClient.connected())
      {
        Serial.println(F("Failed to connect..."));
      }
      else
      {
        String currAddr = base_address;
        currAddr.replace("{key}", wifiConfig.IFTT_Key);
        currAddr.replace("{value1}", String(battPerc));

        if (event == "low")
        {
          if (wifiConfig.IFTT_Event_low.length() > 0)
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
          if (wifiConfig.IFTT_Event_high.length() > 0)
          {
            currAddr.replace("{event}", wifiConfig.IFTT_Event_high);
          }
          else
          {
            currAddr = "";
          }
        }
#if DEBUG <= 5
        Serial.print(F("Request resource: "));
        Serial.println(currAddr);
#endif
        if (currAddr.length() > 0)
        {
          reqClient.println(String("GET ") + currAddr + " HTTP/1.1");

          reqClient.println(String("Host: ") + IFTTT_Server);
          reqClient.println(F("Connection: close"));
          reqClient.println();

          int timeout = 5; // 5 seconds
          while (!!!reqClient.available() && (timeout-- > 0))
          {
            delay(1000);
            timeout--;
          }

          if (!!!reqClient.available())
          {
            Serial.println(F("No response..."));
          }

#if DEBUG <= 5
          while (reqClient.available())
          {
            Serial.write(reqClient.read());
          }
          Serial.println(F("\nClosing connection"));
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
#endif