#include "BluettiConfig.h"
#include "BTooth.h"
#include "utils.h"
#include "PayloadParser.h"
#include "BWifi.h"


int pollTick = 0;

struct command_handle {
  uint8_t page;
  uint8_t offset;
  int length;
};

QueueHandle_t commandHandleQueue;
QueueHandle_t sendQueue;

BLEScan* pBLEScan;

unsigned long lastBTMessage = 0;

ESPBluettiSettings settings = get_esp32_bluetti_settings();

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("onDisconnect");
    #if RELAISMODE == 1
      #if DEBUG <= 5
        Serial.println("deactivate relais contact");
      #endif
      digitalWrite(RELAIS_PIN, RELAIS_LOW);
    #endif
  }
};

/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class BluettiAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print(F("BLE Advertised Device found: "));
    Serial.println(advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID) && advertisedDevice.getName().compare(settings.bluetti_device_id)) {
      Serial.println(F("device found stop scan."));
      BLEDevice::getScan()->stop();
      pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
      Serial.println(F("set variables for next loop to connect."));
      bluettiDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;
    }else{
      Serial.println(F("device id not matched, no connect "));
    }
  } 
};

void initBluetooth(){
  settings = get_esp32_bluetti_settings();
  #if DEBUG <=5
  Serial.print("strlen(settings.bluetti_device_id) = ");
  Serial.println(strlen(settings.bluetti_device_id));
  #endif

  if (strlen(settings.bluetti_device_id)!=0){

    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new BluettiAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, false);

    commandHandleQueue = xQueueCreate( 5, sizeof(bt_command_t ) );
    sendQueue = xQueueCreate( 5, sizeof(bt_command_t) );
  }
}


static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {

#if DEBUG <= 4
    Serial.println("F01 - Write Response");
    /* pData Debug... */
    for (int i=1; i<=length; i++){
       
      Serial.printf("%02x", pData[i-1]);
      
      if(i % 2 == 0){
        Serial.print(" "); 
      } 

      if(i % 16 == 0){
        Serial.println();  
      }
    }
    Serial.println();
#endif

    bt_command_t command_handle;
    if(xQueueReceive(commandHandleQueue, &command_handle, 500)){
      parse_bluetooth_data(command_handle.page, command_handle.offset, pData, length);
      #if DEBUG <= 4
      Serial.print("BT notifyCallback() running on core ");
      Serial.println(xPortGetCoreID());
      #endif
    }
   
}

BLEClient* pClient;

bool connectToServer() {
  pinMode(2, OUTPUT);
  for (int16_t i = 0;i++;i<10){
    digitalWrite(2, HIGH);
    delay(100);
  }
    Serial.print(F("Forming a connection to "));
    Serial.println(bluettiDevice->getAddress().toString().c_str());
    
    pClient = BLEDevice::createClient();
    Serial.println(F(" - Created client"));

    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(bluettiDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    Serial.println(F(" - Connected to server"));
    pClient->setMTU(517); //set client to request maximum MTU from server (default is 23 otherwise)
  
    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print(F("Failed to find our service UUID: "));
      Serial.println(serviceUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(F(" - Found our service"));


    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteWriteCharacteristic = pRemoteService->getCharacteristic(WRITE_UUID);
    if (pRemoteWriteCharacteristic == nullptr) {
      Serial.print(F("Failed to find our characteristic UUID: "));
      Serial.println(WRITE_UUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(F(" - Found our Write characteristic"));

        // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteNotifyCharacteristic = pRemoteService->getCharacteristic(NOTIFY_UUID);
    if (pRemoteNotifyCharacteristic == nullptr) {
      Serial.print(F("Failed to find our characteristic UUID: "));
      Serial.println(NOTIFY_UUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(F(" - Found our Notifyite characteristic"));

    // Read the value of the characteristic.
    if(pRemoteWriteCharacteristic->canRead()) {
      std::string value = pRemoteWriteCharacteristic->readValue();
      Serial.print(F("The characteristic value was: "));
      Serial.println(value.c_str());
    }

    if(pRemoteNotifyCharacteristic->canNotify())
      pRemoteNotifyCharacteristic->registerForNotify(notifyCallback);

    connected = true;
     #if RELAISMODE == 1
      #if DEBUG <= 5
        Serial.println("activate relais contact");
      #endif
      digitalWrite(RELAIS_PIN, RELAIS_HIGH);
    #endif

    return true;
}

void disconnectBT(){
  //Mainly for testing
  pClient->disconnect(); 
  connected = false; //to be sure
}

void handleBTCommandQueue(){

    bt_command_t command;
    if(xQueueReceive(sendQueue, &command, 0)) {
      
      #if DEBUG <= 5
        Serial.print("Write Request FF02 - Value: ");
        
        for(int i=0; i<8; i++){
          if ( i % 2 == 0){ Serial.print(" "); };
          Serial.printf("%02x", ((uint8_t*)&command)[i]);
        }
        
        Serial.println("");
      #endif
      pRemoteWriteCharacteristic->writeValue((uint8_t*)&command, sizeof(command),true);
 
     };  
}

void sendBTCommand(bt_command_t command){
    bt_command_t cmd = command;
    xQueueSend(sendQueue, &cmd, 500);
}

void handleBluetooth(){
  
  //Check BT only if an Id is available
  if (strlen(settings.bluetti_device_id)!=0){
    if (doConnect == true) {
      Serial.println(F("Trying to connect Bluetti BLE Server."));
      if (connectToServer()) {
        Serial.println(F("We are now connected to the Bluetti BLE Server."));
      } else {
        Serial.println(F("We have failed to connect to the server; there is nothin more we will do."));
      }
      doConnect = false;
    }

    if ((millis() - lastBTMessage) > (MAX_DISCONNECTED_TIME_UNTIL_REBOOT * 60000)){ 
      Serial.println(F("BT is disconnected over allowed limit, reboot device"));
      ESP.restart();
    }

    if (connected) {

      // poll for device state
      if ( millis() - lastBTMessage > BLUETOOTH_QUERY_MESSAGE_DELAY){

        bt_command_t command;
        command.prefix = 0x01;
        command.field_update_cmd = 0x03;
        command.page = bluetti_polling_command[pollTick].f_page;
        command.offset = bluetti_polling_command[pollTick].f_offset;
        command.len = (uint16_t) bluetti_polling_command[pollTick].f_size << 8;
        command.check_sum = modbus_crc((uint8_t*)&command,6);

        if (xQueueSend(commandHandleQueue, &command, 500) &&
            xQueueSend(sendQueue, &command, 500))
        {
          //mark BT message as handled only if we could add them to the queues
          lastBTMessage = millis();
        }

        if (pollTick == sizeof(bluetti_polling_command)/sizeof(device_field_data_t)-1 ){
            pollTick = 0;
        } else {
            pollTick++;
        }
        
      }

      handleBTCommandQueue();
      
    }else if(doScan){
      BLEDevice::getScan()->start(0);  
    }
  }
}

bool isBTconnected(){
  return connected;
}

unsigned long getLastBTMessageTime(){
    return lastBTMessage;
}
