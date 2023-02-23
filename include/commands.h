#ifndef commands_h
#define commands_h

// Contains functions used in web serial commands
// std::map<const char*, void*> cmdMap;

class WebSerialCmdHandler {
public:
  WebSerialCmdHandler();
  void runCommand(String name);
  void setRfidReader(MFRC522* t_reader);
  void setBtServerCallbacks(ServerCallbacks* t_btServerCallbacks);
private:
  MFRC522* m_rfidReader;
  ServerCallbacks* m_btServerCallbacks;

  void dumpToSerial(byte *buffer, byte size);
  void reboot();
  void wifiInfo();
  void rfidRead();
  void bluetoothInfo();
};

#endif