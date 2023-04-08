#ifndef ServerCallbacks_h
#define ServerCallbacks_h

#include "RFID.h"

#include <BLEDevice.h>
#include <BLEServer.h>

class ServerCallbacks : public BLEServerCallbacks {
private:
  int n_devicesConnected;
  int n_devicesPreviouslyConnected;
  bool *m_rfidWriteMode;

public:
  ServerCallbacks();
  ~ServerCallbacks();
  void setRfidWriteModeValue(bool *t_rfidWriteMode);
  int getNumConnected();
  bool deviceDidConnect();
  bool deviceDidDisconnect();
  void onConnect(BLEServer *pServer);
  void onDisconnect(BLEServer *pServer);
};

class CharacteristicCallbacks : public BLECharacteristicCallbacks {
private:
  RFID* m_rfidReader;
  bool* m_rfidWriteMode;
  BLECharacteristic* m_statusCharacteristic;
  void statusCharHandler(BLECharacteristic *statusCharacteristic, uint8_t data);
  void secretCharHandler(uint8_t *data, size_t size);
  void timeCharHandler(uint8_t *data, size_t size);
  void sectorCharHandler(uint8_t *data, size_t size);

  // Helpers
  void sendStatus(uint8_t* statusCode);

public:
  CharacteristicCallbacks(BLECharacteristic* t_statusCharacteristic);
  ~CharacteristicCallbacks();
  void onWrite(BLECharacteristic *pCharacteristic,
               esp_ble_gatts_cb_param_t *param);
  void onStatus(BLECharacteristic *pCharacteristic, Status s, uint32_t code);
  void setRfidWriteModeValue(bool *t_rfidWriteMode);
  void setRfidReader(RFID *t_rfidReader);
};

#endif