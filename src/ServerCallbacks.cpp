#include "ServerCallbacks.h"
#include "creds.h"
#include "statuses.h"
#include "RFID.h"

#include <WebSerialLite.h>
#include <time.h>

// == Server Callbacks ==

ServerCallbacks::ServerCallbacks() { n_devicesConnected = 0; }

ServerCallbacks::~ServerCallbacks() {}

void ServerCallbacks::setRfidWriteModeValue(bool *t_rfidWriteMode) {
  m_rfidWriteMode = t_rfidWriteMode;
}

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

  // Reset RFID mode to read when BT disconnects
  *m_rfidWriteMode = false;
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
    if (size == sizeof(uint8_t)) {
      statusCharHandler(pCharacteristic, data[0]);
    }
  } else if (uuid.toString() == SECRET_CHARACTERISTIC) {
    // Stuff that happens when secret is written to
    if (size != 0) {
      secretCharHandler(data, size);
    }
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
    WebSerial.print("\n====");
    WebSerial.print("BLE status error: ");
    WebSerial.println(s);
    WebSerial.print("Characteristic: ");
    WebSerial.println(uuid.toString().c_str());
    WebSerial.print("Additional info: ");
    WebSerial.println(code);
  }
}

// Handler for the status characteristic
void CharacteristicCallbacks::statusCharHandler(BLECharacteristic *statusCharacteristic, uint8_t data) {
  try
  {
    if (data == ST_WriteFlowRequested) {
      *m_rfidWriteMode = true;
      statusCharacteristic->setValue(&ST_WriteFlowReady, sizeof(uint8_t));
      statusCharacteristic->notify();
    } else if (data == ST_WriteFlowEndRequest) {
      *m_rfidWriteMode = false;
      statusCharacteristic->setValue(&ST_Ready, sizeof(uint8_t));
      statusCharacteristic->notify();
    }
  }
  catch(const std::exception& e)
  {
    WebSerial.println(e.what());
    statusCharacteristic->setValue(&ST_WriteFlowRequestFailed, sizeof(uint8_t));
  }
}

// Handler for the secret characteristic
void CharacteristicCallbacks::secretCharHandler(uint8_t *data, size_t size) {
  Serial.printf("Secret characteristic handler:\nData size: %d\n", size);
  WebSerial.print("Secret characteristic handler:\nData size: ");
  WebSerial.println(size);
  std::vector<byte> dataVector(data, data+size);
  try {
    Serial.println("Inside try");
    WebSerial.println("Inside try");
    auto mfrc = m_rfidReader->getReader();

    int bytesWritten = m_rfidReader->appendAccount(dataVector);

    // String d = "";
    // for (unsigned int i = 0; i < res.size(); ++i) {
    //   d += res.data()[i];
    //   d += ' ';
    // }
    // WebSerial.println(d);
  } catch(const std::exception& e) {
    Serial.print("RFID Exception: ");
    Serial.println(e.what());
    Serial.printf("Free mem: %d", ESP.getFreeHeap());
  }
}

// Handler for the time characteristic
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

void CharacteristicCallbacks::setRfidWriteModeValue(bool *t_rfidWriteMode) {
  m_rfidWriteMode = t_rfidWriteMode;
}

void CharacteristicCallbacks::setRfidReader(RFID *t_rfidReader) {
  m_rfidReader = t_rfidReader;
}
