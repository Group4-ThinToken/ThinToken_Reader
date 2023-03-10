#ifndef commands_h
#define commands_h

#include <string>
#include <MFRC522.h>
#include "ServerCallbacks.h"

// Contains functions used in web serial commands
// std::map<const char*, void*> cmdMap;

class WebSerialCmdHandler {
public:
  WebSerialCmdHandler();
  void runCommand(String name, String arg);
  void setRfidReader(MFRC522* t_reader);
  void setBtServerCallbacks(ServerCallbacks* t_btServerCallbacks);
  void dumpToSerial(byte *buffer, byte size);
private:
  MFRC522* m_rfidReader;
  ServerCallbacks* m_btServerCallbacks;

  void reboot();
  void wifiInfo();
  void rfidRead();
  void bluetoothInfo();
  void otpTest(std::string testKey);
};

#endif