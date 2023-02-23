#include "ServerCallbacks.h"

#include <WebSerialLite.h>

ServerCallbacks::ServerCallbacks() {
    n_devicesConnected = 0;
}

ServerCallbacks::~ServerCallbacks() {

}

int ServerCallbacks::getNumConnected() {
    return n_devicesConnected;
}

bool ServerCallbacks::deviceDidConnect() {
    return n_devicesConnected > n_devicesPreviouslyConnected;
}

bool ServerCallbacks::deviceDidDisconnect() {
    return n_devicesConnected < n_devicesPreviouslyConnected;
}

void ServerCallbacks::onConnect(BLEServer *pServer) {
    n_devicesConnected += 1;
    BLEDevice::stopAdvertising();
    WebSerial.println("Bluetooth device connected");
}

void ServerCallbacks::onDisconnect(BLEServer *pServer) {
    n_devicesConnected -= 1;
    WebSerial.println("Bluetooth device disconnected");
}