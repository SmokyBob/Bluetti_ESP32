#include "BluettiConfig.h"
#include "MQTT.h"
#include "BWifi.h"
#include "BTooth.h"
#include "utils.h"
#include "config.h"

#include <WiFi.h>
#include <PubSubClient.h>

WiFiClient mqttClient;
PubSubClient client(mqttClient);
int publishErrorCount = 0;
unsigned long lastMQTTMessage = 0;
unsigned long previousDeviceStatePublish = 0;
unsigned long previousDeviceStateStatusPublish = 0;
unsigned long previousMqttReconnect = 0;
unsigned long lastOtherMillis = 0;

// Callback function
void callback(char *topic, byte *payload, unsigned int length)
{
  payload[length] = '\0';
  String topic_path = String(topic);
  topic_path.toLowerCase(); // in case we recieve DC_OUTPUT_ON instead of the expected dc_output_on

  Serial.print("MQTT Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(" Payload: ");
  String strPayload = String((char *)payload);
  Serial.println(strPayload);

  bool customSwitches = false;

  if (topic_path.indexOf("bat_pwm_switch") > -1)
  {
    customSwitches = true;
#if USE_EXT_BAT == 1
    if (strPayload == "1")
    {
      setSwitch(true);
    }
    else
    {
      setSwitch(false);
    }
#endif
  }
  if (topic_path.indexOf("220_relay") > -1)
  {
    customSwitches = true;
#ifdef RELAY_220_PIN
    if (strPayload == "1")
    {
      set220Relay(true);
    }
    else
    {
      set220Relay(false);
    }
#endif
  }
  if (customSwitches == false)
  {
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
    Serial.print(" Payload - switched: ");
    Serial.println(strPayload);

    command.len = swap_bytes(strPayload.toInt());
    command.check_sum = modbus_crc((uint8_t *)&command, 6);
    lastMQTTMessage = millis();

    sendBTCommand(command);
  }
}

void subscribeTopic(enum field_names field_name)
{
#ifdef DEBUG
  Serial.println("[MQTT] subscribe to topic: " + map_field_name(field_name));
#endif
  char subscribeTopicBuf[512];

  sprintf(subscribeTopicBuf, "bluetti/%s/command/%s", wifiConfig.bluetti_device_id.c_str(), map_field_name(field_name).c_str());
  client.subscribe(subscribeTopicBuf);

  lastMQTTMessage = millis();
}

void publishTopic(enum field_names field_name, String value)
{
  char publishTopicBuf[1024];

#ifdef DEBUG
  Serial.println("[MQTT] publish topic for field: " + map_field_name(field_name));
#endif

  // sometimes we get empty values / wrong vales - all the time device_type is empty
  if (map_field_name(field_name) == "device_type" && value.length() < 3)
  {

    // Serial.println(F("[MQTT] Error while publishTopic! 'device_type' can't be empty, reboot device)"));
    ESP.restart();
    Serial.println(F("[MQTT] Error while publishTopic! 'device_type' can't be empty, restarting BlueTooth Stack)"));
    // btResetStack();
  }

  sprintf(publishTopicBuf, "bluetti/%s/state/%s", wifiConfig.bluetti_device_id.c_str(), map_field_name(field_name).c_str());
  if (wifiConfig.mqtt_server.length() == 0)
  {
    writeLog(String(millis()) + ": " + map_field_name(field_name) + " -> " + value);
#ifdef DEBUG
    Serial.println("[MQTT] No MQTT server specified!");
#endif
  }
  else
  {
    lastMQTTMessage = millis();
    if (!client.publish(publishTopicBuf, value.c_str()))
    {
      publishErrorCount++;
#ifdef DEBUG
      Serial.println("[MQTT] Publish error: " + String(lastMQTTMessage) + ": publish ERROR! " + map_field_name(field_name) + " -> " + value);
#endif
      writeLog(String(lastMQTTMessage) + ": publish ERROR! " + map_field_name(field_name) + " -> " + value);
    }
    else
    {
#ifdef DEBUG
      Serial.println("[MQTT] Last Message: " + String(lastMQTTMessage) + ": " + map_field_name(field_name) + " -> " + value);
#endif
      writeLog(String(lastMQTTMessage) + ": " + map_field_name(field_name) + " -> " + value);
    }
  }
}

void publishDeviceState()
{
  char publishTopicBuf[1024];

  sprintf(publishTopicBuf, "bluetti/%s/state/%s", wifiConfig.bluetti_device_id.c_str(), "device");
  String currTime;
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

    currTime = String(buffer);
  }
  String value = "{\"IP\":\"" + WiFi.localIP().toString() + "\", \"MAC\":\"" + WiFi.macAddress() + "\", \"Uptime\":" + millis() + ", \"LastUpdate\":" + currTime + "}";
#ifdef DEBUG
  Serial.println("publishTopicBuf: " + String(publishTopicBuf));
  Serial.println("[MQTT] PublishingDeviceState: " + value);
#endif
  if (!client.publish(publishTopicBuf, value.c_str()))
  {
    publishErrorCount++;
  }
  lastMQTTMessage = millis();
  previousDeviceStatePublish = millis();
}

void publishDeviceStateStatus()
{
  char publishTopicBuf[1024];

  sprintf(publishTopicBuf, "bluetti/%s/state/%s", wifiConfig.bluetti_device_id.c_str(), "device_status");
  String value = "{\"MQTTconnected\":" + String(isMQTTconnected()) + ", \"BTconnected\":" + String(isBTconnected()) + "}";
#ifdef DEBUG
  Serial.println("[MQTT] PublishingDeviceStateStatus: " + value);
#endif
  if (!client.publish(publishTopicBuf, value.c_str()))
  {
    publishErrorCount++;
  }
  lastMQTTMessage = millis();
  previousDeviceStateStatusPublish = millis();
}

void initMQTT()
{

  enum field_names f_name;

  Serial.println("[MQTT] init MQTT");
  if (wifiConfig.mqtt_server.length() == 0)
  {
    Serial.println("[MQTT] No MQTT server configured");
    return;
  }
  Serial.print("[MQTT] Connecting to MQTT at: ");
  Serial.print(wifiConfig.mqtt_server);
  Serial.print(":");
  Serial.println(wifiConfig.mqtt_port);

  client.setServer(wifiConfig.mqtt_server.c_str(), atoi(wifiConfig.mqtt_port.c_str()));
  client.setCallback(callback);

  bool connect_result;
  const char connect_id[] = "Bluetti_ESP32";
  if (wifiConfig.mqtt_username)
  {
    connect_result = client.connect(connect_id, wifiConfig.mqtt_username.c_str(), wifiConfig.mqtt_password.c_str());
  }
  else
  {
    connect_result = client.connect(connect_id);
  }

  if (connect_result)
  {

    Serial.println(F("[MQTT] Connected to MQTT Server... "));

    // subscribe to topics for commands
    for (int i = 0; i < sizeof(bluetti_device_command) / sizeof(device_field_data_t); i++)
    {
      subscribeTopic(bluetti_device_command[i].f_name);
    }

    char subscribeTopicBuf[512];

#if USE_EXT_BAT == 1
    sprintf(subscribeTopicBuf, "bluetti/%s/command/%s", wifiConfig.bluetti_device_id.c_str(), "BAT_PWM_SWITCH");
    client.subscribe(subscribeTopicBuf);
#endif
#ifdef RELAY_220_PIN
    sprintf(subscribeTopicBuf, "bluetti/%s/command/%s", wifiConfig.bluetti_device_id.c_str(), "220_RELAY");
    client.subscribe(subscribeTopicBuf);
#endif

    publishDeviceState();
    publishDeviceStateStatus();
  }
};

void handleMQTT()
{
  if (wifiConfig.mqtt_server.length() == 0)
  {
    return;
  }
  if ((millis() - lastMQTTMessage) > (MAX_DISCONNECTED_TIME_UNTIL_REBOOT * 60000))
  {
    Serial.println(F("MQTT is disconnected over allowed limit, reboot device"));
    // TODO: manage like BT with retries and no restart
    // ESP.restart();
  }

  if ((millis() - previousDeviceStatePublish) > (DEVICE_STATE_UPDATE * 60000))
  {
    publishDeviceState();
    publishDeviceStateStatus();
  }
  char publishTopicBuf[1024];
  String value;
  if ((millis() - lastOtherMillis) > (DEVICE_STATE_UPDATE * 1000))
  {
#if USE_TEMPERATURE_SENSOR == 1
    sprintf(publishTopicBuf, "bluetti/%s/state/%s", wifiConfig.bluetti_device_id.c_str(), "temperature");
    value = String(temperature);
    if (!client.publish(publishTopicBuf, value.c_str()))
    {
      publishErrorCount++;
    }
    sprintf(publishTopicBuf, "bluetti/%s/state/%s", wifiConfig.bluetti_device_id.c_str(), "humidity");
    value = String(humidity);
    if (!client.publish(publishTopicBuf, value.c_str()))
    {
      publishErrorCount++;
    }
#endif
#if USE_EXT_BAT == 1
    sprintf(publishTopicBuf, "bluetti/%s/state/%s", wifiConfig.bluetti_device_id.c_str(), "EXT_BAT_Voltage");
    value = String(curr_EXT_Voltage);
    if (!client.publish(publishTopicBuf, value.c_str()))
    {
      publishErrorCount++;
    }
    sprintf(publishTopicBuf, "bluetti/%s/state/%s", wifiConfig.bluetti_device_id.c_str(), "BAT_PWM_SWITCH");
    value = "0";
    if (_pwm_switch_status)
    {
      value = "1";
    }
    if (!client.publish(publishTopicBuf, value.c_str()))
    {
      publishErrorCount++;
    }
#endif
#ifdef RELAY_220_PIN
    sprintf(publishTopicBuf, "bluetti/%s/state/%s", wifiConfig.bluetti_device_id.c_str(), "220_RELAY");
    value = "0";
    if (_220_relay_status)
    {
      value = "1";
    }
    if (!client.publish(publishTopicBuf, value.c_str()))
    {
      publishErrorCount++;
    }
#endif
    lastOtherMillis = millis();
  }

  if (!isMQTTconnected() && publishErrorCount > 5)
  {
    if ((millis() - previousMqttReconnect) > 5000)
    {
      previousMqttReconnect = millis();
      Serial.println(F("[MQTT] lost connection, try to reconnect"));
      client.disconnect();
      lastMQTTMessage = 0;
      previousDeviceStatePublish = 0;
      previousDeviceStateStatusPublish = 0;
      publishErrorCount = 0;
      writeLog(String(millis()) + ": MQTT connection lost, try reconnect");
      initMQTT();
    }
  }

  client.loop();
}

bool isMQTTconnected()
{
  if (client.connected())
  {
    return true;
  }
  else
  {
    return false;
  }
}

int getPublishErrorCount()
{
  return publishErrorCount;
}
unsigned long getLastMQTTMessageTime()
{
  return lastMQTTMessage;
}
unsigned long getLastMQTTDeviceStateMessageTime()
{
  return previousDeviceStatePublish;
}
unsigned long getLastMQTTDeviceStateStatusMessageTime()
{
  return previousDeviceStateStatusPublish;
}
