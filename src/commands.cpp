#include "commands.h"
#include "pins.h"

#include <WebSerialLite.h>
#include <MFRC522.h>

class WebSerialCmdHandler {
  MFRC522* rfidReader;

  WebSerialCmdHandler() {
  }

  void setRfidReader(MFRC522* t_reader) {
    rfidReader = t_reader;
  }

  void runCommand(String name) {
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

  void dumpToSerial(byte *buffer, byte size) {
    for (byte i = 0; i < size; ++i) {
      WebSerial.print(buffer[i]);
      WebSerial.print(" ");
    }
    WebSerial.println();
  }

  void reboot() {
    WebSerial.print("Device restarting ...");
    WebSerial.print("Refresh this page if it does not automatically reconnect");

    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    delay(1000);
    delay(1000);
    esp_restart();
  }

  void wifiInfo() {
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

  void rfidRead() {
    if (rfidReader->PICC_IsNewCardPresent()) {
      if (rfidReader->PICC_ReadCardSerial()) {
        WebSerial.print("Card UID: ");
        // String data = readRfidBytes(rfidReader.uid.uidByte, rfidReader.uid.size);
        // WebSerial.print(data);
        dumpToSerial(rfidReader->uid.uidByte, rfidReader->uid.size);
        WebSerial.println();
        MFRC522::PICC_Type type = rfidReader->PICC_GetType(rfidReader->uid.sak);
        WebSerial.print("Card Type: ");
        WebSerial.println(rfidReader->PICC_GetTypeName(type));
      }
    } else {
      WebSerial.println("No tag detected");
    }
  }
};