#ifndef ServerCallbacks_h
#define ServerCallbacks_h

#include <BLEDevice.h>
#include <BLEServer.h>

class ServerCallbacks : public BLEServerCallbacks {
private:
  int n_devicesConnected;
  int n_devicesPreviouslyConnected;

public:
  ServerCallbacks();
  ~ServerCallbacks();
  int getNumConnected();
  bool deviceDidConnect();
  bool deviceDidDisconnect();
  void onConnect(BLEServer *pServer);
  void onDisconnect(BLEServer *pServer);
};

class CharacteristicCallbacks : public BLECharacteristicCallbacks {
private:
  bool *m_rfidWriteMode;
  void statusCharHandler(BLECharacteristic *statusCharacteristic, uint8_t data);
  void secretCharHandler();
  void timeCharHandler(uint8_t *data, size_t size);

public:
  CharacteristicCallbacks();
  ~CharacteristicCallbacks();
  void onWrite(BLECharacteristic *pCharacteristic,
               esp_ble_gatts_cb_param_t *param);
  void onStatus(BLECharacteristic *pCharacteristic, Status s, uint32_t code);
  void setRfidWriteModeValue(bool *t_rfidWriteMode);
};

#endif