#include "BluettiConfig.h"
#include "BWifi.h"
#include "BTooth.h"
#include <ESPmDNS.h>
#include "utils.h"
#include <WebServer.h>
#include <DNSServer.h>
#include "html.h"
#include "WebSocketsServer.h"
#include <ElegantOTA.h>

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81); // create instance for webSocket server on port"81"

// Dns infos for captive portal
DNSServer dnsServer;
const byte DNS_PORT = 53;

void resetConfig()
{
  ESPBluettiSettings defaults;
  wifiConfig = defaults;
  saveConfig();
}

bool doCaptivePortal = false;

// Adapted from https://github.com/tzapu/WiFiManager/blob/master/WiFiManager.cpp
boolean captivePortal()
{
#if DEBUG <= 4
  Serial.println(F(" captive portal check"));
#endif
  if (!doCaptivePortal)
    return false; // skip redirections, @todo maybe allow redirection even when no cp ? might be useful

#if DEBUG <= 4
  Serial.println(F("Captive portal enabled"));
#endif
  String serverLoc = server.client().localIP().toString();
  if (server.client().localPort() != 80)
    serverLoc += ":80";                               // add port if not default
  bool doredirect = serverLoc != server.hostHeader(); // redirect if hostheader not server ip, prevent redirect loops

#if DEBUG <= 4
  Serial.print(F("doredirect: "));
  Serial.println(doredirect);
#endif

  if (doredirect)
  {
    Serial.println(F("<- Request redirected to captive portal"));
    server.sendHeader(F("Location"), (String)F("http://") + serverLoc, true); // @HTTPHEAD send redirect
    server.send(302, "text/plain", "");                                       // Empty content inhibits Content-length header so we have to close the socket ourselves.
    server.client().stop();                                                   // Stop is needed because we sent no content length
    return true;
  }
  return false;
}

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
    rows += wifiNoNet_html; // No Networks
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
    const String HTTP_ITEM_STR = wifiRow_html;

    // build networks rows for display
    for (int i = 0; i < n; i++)
    {
      if (indices[i] == -1)
        continue; // skip dups

      uint8_t enc_type = WiFi.encryptionType(indices[i]);

      // if (_minimumQuality == -1 || _minimumQuality < rssiperc) {
      String item = HTTP_ITEM_STR;
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
      toRet = bt_log_Link_html;
    }
  }
  else if (var == F("DEBUG_INFOS"))
  {
    if (wifiConfig.showDebugInfos)
    {
      toRet = debugInfos_html;
    }
  }

  return toRet;
}

// Should help with the transition to AsyncWebServer
String replacer(PGM_P content)
{
  String toRet = String(content);

  toRet.replace(F("%TITLE%"), processor("TITLE"));
  toRet.replace(F("%AUTO_REFRESH_B%"), processor("AUTO_REFRESH_B"));
  toRet.replace(F("%HOST%"), processor("HOST"));
  toRet.replace(F("%SSID%"), processor("SSID"));
  toRet.replace(F("%WiFiRSSI%"), processor("WiFiRSSI"));
  toRet.replace(F("%MAC%"), processor("MAC"));
  toRet.replace(F("%BLUETTI_ID%"), processor("BLUETTI_ID"));
  toRet.replace(F("%BT_FILE_LOG%"), processor("BT_FILE_LOG"));
  toRet.replace(F("%DEBUG_INFOS%"), processor("DEBUG_INFOS"));

  return toRet;
}

#pragma region Async Ws handlers

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

  jsonString += "\"MAX_ALLOC\" : \"" + String(ESP.getMaxAllocHeap()) + "\"" + ",";
  jsonString += "\"MIN_ALLOC\" : \"" + String(ESP.getMinFreeHeap()) + "\"" + ",";

  jsonString += "\"SPIFFS_USED\" : \"" + String(SPIFFS.usedBytes()) + "\"" + ",";
  jsonString += "\"SPIFFS_TOTAL\" : \"" + String(SPIFFS.totalBytes()) + "\"" + ",";
  jsonString += "\"SPIFFS_PERC\" : \"" + String((float(SPIFFS.usedBytes()) / float(SPIFFS.totalBytes())) * 100) + "\"" + ",";

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

  jsonString += "\"lastWebSocketTime\" : \"" + lastWebSocketTime + "\"" + "}";
#if DEBUG <= 4
  Serial.println(jsonString); // print the string for debugging.
#endif
  webSocket.broadcastTXT(jsonString); // send the JSON object through the websocket
}

void root_HTML()
{
  if (captivePortal())
    return; // If captive portal redirect instead of displaying the page
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

  // Replace tags in the html template before sending it to the client
  server.send(200, "text/html; charset=utf-8", replacer(index_html));
}

void notFound()
{
  if (captivePortal())
    return; // If captive portal redirect instead of displaying the page

  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
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

void command_HTML()
{

  String topic = "";
  String payload = "";

  if (server.hasArg(PARAM_TYPE))
  {
    topic = server.arg(PARAM_TYPE);
  }
  if (server.hasArg(PARAM_VAL))
  {
    payload = server.arg(PARAM_VAL);
  }

  if (topic == "" || payload == "")
  {
    server.send(200, "text/plain", "command data invalid: " + String(PARAM_TYPE) + " " + topic + " - " + String(PARAM_VAL) + " " + payload);
  }
  else
  {
    server.send(200, "text/plain", "Hello, POST: " + String(PARAM_TYPE) + " " + topic + " - " + String(PARAM_VAL) + " " + payload);

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
      toRet = F("display:none");
    }
  }
  else if (var == F("RESTART_REQUIRED"))
  {
    if (!b_resetRequired)
    {
      toRet = F("display:none");
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

// Should help with the transition to AsyncWebServer
String replacer_config(PGM_P content)
{
  String toRet = String(content);

#ifdef IFTTT
  toRet.replace(F("%IFTTT%"), IFTTT_html);
#else
  toRet.replace(F("%IFTTT%"), F(""));
#endif

  toRet.replace(F("%TITLE%"), processor_config("TITLE"));
  toRet.replace(F("%PARAM_SAVED%"), processor_config("PARAM_SAVED"));
  toRet.replace(F("%RESTART_REQUIRED%"), processor_config("RESTART_REQUIRED"));
  toRet.replace(F("%BLUETTI_DEVICE_ID%"), processor_config("BLUETTI_DEVICE_ID"));
  toRet.replace(F("%WIFI_SCAN%"), processor_config("WIFI_SCAN"));
  toRet.replace(F("%B_APMODE%"), processor_config("B_APMODE"));
  toRet.replace(F("%SSID%"), processor_config("SSID"));
  toRet.replace(F("%PASSWORD%"), processor_config("PASSWORD"));
  toRet.replace(F("%B_USE_IFTT%"), processor_config("B_USE_IFTT"));
  toRet.replace(F("%IFTT_KEY%"), processor_config("IFTT_KEY"));
  toRet.replace(F("%IFTT_EVENT_LOW%"), processor_config("IFTT_EVENT_LOW"));
  toRet.replace(F("%IFTT_LOW_BL%"), processor_config("IFTT_LOW_BL"));
  toRet.replace(F("%IFTT_EVENT_HIGH%"), processor_config("IFTT_EVENT_HIGH"));
  toRet.replace(F("%IFTT_HIGH_BL%"), processor_config("IFTT_HIGH_BL"));
  toRet.replace(F("%B_SHOW_DEBUG_INFOS%"), processor_config("B_SHOW_DEBUG_INFOS"));
  toRet.replace(F("%B_USE_DBG_FILE_LOG%"), processor_config("B_USE_DBG_FILE_LOG"));
  toRet.replace(F("%BTLOGTIME_START%"), processor_config("BTLOGTIME_START"));
  toRet.replace(F("%BTLOGTIME_STOP%"), processor_config("BTLOGTIME_STOP"));
  toRet.replace(F("%B_CLRSPIFF_ON_RST%"), processor_config("B_CLRSPIFF_ON_RST"));

  return toRet;
}

void config_HTML(bool paramsSaved = false, bool resetRequired = false)
{
  b_paramsSaved = paramsSaved;
  b_resetRequired = resetRequired;

  server.send(200, "text/html; charset=utf-8", replacer_config(config_html));

  if (resetRequired)
  {
    delay(2000);
    ESP.restart();
  }
}

void config_POST()
{
  bool resetRequired = false;

  char tmp[40];
  strcpy(tmp, server.arg("bluetti_device_id").c_str());

  if (strcmp(tmp, wifiConfig.bluetti_device_id.c_str()) != 0)
  {
    wifiConfig.bluetti_device_id = server.arg("bluetti_device_id");
    resetRequired = true;
  }

  if (server.hasArg("APMode"))
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
  strcpy(tmp1, server.arg("ssid").c_str());
  if (strcmp(tmp1, wifiConfig.ssid.c_str()) != 0)
  {
    wifiConfig.ssid = server.arg("ssid");
    resetRequired = true;
  }
  strcpy(tmp1, server.arg("password").c_str());
  if (strcmp(tmp1, wifiConfig.password.c_str()) != 0)
  {
    wifiConfig.password = server.arg("password");
    resetRequired = true;
  }

#ifdef IFTTT
  // IFTT (no restart required)
  if (server.hasArg("useIFTT"))
  {
    wifiConfig.useIFTT = true;
    wifiConfig.IFTT_Key = server.arg("IFTT_Key");
    wifiConfig.IFTT_Event_low = server.arg("IFTT_Event_low");
    wifiConfig.IFTT_low_bl = server.arg("IFTT_low_bl").toInt();
    wifiConfig.IFTT_Event_high = server.arg("IFTT_Event_high");
    wifiConfig.IFTT_high_bl = server.arg("IFTT_high_bl").toInt();
  }
  else
  {
    wifiConfig.useIFTT = false;
  }
#endif

  wifiConfig.showDebugInfos = server.hasArg("showDebugInfos");
  wifiConfig._useBTFilelog = server.hasArg("_useBTFilelog");
  wifiConfig.useDbgFilelog = server.hasArg("useDbgFilelog");
  wifiConfig.BtLogTime_Start = server.arg("BtLogTime_Start");
  wifiConfig.BtLogTime_Stop = server.arg("BtLogTime_Stop");

  if (server.hasArg("clrSpiffOnRst"))
  {
    wifiConfig.clrSpiffOnRst = true;
    resetRequired = true;
  }

  // Save configuration to Eprom
  saveConfig();

  // Config Data Updated, refresh the page
  config_HTML(true, resetRequired);
}

void invertBT_HTML()
{
  bool restartToReconnect = false;
  if (isBTconnected())
  {
    disconnectBT();
  }
  else
  {
    // IF BLE didn't create memory issues
    //  manualDisconnect = false;
    //  doScan = true;
    //  initBluetooth();//restart the scan task
    // Bud it does... so...
    restartToReconnect = true;
  }

  server.send(200, "text/html",
              "<html><body onload='location.href=\"./\"';></body></html>");

  if (restartToReconnect)
  {
    ESP.restart();
  }
}

void showDebugLog_HTML()
{
  // File Download instead of show
  File download = SPIFFS.open("/debug_log.txt", FILE_READ);
  server.sendHeader("Content-Type", "text/text");
  server.sendHeader("Content-Disposition", "attachment; filename=debug_log.txt");
  server.sendHeader("Connection", "close");
  server.streamFile(download, "application/octet-stream");
  download.close();
}

void showDataLog_HTML()
{
  // File Download instead of show
  File download = SPIFFS.open("/bluetti_data.json", FILE_READ);
  server.sendHeader("Content-Type", "text/text"); // Return as text
  server.sendHeader("Content-Disposition", "attachment; filename=bluetti_data.json");
  server.sendHeader("Connection", "close");
  server.streamFile(download, "application/octet-stream");
  download.close();
}

void webSocketEvent(byte num, WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED: // enum that read status this is used for debugging.
    Serial.print("WS Type ");
    Serial.print(type);
    Serial.println(": DISCONNECTED");
    break;
  case WStype_CONNECTED: // Check if a WebSocket client is connected or not
    Serial.print("WS Type ");
    Serial.print(type);
    Serial.println(": CONNECTED");
    update_root(); // update the webpage accordingly
    break;
  case WStype_TEXT:   // check responce from client
    Serial.println(); // the payload variable stores the status internally
    Serial.println(payload[0]);
    // TODO: instead of post command
    //  if (payload[0] == '1') {
    //    pin_status = "ON";
    //    digitalWrite(22, HIGH);
    //  }
    //  if (payload[0] == '0') {
    //    pin_status = "OFF";
    //    digitalWrite(22, LOW);
    //  }
    break;
  }
}

void setWebHandles()
{
  // WebServerConfig
  server.on("/", HTTP_GET, root_HTML);

  server.on("/rebootDevice", HTTP_GET, []()
            {
    server.send(200, "text/plain", "reboot in 2sec"); //TODO: add js to reload homepage
    writeLog("Device Reboot requested!");
    disconnectBT();//Gracefully disconnect from BT
    delay(2000);
    ESP.restart(); });

  server.on("/resetWifiConfig", HTTP_GET, []()
            {
    server.send(200, "text/plain", "reset Wifi and reboot in 2sec");//TODO: add js to reload homepage
    delay(2000);
    initBWifi(true); });

  server.on("/resetConfig", HTTP_GET, []()
            {
    server.send(200, "text/plain", "reset FULL CONFIG and reboot in 2sec"); //TODO: add js to reload homepage
    resetConfig();
    delay(2000);
    ESP.restart(); });

  // Commands come in with a form to the /post endpoint
  server.on("/post", HTTP_POST, command_HTML);
  // ONLY FOR TESTING
  // server.on("/get", HTTP_GET, command_HTML);

  // endpoints to update the config WITHOUT reset
  server.on("/config", HTTP_GET, []()
            { config_HTML(false); });
  server.on("/config", HTTP_POST, config_POST);

  // BTDisconnect //TODO: post only with JS... when the html code is in a separate file
  server.on("/btConnectionInvert", HTTP_GET, invertBT_HTML);

  server.on("/debugLog", HTTP_GET, showDebugLog_HTML);

  server.on("/clearLog", HTTP_GET, []()
            {
              clearLog();
              server.send(200, "text/plain", "Log Cleared"); });

  server.on("/clearBtData", HTTP_GET, []()
            {
              clearBtData();
              server.send(200, "text/plain", "BtData Cleared"); });

  server.on("/dataLog", HTTP_GET, showDataLog_HTML);

  server.onNotFound(notFound);

  ElegantOTA.begin(&server); // Start ElegantOTA
  server.begin();

  webSocket.begin();                 // init the Websocketserver
  webSocket.onEvent(webSocketEvent); // init the webSocketEvent function when a websocket event occurs
}

#pragma endregion Async Ws handlers

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
      if ((millis() - connectTimeout) > (DEVICE_STATE_UPDATE * 1000))
      {
        Serial.println("Wifi Not connected with ssid: " + wifiConfig.ssid + " ! Reset config");
        writeLog("Wifi Not connected with ssid: " + wifiConfig.ssid + " ! Reset config");
        wifiConfig.APMode = true;
        wifiConfig.ssid = "";
        wifiConfig.password = "";
        saveConfig();
        ESP.restart();
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
  else
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

  server.handleClient();

  webSocket.loop(); // websocket server methode that handles all Client
  if ((unsigned long)(millis() - webSockeUpdate) >= DEVICE_STATE_UPDATE * 1000)
  {
    update_root();             // Update the root page with the latest data
    webSockeUpdate = millis(); // Use the snapshot to set track time until next event
  }
}

String runningSince;