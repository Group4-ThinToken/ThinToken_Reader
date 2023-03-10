#include "commands.h"
#include "pins.h"
#include "creds.h"
#include "Authenticator.h"

#include <WebSerialLite.h>
#include <vector>
#include <string>

WebSerialCmdHandler::WebSerialCmdHandler() { }

void WebSerialCmdHandler::setRfidReader(MFRC522* t_reader) {
  m_rfidReader = t_reader;
}

void WebSerialCmdHandler::setBtServerCallbacks(ServerCallbacks* t_serverCallbacks) {
  m_btServerCallbacks = t_serverCallbacks;
}

void WebSerialCmdHandler::runCommand(String name, String arg) {
  WebSerial.print(name);
  WebSerial.print(" ");
  WebSerial.println(arg);

  if (name.equals("reboot")) {
    reboot();
  } else if (name.equals("wifi info")) {
    wifiInfo();
  } else if (name.equals("rfid read")) {
    rfidRead();
  } else if (name.equals("bt info")) {
    bluetoothInfo();
  } else if (name.equals("otp test")) {
    const char* testKey = arg.c_str();
    otpTest(testKey);
  } else {
    WebSerial.println("Invalid command");
  }
}

void WebSerialCmdHandler::otpTest(std::string testKey) {
  std::vector<uint8_t> decodeBuffer;

  // Predict size of secret
  auto secretSizePredict = (int) ceil(testKey.size() / 1.6);
  decodeBuffer.resize(secretSizePredict);
  int count = Authenticator::base32Decode(testKey.data(), decodeBuffer.data(), secretSizePredict);
  decodeBuffer.resize(count);

  WebSerial.println(count);

  WebSerial.print("Secret: ");
  WebSerial.println(testKey.c_str());
  WebSerial.print("Decoded: ");
  String decodeBufferStr = "";
  for (int i = 0; i < count; i++) {
    decodeBufferStr += int(decodeBuffer[i]);
  }

  WebSerial.println(decodeBufferStr);
  WebSerial.print("Current Time: ");
  WebSerial.println((unsigned int)Authenticator::getCurrTime());
  WebSerial.print("OTP: ");
  WebSerial.println(Authenticator::generateOtp(decodeBuffer.data(), decodeBuffer.size()));
}

void WebSerialCmdHandler::dumpToSerial(byte *buffer, byte size) {
  String d = "";
  for (byte i = 0; i < size; ++i) {
    d += buffer[i];
    d += ' ';
  }
  WebSerial.println(d);
}

void WebSerialCmdHandler::dumpToSerial(byte *buffer, size_t size) {
  String d = "";
  for (unsigned int i = 0; i < size; ++i) {
    d += buffer[i];
    d += ' ';
  }
  WebSerial.println(d);
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
  if (WiFi.status() == WL_CONNECTED) {
    WebSerial.print("Connected to: ");
    WebSerial.println(WiFi.SSID());
    WebSerial.print("IP: ");
    WebSerial.println(WiFi.localIP().toString());
  } else {
    WebSerial.println("Not operating in STA mode.");
  }
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