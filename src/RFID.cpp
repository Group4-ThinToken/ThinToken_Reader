#include "RFID.h"

#include <WebSerialLite.h>
#include <mbedtls/aes.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>

const size_t BLOCK_SIZE = 16;

RFID::RFID(MFRC522* t_reader) {
  reader = t_reader;

  for (byte i = 0; i < 6; ++i) {
    MiKey.keyByte[i] = 0xFF;
  }

  mutexLock = false;
}

MFRC522 *RFID::getReader() { 
  return reader;
}

std::vector<byte> RFID::readSector(byte sector) {
  byte blockStart = 4 * sector;
  byte blockEnd = blockStart + 3;

  MFRC522::StatusCode status;
  byte bufferRaw[18];  // Each block read is 16 bytes + 2 CRC bytes
  byte bufferRawSize = sizeof(bufferRaw);
  std::vector<byte> buffer;

  Serial.println("Read: Before Auth");
  // Authenticate via key A
  status = reader->PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockEnd, &MiKey, &(reader->uid));

  if (status != MFRC522::STATUS_OK) {
    // Failed read
    // return buffer;
    throw status;
  }

  for (int i = 0; i < 3; ++i) {
    status = reader->MIFARE_Read(blockStart + i, bufferRaw, &bufferRawSize);

    Serial.printf("Reading block %d", blockStart + i);
    Serial.println(MFRC522::GetStatusCodeName(status));

    if (status != MFRC522::STATUS_OK) {
      throw status;
    }

    for (byte j = 0; j < 16; ++j) {
      buffer.push_back(bufferRaw[j]);
    }

    std::fill_n(bufferRaw, 18, 0);
  }

  reader->PICC_HaltA();
  reader->PCD_StopCrypto1();

  return buffer;
}

int RFID::writeToSector(byte sector, std::vector<byte> data) {
  if (sector < 1) {
    Serial.println("writeToSector: Invalid sector");
    throw 2;
  }
  Serial.print("Write to sector ");
  Serial.println(sector);
  MFRC522::StatusCode status;
  byte blockStart = 4 * sector;
  byte blockEnd = blockStart + 3;

  Serial.println("Auth");
  // Authenticate via key B
  status = reader->PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_B, blockEnd, &MiKey, &(reader->uid));

  Serial.printf("Write Auth: %s\n", MFRC522::GetStatusCodeName(status));
  if (status != MFRC522::STATUS_OK) {
    throw status;
  }

  // Pad data with zeroes so size is multiple of 16
  // while ((data.size() % 16) != 0) {
  //   data.push_back(0);
  // }

  if (data.size() > BLOCK_SIZE * 3) {
    Serial.printf("writeToSector: Data exceeds maximum 48 bytes. Received %d bytes padded", data.size());
    WebSerial.print("writeToSector: Data exceeds maximum 48 bytes. Received ");
    WebSerial.print(data.size());
    WebSerial.println(" padded.");
    reader->PICC_HaltA();
    reader->PCD_StopCrypto1();
    throw 999; // Data exceed size exception
  }

  data.resize(BLOCK_SIZE * 3);

  const size_t dataSizeInBlocks = ceil(data.size() / 16);
  WebSerial.print("Num of Blocks: ");
  WebSerial.println(dataSizeInBlocks);

  for (int i = 0; i < dataSizeInBlocks; ++i) {
    // TODO: Test addressing logic, cuz read fn was wrong
    // Comment for now to avoid wrong writes
    WebSerial.print("Data Written to addr ");
    WebSerial.println(blockStart + i);
    status = reader->MIFARE_Write(blockStart + i, std::vector<byte>(data.data() + (BLOCK_SIZE*i), data.data() + (BLOCK_SIZE*(i+1))).data(), 16);

    Serial.print("Write: ");
    Serial.println(MFRC522::GetStatusCodeName(status));
    if (status != MFRC522::STATUS_OK) {
      throw status;
    }
  }

  reader->PICC_HaltA();
  reader->PCD_StopCrypto1();

  return data.size();
}

int RFID::writeToTagUsingQueue() { 
  int bytesWritten = 0;
  while (!m_writeQueue.empty()) {

    bool writeDone = false;
    try {
      WebSerial.print("Writing to Sector ");
      WebSerial.println(std::get<0>(m_writeQueue.front()));

      bytesWritten += RFID::writeToSector(std::get<0>(m_writeQueue.front()),
                                          std::get<1>(m_writeQueue.front()));
    } catch(int e) {
      if (e != 999) {
        throw e;
      }
    }

    reader->PICC_HaltA();
    bool updateDone = false;
    while (updateDone == false) {
      byte bufferATQA[2];
      byte bufferSize = sizeof(bufferATQA);
      reader->PICC_WakeupA(bufferATQA, &bufferSize);
      if (reader->PICC_ReadCardSerial()) {
        WebSerial.println("Update available space");
        updateAvailableSectors(std::get<0>(m_writeQueue.front()));
        updateDone = true;
      }
    }

    WebSerial.print("Pop Queue: N: ");
    m_writeQueue.pop();
    WebSerial.println(m_writeQueue.size());
  }
  return bytesWritten; 
}

int RFID::appendAccount(std::vector<byte> data) {
  Serial.println("Append account");
  byte tempAddr = 1;

  m_writeQueue.push(std::make_tuple(tempAddr, data));
  return data.size();
}

void RFID::setPreviousUid(byte *uid, size_t size) {
  m_previousUid.clear();
  for (unsigned int i = 0; i < size; i++) {
    m_previousUid.push_back(uid[i]);
  }
}

bool RFID::sameAsPreviousUid(byte *uid, size_t size) { 
  if (size != m_previousUid.size()) {
    Serial.println("UIDs are not same size");
    return false;
  }

  String dCurr = "";
  String dPrev = "";
  bool isSame = true;
  unsigned int i = 0;
  while (isSame && i < size) {
    dCurr += uid[i];
    dCurr += ' ';
    dPrev += m_previousUid[i];
    dPrev += ' ';

    isSame = uid[i] == m_previousUid[i];
  }
  Serial.printf("Current uid: %s\n", dCurr);
  Serial.printf("Previous uid: %s\n", dPrev);
  return isSame; 
}

std::vector<byte> RFID::getAvailableSectors() {
  const int resAddr = 15;
  std::vector<byte> currAvail;

  reader->PICC_HaltA();
  reader->PCD_StopCrypto1();

  bool readDone = false;
  while (readDone == false) {
    byte bufferATQA[2];
    byte bufferSize = sizeof(bufferATQA);
    reader->PICC_WakeupA(bufferATQA, &bufferSize);

    if (reader->PICC_ReadCardSerial()) {
      currAvail = readSector(resAddr);
      readDone = true;
    }
  }

  WebSerial.print("Available sectors: ");
  String displayBuffer = "";
  for (int i = 0; i < currAvail.size(); i++) {
    displayBuffer += currAvail[i];
    displayBuffer += ' ';
  }
  WebSerial.println(displayBuffer);

  reader->PICC_HaltA();
  reader->PCD_StopCrypto1();

  return currAvail;
}

void RFID::updateAvailableSectors(byte occupiedSector) {
  if (occupiedSector > 14) {
    throw std::out_of_range("Sector must be less than 14");
  }

  // Read current available sectors
  const byte resAddr = 15;
  std::vector<byte> currAvail = readSector(resAddr);

  String displayBuffer = "";
  for (int i = 0; i < currAvail.size(); i++) {
    displayBuffer += currAvail[i];
    displayBuffer += ' ';
  }
  WebSerial.println("Raw Available Sectors: ");
  WebSerial.println(displayBuffer);
  Serial.println("Raw Available Sectors: ");
  Serial.println(displayBuffer);

  if (isBufferAllZeroes(currAvail)) {
    currAvail = {1, 4, 7, 10};
  }

  for (int i = 0; i < currAvail.size(); i++) {
    if (currAvail[i] == occupiedSector) {
      currAvail.erase(currAvail.begin() + i);
    }
  }

  currAvail.resize(48);
  std::sort(currAvail.begin(), currAvail.end(), std::greater<byte>());

  bool writeDone = false;
  while (writeDone == false) {
    byte bufferATQA[2];
    byte bufferSize = sizeof(bufferATQA);
    reader->PICC_WakeupA(bufferATQA, &bufferSize);
    if (reader->PICC_ReadCardSerial()) {
      // Write to sector 15 the newly occupied sector
      writeToSector(resAddr, currAvail);
      writeDone = true;
    }
  }

  reader->PICC_HaltA();
  reader->PCD_StopCrypto1();
}

void RFID::decrypt() {
  mbedtls_aes_context context;

  unsigned char key[32];
  unsigned char iv[16];

  unsigned char input[512];
  unsigned char output[512];

  size_t input_len = 40;
  size_t output_len = 0;

  size_t* adjustedInput = &input_len;

  // Generate key
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_entropy_context entropy;

  char *pers = "Test personalization string";
  int ret;

  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctr_drbg);

  if( ( ret = mbedtls_ctr_drbg_seed( &ctr_drbg, mbedtls_entropy_func, &entropy,
    (unsigned char *) pers, strlen( pers ) ) ) != 0 )
  {
    printf( " failed\n ! mbedtls_ctr_drbg_init returned -0x%04x\n", -ret );
  }

  if( ( ret = mbedtls_ctr_drbg_random( &ctr_drbg, key, 32 ) ) != 0 )
  {
    printf( " failed\n ! mbedtls_ctr_drbg_random returned -0x%04x\n", -ret );
  }
  // End generate key

  mbedtls_aes_init(&context);
  mbedtls_aes_setkey_enc(&context, key, 256);
  mbedtls_aes_crypt_cbc(&context, MBEDTLS_AES_ENCRYPT, 48, iv, input, output);
}

void RFID::generateKey(char* personalString) {

}

bool RFID::isBufferAllZeroes(std::vector<byte> buffer) {
  bool isAll0 = true;
  for (int i = 0; i < buffer.size(); i++) {
    if (buffer[i] != 0) {
      isAll0 = false;
    }
  }
  return isAll0;
}