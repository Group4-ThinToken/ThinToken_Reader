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
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include "helpers.h"
#include "creds.h"
#include "pins.h"
#include "ServerCallbacks.h"
#include "commands.h"
#include "RFID.h"
#include "statuses.h"
#include "Crypto.h"
#include "Authenticator.h"
#include "CredentialHandler.h"

// bool wifiApMode = false;  // This is currently unused 2/17/22

WebSerialCmdHandler wsCmdHandler;

AsyncWebServer server(80);

MFRC522 rfidReader(SPI_SS, RST_PIN);
RFID reader(&rfidReader);
MFRC522::MIFARE_Key key;
bool rfidWriteMode;
bool otpMode;
unsigned long lastRfidOp;
bool printWakeupStatus;

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
BLECharacteristic *idCharacteristic = NULL;
BLECharacteristic *sectorCharacteristic = NULL;
CharacteristicCallbacks *timeCallback = NULL;

Crypto crypto;

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
  // Retrieve stored credentials
  CredentialHandler creds;

  // Connect to WiFi network
  WiFi.mode(WIFI_AP_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(creds.getWifiHost());
  WiFi.softAP(creds.getApSsid(), creds.getApPass());
  WiFi.begin(creds.getWifiSsid(), creds.getWifiPass());
  Serial.println("");

  unsigned long timeoutBegin = millis();
  unsigned long timeoutMax = 10000;
  // Wait for connection
  while (millis() - timeoutBegin < timeoutMax &&
          WiFi.status() != WL_CONNECTED) {
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
    Serial.printf("\nConnection to %s failed.\n", creds.getWifiSsid());
    WiFi.disconnect(false, true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(creds.getApSsid(), creds.getApPass());
    Serial.println("Connect to ThinToken Reader AP at");
    Serial.printf("SSID: %s\nPass: %s\n", creds.getApSsid(),
                  creds.getApPass());
  }

  // const char* ntpServer = "pool.ntp.org";
  // configTime(0, 0, ntpServer);

  // Basic index page for HTTP server, HTTP server will be
  // used for OTA and WebSerial
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain",
                  "/update for OTA upload, /webserial for serial monitor");
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

  wsCmdHandler.setCredentialHandler(&creds);
}

void initializeRfid() {
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_SS);
  rfidReader.PCD_Init();
  delay(12);
  rfidReader.PCD_DumpVersionToSerial();

  rfidWriteMode = false;
  lastRfidOp = 0;
  printWakeupStatus = false;

  wsCmdHandler.setRfidReader(&rfidReader, &reader);
  wsCmdHandler.setRfidWriteMode(&rfidWriteMode);
  wsCmdHandler.setOtpMode(&otpMode);
  wsCmdHandler.setPrintWakeupStatus(&printWakeupStatus);

  reader.immediateOpRequested = false;
  rfidReader.PCD_SoftPowerDown();
}

void initializeBluetooth() {
  BLEDevice::init(DEVICE_NAME);
  pServer = BLEDevice::createServer();
  pServerCallbacks = new ServerCallbacks();
  pServerCallbacks->setRfidWriteModeValue(&rfidWriteMode);
  pServerCallbacks->setRfidReader(&reader);
  pServer->setCallbacks(pServerCallbacks);
  pService = pServer->createService(SERVICE_UUID);

  statusCharacteristic = pService->createCharacteristic(
    STATUS_CHARACTERISTIC,
    BLECharacteristic::PROPERTY_NOTIFY |
    BLECharacteristic::PROPERTY_WRITE
  );
  statusCharacteristic->addDescriptor(new BLE2902());
  statusCallback = new CharacteristicCallbacks(statusCharacteristic);
  statusCallback->setRfidWriteModeValue(&rfidWriteMode);
  statusCallback->setOtpMode(&otpMode);
  statusCallback->setRfidReader(&reader);
  statusCharacteristic->setCallbacks(statusCallback);

  secretCharacteristic = pService->createCharacteristic(
    SECRET_CHARACTERISTIC,
    BLECharacteristic::PROPERTY_WRITE_NR
  );
  secretCallback = new CharacteristicCallbacks(statusCharacteristic);
  secretCallback->setRfidReader(&reader);
  secretCallback->setCryptoModule(&crypto);
  secretCallback->setOtpMode(&otpMode);
  secretCharacteristic->setCallbacks(secretCallback);
  
  otpCharacteristic = pService->createCharacteristic(
    OTP_CHARACTERISTIC,
    BLECharacteristic::PROPERTY_INDICATE
  );
  otpCharacteristic->addDescriptor(new BLE2902());
  otpCallback = new CharacteristicCallbacks(statusCharacteristic);
  otpCallback->setRfidReader(&reader);
  otpCallback->setCryptoModule(&crypto);
  otpCharacteristic->setCallbacks(otpCallback);

  timeCharacteristic = pService->createCharacteristic(
    TIME_CHARACTERISTIC,
    BLECharacteristic::PROPERTY_WRITE
  );
  timeCallback = new CharacteristicCallbacks(statusCharacteristic);
  timeCharacteristic->setCallbacks(timeCallback);
  pService->addCharacteristic(timeCharacteristic);

  idCharacteristic = pService->createCharacteristic(
    ID_CHARACTERISTIC,
    BLECharacteristic::PROPERTY_READ
  );
  pService->addCharacteristic(idCharacteristic);

  sectorCharacteristic = pService->createCharacteristic(
    SECTOR_CHARACTERISTIC,
    BLECharacteristic::PROPERTY_NOTIFY |
    BLECharacteristic::PROPERTY_WRITE
  );
  sectorCharacteristic->setCallbacks(statusCallback);
  pService->addCharacteristic(sectorCharacteristic);

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
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  setCpuFrequencyMhz(80);
  uint32_t cpuFreq = getCpuFrequencyMhz();
  pinMode(LED_BUILTIN, OUTPUT);

  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(115200);
  Serial.printf("CPU Freq: %d\n", cpuFreq);

  initializeWifiAndOTA();
  initializeRfid();
  initializeBluetooth();

  digitalWrite(LED_BUILTIN, HIGH);

  lastBtUpdate = millis();
}

void loop() {
	byte bufferATQA[2];
	byte bufferSize = sizeof(bufferATQA);

  MFRC522::StatusCode wakeupRes = rfidReader.PICC_WakeupA(bufferATQA, &bufferSize);

  if (printWakeupStatus == true) {
    Serial.println(MFRC522::GetStatusCodeName(wakeupRes));
  }

  byte idBuffer[5] = {0, 0, 0, 0, 0};

  if (wakeupRes == MFRC522::STATUS_OK) {
    if (rfidReader.PICC_ReadCardSerial()) {
      if (millis() - lastRfidOp > 3000 &&
          reader.mutexLock != true ||
          reader.immediateOpRequested == true) {
        reader.mutexLock = true;

        digitalWrite(LED_BUILTIN, LOW);
        idCharacteristic->setValue(rfidReader.uid.uidByte, rfidReader.uid.size);
        // First byte (byte 0) is the status code
        // Byte 1, 2, 3, 4 is the tag id
        idBuffer[0] = ST_TagRead;
        idBuffer[1] = rfidReader.uid.uidByte[0];
        idBuffer[2] = rfidReader.uid.uidByte[1];
        idBuffer[3] = rfidReader.uid.uidByte[2];
        idBuffer[4] = rfidReader.uid.uidByte[3];
        statusCharacteristic->setValue(idBuffer, sizeof(uint8_t) * 5);
        statusCharacteristic->notify();

        reader.setPreviousUid(rfidReader.uid.uidByte, rfidReader.uid.size);
        WebSerial.println("Card UID: ");
        wsCmdHandler.dumpToSerial(rfidReader.uid.uidByte, rfidReader.uid.size);
        WebSerial.println();

        WebSerial.print("RFID Write Mode: ");
        WebSerial.println(rfidWriteMode);

        switch (rfidWriteMode) {
          case true: {
            statusCharacteristic->setValue(&ST_WriteTagReady, 1);
            statusCharacteristic->notify();

            WebSerial.println("Writing to RFID Tag");
            int nBytes;
            byte sectorWrittenTo;
            try {
              nBytes = reader.writeToTagUsingQueue(&sectorWrittenTo);
              WebSerial.print(nBytes);
              WebSerial.println(" bytes written in total.");
            } catch(const std::exception& e) {
              WebSerial.println(e.what());
              statusCharacteristic->setValue(&ST_WriteFailed, sizeof(uint8_t));
              statusCharacteristic->notify();
            } catch(MFRC522::StatusCode& status) {
              WebSerial.println(MFRC522::GetStatusCodeName(status));
              statusCharacteristic->setValue(&ST_WriteFailed, sizeof(uint8_t));
              statusCharacteristic->notify();
            }
            
            rfidReader.PICC_HaltA();
            rfidReader.PCD_StopCrypto1();

            if (nBytes > 0) {
              // Send a success response containing the status code
              // tag id, and sector where the last write occured
              byte out[6] = {ST_WriteSuccess};
              for (unsigned int i = 0; i < rfidReader.uid.size; i++) {
                out[i+1] = rfidReader.uid.uidByte[i];
              }
              out[5] = sectorWrittenTo;
              statusCharacteristic->setValue(out, 6);
              statusCharacteristic->notify();
            }
            rfidWriteMode = false;
            break;
          }
          case false: {
            const size_t nItems = reader.getItemsInReadQueue();
            for (int i = 0; i < nItems; i++) {
              std::vector<byte> data = reader.readOnceFromQueue();
              wsCmdHandler.dumpToSerial(data.data(), data.size());

              if (data.size() > 0) {
                // If otpMode, decrypt data then only return otp
                WebSerial.print("OTP Mode: ");
                WebSerial.println(otpMode);
                if (otpMode) {
                  auto ptext = crypto.decrypt(data);
                  auto totpSecret = Authenticator::getSecret(&ptext);
                  std::string secretStr(totpSecret.begin(), totpSecret.end());

                  std::vector<uint8_t> decodeBuffer;
                  auto secretSizePredict = (int)ceil(secretStr.size() / 1.6);
                  decodeBuffer.resize(secretSizePredict);
                  int count = Authenticator::base32Decode(secretStr.data(),
                                                          decodeBuffer.data(),
                                                          secretSizePredict);
                  decodeBuffer.resize(count);
                  uint32_t otp = Authenticator::generateOtp(decodeBuffer.data(),
                                                            decodeBuffer.size());

                  WebSerial.println("Sending via bluetooth");
                  otpCharacteristic->setValue(otp);
                  otpCharacteristic->indicate();
                } else {
                  otpCharacteristic->setValue(data.data(), data.size());
                  otpCharacteristic->indicate();
                }
              }
            }
            
            rfidReader.PICC_HaltA();
            rfidReader.PCD_StopCrypto1();
            WebSerial.println("Done reading.");
            reader.immediateOpRequested = false;
            otpMode = false;
          }
        }

        reader.mutexLock = false;
        lastRfidOp = millis();
      }
    }
  }
  digitalWrite(LED_BUILTIN, HIGH);
}