#include "BluettiConfig.h"
#include "BWifi.h"
#include "BTooth.h"
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include "utils.h"
#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);

void resetConfig()
{
  WiFiManager wifiManager;
  wifiManager.resetSettings();

  ESPBluettiSettings defaults;
  wifiConfig = defaults;
  saveConfig();
}

#pragma region Async Ws handlers

device_field_data_t bluetti_state_data[sizeof(bluetti_device_state) / sizeof(device_field_data_t)];

void root_HTML(AsyncWebServerRequest *request)
{
#if DEBUG <= 4
  Serial.print("handleRoot() running on core ");
  Serial.println(xPortGetCoreID());
#endif
  device_field_data_t return_data[sizeof(bluetti_device_state) / sizeof(device_field_data_t)];

  if (xQueueReceive(bluetti_data_queue, &return_data, 200) == pdTRUE)
  {
#if DEBUG <= 5
    Serial.println("Data Read from queue ");
#endif
    copyArray(return_data, bluetti_state_data, sizeof(bluetti_device_state) / sizeof(device_field_data_t));
  }

  String data = "<HTML>";
  data = data + "<HEAD>" +
         "<TITLE>" + DEVICE_NAME + "</TITLE>";

  if (wifiConfig.homeRefreshS > 0)
  {
    data = data + "<meta http-equiv='refresh' content='" + wifiConfig.homeRefreshS + "'>";
  }
  data = data + "</HEAD>";
  data = data + "<BODY>";
  if (wifiConfig.homeRefreshS > 0)
  {
    data = data + "<b>Page autorefresh every " + wifiConfig.homeRefreshS + " secs</b></br>";
  }
  data = data + "<table border='0'>";
  data = data + "<tr><td>host:</td><td>" + WiFi.localIP().toString() + "</td>" +
         "<td><a href='./rebootDevice'>Reboot this device</a></td></tr>";
  data = data + "<tr><td>SSID:</td><td>" + WiFi.SSID() + "</td>" +
         "<td><a href='./resetWifiConfig'>Reset WIFI config</a></td></tr>";
  data = data + "<tr><td>WiFiRSSI:</td><td>" + (String)WiFi.RSSI() + "</td>" +
         "<td><b><a href='./config'>Change Configuration</a></b></td></tr>";
  data = data + "<tr><td>MAC:</td><td>" + WiFi.macAddress() + "</td></tr>";
  data = data + "<tr><td>uptime :</td><td>" + convertMilliSecondsToHHMMSS(millis()) + "</td>" +
         "<td>Running since: " + runningSince + "</td></tr>";
  data = data + "<tr><td>uptime (d):</td><td>" + millis() / 3600000 / 24 + "</td></tr>";
  data = data + "<tr><td>Bluetti device id:</td><td>" + wifiConfig.bluetti_device_id + "</td>";
  if (wifiConfig._useBTFilelog)
  {
    data = data + "<td><a href='./dataLog' target='_blank'>Bluetti data Log</a></td>";
  }
  data = data + "</tr>";

  data = data + "<tr><td>BT connected:</td><td><input type='checkbox' " + ((isBTconnected()) ? "checked" : "") + " onclick='return false;' /></td>" +
         ((!isBTconnected()) ? "" : "<td><input type='button' onclick='location.href=\"./btDisconnect\"' value='Disconnect from BT'/></td>") + "</tr>";
  data = data + "<tr><td>BT last message time:</td><td>" + convertMilliSecondsToHHMMSS(getLastBTMessageTime()) + "</td></tr>";

  float perc;
  perc = float(ESP.getFreeHeap()) / float(ESP.getHeapSize());
  String toLog = "";
  toLog = toLog + "Free Heap (Bytes): " + ESP.getFreeHeap() + " of " + ESP.getHeapSize() + " (" + perc * 100 + " %)";
  writeLog(toLog);
  toLog = "";
  perc = float(SPIFFS.usedBytes()) / float(SPIFFS.totalBytes());
  toLog = toLog + "SPIFFS Used (Bytes): " + SPIFFS.usedBytes() + " of " + SPIFFS.totalBytes() + " (" + perc * 100 + " %)";
  writeLog(toLog);

  if (wifiConfig.showDebugInfos)
  {
    data = data + "<tr><td colspan='3'><hr></td></tr>";

    perc = float(ESP.getFreeHeap()) / float(ESP.getHeapSize());
    data = data + "<tr><td>Free Heap (Bytes):</td><td>" + ESP.getFreeHeap() + " of " + ESP.getHeapSize() + " (" + perc * 100 + " %)</td></tr>";
    data = data + "<tr><td><a href='./debugLog' target='_blank'>Debug Log</a></td>" +
           "<td><b><a href='./clearLog' target='_blank'>Clear Debug Log</a></b></td></tr>";
    if (wifiConfig._useBTFilelog)
    {
      data = data + "<tr><td><b><a href='./clearBtData' target='_blank'>Clear Bluetti Data Log</a></b></td></tr>";
    }

    perc = float(SPIFFS.usedBytes()) / float(SPIFFS.totalBytes());
    data = data + "<tr><td>SPIFFS Used (Bytes):</td><td>" + SPIFFS.usedBytes() + " of " + SPIFFS.totalBytes() + " (" + perc * 100 + " %)</td></tr>";
  }

  data = data + "<tr><td colspan='3'><hr></td></tr>";
  if (isBTconnected())
  {

    data = data + "<tr><td colspan='4'><b>Device States</b></td></tr>";
    // Device states as last received
    for (int i = 0; i < sizeof(bluetti_state_data) / sizeof(device_field_data_t); i++)
    {
#if DEBUG <= 4
      Serial.println("-------------");
      Serial.println(map_field_name(bluetti_state_data[i].f_name).c_str());
      Serial.println(bluetti_state_data[i].f_value);
      Serial.println("-------------");
#endif
      data = data + "<tr><td>" + map_field_name(bluetti_state_data[i].f_name).c_str() + ":</td><td>" + bluetti_state_data[i].f_value + "</td></tr>";
    }
  }
  data = data + "</table></BODY></HTML>";

  request->send(200, "text/html; charset=utf-8", data);
}

void notFound(AsyncWebServerRequest *request)
{
  bool isPost = true;

  if (request->methodToString() == "GET")
    isPost = false;

  String message = "File Not Found\n\n";
  message += "URI: ";
  message += request->url();
  message += "\nMethod: ";
  message += request->methodToString();
  message += "\nArguments: ";
  message += request->args();
  message += "\n";
  for (uint8_t i = 0; i < request->args(); i++)
  {
    message += " " + request->getParam(i)->name() + ": " + request->getParam(i)->value() + "\n";
  }
  request->send(404, "text/plain", message);
}

void handleBTCommand(String topic, String payloadData)
{
  String topic_path = topic;

  Serial.print("Command arrived: ");
  Serial.print(topic);
  Serial.print(" Payload: ");
  String strPayload = payloadData;
  Serial.println(strPayload);

  bt_command_t command;
  command.prefix = 0x01;
  command.field_update_cmd = 0x06;

  for (int i = 0; i < sizeof(bluetti_device_command) / sizeof(device_field_data_t); i++)
  {
    if (topic_path.indexOf(map_field_name(bluetti_device_command[i].f_name)) > -1)
    {
      command.page = bluetti_device_command[i].f_page;
      command.offset = bluetti_device_command[i].f_offset;
    }
  }

  command.len = swap_bytes(strPayload.toInt());
  command.check_sum = modbus_crc((uint8_t *)&command, 6);

  sendBTCommand(command);
}

const char *PARAM_TYPE = "type";
const char *PARAM_VAL = "value";

void command_HTML(AsyncWebServerRequest *request)
{

  String topic = "";
  String payload = "";
  bool isPost = true;

  if (request->methodToString() == "GET")
    isPost = false;

  if (request->hasParam(PARAM_TYPE, isPost))
    topic = request->getParam(PARAM_TYPE, isPost)->value();

  if (request->hasParam(PARAM_VAL, isPost))
    payload = request->getParam(PARAM_VAL, isPost)->value();

  if (topic == "" || payload == "")
  {
    request->send(200, "text/plain", "command data invalid: " + String(PARAM_TYPE) + " " + topic + " - " + String(PARAM_VAL) + " " + payload);
  }
  else
  {
    request->send(200, "text/plain", "Hello, POST: " + String(PARAM_TYPE) + " " + topic + " - " + String(PARAM_VAL) + " " + payload);

    // TODO: Check if the topic is one for the commands (bluetti_device_command[])
    handleBTCommand(topic, payload);
  }
}

void config_HTML(AsyncWebServerRequest *request, bool paramsSaved = false, bool resetRequired = false)
{
  // html to edit ESPBluettiSettings values
  String data = "<HTML>";
  data = data + "<HEAD>" +
         "<TITLE>" + DEVICE_NAME + " Config</TITLE>" +
         "</HEAD>";
  data = data + "<BODY>";
  if (paramsSaved)
  {
    if (resetRequired)
      data = data + "<script>setTimeout(function() { location.href = './'; }, 2000);</script>";

    data = data + "<span style='font-weight:bold;color:red'>Configuration Saved." +
           ((resetRequired) ? "Restart required (will be done in 2 second)" : "") +
           "</span><br/><br/>";
  }
  data = data + "<a href='./resetConfig' target='_blank' style='color:red'>reset FULL configuration</a>";
  data = data + "<form action='/config' method='POST'>";
  data = data + "<table border='0'>";
  data = data + "<tr><td>Bluetti device id:</td>" +
         "<td><input type='text' size=40 name='bluetti_device_id' value='" + wifiConfig.bluetti_device_id + "' /></td>" +
         "<td><a href='./'>Home</a></td></tr>";
  data = data + "<tr><td>Start in AP Mode:</td>" +
         "<td><input type='checkbox' name='APMode' value='APModeBool' " + ((wifiConfig.APMode) ? "checked" : "") + +" /></td></tr>";
  // IFTT Parameters
  // TODO: js or css to hide class showIFTT is useIFTT Unchecked
  data = data + "<tr><td>Use IFTT:</td>" +
         "<td><input type='checkbox' name='useIFTT' value='useIFTTBool' " + ((wifiConfig.useIFTT) ? "checked" : "") + +" /></td></tr>";
  data = data + "<tr class'showIFTT'><td>IFTT Key:</td>" +
         "<td><input type='text' size=25 name='IFTT_Key' value='" + wifiConfig.IFTT_Key + "' /></td></tr>";
  data = data + "<tr class'showIFTT'><td>IFTT Event - Low Battery (empty to not trigger the event):</td>" +
         "<td><input type='text' size=25 name='IFTT_Event_low' value='" + wifiConfig.IFTT_Event_low + "' /></td></tr>";
  data = data + "<tr class'showIFTT'><td>IFTT Low Battery percentage:</td>" +
         "<td><input type='number' placeholder='1.0' step='1' min='0' max='100' name='IFTT_low_bl' value='" + wifiConfig.IFTT_low_bl + "' /></td></tr>";

  data = data + "<tr class'showIFTT'><td>IFTT Event - High Battery (empty to not trigger the event):</td>" +
         "<td><input type='text' size=25 name='IFTT_Event_high' value='" + wifiConfig.IFTT_Event_high + "' /></td></tr>";
  data = data + "<tr class'showIFTT'><td>IFTT high Battery percentage:</td>" +
         "<td><input type='number' placeholder='1.0' step='1' min='0' max='100' name='IFTT_high_bl' value='" + wifiConfig.IFTT_high_bl + "' /></td></tr>";

  data = data + "<tr><td>&nbsp;</td></tr>";
  data = data + "<tr><td>Home Page Auto Refresh (sec, if 0 = No AutoRefresh):</td>" +
         "<td><input type='number' placeholder='1.0' step='1' min='0' max='3600' name='homeRefreshS' value='" + wifiConfig.homeRefreshS + "' /></td></tr>";
  data = data + "<tr><td>Show Debug Infos (FreeHeap, debugLog Link, etc...):</td>" +
         "<td><input type='checkbox' name='showDebugInfos' value='showDebugInfosBool' " + ((wifiConfig.showDebugInfos) ? "checked" : "") + +" /></td></tr>";
  data = data + "<tr><td>Enable Debug logging to File:</td>" +
         "<td><input type='checkbox' name='useDbgFilelog' value='useDbgFilelogBool' " + ((wifiConfig.useDbgFilelog) ? "checked" : "") + +" /></td></tr>";

  data = data + "<tr><td>&nbsp;</td></tr>";
  data = data + "<tr><td>Bluetti Data Log Auto Start (HH:MM) (empty=no start):</td>" +
         "<td><input type='text' size=5 name='BtLogTime_Start' value='" + wifiConfig.BtLogTime_Start + "' /></td></tr>";
  data = data + "<tr><td>Bluetti Data Log Auto Stop (HH:MM)  (empty=no stop):</td>" +
         "<td><input type='text' size=5 name='BtLogTime_Stop' value='" + wifiConfig.BtLogTime_Stop + "' /></td></tr>";

  // TODO: add other parameters accordingly
  data = data + "</table>" +
         "<input type='submit' value='Save'>" +
         "</form></BODY></HTML>";

  request->send(200, "text/html; charset=utf-8", data);

  _rebootDevice = resetRequired;
}

void config_POST(AsyncWebServerRequest *request)
{
  bool resetRequired = false;

  bool isPost = true;

  if (request->methodToString() == "GET")
    isPost = false;

  String tmp = request->getParam("bluetti_device_id", isPost)->value();

  if (tmp.compareTo(wifiConfig.bluetti_device_id) != 0)
  {
    wifiConfig.bluetti_device_id = tmp;
    resetRequired = true;
  }

  if (request->hasParam("APMode", isPost))
  {
    if (wifiConfig.APMode != true)
    {
      resetRequired = true;
    }
    wifiConfig.APMode = true;
  }
  else
  {
    if (wifiConfig.APMode != false)
    {
      resetRequired = true;
    }
    wifiConfig.APMode = false;
  }

  // IFTT (no restart required)
  if (request->hasParam("useIFTT", isPost))
  {
    wifiConfig.useIFTT = true;
    wifiConfig.IFTT_Key = request->getParam("IFTT_Key", isPost)->value();
    wifiConfig.IFTT_Event_low = request->getParam("IFTT_Event_low", isPost)->value();
    wifiConfig.IFTT_low_bl = request->getParam("IFTT_low_bl", isPost)->value().toInt();
    wifiConfig.IFTT_Event_high = request->getParam("IFTT_Event_high", isPost)->value();
    wifiConfig.IFTT_high_bl = request->getParam("IFTT_high_bl", isPost)->value().toInt();
  }
  else
  {
    wifiConfig.useIFTT = false;
  }

  wifiConfig.homeRefreshS = request->getParam("homeRefreshS", isPost)->value().toInt();

  wifiConfig.showDebugInfos = request->hasParam("showDebugInfos", isPost);
  wifiConfig._useBTFilelog = request->hasParam("_useBTFilelog", isPost);
  wifiConfig.useDbgFilelog = request->hasParam("useDbgFilelog", isPost);
  wifiConfig.BtLogTime_Start = request->getParam("BtLogTime_Start",isPost)->value();
  wifiConfig.BtLogTime_Stop = request->getParam("BtLogTime_Stop",isPost)->value();

  // TODO: manage other parameters accordingly

  // Save configuration to Preferences
  saveConfig();

  // Config Data Updated, refresh the page
  config_HTML(request, true, resetRequired);
}

void disableBT_HTML(AsyncWebServerRequest *request)
{
  disconnectBT();
  request->send(200, "text/html",
                "<html><body onload='location.href=\"./\"';></body></html>");
}

void showDebugLog_HTML(AsyncWebServerRequest *request)
{
  // File Download instead of show
  request->send(SPIFFS, "/debug_log.txt", "text/text", true);

}

void showDataLog_HTML(AsyncWebServerRequest *request)
{
  // File Download instead of show
  request->send(SPIFFS, "/bluetti_data.json", "text/text", true);
}

void setWebHandles()
{
  // WebServerConfig
  server.on("/", HTTP_GET, root_HTML);

  server.on("/rebootDevice", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    request->send(200, "text/plain", "reboot in 2sec");
    writeLog("Device Reboot requested!");
    disconnectBT();//Gracefully disconnect from BT
    _rebootDevice = true; });

  server.on("/resetWifiConfig", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    request->send(200, "text/plain", "reset Wifi and reboot in 2sec");
    _resetWifiConfig = true; });

  server.on("/resetConfig", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    request->send(200, "text/plain", "reset FULL CONFIG and reboot in 2sec");
    resetConfig();
    _rebootDevice = true; });

  // Commands come in with a form to the /post endpoint
  server.on("/post", HTTP_POST, command_HTML);
  // ONLY FOR TESTING
  // server.on("/get", HTTP_GET, command_HTML);

  // endpoints to update the config WITHOUT reset
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request)
            { config_HTML(request, false); });
  server.on("/config", HTTP_POST, config_POST);

  // BTDisconnect //TODO: post only with JS... when the html code is in a separate file
  server.on("/btDisconnect", HTTP_GET, disableBT_HTML);

  server.on("/debugLog", HTTP_GET, showDebugLog_HTML);

  server.on("/clearLog", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              clearLog();
              request->send(200, "text/plain", "Log Cleared"); });

  server.on("/clearBtData", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              clearBtData();
              request->send(200, "text/plain", "BtData Cleared"); });

  server.on("/dataLog", HTTP_GET, showDataLog_HTML);

  server.onNotFound(notFound);

  server.begin();
}

#pragma endregion Async Ws handlers

void wifiConnect(bool resetWifi)
{
  WiFiManager wifiManager;

  if (resetWifi)
  {
    wifiManager.resetSettings();
    // Reset AP Mode
    readConfigs();
    wifiConfig.APMode = false;
    saveConfig();
  }

  if (!wifiConfig.APMode)
  {
    wifiManager.setConfigPortalTimeout(180); // 180 sec timeout so that after power failure or restart it doesn't hang in portal mode

    // Use configurations to connect to Wifi Network
    bool result = wifiManager.autoConnect(DEVICE_NAME);

    if (result == false)
    {
      // Restart and try again to connect to the wifi
      ESP.restart();
    }
    // Wait for connection
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    if (MDNS.begin(DEVICE_NAME))
    {
      Serial.println("MDNS responder started");
    }
  }
  else
  {
    // Start AP MODE
    WiFi.softAP(DEVICE_NAME);
    Serial.println("");
    Serial.println("IP address: ");
    Serial.println(WiFi.softAPIP());
  }
}

void initBWifi(bool resetWifi)
{

  readConfigs();

  if (wifiConfig.salt != EEPROM_SALT)
  {
    Serial.println("Invalid settings in Preferences, trying with defaults");
    ESPBluettiSettings defaults;
    wifiConfig = defaults;
  }

  wifiConnect(resetWifi);

  setWebHandles();

  Serial.println("HTTP server started");
  writeLog("HTTP server started");
  Serial.print("Bluetti Device id to search for: ");
  Serial.println(wifiConfig.bluetti_device_id);
  // Init Array
  copyArray(bluetti_device_state, bluetti_state_data, sizeof(bluetti_device_state) / sizeof(device_field_data_t));
}

void handleWebserver()
{
  if (runningSince.length() == 0)
  {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 15 * 1000))
    {
      Serial.println("Failed to obtain time");
    }
    else
    {
      // Save start time as string in the preferred format
      // Full param list
      // https://cplusplus.com/reference/ctime/strftime/
      char buffer[80];
      strftime(buffer, 80, "%F %T", &timeinfo); // ISO

      runningSince = String(buffer);
    }
  }

  // server.handleClient();
}

String runningSince;