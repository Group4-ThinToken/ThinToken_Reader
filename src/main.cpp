#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WebSerialLite.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <AsyncElegantOTA.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <time.h>

#include "helpers.h"
#include "creds.h"
#include "pins.h"
#include "ServerCallbacks.h"
#include "commands.h"
#include "RFID.h"

// bool wifiApMode = false;  // This is currently unused 2/17/22

WebSerialCmdHandler wsCmdHandler;

AsyncWebServer server(80);

MFRC522 rfidReader(SPI_SS, RST_PIN);
RFID reader(&rfidReader);
MFRC522::MIFARE_Key key;
bool rfidWriteMode;
unsigned long lastRfidOp;
// byte lastTagUid[10];
std::array<byte, 10> lastTagUid;

BLEServer *pServer = NULL;
BLEService *pService = NULL;
BLEService *pService2 = NULL;
ServerCallbacks *pServerCallbacks = NULL;
BLECharacteristic *statusCharacteristic = NULL;
CharacteristicCallbacks *statusCallback = NULL;
BLECharacteristic *secretCharacteristic = NULL;
CharacteristicCallbacks *secretCallback = NULL;
BLECharacteristic *otpCharacteristic = NULL;
CharacteristicCallbacks *otpCallback = NULL;
BLECharacteristic *timeCharacteristic = NULL;
CharacteristicCallbacks *timeCallback = NULL;

void receiveWebSerial(uint8_t* data, size_t len) {
  String d = "";
  String arg = "";
  int nSpaces = 0;
  // for(int i = 0; i < len; i++){
  //   if (char(data[i]) == ' ') {
  //     nSpaces++;
  //   }
  //   d += char(data[i]);
  // }
  int i = 0;
  while (i < len && nSpaces < 2) {
    if (char(data[i]) == ' ') {
      nSpaces++;
    }
    d += char(data[i]);
    i++;
  }

  while (i < len) {
    arg += char(data[i]);
    i++;
  }

  d.trim();
  arg.trim();

  wsCmdHandler.runCommand(d, arg);
}

void initializeWifiAndOTA() {
  // Connect to WiFi network
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("");

  unsigned long timeoutBegin = millis();
  unsigned long timeoutMax = 10000;
  // Wait for connection
  while (millis() - timeoutBegin < timeoutMax && WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    // TODO: This is not working as intended
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(WIFI_SSID);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.printf("Connection to %s failed.", WIFI_SSID);
  }

  // const char* ntpServer = "pool.ntp.org";
  // configTime(0, 0, ntpServer);

  // Basic index page for HTTP server, HTTP server will be
  // used for OTA and WebSerial
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "/update for OTA upload, /webserial for serial monitor");
  });

  // Elegant Async OTA
  // Start OTA server so we don't need USB cable to upload code
  AsyncElegantOTA.begin(&server);
  server.begin();

  // Start webserial server so we don't need USB cable to view serial monitor
  WebSerial.onMessage(receiveWebSerial);
  WebSerial.begin(&server);

  Serial.print("Device time: ");
  Serial.println(time(nullptr));
}

void initializeRfid() {
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_SS);
  rfidReader.PCD_Init();
  delay(12);
  rfidReader.PCD_DumpVersionToSerial();
  rfidWriteMode = false;
  lastRfidOp = 0;
  lastTagUid = {0};

  wsCmdHandler.setRfidReader(&rfidReader);
  wsCmdHandler.setRfidWriteMode(&rfidWriteMode);
}

void initializeBluetooth() {
  BLEDevice::init(DEVICE_NAME);
  pServer = BLEDevice::createServer();
  pServerCallbacks = new ServerCallbacks();
  pServer->setCallbacks(pServerCallbacks);
  pService = pServer->createService(SERVICE_UUID);

  statusCharacteristic = pService->createCharacteristic(
    STATUS_CHARACTERISTIC,
    BLECharacteristic::PROPERTY_NOTIFY |
    BLECharacteristic::PROPERTY_WRITE
  );
  statusCharacteristic->addDescriptor(new BLE2902());
  statusCallback = new CharacteristicCallbacks();
  statusCallback->setRfidWriteModeValue(&rfidWriteMode);
  statusCharacteristic->setCallbacks(statusCallback);

  secretCharacteristic = pService->createCharacteristic(
    SECRET_CHARACTERISTIC,
    BLECharacteristic::PROPERTY_WRITE_NR
  );
  secretCallback = new CharacteristicCallbacks();
  secretCharacteristic->setCallbacks(secretCallback);
  
  otpCharacteristic = pService->createCharacteristic(
    OTP_CHARACTERISTIC,
    BLECharacteristic::PROPERTY_INDICATE
  );
  otpCharacteristic->addDescriptor(new BLE2902());
  otpCallback = new CharacteristicCallbacks();
  otpCharacteristic->setCallbacks(otpCallback);

  timeCharacteristic = pService->createCharacteristic(
    TIME_CHARACTERISTIC,
    BLECharacteristic::PROPERTY_WRITE
  );
  timeCallback = new CharacteristicCallbacks();
  timeCharacteristic->setCallbacks(timeCallback);
  pService->addCharacteristic(timeCharacteristic);

  statusCharacteristic->setValue("Hello world");
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x10);
  BLEDevice::startAdvertising();

  wsCmdHandler.setBtServerCallbacks(pServerCallbacks);
  wsCmdHandler.setOtpCharacteristic(otpCharacteristic);
  wsCmdHandler.setStatusCharacteristic(statusCharacteristic);
  WebSerial.println("Bluetooth initialized:\nDevice Name: ThinToken Reader");
}

unsigned long lastBtUpdate;
int btTest = 0;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(115200);
  initializeWifiAndOTA();
  initializeRfid();
  initializeBluetooth();

  digitalWrite(LED_BUILTIN, LOW);

  lastBtUpdate = millis();
}

void loop() {
  // if (millis() - lastBtUpdate > 5000) {
  //   statusCharacteristic->setValue(btTest);
  //   statusCharacteristic->notify();

  //   otpCharacteristic->setValue(btTest);
  //   otpCharacteristic->indicate();

  //   lastBtUpdate = millis();
  //   btTest += 1;
  // }

  if (rfidReader.PICC_IsNewCardPresent()) {
    if (rfidReader.PICC_ReadCardSerial()) {
      if (millis() - lastRfidOp > 5000) {
        WebSerial.println("Card UID: ");
        wsCmdHandler.dumpToSerial(rfidReader.uid.uidByte, rfidReader.uid.size);
        WebSerial.println();

        WebSerial.print("RFID Write Mode: ");
        WebSerial.println(rfidWriteMode);
        switch (rfidWriteMode) {
          case true: {
            WebSerial.println("Writing to RFID Tag - Sector 1");
            std::vector<byte> data = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF };

            try {
              int nWritten = reader.writeTag(1, data);

              WebSerial.print("Successfully written ");
              WebSerial.print(nWritten);
              WebSerial.println(" bytes.");
            } catch(MFRC522::StatusCode e) {
              WebSerial.print("Write failed. Status: ");
              WebSerial.println(e);
            }
          }
          case false: {
            WebSerial.println("Card Data - Sector 1");
            std::vector<byte> data = reader.readTag(1);
            wsCmdHandler.dumpToSerial(data.data(), data.size());

            WebSerial.println("Done reading.");
          }
        }

        lastRfidOp = millis();
      }
    }
  }

}