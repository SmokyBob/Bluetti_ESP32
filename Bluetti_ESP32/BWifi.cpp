#include "BluettiConfig.h"
#include "BWifi.h"
#include "BTooth.h"
#include <EEPROM.h>
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
bool shouldSaveConfig = false;

char bluetti_device_id[40] = "e.g. ACXXXYYYYYYYY";
char use_Meross[6] = "false";
char use_Relay[6] = "false";

void saveConfigCallback () {
  shouldSaveConfig = true;
}

ESPBluettiSettings wifiConfig;

ESPBluettiSettings get_esp32_bluetti_settings(){
    return wifiConfig;  
}

void eeprom_read(){
  //TODO: use preferences
  Serial.println("Loading Values from EEPROM");
  EEPROM.begin(512);
  EEPROM.get(0, wifiConfig);
  EEPROM.end();
}

void eeprom_saveconfig(){
  //TODO: use preferences
  Serial.println("Saving Values to EEPROM");
  EEPROM.begin(512);
  EEPROM.put(0, wifiConfig);
  EEPROM.commit();
  EEPROM.end();
}

#pragma region Async Ws handlers

void root(AsyncWebServerRequest *request) {
  #if DEBUG <= 4
  Serial.print("handleRoot() running on core ");
  Serial.println(xPortGetCoreID());
  #endif
  // server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  // server.send(200, "text/html; charset=utf-8", "");
  String data = "<HTML><HEAD><TITLE>device status</TITLE></HEAD><BODY>";
  data = data + "<table border='0'>";
  data = data + "<tr><td>host:</td><td>" + WiFi.localIP().toString() + "</td><td><a href='http://"+WiFi.localIP().toString()+"/rebootDevice' target='_blank'>reboot this device</a></td></tr>";
  data = data + "<tr><td>SSID:</td><td>" + WiFi.SSID() + "</td><td><a href='http://"+WiFi.localIP().toString()+"/resetConfig' target='_blank'>reset device config</a></td></tr>";
  data = data + "<tr><td>WiFiRSSI:</td><td>" + (String)WiFi.RSSI() + "</td></tr>";
  data = data + "<tr><td>MAC:</td><td>" + WiFi.macAddress() + "</td></tr>";
  data = data + "<tr><td>uptime (ms):</td><td>" + millis() + "</td></tr>";
  data = data + "<tr><td>uptime (h):</td><td>" + millis() / 3600000 + "</td></tr>";
  data = data + "<tr><td>uptime (d):</td><td>" + millis() / 3600000/24 + "</td></tr>";
  data = data + "<tr><td>Bluetti device id:</td><td>" + wifiConfig.bluetti_device_id + "</td></tr>";
  data = data + "<tr><td>BT connected:</td><td>" + isBTconnected() + "</td></tr>";
  data = data + "<tr><td>BT last message time:</td><td>" + getLastBTMessageTime() + "</td></tr>";
  //TODO: Create function to read from the parameters the bluetti statuses and add them to the result
  //TODO: DC e AC status change buttons
  //TODO: Change config

  data = data + "</table></BODY></HTML>";
  request->send(200, "text/html", data);

}

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
  //TODO: vedere se si riesce a ricostruire una stringa di risposta come sotto
  // String message = "File Not Found\n\n";
  // message += "URI: ";
  // message += server.uri();
  // message += "\nMethod: ";
  // message += (server.method() == HTTP_GET) ? "GET" : "POST";
  // message += "\nArguments: ";
  // message += server.args();
  // message += "\n";
  // for (uint8_t i = 0; i < server.args(); i++) {
  //   message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  // }
  // server.send(404, "text/plain", message);
}

//TODO: Rename parameters, add to header file and use in html
void handleCommand(String topic, String payloadData) {
  String topic_path = topic;
  
  Serial.print("Command arrived: ");
  Serial.print(topic);
  Serial.print(" Payload: ");
  String strPayload = payloadData;
  Serial.println(strPayload);
  
  bt_command_t command;
  command.prefix = 0x01;
  command.field_update_cmd = 0x06;

  for (int i=0; i< sizeof(bluetti_device_command)/sizeof(device_field_data_t); i++){
      if (topic_path.indexOf(map_field_name(bluetti_device_command[i].f_name)) > -1){
            command.page = bluetti_device_command[i].f_page;
            command.offset = bluetti_device_command[i].f_offset;
      }
  }
  
  command.len = swap_bytes(strPayload.toInt());
  command.check_sum = modbus_crc((uint8_t*)&command,6);
  
  sendBTCommand(command);
}

const char* PARAM_TYPE = "type";
const char* PARAM_VAL = "value";

void commandHandler(AsyncWebServerRequest *request) {
  
  String topic = "";
  String payload = "";
  bool isPost = true;

  if (request->methodToString() == "GET")
    isPost=false;
  
  if (request->hasParam(PARAM_TYPE, isPost)) {
      topic = request->getParam(PARAM_TYPE, isPost)->value();
  }
  if (request->hasParam(PARAM_VAL, isPost)) {
      payload = request->getParam(PARAM_VAL, isPost)->value();
  }

  if (topic == "" || payload == "") {
    request->send(200, "text/plain", "command data invalid: " + 
      String(PARAM_TYPE) + " " + topic + " - "+
      String(PARAM_VAL) + " " + payload);
  }else{
    request->send(200, "text/plain", "Hello, POST: " + 
      String(PARAM_TYPE) + " " + topic + " - "+
      String(PARAM_VAL) + " " + payload);

     handleCommand(topic,payload);
  }
}
#pragma endregion Async Ws handlers

void initBWifi(bool resetWifi){

  eeprom_read();
  
  if (wifiConfig.salt != EEPROM_SALT) {
    Serial.println("Invalid settings in EEPROM, trying with defaults");
    ESPBluettiSettings defaults;
    wifiConfig = defaults;
  }

  WiFiManagerParameter custom_bluetti_device("bluetti", "Bluetti Bluetooth ID", bluetti_device_id, 40);
  WiFiManagerParameter custom_use_Meross("bMeross", "Use Meross", use_Meross, 6);
  WiFiManagerParameter custom_use_Relay("bRelay", "Use Relay", use_Relay, 6);

  WiFiManager wifiManager;

  if (resetWifi){
    wifiManager.resetSettings();
    ESPBluettiSettings defaults;
    wifiConfig = defaults;
    eeprom_saveconfig();
  }

  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  wifiManager.addParameter(&custom_bluetti_device);
  wifiManager.addParameter(&custom_use_Meross);
  wifiManager.addParameter(&custom_use_Relay);

  wifiManager.autoConnect("Bluetti_ESP32");

  if (shouldSaveConfig) {
     strlcpy(wifiConfig.bluetti_device_id, custom_bluetti_device.getValue(), 40);
     strlcpy(wifiConfig.use_Meross, custom_use_Meross.getValue(), 6);
     strlcpy(wifiConfig.use_Relay, custom_use_Relay.getValue(), 6);
     eeprom_saveconfig();
  }

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());


  if (MDNS.begin(DEVICE_NAME)) {
    Serial.println("MDNS responder started");
  }

  server.on("/",HTTP_GET,root);
  server.on("/rebootDevice", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "reboot in 2sec");
    delay(2000);
    ESP.restart();
  });
  server.on("/resetConfig", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "reset Wifi and reboot in 2sec");
    delay(2000);
    initBWifi(true);
  });
  //TODO: Endpoints for commands to change AC & DC
  //TODO: endpoints to update the config WITHOUT reset

  //Commands come in with a form to the /post endpoint
  server.on("/post", HTTP_POST, commandHandler);
  #if DEBUG<=5
  server.on("/get", HTTP_GET, commandHandler); //to make thing simpler
  #endif
  
  server.onNotFound(notFound);

  server.begin();
  Serial.println("HTTP server started");
  Serial.print("Bluetti Device id to search for:");
  Serial.println(wifiConfig.bluetti_device_id);
}

void handleWebserver() {
  // server.handleClient();
}

