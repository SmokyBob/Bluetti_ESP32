#include "BluettiConfig.h"
#include "BWifi.h"
#include "BTooth.h"
#include <ESPmDNS.h>
#include "utils.h"
#include <WebServer.h>
#include <DNSServer.h>

WebServer server(80);

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
  Serial.print("doredirect: ");
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

int _numNetworks = 0;                // init index for numnetworks wifiscans
unsigned long _lastscan = 0;         // ms for timing wifi scans
unsigned int _scancachetime = 30000; // ms cache time for preload scans
const boolean _asyncScan = false;          // perform wifi network scan async
unsigned long _startscan = 0;        // ms for timing wifi scans
const boolean _autoforcerescan = false;    // automatically force rescan if scan networks is 0, ignoring cache

bool WiFi_scanNetworks(bool force, bool async)
{

  // if 0 networks, rescan @note this was a kludge, now disabling to test real cause ( maybe wifi not init etc)
  // enable only if preload failed?
  if (_numNetworks == 0 && _autoforcerescan)
  {
    Serial.println("NO APs found forcing new scan");
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
      Serial.println("Wifi Scan running:");
      while (WiFi.scanComplete() == WIFI_SCAN_RUNNING)
      {
        Serial.println(".");
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
    rows += "No networks found. Refresh to scan again."; // No Networks
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
    const String HTTP_ITEM_STR = "<tr><td colspan='3'><a href='#' onclick='document.getElementsByName(\"ssid\")[0].value=\"{V}\";' data-ssid='{V}'>{v}</a></td></tr>";

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
      item.replace("{V}", htmlEntities(WiFi.SSID(indices[i]), false)); // ssid no encoding
      item.replace("{v}", htmlEntities(WiFi.SSID(indices[i]), true));  // ssid no encoding
      // if(tok_e) item.replace("{e}", encryptionTypeStr(enc_type));

      rows += item;
    }
  }

  return rows;
}

#pragma region Async Ws handlers

void root_HTML()
{
  if (captivePortal())
    return; // If captive portal redirect instead of displaying the page
#if DEBUG <= 4
  Serial.print("handleRoot() running on core ");
  Serial.println(xPortGetCoreID());
#endif

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
    data = data + "<tr><td>Alloc Heap / Min Free Heap(Bytes):</td><td>" +  ESP.getMaxAllocHeap() + " / " + ESP.getMinFreeHeap() + "</td></tr>";
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

  server.send(200, "text/html; charset=utf-8", data);
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

    // TODO: Check if the topic is one for the commands (bluetti_device_command[])
    handleBTCommand(topic, payload);
  }
}

void config_HTML(bool paramsSaved = false, bool resetRequired = false)
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
  data = data + "<b><a href='./'>Home</a></b>";
  data = data + "<form action='/config' method='POST'>";
  data = data + "<table border='0'>";

  data = data + "<tr><td>Bluetti device id:</td>" +
         "<td><input type='text' size=40 name='bluetti_device_id' value='" + wifiConfig.bluetti_device_id + "' /></td>" +
         "<td><a href='./resetConfig' target='_blank' style='color:red'>reset FULL configuration</a></td></tr>";

  // Scan Wifi Networks
  data = data + "<tr><td>&nbsp;</td></tr>";
  data = data + "<tr><td>Available Wifi Networs:</td><td><a href='./config'>Refresh</a></td></tr>";
  data = data + getScanItemOut();
  data = data + "<tr><td>&nbsp;</td></tr>";
  data = data + "<tr><td>Start in AP Mode:</td>" +
         "<td><input type='checkbox' name='APMode' value='APModeBool' " + ((wifiConfig.APMode) ? "checked" : "") + +" /></td></tr>";
  data = data + "<tr><td>Wifi SSID:</td><td><input type='text' size='100' name='ssid' value='" + wifiConfig.ssid + "'></td></tr>";
  data = data + "<tr><td>Wifi PWD:</td><td><input type='password' size='100' name='password' value='" + wifiConfig.password + "'></td></tr>";

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

  data = data + "<tr><td>&nbsp;</td></tr>";
  data = data + "<tr><td>Clear All Logs and Reboot:</td>" +
         "<td><input type='checkbox' name='clrSpiffOnRst' value='clrSpiffOnRstBool' " + ((wifiConfig.clrSpiffOnRst) ? "checked" : "") + +" /></td></tr>";

  // TODO: add other parameters accordingly
  data = data + "</table>" +
         "<input type='submit' value='Save'>" +
         "</form></BODY></HTML>";

  server.send(200, "text/html; charset=utf-8", data);

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

  char tmp1[100];
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

  wifiConfig.homeRefreshS = server.arg("homeRefreshS").toInt();

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

  // TODO: manage other parameters accordingly

  // Save configuration to Eprom
  saveConfig();

  // Config Data Updated, refresh the page
  config_HTML(true, resetRequired);
}

void disableBT_HTML()
{
  disconnectBT();
  server.send(200, "text/html",
              "<html><body onload='location.href=\"./\"';></body></html>");
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

void setWebHandles()
{
  // WebServerConfig
  server.on("/", HTTP_GET, root_HTML);

  server.on("/rebootDevice", HTTP_GET, []()
            {
    server.send(200, "text/plain", "reboot in 2sec");
    writeLog("Device Reboot requested!");
    disconnectBT();//Gracefully disconnect from BT
    delay(2000);
    ESP.restart(); });

  server.on("/resetWifiConfig", HTTP_GET, []()
            {
    server.send(200, "text/plain", "reset Wifi and reboot in 2sec");
    delay(2000);
    initBWifi(true); });

  server.on("/resetConfig", HTTP_GET, []()
            {
    server.send(200, "text/plain", "reset FULL CONFIG and reboot in 2sec");
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
  server.on("/btDisconnect", HTTP_GET, disableBT_HTML);

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

  server.begin();
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
      Serial.print(".");
      if ((millis() - connectTimeout) > ((WDT_TIMEOUT - 5) * 1000))
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

    //  Start AP MODE
    WiFi.softAP(DEVICE_NAME);
    Serial.println("");
    Serial.println("IP address: ");
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
    Serial.println("Invalid settings in EEPROM, trying with defaults");
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
  if (doCaptivePortal)
  {
    // DNS
    dnsServer.processNextRequest();
  }

  if (!wifiConfig.APMode && wifiConfig.ssid.length() > 0 && runningSince.length() == 0)
  {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, (WDT_TIMEOUT - 5) * 1000))
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

  server.handleClient();
}

String runningSince;