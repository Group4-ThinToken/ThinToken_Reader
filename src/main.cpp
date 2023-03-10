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

// bool wifiApMode = false;  // This is currently unused 2/17/22

WebSerialCmdHandler wsCmdHandler;

AsyncWebServer server(80);

MFRC522 rfidReader(SPI_SS, RST_PIN);
MFRC522::MIFARE_Key key;

BLEServer *pServer = NULL;
BLEService *pService = NULL;
BLECharacteristic *pCharacteristic = NULL;
ServerCallbacks *pServerCallbacks = NULL;

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

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(WIFI_SSID);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  const char* ntpServer = "pool.ntp.org";
  configTime(0, 0, ntpServer);

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

  wsCmdHandler.setRfidReader(&rfidReader);
}

void initializeBluetooth() {
  BLEDevice::init(DEVICE_NAME);
  pServer = BLEDevice::createServer();
  pServerCallbacks = new ServerCallbacks();
  pServer->setCallbacks(pServerCallbacks);
  pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setValue("Hello world");
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x10);
  BLEDevice::startAdvertising();

  wsCmdHandler.setBtServerCallbacks(pServerCallbacks);
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
  if (millis() - lastBtUpdate > 5000) {
    pCharacteristic->setValue(btTest);
    pCharacteristic->notify();
    lastBtUpdate = millis();
    btTest += 1;
  }

  if (!rfidReader.PICC_IsNewCardPresent()) {
    return;
  }

  if (!rfidReader.PICC_ReadCardSerial()) {
    return;
  }

  WebSerial.println("Card UID: ");
  wsCmdHandler.dumpToSerial(rfidReader.uid.uidByte, rfidReader.uid.size);
  WebSerial.println();
}