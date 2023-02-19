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

#include "helpers.h"
#include "creds.h"
#include "pins.h"
#include "ServerCallbacks.h"

// bool wifiApMode = false;  // This is currently unused 2/17/22

AsyncWebServer server(80);

MFRC522 rfidReader(SPI_SS, RST_PIN);
MFRC522::MIFARE_Key key;

BLEServer *pServer = NULL;
BLEService *pService = NULL;
BLECharacteristic *pCharacteristic = NULL;
ServerCallbacks *pServerCallbacks = NULL;

void receiveWebSerial(uint8_t* data, size_t len) {
  String d = "";
  for(int i = 0; i < len; i++){
    d += char(data[i]);
  }

  WebSerial.println(d);

  if (d.equals("reboot")) {
    WebSerial.print("Device restarting ...");
    WebSerial.print("Refresh this page if it does not automatically reconnect");

    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    delay(1000);
    delay(1000);
    esp_restart();
  }

  if (d.equals("wifi info")) {
    WebSerial.println("[ WiFi STA Info ]");
    WebSerial.print("Connected to: ");
    WebSerial.println(ssid);
    WebSerial.print("IP: ");
    WebSerial.println(WiFi.localIP().toString());
    WebSerial.println("[ WiFi AP Info ]");
    WebSerial.print("SSID: ");
    WebSerial.println(WiFi.softAPSSID());
    WebSerial.print("IP: ");
    WebSerial.println(WiFi.softAPIP().toString());
  }

  if (d.equals("rfid test")) {
    WebSerial.println("Performing MFRC522 Self Test");
    bool res = rfidReader.PCD_PerformSelfTest();
    if (res) {
      WebSerial.println("Self Test OK");
    } else {
      WebSerial.println("Self Test FAIL");
    }
  }

  if (d.equals("rfid read")) {
    if (rfidReader.PICC_IsNewCardPresent()) {
      if (rfidReader.PICC_ReadCardSerial()) {
        WebSerial.print("Card UID: ");
        // String data = readRfidBytes(rfidReader.uid.uidByte, rfidReader.uid.size);
        // WebSerial.print(data);
        dumpToSerial(rfidReader.uid.uidByte, rfidReader.uid.size);
        WebSerial.println();
        MFRC522::PICC_Type type = rfidReader.PICC_GetType(rfidReader.uid.sak);
        WebSerial.print("Card Type: ");
        WebSerial.println(rfidReader.PICC_GetTypeName(type));
      }
    } else {
      WebSerial.println("No tag detected");
    }
  }
}

void initializeWifiAndOTA() {
  // Connect to WiFi network
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ap_ssid, ap_password);
  WiFi.begin(ssid, password);
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
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

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
}

void initializeRfid() {
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_SS);
  rfidReader.PCD_Init();
  delay(12);
  rfidReader.PCD_DumpVersionToSerial();
}

void initializeBluetooth() {
  BLEDevice::init("ThinToken Reader");
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
  dumpToSerial(rfidReader.uid.uidByte, rfidReader.uid.size);
  WebSerial.println();
}