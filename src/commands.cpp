#include "commands.h"
#include "pins.h"
#include "creds.h"
#include "ServerCallbacks.h"

#include <WebSerialLite.h>
#include <MFRC522.h>

WebSerialCmdHandler::WebSerialCmdHandler() { }

void WebSerialCmdHandler::setRfidReader(MFRC522* t_reader) {
  m_rfidReader = t_reader;
}

void WebSerialCmdHandler::setBtServerCallbacks(ServerCallbacks* t_serverCallbacks) {
  m_btServerCallbacks = t_serverCallbacks;
}

void WebSerialCmdHandler::runCommand(String name) {
  WebSerial.println(name);

  if (name.equals("reboot")) {
    reboot();
  } else if (name.equals("wifi info")) {
    wifiInfo();
  } else if (name.equals("rfid read")) {
    rfidRead();
  } else {
    WebSerial.println("Invalid command");
  }
}

void WebSerialCmdHandler::dumpToSerial(byte *buffer, byte size) {
  for (byte i = 0; i < size; ++i) {
    WebSerial.print(buffer[i]);
    WebSerial.print(" ");
  }
  WebSerial.println();
}

void WebSerialCmdHandler::reboot() {
  unsigned long t = millis();
  WebSerial.print("Device restarting ...");
  WebSerial.print("Refresh this page if it does not automatically reconnect");

  digitalWrite(LED_BUILTIN, HIGH);
  delay(3000);
  esp_restart();
}

void WebSerialCmdHandler::wifiInfo() {
  WebSerial.println("[ WiFi STA Info ]");
  WebSerial.print("Connected to: ");
  WebSerial.println(WiFi.SSID());
  WebSerial.print("IP: ");
  WebSerial.println(WiFi.localIP().toString());
  WebSerial.println("[ WiFi AP Info ]");
  WebSerial.print("SSID: ");
  WebSerial.println(WiFi.softAPSSID());
  WebSerial.print("IP: ");
  WebSerial.println(WiFi.softAPIP().toString());
}

void WebSerialCmdHandler::rfidRead() {
  if (m_rfidReader->PICC_IsNewCardPresent()) {
    if (m_rfidReader->PICC_ReadCardSerial()) {
      WebSerial.print("Card UID: ");
      // String data = readRfidBytes(m_rfidReader.uid.uidByte, m_rfidReader.uid.size);
      // WebSerial.print(data);
      dumpToSerial(m_rfidReader->uid.uidByte, m_rfidReader->uid.size);
      WebSerial.println();
      MFRC522::PICC_Type type = m_rfidReader->PICC_GetType(m_rfidReader->uid.sak);
      WebSerial.print("Card Type: ");
      WebSerial.println(m_rfidReader->PICC_GetTypeName(type));
    }
  } else {
    WebSerial.println("No tag detected");
  }
}

void WebSerialCmdHandler::bluetoothInfo() {
  WebSerial.println("[ Bluetooth Info ]");
  WebSerial.print("Device Name: ");
  WebSerial.println(DEVICE_NAME);
  WebSerial.print("Connected Devices: ");
  WebSerial.println(m_btServerCallbacks->getNumConnected());
}