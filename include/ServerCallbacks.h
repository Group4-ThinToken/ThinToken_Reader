#ifndef ServerCallbacks_h
#define ServerCallbacks_h

#include <BLEDevice.h>
#include <BLEServer.h>

class ServerCallbacks: public BLEServerCallbacks {
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

#endif