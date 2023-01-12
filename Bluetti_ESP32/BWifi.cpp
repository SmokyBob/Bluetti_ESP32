#include "BluettiConfig.h"
#include "BWifi.h"
#include "BTooth.h"
#include <EEPROM.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include "utils.h"
#include <WebServer.h>

WebServer server(80);
// bool shouldSaveConfig = false;

// char bluetti_device_id[40] = "e.g. ACXXXYYYYYYYY";
// char use_Meross[6] = "false";
// char use_Relay[6] = "false";

// void saveConfigCallback () {
//   shouldSaveConfig = true;
// }

ESPBluettiSettings wifiConfig;

ESPBluettiSettings get_esp32_bluetti_settings(){
    return wifiConfig;  
}

void readConfigs(){
  Serial.println("Loading Values from EEPROM");
  EEPROM.begin(512);
  EEPROM.get(0, wifiConfig);
  EEPROM.end();
}

void saveConfig(){
  Serial.println("Saving Values to EEPROM");
  EEPROM.begin(512);
  EEPROM.put(0, wifiConfig);
  EEPROM.commit();
  EEPROM.end();
}

void resetConfig(){
  WiFiManager wifiManager;
  wifiManager.resetSettings();

  ESPBluettiSettings defaults;
  wifiConfig = defaults;
  saveConfig();
}


#pragma region Async Ws handlers

device_field_data_t bluetti_state_data[sizeof(bluetti_device_state)/sizeof(device_field_data_t)];

void root_HTML() {
  #if DEBUG <= 4
  Serial.print("handleRoot() running on core ");
  Serial.println(xPortGetCoreID());
  #endif
  device_field_data_t return_data[sizeof(bluetti_device_state)/sizeof(device_field_data_t)];
  
  if (xQueueReceive(bluetti_data_queue, &return_data, 200)==pdTRUE){
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
  data = data + "<tr><td>host:</td><td>" + WiFi.localIP().toString() + "</td>"+
                "<td><a href='http://"+WiFi.localIP().toString()+"/rebootDevice' target='_blank'>reboot this device</a></td></tr>";
  data = data + "<tr><td>SSID:</td><td>" + WiFi.SSID() + "</td>"+
                "<td><a href='http://"+WiFi.localIP().toString()+"/resetWifiConfig' target='_blank'>reset WIFI config</a></td></tr>";
  data = data + "<tr><td>WiFiRSSI:</td><td>" + (String)WiFi.RSSI() + "</td>"+
                "<td><b><a href='http://"+WiFi.localIP().toString()+"/config'>Change Configuration</a></b></td></tr>";
  data = data + "<tr><td>MAC:</td><td>" + WiFi.macAddress() + "</td></tr>";
  data = data + "<tr><td>uptime :</td><td>" + convertMilliSecondsToHHMMSS(millis()) + "</td><td>Running since: "+runningSince+"</td></tr>";
  data = data + "<tr><td>uptime (d):</td><td>" + millis() / 3600000/24 + "</td></tr>";
  data = data + "<tr><td>Bluetti device id:</td><td>" + wifiConfig.bluetti_device_id+ "</td></tr>";
  data = data + "<tr><td>BT connected:</td><td><input type='checkbox' " + ((isBTconnected())?"checked":"") + " onclick='return false;' /></td>"+
                ((!isBTconnected())?"":"<td><input type='button' onclick='location.href=""./btDisconnect""' value='Disconnect from BT'/></td>")+"</tr>";
  data = data + "<tr><td>BT last message time:</td><td>" + convertMilliSecondsToHHMMSS(getLastBTMessageTime()) + "</td></tr>";
  data = data + "<tr><td colspan='3'><hr></td></tr>";
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

void handleBTCommand(String topic, String payloadData) {
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

void command_HTML() {
  
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
    
    //TODO: Check if the topic is one for the commands (bluetti_device_command[])
    handleBTCommand(topic,payload);
  }
}

void config_HTML(bool paramsSaved = false,bool resetRequired = false){
  //html to edit ESPBluettiSettings values
  String data = "<HTML>";
  data = data + "<HEAD>"+
                "<TITLE>"+DEVICE_NAME+" Config</TITLE>" +
                "</HEAD>";
  data = data + "<BODY>";
  if (paramsSaved){
    if(resetRequired)
      data = data + "<script>setTimeout(function() { location.href = './'; }, 2000);</script>";
    
    data = data + "<span style='font-weight:bold;color:red'>Configuration Saved."+
                  ((resetRequired)?"Restart required (will be done in 2 second)":"")+
                  "</span><br/><br/>";
  }
  data = data + "<a href='http://"+WiFi.localIP().toString()+"/resetConfig' target='_blank' style='color:red'>reset FULL configuration</a>";
  data = data + "<form action='/config' method='POST'>";
  data = data + "<table border='0'>";
  data = data + "<tr><td>Bluetti device id:</td>"+
                "<td><input type='text' size=40 name='bluetti_device_id' value='"+ wifiConfig.bluetti_device_id  +"' /></td>"+
                "<td><a href='http://"+WiFi.localIP().toString()+"/'>Home</a></td></tr>";
  data = data + "<tr><td>Start in AP Mode:</td>"+
                "<td><input type='checkbox' name='APMode' value='APModeBool' "+ ((wifiConfig.APMode)?"checked":"") + +" /></td></tr>";
  //TODO: add other parameters accordingly
  data = data + "</table>"+
                "<input type='submit' value='Save'>"+
                "</form></BODY></HTML>";

  server.send(200, "text/html; charset=utf-8", data);

  if(resetRequired){
    delay(2000);
    ESP.restart();
  }

}

void config_POST(){
  bool resetRequired = false;

  char tmp[40];
  strcpy(tmp, server.arg("bluetti_device_id").c_str());

  if (strcmp(tmp,wifiConfig.bluetti_device_id) != 0){
    strcpy(wifiConfig.bluetti_device_id,tmp);
    resetRequired=true;
  }

  if (server.hasArg("APMode")) {
    if (wifiConfig.APMode != true){
      resetRequired = true;
    }
    wifiConfig.APMode = true;
  }else{
    if (wifiConfig.APMode != false){
      resetRequired = true;
    }
    wifiConfig.APMode = false;
  }

  //TODO: manage other parameters accordingly

  //Save configuration to Eprom
  saveConfig();

  //Config Data Updated, refresh the page 
  config_HTML(true,resetRequired);
}

void disableBT_HTML(){
  disconnectBT();
  root_HTML();

}

#pragma endregion Async Ws handlers

void initBWifi(bool resetWifi){

  readConfigs();
  
  if (wifiConfig.salt != EEPROM_SALT) {
    Serial.println("Invalid settings in EEPROM, trying with defaults");
    ESPBluettiSettings defaults;
    wifiConfig = defaults;
  }
  
  WiFiManager wifiManager;

  if (resetWifi){
    wifiManager.resetSettings();
    //Reset AP Mode
    readConfigs();
    wifiConfig.APMode = false;
    saveConfig();
  }

  // wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  if (!wifiConfig.APMode){
    //Use configurations to connect to Wifi Network
    wifiManager.autoConnect(DEVICE_NAME);
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

  }else{
    //Start AP MODE
    WiFi.softAP(DEVICE_NAME);
    Serial.println("");
    Serial.println("IP address: ");
    Serial.println(WiFi.softAPIP());
  }

  
  
  //WebServerConfig
  server.on("/",HTTP_GET,root_HTML);
  server.on("/rebootDevice", HTTP_GET, [] () {
    server.send(200, "text/plain", "reboot in 2sec");
    disconnectBT();//Gracefully disconnect from BT
    delay(2000);
    ESP.restart();
  });
  server.on("/resetWifiConfig", HTTP_GET, [] () {
    server.send(200, "text/plain", "reset Wifi and reboot in 2sec");
    delay(2000);
    initBWifi(true);
  });
  server.on("/resetConfig", HTTP_GET, [] () {
    server.send(200, "text/plain", "reset FULL CONFIG and reboot in 2sec");
    resetConfig();
    delay(2000);
    ESP.restart();
  });
  

  //Commands come in with a form to the /post endpoint
  server.on("/post", HTTP_POST, command_HTML);
  //ONLY FOR TESTING 
  server.on("/get", HTTP_GET, command_HTML);

  //endpoints to update the config WITHOUT reset
  server.on("/config", HTTP_GET, [] () {config_HTML(false);});
  server.on("/config", HTTP_POST, config_POST);

  //BTDisconnect //TODO: post only with JS... when the html code is in a separate file
  server.on("/btDisconnect", HTTP_GET, disableBT_HTML);
  
  server.onNotFound(notFound);

  server.begin();
  Serial.println("HTTP server started");
  Serial.print("Bluetti Device id to search for: ");
  Serial.println(wifiConfig.bluetti_device_id);
  //Init Array
  copyArray(bluetti_device_state,bluetti_state_data,sizeof(bluetti_device_state)/sizeof(device_field_data_t));
}

void handleWebserver() {
  if (runningSince.length()==0){
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
      Serial.println("Failed to obtain time");
    }else{
      //Save start time as string in the preferred format
      //Full param list
      //https://cplusplus.com/reference/ctime/strftime/
      char buffer [80];
      strftime(buffer,80,"%F %T",&timeinfo);//ISO

      runningSince = String(buffer);
    }

  }
  server.handleClient();
}

String runningSince;