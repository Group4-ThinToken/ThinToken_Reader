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

CharacteristicCallbacks::CharacteristicCallbacks(BLECharacteristic* t_statusCharacteristic) {
  m_statusCharacteristic = t_statusCharacteristic;
}

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
  } else if (uuid.toString() == SECTOR_CHARACTERISTIC) {
    // Stuff that happends when sector is written to
    try {
      sectorCharHandler(pCharacteristic->getData(), pCharacteristic->getLength());
    } catch(std::logic_error& e) {
      WebSerial.println(e.what());
      sendStatus(&ST_MutexLocked);
    }
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
    } else if (data == ST_ReadAllRequested) {
      *m_rfidWriteMode = false;
      m_rfidReader->queueRead(10);
      m_rfidReader->queueRead(7);
      m_rfidReader->queueRead(4);
      m_rfidReader->queueRead(1);
      m_rfidReader->immediateOpRequested = true;
    } else if (data == ST_OtpRequested) {
      *m_otpMode = true;
      WebSerial.println("OTP Requested");
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

    WebSerial.print("OTP Mode: ");
    WebSerial.println(*m_otpMode);
    Serial.print("OTP Mode: ");
    Serial.println(*m_otpMode);
    if (*m_otpMode == true) {
      m_crypto->setCryptoKeyAndIv(dataVector);
    } else {
      int bytesWritten = m_rfidReader->appendAccount(dataVector);
    }

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

void CharacteristicCallbacks::sectorCharHandler(uint8_t *data, size_t size) {
  Serial.printf("Sector characteristic handler:\nData size: %d\n", size);
  WebSerial.print("Sector characteristic handler:\nData size: ");
  WebSerial.println(size);

  // TODO: The 2nd byte is no longer used, the otpMode argument
  // is now handled by the statusCharHandler for otpRequested
  if (size != 2) {
    std::string e = "Sector size must be exactly 2. Got " + std::to_string(size);
    throw std::logic_error(e);
  }

  // if (m_rfidReader->mutexLock == true) {
  //   std::string e = "RFID is in use. Retry later";
  //   throw std::logic_error(e);
  // }
  std::vector<byte> dataVector(data, data+size);
  try {
    Serial.println("Inside try sector char handler");
    WebSerial.println("Inside try sector char handler");
    auto mfrc = m_rfidReader->getReader();

    byte sector = dataVector[0];
    m_rfidReader->queueRead(sector);
    m_rfidReader->immediateOpRequested = true;
  } catch(const std::exception& e) {
    Serial.print("RFID Exception: ");
    Serial.println(e.what());
    Serial.printf("Free mem: %d", ESP.getFreeHeap());
  }
}

void CharacteristicCallbacks::sendStatus(uint8_t* statusCode) {
  m_statusCharacteristic->setValue(statusCode, sizeof(uint8_t));
  m_statusCharacteristic->notify();
}

void CharacteristicCallbacks::setRfidWriteModeValue(bool *t_rfidWriteMode) {
  m_rfidWriteMode = t_rfidWriteMode;
}

void CharacteristicCallbacks::setRfidReader(RFID *t_rfidReader) {
  m_rfidReader = t_rfidReader;
}

void CharacteristicCallbacks::setOtpMode(bool *t_otpMode) {
  m_otpMode = t_otpMode;
}

void CharacteristicCallbacks::setCryptoModule(Crypto *t_cryptoModule) {
  m_crypto = t_cryptoModule;
}
