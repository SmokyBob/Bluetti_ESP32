#include "BluettiConfig.h"
#include "MQTT.h"
#include "BWifi.h"
#include "BTooth.h"
#include "utils.h"

#include <WiFi.h>
#include <PubSubClient.h>

WiFiClient mqttClient;  
PubSubClient client(mqttClient);
int publishErrorCount = 0;
unsigned long lastMQTTMessage = 0;
unsigned long previousDeviceStatePublish = 0;

String map_field_name(enum field_names f_name){
   switch(f_name) {
      case DC_OUTPUT_POWER:
        return "dc_output_power";
        break; 
      case AC_OUTPUT_POWER:
        return "ac_output_power";
        break; 
      case DC_OUTPUT_ON:
        return "dc_output_on";
        break; 
      case AC_OUTPUT_ON:
        return "ac_output_on";
        break; 
      case POWER_GENERATION:
        return "power_generation";
        break;       
      case TOTAL_BATTERY_PERCENT:
        return "total_battery_percent";
        break; 
      case DC_INPUT_POWER:
        return "dc_input_power";
        break;
      case AC_INPUT_POWER:
        return "ac_input_power";
        break;
      case PACK_VOLTAGE:
        return "pack_voltage";
        break;
      case SERIAL_NUMBER:
        return "serial_number";
        break;
      case ARM_VERSION:
        return "arm_version";
        break;
      case DSP_VERSION:
        return "dsp_version";
        break;
      case DEVICE_TYPE:
        return "device_type";
        break;
      case UPS_MODE:
        return "ups_mode";
        break;
      case AUTO_SLEEP_MODE:
        return "auto_sleep_mode";
        break;
      case GRID_CHANGE_ON:
        return "grid_change_on";
        break;
      case INTERNAL_AC_VOLTAGE:
        return "internal_ac_voltage";
        break;
      case INTERNAL_CURRENT_ONE:
        return "internal_current_one";
        break;
      case PACK_NUM_MAX:
        return "pack_max_num";
      break;
      default:
        return "unknown";
        break;
   }
  
}

// Callback function
void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String topic_path = String(topic);
  
  Serial.print("MQTT Message arrived on topic: ");
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
  lastMQTTMessage = millis();
  
  sendBTCommand(command);
}

void subscribeTopic(enum field_names field_name) {
  char subscribeTopicBuf[512];
  ESPBluettiSettings settings = get_esp32_bluetti_settings();

  sprintf(subscribeTopicBuf, "bluetti/%s/command/%s", settings.bluetti_device_id, map_field_name(field_name).c_str() );
  client.subscribe(subscribeTopicBuf);
  lastMQTTMessage = millis();

}

void publishTopic(enum field_names field_name, String value){
  char publishTopicBuf[1024];

  #ifdef DEBUG
    Serial.print("field_name: ");
    Serial.print(map_field_name(field_name).c_str());

    Serial.print("value: ");
    Serial.print(value.c_str());
  
    Serial.println("");
  #endif

  ESPBluettiSettings settings = get_esp32_bluetti_settings();
  sprintf(publishTopicBuf, "bluetti/%s/state/%s", settings.bluetti_device_id, map_field_name(field_name).c_str() ); 
  if (!client.publish(publishTopicBuf, value.c_str() )){
    publishErrorCount++;
  }
  lastMQTTMessage = millis();
 
}

void publishDeviceState(){
  char publishTopicBuf[1024];

  ESPBluettiSettings settings = get_esp32_bluetti_settings();
  sprintf(publishTopicBuf, "bluetti/%s/state/%s", settings.bluetti_device_id, "device" ); 
  String value = "{\"IP\":\"" + WiFi.localIP().toString() + "\", \"MAC\":\"" + WiFi.macAddress() + "\", \"Uptime\":" + millis() + "}";
  if (!client.publish(publishTopicBuf, value.c_str() )){
    publishErrorCount++;
  }
  lastMQTTMessage = millis();
  previousDeviceStatePublish = millis();
 
}

void initMQTT(){

    enum field_names f_name;
    ESPBluettiSettings settings = get_esp32_bluetti_settings();
    Serial.print("Connecting to MQTT at: ");
    Serial.print(settings.mqtt_server);
    Serial.print(":");
    Serial.println(settings.mqtt_port);
    
    client.setServer(settings.mqtt_server, atoi(settings.mqtt_port));
    client.setCallback(callback);

    bool connect_result;
    const char connect_id[] = "Bluetti_ESP32";
    if (settings.mqtt_username) {
        connect_result = client.connect(connect_id, settings.mqtt_username, settings.mqtt_password);
    } else {
        connect_result = client.connect(connect_id);
    }
    
    if (connect_result) {
        
        Serial.println("Connected to MQTT Server... ");

        // subscribe to topics for commands
        for (int i=0; i< sizeof(bluetti_device_command)/sizeof(device_field_data_t); i++){
          subscribeTopic(bluetti_device_command[i].f_name);
        }
    }

    publishDeviceState();
      
};

void handleMQTT(){
    if ((millis() - lastMQTTMessage) > (MAX_DISCONNECTED_TIME_UNTIL_REBOOT * 60000)){ 
      Serial.println(F("MQTT is disconnected over allowed limit, reboot device"));
      ESP.restart();
    }
      
    if ((millis() - previousDeviceStatePublish) > (DEVICE_STATE_UPDATE * 60000)){ 
      publishDeviceState();
    }

    if (!isMQTTconnected() && publishErrorCount > 50){
      Serial.println(F("MQTT lost connection, try to reconnet"));
      client.disconnect();
      lastMQTTMessage=0;
      previousDeviceStatePublish=0;
      publishErrorCount=0;
      initMQTT();

    }
    
    client.loop();
}

bool isMQTTconnected(){
    if (client.connected()){
      return true;
    }
    else
    {
      return false;
    }  
}

int getPublishErrorCount(){
    return publishErrorCount;
}
unsigned long getLastMQTTMessageTime(){
    return lastMQTTMessage;
}
unsigned long getLastMQTDeviceStateMessageTime(){
    return previousDeviceStatePublish;
}
