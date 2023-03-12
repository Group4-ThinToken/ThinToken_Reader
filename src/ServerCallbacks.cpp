#include "ServerCallbacks.h"
#include "creds.h"

#include <WebSerialLite.h>
#include <time.h>

// == Server Callbacks ==

ServerCallbacks::ServerCallbacks() { n_devicesConnected = 0; }

ServerCallbacks::~ServerCallbacks() {}

int ServerCallbacks::getNumConnected() { return n_devicesConnected; }

bool ServerCallbacks::deviceDidConnect() {
  return n_devicesConnected > n_devicesPreviouslyConnected;
}

bool ServerCallbacks::deviceDidDisconnect() {
  return n_devicesConnected < n_devicesPreviouslyConnected;
}

void ServerCallbacks::onConnect(BLEServer *pServer) {
  n_devicesConnected += 1;
  WebSerial.println("Bluetooth device connected");
}

void ServerCallbacks::onDisconnect(BLEServer *pServer) {
  n_devicesConnected -= 1;
  WebSerial.println("Bluetooth device disconnected");
  pServer->startAdvertising();
}

// == Characteristic Callbacks

CharacteristicCallbacks::CharacteristicCallbacks() {}

CharacteristicCallbacks::~CharacteristicCallbacks() {}

void CharacteristicCallbacks::onWrite(BLECharacteristic *pCharacteristic,
                                      esp_ble_gatts_cb_param_t *param) {
  auto uuid = pCharacteristic->getUUID();
  auto data = pCharacteristic->getData();
  auto size = pCharacteristic->getLength();
  String display = "";

  WebSerial.println("Characteristic has been written to.");
  WebSerial.print("Char UUID: ");
  WebSerial.println(uuid.toString().c_str());
  WebSerial.print("Raw Value: ");

  for (unsigned int i = 0; i < size; ++i) {
    display += data[i];
    display += ' ';
  }

  WebSerial.println(display);
  WebSerial.print("Size: ");
  WebSerial.println(pCharacteristic->getLength());

  if (uuid.toString() == STATUS_CHARACTERISTIC) {
    statusCharHandler();
  } else if (uuid.toString() == SECRET_CHARACTERISTIC) {

  } else if (uuid.toString() == TIME_CHARACTERISTIC) {
    timeCharHandler(pCharacteristic->getData(), pCharacteristic->getLength());
  }
}

void CharacteristicCallbacks::onStatus(BLECharacteristic *pCharacteristic,
                                       Status s, uint32_t code) {
  auto uuid = pCharacteristic->getUUID();
  if (s == BLECharacteristicCallbacks::SUCCESS_INDICATE &&
      uuid.toString() == OTP_CHARACTERISTIC &&
      !pCharacteristic->getValue().empty()) {
    WebSerial.println("OTP Sent");

    pCharacteristic->setValue("");
    pCharacteristic->indicate();
  }

  if (s == BLECharacteristicCallbacks::ERROR_GATT ||
      s == BLECharacteristicCallbacks::ERROR_INDICATE_TIMEOUT ||
      s == BLECharacteristicCallbacks::ERROR_INDICATE_FAILURE) {
    WebSerial.print("BLE status error: ");
    WebSerial.println(s);
    WebSerial.print("Additional info: ");
    WebSerial.println(code);
  }
}

void CharacteristicCallbacks::statusCharHandler() {}

void CharacteristicCallbacks::secretCharHandler() {}

void CharacteristicCallbacks::timeCharHandler(uint8_t *data, size_t size) {
  if (size != 4) {
    WebSerial.println("Data size invalid to be UNIX timestamp.");
    return;
  }

  uint32_t data32 =
      data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);

  WebSerial.print("Setting time to: ");
  WebSerial.println(data32);

  timeval currTime;
  int res;
  currTime.tv_sec = data32;
  currTime.tv_usec = 0;

  res = settimeofday(&currTime, NULL);

  if (res != 0) {
    WebSerial.print("Error setting time. ");
    WebSerial.println(res);
    return;
  }

  WebSerial.println("Time set successfully");
}