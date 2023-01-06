#include "BluettiConfig.h"
#include "BWifi.h"
#include "BTooth.h"
#include <EEPROM.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "utils.h"

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

  server.on("/", handleRoot);
  server.on("/rebootDevice", []() {
    server.send(200, "text/plain", "reboot in 2sec");
    delay(2000);
    ESP.restart();
  });
  server.on("/resetConfig", []() {
    server.send(200, "text/plain", "reset Wifi and reboot in 2sec");
    delay(2000);
    initBWifi(true);
  });
  //TODO: Endpoints for commands to change AC & DC
  //TODO: endpoints to update the config WITHOUT reset
  
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");

}

void handleWebserver() {
  server.handleClient();
}

void handleRoot() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html; charset=utf-8", "");
  server.sendContent("<HTML><HEAD><TITLE>device status</TITLE></HEAD><BODY>");
  server.sendContent("<table border='0'>");
  String data = "<tr><td>host:</td><td>" + WiFi.localIP().toString() + "</td><td><a href='http://"+WiFi.localIP().toString()+"/rebootDevice' target='_blank'>reboot this device</a></td></tr>";
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

  server.sendContent(data);
  server.sendContent("</table></BODY></HTML>");
  server.client().stop();

}

void handleNotFound() {
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

//TODO: Rename parameters, add to header file and use in html
void handleCommand(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String topic_path = String(topic);
  
  Serial.print("Command arrived: ");
  Serial.print(topic);
  Serial.print(" Payload: ");
  String strPayload = String((char * ) payload);
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
  // lastMQTTMessage = millis();
  
  sendBTCommand(command);
}
