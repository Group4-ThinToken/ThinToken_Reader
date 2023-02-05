#ifndef commands_h
#define commands_h

// Contains functions used in web serial commands
// std::map<const char*, void*> cmdMap;

class WebSerialCmdHandler {
public:
  WebSerialCmdHandler();
  void runCommand(String name);
  void setRfidReader(MFRC522* t_reader);
private:
  void dumpToSerial(byte *buffer, byte size);
  void reboot();
  void wifiInfo();
  void rfidRead();
};

#endif