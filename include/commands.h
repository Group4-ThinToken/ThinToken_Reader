#ifndef commands_h
#define commands_h

#include <string>
#include <MFRC522.h>
#include "ServerCallbacks.h"
#include "CredentialHandler.h"

// Contains functions used in web serial commands
// std::map<const char*, void*> cmdMap;

class WebSerialCmdHandler {
public:
  WebSerialCmdHandler();
  void runCommand(String name, String arg);
  void setRfidReader(MFRC522* t_reader, RFID* t_rfid);
  void setBtServerCallbacks(ServerCallbacks* t_btServerCallbacks);
  void setOtpCharacteristic(BLECharacteristic *t_otpCharacteristic);
  void setStatusCharacteristic(BLECharacteristic *t_statusCharacteristic);
  void setRfidWriteMode(bool* t_rfidWriteMode);
  void setOtpMode(bool* t_otpMode);
  void setPrintWakeupStatus(bool* t_printWakeupStatus);
  void setCredentialHandler(CredentialHandler* t_creds);
  void setEnablePeriodicSleep(bool* t_enablePeriodicSleep);
  void dumpToSerial(byte *buffer, byte size);
  void dumpToSerial(byte *buffer, size_t size);
private:
  MFRC522* m_rfidReader;
  RFID* m_rfid;
  ServerCallbacks* m_btServerCallbacks;
  BLECharacteristic* m_otpCharacteristic;
  BLECharacteristic* m_statusCharacteristic;
  bool* m_rfidWriteMode;
  bool* m_otpMode;
  bool* m_printWakeupStatus;
  CredentialHandler* m_creds;
  bool* m_enablePeriodicSleep;

  void reboot();
  void wifiInfo();
  void rfidRead();
  void rfidSector(String arg);
  void availSectors();
  void rfidInfo();
  void rfidSetGain(std::string val);
  void bluetoothInfo();
  void otpTest(std::string testKey);
  void sendStatus(uint8_t val);
  void togglePeriodicSleep();
  void deepSleepTest();
};

#endif