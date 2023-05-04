#include "BluettiConfig.h"
#include "BWifi.h"
#include "BTooth.h"
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
#include <DNSServer.h>
#include <AsyncElegantOTA.h>

AsyncWebServer server(80);
AsyncWebSocket webSocket("/ws");

// Dns infos for captive portal
DNSServer dnsServer;
const byte DNS_PORT = 53;

void resetConfig()
{
  ESPBluettiSettings defaults;
  wifiConfig = defaults;
  saveConfig();
}

String getFileContent(const char *path)
{
  String toRet;
  File file = SPIFFS.open(path, FILE_READ);
  if (!file)
  {
    toRet = "";
  }
  else
  {
    toRet = file.readString();
  }
  file.close();
  return toRet;
}

bool doCaptivePortal = false;

int _numNetworks = 0;                   // init index for numnetworks wifiscans
unsigned long _lastscan = 0;            // ms for timing wifi scans
unsigned int _scancachetime = 30000;    // ms cache time for preload scans
const boolean _asyncScan = false;       // perform wifi network scan async
unsigned long _startscan = 0;           // ms for timing wifi scans
const boolean _autoforcerescan = false; // automatically force rescan if scan networks is 0, ignoring cache

bool WiFi_scanNetworks(bool force, bool async)
{

  // if 0 networks, rescan @note this was a kludge, now disabling to test real cause ( maybe wifi not init etc)
  // enable only if preload failed?
  if (_numNetworks == 0 && _autoforcerescan)
  {
    Serial.println(F("NO APs found forcing new scan"));
    force = true;
  }

  // if scan is empty or stale (last scantime > _scancachetime), this avoids fast reloading wifi page and constant scan delayed page loads appearing to freeze.
  if (!_lastscan || (_lastscan > 0 && (millis() - _lastscan > _scancachetime)))
  {
    force = true;
  }

  if (force)
  {
    int8_t res;
    _startscan = millis();
    if (async && _asyncScan)
    {
#ifdef ESP8266
#ifndef WM_NOASYNC // no async available < 2.4.0
#ifdef WM_DEBUG_LEVEL
      Serial.println(F("WiFi Scan ASYNC started"));
#endif
      using namespace std::placeholders; // for `_1`
      WiFi.scanNetworksAsync(std::bind(&WiFiManager::WiFi_scanComplete, this, _1));
#else
      Serial.println(F("WiFi Scan SYNC started"));
      res = WiFi.scanNetworks();
#endif
#else
      res = WiFi.scanNetworks(true);
#endif
      return false;
    }
    else
    {
      Serial.println(F("WiFi Scan SYNC started"));
      res = WiFi.scanNetworks();
    }
    if (res == WIFI_SCAN_FAILED)
    {
      Serial.println(F("[ERROR] scan failed"));
    }
    else if (res == WIFI_SCAN_RUNNING)
    {
      Serial.println(F("Wifi Scan running:"));
      while (WiFi.scanComplete() == WIFI_SCAN_RUNNING)
      {
        Serial.println(F("."));
        delay(100);
      }
      _numNetworks = WiFi.scanComplete();
    }
    else if (res >= 0)
      _numNetworks = res;
    _lastscan = millis();

    // Serial.println(F("WiFi Scan completed in " + (String)(_lastscan - _startscan) + " ms"));

    return true;
  }
  else
  {
    // Serial.println(F("Scan is cached " + (String)(millis() - _lastscan) + " ms ago"));
  }
  return false;
}

// based on https://github.com/tzapu/WiFiManager/blob/master/WiFiManager.cpp
String getScanItemOut()
{
  String rows;

  // Scan for wifi networks
  WiFi_scanNetworks(true, false);

  int n = _numNetworks;
  if (n == 0)
  {
    rows += R"rawliteral(
            <tr><td>No networks found. Refresh to scan again.</td></tr>
          )rawliteral"; // No Networks
  }
  else
  {
    // sort networks
    int indices[n];
    for (int i = 0; i < n; i++)
    {
      indices[i] = i;
    }

    // RSSI SORT
    for (int i = 0; i < n; i++)
    {
      for (int j = i + 1; j < n; j++)
      {
        if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i]))
        {
          std::swap(indices[i], indices[j]);
        }
      }
    }

    // remove duplicates ( must be RSSI sorted )
    String cssid;
    for (int i = 0; i < n; i++)
    {
      if (indices[i] == -1)
        continue;
      cssid = WiFi.SSID(indices[i]);
      for (int j = i + 1; j < n; j++)
      {
        if (cssid == WiFi.SSID(indices[j]))
        {
          indices[j] = -1; // set dup aps to index -1
        }
      }
    }

    // Base Row
    const String WIFI_ITEM_STR = getFileContent("/templates/wifi_item.html");
    ;

    // build networks rows for display
    for (int i = 0; i < n; i++)
    {
      if (indices[i] == -1)
        continue; // skip dups

      uint8_t enc_type = WiFi.encryptionType(indices[i]);

      // if (_minimumQuality == -1 || _minimumQuality < rssiperc) {
      String item = WIFI_ITEM_STR;
      if (WiFi.SSID(indices[i]) == "")
      {
        // Serial.println(WiFi.BSSIDstr(indices[i]));
        continue; // No idea why I am seeing these, lets just skip them for now
      }
      item.replace(F("%CURR_SSID%"), htmlEntities(WiFi.SSID(indices[i]), false)); // ssid no encoding

      rows += item;
    }
  }

  return rows;
}

String processor(const String &var)
{
  String toRet = "";

  if (var == F("TITLE"))
  {
    toRet = DEVICE_NAME;
  }
  else if (var == F("AUTO_REFRESH_B"))
  {
    toRet = toRet + "<b>Page Data autorefreshed every " + String(DEVICE_STATE_UPDATE) + " secs</b></br>";
  }
  else if (var == F("HOST"))
  {
    toRet = WiFi.localIP().toString();
  }
  else if (var == F("SSID"))
  {
    toRet = WiFi.SSID();
  }
  else if (var == F("WiFiRSSI"))
  {
    toRet = WiFi.RSSI();
  }
  else if (var == F("MAC"))
  {
    toRet = WiFi.macAddress();
  }
  else if (var == F("BLUETTI_ID"))
  {
    toRet = wifiConfig.bluetti_device_id;
  }
  else if (var == F("BT_FILE_LOG"))
  {
    if (wifiConfig._useBTFilelog)
    {
      toRet = R"rawliteral(
                <td><a href="./dataLog" target="_blank" class="btConnected">Bluetti data Log</a></td>
              )rawliteral";
    }
  }
  else if (var == F("DEBUG_INFOS"))
  {
    if (wifiConfig.showDebugInfos)
    {
      toRet = getFileContent("/templates/debug_info.html");
    }
  }

  return toRet;
}

#pragma region Async Ws handlers

int BATT_PERC_MIN = 100;
int BATT_PERC_MAX = 0;
int DC_INPUT_MAX = 0;
int DC_OUTPUT_MAX = 0;
int AC_INPUT_MAX = 0;
int AC_OUTPUT_MAX = 0;

void update_root()
{
  String jsonString = "{";

  jsonString += "\"UPTIME\" : \"" + convertMilliSecondsToHHMMSS(millis()) + "\"" + ",";
  jsonString += "\"RUNNING_SINCE\" : \"" + runningSince + "\"" + ",";
  jsonString += "\"UPTIME_D\" : \"" + String(millis() / 3600000 / 24) + "\"" + ",";
  jsonString += "\"B_BT_CONNECTED\" : " + String(isBTconnected()) + "" + ",";
  jsonString += "\"BT_LAST_MEX_TIME\" : \"" + convertMilliSecondsToHHMMSS(getLastBTMessageTime()) + "\"" + ",";
  jsonString += "\"B_AC_OUTPUT_ON\" : " + String((bluetti_state_data[AC_OUTPUT_ON].f_value == "1")) + "" + ",";

  jsonString += "\"FREE_HEAP\" : \"" + String(ESP.getFreeHeap()) + "\"" + ",";
  jsonString += "\"TOTAL_HEAP\" : \"" + String(ESP.getHeapSize()) + "\"" + ",";
  jsonString += "\"PERC_HEAP\" : \"" + String((float(ESP.getFreeHeap()) / float(ESP.getHeapSize())) * 100) + "\"" + ",";

  if (((float(ESP.getFreeHeap()) / float(ESP.getHeapSize())) * 100) > 90 && wifiConfig.useDbgFilelog)
  {
    writeLog(F("SPIFFS usage over 90%. Log disabled"));
    wifiConfig.useDbgFilelog = false; // Disable Spiffs log
    saveConfig();
  }

  jsonString += "\"MAX_ALLOC\" : \"" + String(ESP.getMaxAllocHeap()) + "\"" + ",";
  jsonString += "\"MIN_ALLOC\" : \"" + String(ESP.getMinFreeHeap()) + "\"" + ",";

  jsonString += "\"SPIFFS_USED\" : \"" + String(SPIFFS.usedBytes()) + "\"" + ",";
  jsonString += "\"SPIFFS_TOTAL\" : \"" + String(SPIFFS.totalBytes()) + "\"" + ",";
  jsonString += "\"SPIFFS_PERC\" : \"" + String((float(SPIFFS.usedBytes()) / float(SPIFFS.totalBytes())) * 100) + "\"" + ",";

  jsonString += "\"SPIFFS_TOTAL\" : \"" + String(SPIFFS.totalBytes()) + "\"" + ",";

  int tmpInt = bluetti_state_data[TOTAL_BATTERY_PERCENT].f_value.toInt();

  if (BATT_PERC_MIN > tmpInt && tmpInt != 0)
  {
    BATT_PERC_MIN = tmpInt;
  }
  jsonString += "\"BATT_PERC_MIN\" : \"" + String(BATT_PERC_MIN) + "\"" + ",";
  if (BATT_PERC_MAX < tmpInt)
  {
    BATT_PERC_MAX = tmpInt;
  }
  jsonString += "\"BATT_PERC_MAX\" : \"" + String(BATT_PERC_MAX) + "\"" + ",";

  tmpInt = bluetti_state_data[DC_INPUT_POWER].f_value.toInt();

  if (DC_INPUT_MAX < tmpInt)
  {
    DC_INPUT_MAX = tmpInt;
  }
  jsonString += "\"DC_INPUT_MAX\" : \"" + String(DC_INPUT_MAX) + "\"" + ",";

  tmpInt = bluetti_state_data[DC_OUTPUT_POWER].f_value.toInt();

  if (DC_OUTPUT_MAX < tmpInt)
  {
    DC_OUTPUT_MAX = tmpInt;
  }
  jsonString += "\"DC_OUTPUT_MAX\" : \"" + String(DC_OUTPUT_MAX) + "\"" + ",";

  tmpInt = bluetti_state_data[AC_INPUT_POWER].f_value.toInt();

  if (AC_INPUT_MAX < tmpInt)
  {
    AC_INPUT_MAX = tmpInt;
  }
  jsonString += "\"AC_INPUT_MAX\" : \"" + String(AC_INPUT_MAX) + "\"" + ",";

  tmpInt = bluetti_state_data[AC_OUTPUT_POWER].f_value.toInt();

  if (AC_OUTPUT_MAX < tmpInt)
  {
    AC_OUTPUT_MAX = tmpInt;
  }
  jsonString += "\"AC_OUTPUT_MAX\" : \"" + String(AC_OUTPUT_MAX) + "\"" + ",";

  jsonString += "\"bluetti_state_data\" : {";
  for (int i = 0; i < sizeof(bluetti_state_data) / sizeof(device_field_data_t); i++)
  {
    String tmp = map_field_name(bluetti_state_data[i].f_name);
    tmp.toUpperCase();
    jsonString += "\"" + tmp + "\": " +
                  " \"" + bluetti_state_data[i].f_value + "\",";
  }
  jsonString = jsonString.substring(0, jsonString.length() - 1);
  jsonString += "},";
  String lastWebSocketTime = runningSince;
  if (!wifiConfig.APMode && wifiConfig.ssid.length() > 0)
  {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, DEVICE_STATE_UPDATE * 1000))
    {
      Serial.println(F("Failed to obtain time"));
    }
    else
    {
      // Save start time as string in the preferred format
      // Full param list
      // https://cplusplus.com/reference/ctime/strftime/
      char buffer[80];
      strftime(buffer, 80, "%F %T", &timeinfo); // ISO

      lastWebSocketTime = String(buffer);
    }
  }

  jsonString += "\"lastWebSocketTime\" : \"" + lastWebSocketTime + "\"" + "}";
#if DEBUG <= 4
  Serial.println(jsonString); // print the string for debugging.
#endif
  webSocket.textAll(jsonString); // send the JSON object through the websocket
}

void root_HTML(AsyncWebServerRequest *request)
{
#if DEBUG <= 4
  Serial.print(F("handleRoot() running on core "));
  Serial.println(xPortGetCoreID());
#endif

  // logging
  float perc;
  perc = float(ESP.getFreeHeap()) / float(ESP.getHeapSize());
  String toLog = "";
  toLog = toLog + "Free Heap (Bytes): " + ESP.getFreeHeap() + " of " + ESP.getHeapSize() + " (" + perc * 100 + " %)";
  writeLog(toLog);

  toLog = "";
  perc = float(SPIFFS.usedBytes()) / float(SPIFFS.totalBytes());
  toLog = toLog + "SPIFFS Used (Bytes): " + SPIFFS.usedBytes() + " of " + SPIFFS.totalBytes() + " (" + perc * 100 + " %)";
  writeLog(toLog);

  toLog = "";
  toLog = toLog + "Alloc Heap (Bytes): " + ESP.getMaxAllocHeap() + " Min Free Heap (Bytes):" + ESP.getMinFreeHeap();
  writeLog(toLog);

  String html = getFileContent("/index.html");

  if (html.length() == 0)
  {
    request->send(200, "text/text; charset=utf-8", "index.html not found");
  }
  else
  {
    // Replace tags in the html template before sending it to the client
    request->send_P(200, "text/html; charset=utf-8", html.c_str(), processor);
  }
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
  topic_path.toLowerCase(); // in case we recieve DC_OUTPUT_ON instead of the expected dc_output_on

  Serial.print(F("Command arrived: "));
  Serial.print(topic);
  Serial.print(F(" Payload: "));
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

      String current_name = map_field_name(bluetti_device_command[i].f_name);
      strPayload = map_command_value(current_name, strPayload);
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

    handleBTCommand(topic, payload);
  }
}

bool b_paramsSaved = false;
bool b_resetRequired = false;

String processor_config(const String &var)
{
  String toRet = "";

  if (var == F("TITLE"))
  {
    toRet = DEVICE_NAME;
  }
  else if (var == F("PARAM_SAVED"))
  {
    if (!b_paramsSaved)
    {
      toRet = F("hide");
    }
  }
  else if (var == F("RESTART_REQUIRED"))
  {
    if (!b_resetRequired)
    {
      toRet = F("hide");
    }
  }
  else if (var == F("BLUETTI_DEVICE_ID"))
  {
    toRet = wifiConfig.bluetti_device_id;
  }
  else if (var == F("WIFI_SCAN"))
  {
    toRet = getScanItemOut();
  }
  else if (var == F("B_APMODE"))
  {
    toRet = (wifiConfig.APMode) ? "checked" : "";
  }
  else if (var == F("SSID"))
  {
    toRet = wifiConfig.ssid;
  }
  else if (var == F("PASSWORD"))
  {
    toRet = wifiConfig.password;
  }
#ifdef IFTTT
  else if (var == F("B_USE_IFTT"))
  {
    toRet = (wifiConfig.useIFTT) ? "checked" : "";
  }
  else if (var == F("IFTT_KEY"))
  {
    toRet = wifiConfig.IFTT_Key;
  }
  else if (var == F("IFTT_EVENT_LOW"))
  {
    toRet = wifiConfig.IFTT_Event_low;
  }
  else if (var == F("IFTT_LOW_BL"))
  {
    toRet = wifiConfig.IFTT_low_bl;
  }
  else if (var == F("IFTT_EVENT_HIGH"))
  {
    toRet = wifiConfig.IFTT_Event_high;
  }
  else if (var == F("IFTT_HIGH_BL"))
  {
    toRet = wifiConfig.IFTT_high_bl;
  }
#endif
  else if (var == F("B_SHOW_DEBUG_INFOS"))
  {
    toRet = (wifiConfig.showDebugInfos) ? "checked" : "";
  }
  else if (var == F("B_USE_DBG_FILE_LOG"))
  {
    toRet = (wifiConfig.useDbgFilelog) ? "checked" : "";
  }
  else if (var == F("BTLOGTIME_START"))
  {
    toRet = wifiConfig.BtLogTime_Start;
  }
  else if (var == F("BTLOGTIME_STOP"))
  {
    toRet = wifiConfig.BtLogTime_Stop;
  }
  else if (var == F("B_CLRSPIFF_ON_RST"))
  {
    toRet = (wifiConfig.clrSpiffOnRst) ? "checked" : "";
  }

  return toRet;
}

void config_HTML(AsyncWebServerRequest *request, bool paramsSaved = false, bool resetRequired = false)
{
  b_paramsSaved = paramsSaved;
  b_resetRequired = resetRequired;

  String html = getFileContent("/config.html");

  if (html.length() == 0)
  {
    request->send(200, "text/text; charset=utf-8", "config.html not found");
  }
  else
  {
#ifdef IFTTT
    html.replace(F("%IFTTT%"), getFileContent("/templates/IFTTT.html"));
#else
    html.replace(F("%IFTTT%"), F(""));
#endif
    // Replace tags in the html template before sending it to the client
    request->send_P(200, "text/html; charset=utf-8", html.c_str(), processor_config);
  }

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

  char tmp1[50];
  strcpy(tmp1, request->getParam("ssid", isPost)->value().c_str());
  if (strcmp(tmp1, wifiConfig.ssid.c_str()) != 0)
  {
    wifiConfig.ssid = request->getParam("ssid", isPost)->value();
    resetRequired = true;
  }
  strcpy(tmp1, request->getParam("password", isPost)->value().c_str());
  if (strcmp(tmp1, wifiConfig.password.c_str()) != 0)
  {
    wifiConfig.password = request->getParam("password", isPost)->value();
    resetRequired = true;
  }

#ifdef IFTTT
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
#endif

  wifiConfig.showDebugInfos = request->hasParam("showDebugInfos", isPost);
  wifiConfig._useBTFilelog = request->hasParam("_useBTFilelog", isPost);
  wifiConfig.useDbgFilelog = request->hasParam("useDbgFilelog", isPost);
  wifiConfig.BtLogTime_Start = request->getParam("BtLogTime_Start", isPost)->value();
  wifiConfig.BtLogTime_Stop = request->getParam("BtLogTime_Stop", isPost)->value();

  if (request->hasParam("clrSpiffOnRst", isPost))
  {
    wifiConfig.clrSpiffOnRst = true;
    resetRequired = true;
  }

  // Save configuration to Preferences
  saveConfig();

  // Config Data Updated, refresh the page
  config_HTML(request, true, resetRequired);
}

void invertBT_HTML(AsyncWebServerRequest *request)
{
  bool restartToReconnect = false;
  if (isBTconnected())
  {
    disconnectBT();
  }
  else
  {
    // IF BLE didn't\ create memory issues
    manualDisconnect = false;
    doScan = true;
    initBluetooth(); // restart the scan task
  }

  request->send(200, "text/html",
                "<html><body onload='location.href=\"./\"';></body></html>");

  if (restartToReconnect)
  {
    ESP.restart();
  }
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

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT:
    Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    update_root(); // update the webpage accordingly
    break;
  case WS_EVT_DISCONNECT:
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
    break;
  case WS_EVT_DATA:
    // TODO: instead of post command
    // AwsFrameInfo *info = (AwsFrameInfo *)arg;
    // if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
    // {
    //   data[len] = 0;
    //   message = (char *)data;
    //   steps = message.substring(0, message.indexOf("&"));
    //   direction = message.substring(message.indexOf("&") + 1, message.length());
    //   Serial.print("steps");
    //   Serial.println(steps);
    //   Serial.print("direction");
    //   Serial.println(direction);
    //   notifyClients(direction);
    //   newRequest = true;
    // }
    break;
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  }
}

void streamFile(String filepath, String mime)
{
  File dataFile = SPIFFS.open(filepath, "r");

  dataFile.close();
}

void setWebHandles()
{
  // WebServerConfig
  server.serveStatic("/", SPIFFS, "/"); // Try the FS first for static files

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
  server.on("/btConnectionInvert", HTTP_GET, invertBT_HTML);

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

  webSocket.onEvent(onWebSocketEvent); // Register WS event handler
  server.addHandler(&webSocket);

  AsyncElegantOTA.begin(&server); // Start AsyncElegantOTA
  server.begin();
}

#pragma endregion Async Ws handlers

class CaptiveRequestHandler : public AsyncWebHandler
{
public:
  CaptiveRequestHandler()
  {
    setWebHandles(); // Routes that are managed and don't need to be redirected
  }
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request)
  {
    // request->addInterestingHeader("ANY");
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request)
  {
    Serial.println(F("<- Request redirected to captive portal"));

    AsyncResponseStream *response = request->beginResponseStream("text/plain");
    response->setCode(302);
    response->addHeader(F("Location"), (String)F("http://") + WiFi.softAPIP().toString());
    request->send(response);
  }
};

void wifiConnect(bool resetWifi)
{

  if (resetWifi)
  {
    // Reset AP Mode
    readConfigs();
    wifiConfig.APMode = true;
    wifiConfig.ssid = "";
    wifiConfig.password = "";
    saveConfig();
  }

  if (!wifiConfig.APMode && wifiConfig.ssid.length() > 0)
  {

    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiConfig.ssid.c_str(), wifiConfig.password.c_str());

    unsigned long connectTimeout = millis();
    // Wait for connection
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(F("."));
      if ((millis() - connectTimeout) > (60 * 1000 * 5))
      {
        Serial.println("Wifi Not connected with ssid: " + wifiConfig.ssid + " ! Force AP Mode");
        writeLog("Wifi Not connected with ssid: " + wifiConfig.ssid + " ! Force AP Mode");
        wifiConfig.APMode = true;
        saveConfig();
        break;
      }
    }
    Serial.println(F(""));
    Serial.println(F("IP address: "));
    Serial.println(WiFi.localIP());

    if (MDNS.begin(DEVICE_NAME))
    {
      Serial.println(F("MDNS responder started"));
    }
  }

  if (wifiConfig.APMode || wifiConfig.ssid.length() == 0)
  {

    //  Start AP MODE
    WiFi.softAP(DEVICE_NAME);
    Serial.println(F(""));
    Serial.println(F("IP address: "));
    Serial.println(WiFi.softAPIP());

    doCaptivePortal = true;
    /* Setup the DNS server redirecting all the domains to the apIP */
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER); // only when requested from AP
  }
}

void initBWifi(bool resetWifi)
{

  readConfigs();

  if (wifiConfig.salt != EEPROM_SALT)
  {
    Serial.println(F("Invalid settings in EEPROM, trying with defaults"));
    ESPBluettiSettings defaults;
    wifiConfig = defaults;
  }

  wifiConnect(resetWifi);

  if (!doCaptivePortal)
    setWebHandles();

  Serial.println(F("HTTP server started"));
  writeLog("HTTP server started");
  Serial.print(F("Bluetti Device id to search for: "));
  Serial.println(wifiConfig.bluetti_device_id);
  // Init Array
  copyArray(bluetti_device_state, bluetti_state_data, sizeof(bluetti_device_state) / sizeof(device_field_data_t));
}

unsigned long webSockeUpdate = 0;

void handleWebserver()
{
  if (doCaptivePortal)
  {
    // DNS
    dnsServer.processNextRequest();
  }

  if (!wifiConfig.APMode && wifiConfig.ssid.length() > 0 && runningSince.length() == 0)
  {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, DEVICE_STATE_UPDATE * 1000))
    {
      Serial.println(F("Failed to obtain time"));
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

  if ((unsigned long)(millis() - webSockeUpdate) >= DEVICE_STATE_UPDATE * 1000)
  {
    update_root();             // Update the root page with the latest data
    webSockeUpdate = millis(); // Use the snapshot to set track time until next event
  }

  webSocket.cleanupClients();
}

String runningSince;