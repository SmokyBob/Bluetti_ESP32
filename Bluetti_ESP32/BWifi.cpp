#include "BluettiConfig.h"
#include "BWifi.h"
#include "BTooth.h"
#include <EEPROM.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include "utils.h"
#include <WebServer.h>

WebServer server(80);
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

device_field_data_t bluetti_state_data[sizeof(bluetti_device_state)/sizeof(device_field_data_t)];

void root() {
  #if DEBUG <= 4
  Serial.print("handleRoot() running on core ");
  Serial.println(xPortGetCoreID());
  #endif
  device_field_data_t return_data[sizeof(bluetti_device_state)/sizeof(device_field_data_t)];
  
  if (xQueueReceive(bluetti_data_queue, &return_data, 0)==pdTRUE){
    #if DEBUG <= 5
    Serial.println("Data Read from queue ");
    #endif
    copyArray(return_data,bluetti_state_data,sizeof(bluetti_device_state)/sizeof(device_field_data_t));
  }

  String data = "<HTML>";
  data = data + "<HEAD>"+
                "<TITLE>"+DEVICE_NAME+"</TITLE>" +
                "<meta http-equiv='refresh' content='10'>" +
                "</HEAD>";
  data = data + "<BODY>";
  data = data + "<table border='0'>";
  data = data + "<tr><td>host:</td><td>" + WiFi.localIP().toString() + "</td><td><a href='http://"+WiFi.localIP().toString()+"/rebootDevice' target='_blank'>reboot this device</a></td></tr>";
  data = data + "<tr><td>SSID:</td><td>" + WiFi.SSID() + "</td><td><a href='http://"+WiFi.localIP().toString()+"/resetConfig' target='_blank'>reset device config</a></td></tr>";
  data = data + "<tr><td>WiFiRSSI:</td><td>" + (String)WiFi.RSSI() + "</td></tr>";
  data = data + "<tr><td>MAC:</td><td>" + WiFi.macAddress() + "</td></tr>";
  data = data + "<tr><td>uptime :</td><td>" + convertMilliSecondsToHHMMSS(millis()) + "</td></tr>";
  data = data + "<tr><td>uptime (d):</td><td>" + millis() / 3600000/24 + "</td></tr>";
  data = data + "<tr><td>Bluetti device id:</td><td>" + wifiConfig.bluetti_device_id + "</td></tr>";
  data = data + "<tr><td>BT connected:</td><td>" + isBTconnected() + "</td></tr>";
  data = data + "<tr><td>BT last message time:</td><td>" + convertMilliSecondsToHHMMSS(getLastBTMessageTime()) + "</td></tr>";
  data = data + "<tr><td colspan='4'><hr></td></tr>";
  if (isBTconnected()){
    
    data = data + "<tr><td colspan='4'><b>Device States</b></td></tr>";
    //Device states as last received
    for(int i=0; i< sizeof(bluetti_state_data)/sizeof(device_field_data_t); i++){
      #if DEBUG <= 4
      Serial.println("-------------");
      Serial.println(map_field_name(bluetti_state_data[i].f_name).c_str());
      Serial.println(bluetti_state_data[i].f_value);
      Serial.println("-------------");
      #endif
      data = data + "<tr><td>"+map_field_name(bluetti_state_data[i].f_name).c_str()+":</td><td>" + bluetti_state_data[i].f_value + "</td></tr>";    
    }
  }
  data = data + "</table></BODY></HTML>";

  server.send(200, "text/html; charset=utf-8", data);

}

void notFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

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

void commandHandler() {
  
  String topic = "";
  String payload = "";
  
  if (server.hasArg(PARAM_TYPE)) {
      topic = server.arg(PARAM_TYPE);
  }
  if (server.hasArg(PARAM_VAL)) {
      payload = server.arg(PARAM_VAL);
  }

  if (topic == "" || payload == "") {
    server.send(200, "text/plain", "command data invalid: " + 
      String(PARAM_TYPE) + " " + topic + " - "+
      String(PARAM_VAL) + " " + payload);
  }else{
    server.send(200, "text/plain", "Hello, POST: " + 
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

  wifiManager.autoConnect(DEVICE_NAME);

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

  //WebServerConfig
  server.on("/",HTTP_GET,root);
  server.on("/rebootDevice", HTTP_GET, [] () {
    server.send(200, "text/plain", "reboot in 2sec");
    delay(2000);
    ESP.restart();
  });
  server.on("/resetConfig", HTTP_GET, [] () {
    server.send(200, "text/plain", "reset Wifi and reboot in 2sec");
    delay(2000);
    initBWifi(true);
  });

  //TODO: endpoints to update the config WITHOUT reset

  //Commands come in with a form to the /post endpoint
  server.on("/post", HTTP_POST, commandHandler);
  //ONLY FOR TESTING 
  server.on("/get", HTTP_GET, commandHandler);
  
  server.onNotFound(notFound);

  server.begin();
  Serial.println("HTTP server started");
  Serial.print("Bluetti Device id to search for: ");
  Serial.println(wifiConfig.bluetti_device_id);
  //Init Array
  copyArray(bluetti_device_state,bluetti_state_data,sizeof(bluetti_device_state)/sizeof(device_field_data_t));
}

void handleWebserver() {
  server.handleClient();
}