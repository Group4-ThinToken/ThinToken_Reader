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

  // reader->PICC_HaltA();
  // reader->PCD_StopCrypto1();

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
    // reader->PICC_HaltA();
    // reader->PCD_StopCrypto1();
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

  // reader->PICC_HaltA();
  // reader->PCD_StopCrypto1();

  return data.size();
}

int RFID::writeToTagUsingQueue() {
  const size_t SECTOR_SIZE = 48;
  int bytesWritten = 0;
  while (!m_writeQueue.empty()) {
    // Assign sector to write
    byte sectorAddr;
    if (std::get<0>(m_writeQueue.front()) != 15) {
      sectorAddr = getAvailableSectors().front();
    } else {
      sectorAddr = 15;
    }

    if (sectorAddr != 0) {
      std::get<0>(m_writeQueue.front()) = sectorAddr;
    } else {
      Serial.printf("sectorAddr: %d", sectorAddr);
      WebSerial.print("Pop Queue: N: ");
      m_writeQueue.pop();
      WebSerial.println(m_writeQueue.size());
      throw std::out_of_range("Not enough space in ThinToken.");
    }

    std::get<0>(m_writeQueue.front()) = sectorAddr;
    try {
      std::get<1>(m_writeQueue.front()).resize(SECTOR_SIZE*3, 0);
      std::vector<byte> sectorData(std::get<1>(m_writeQueue.front()));

      for (unsigned int i = 0; i < 3; i++) {
        if (sectorAddr + i < 16 && sectorAddr > 0) {
          WebSerial.print("Writing to Sector ");
          WebSerial.println(sectorAddr + i);

          bytesWritten += RFID::writeToSector(std::get<0>(m_writeQueue.front()) + i,
                                              std::vector<byte>(sectorData.data() + (SECTOR_SIZE*i), sectorData.data() + (SECTOR_SIZE*(i+1))));
        }
      }
    } catch(int e) {
      if (e != 999) {
        throw e;
      }
    }

    if (sectorAddr != 15) {
      WebSerial.println("Update available space");
      updateAvailableSectors(std::get<0>(m_writeQueue.front()));
    }

    WebSerial.print("Pop Queue: N: ");
    m_writeQueue.pop();
    WebSerial.println(m_writeQueue.size());
  }
  return bytesWritten; 
}

std::vector<byte> RFID::readOnceFromQueue() {
  const size_t SECTOR_SIZE = 48;
  std::vector<byte> data;

  byte sectorAddr = m_readQueue.front();
  if (sectorAddr > 14) {
    throw std::out_of_range("Sector must be less than 14");
  }

  if (m_readQueue.empty()) {
    throw std::logic_error("Read queue empty");
  }

  // Check available sectors, if the sector being read
  // is "available", then dont return anything
  auto avail = getAvailableSectors();
  bool isSectorMarkedBlank = false;
  for (unsigned int i = 0; i < avail.size(); i++) {
    if (avail[i] == sectorAddr) {
      isSectorMarkedBlank = true;
    }
  }

  if (isSectorMarkedBlank) {
    // Short circuit here
    m_readQueue.pop();
    return data;

  }

  WebSerial.print("Read From Queue - Sector ");
  WebSerial.println(sectorAddr);

  try {
    for (byte i = 0; i < 3; i++) {
      std::vector<byte> currSec = readSector(sectorAddr + i);
      currSec.resize(SECTOR_SIZE, 0);
      data.insert(data.end(), currSec.begin(), currSec.end());
    }
  } catch(MFRC522::StatusCode e) {
    WebSerial.print("Read From Queue Error: ");
    WebSerial.println(e);
  }
  
  m_readQueue.pop();
  return data;
}

int RFID::appendAccount(std::vector<byte> data) {
  Serial.println("Append account");
  byte tempAddr = 1;

  m_writeQueue.push(std::make_tuple(tempAddr, data));
  return data.size();
}

int RFID::queueRead(byte sector) {
  Serial.println("Queue read");
  m_readQueue.push(sector);
  return sector;
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

  // reader->PICC_HaltA();
  // reader->PCD_StopCrypto1();

  currAvail = readSector(resAddr);

  WebSerial.print("Available sectors: ");
  String displayBuffer = "";
  for (int i = 0; i < currAvail.size(); i++) {
    displayBuffer += currAvail[i];
    displayBuffer += ' ';
  }
  WebSerial.println(displayBuffer);

  // reader->PICC_HaltA();
  // reader->PCD_StopCrypto1();

  return currAvail;
}

/// @brief Clears the available space metadata of a tag
/// @brief to clear all accounts and secrets
void RFID::clearThinToken() {
  const unsigned int BLOCK_SIZE = 16;
  std::vector<byte> data = {10, 7, 4, 1};
  data.resize(BLOCK_SIZE, 0);

  m_writeQueue.push(std::make_tuple(15, data));
}

void RFID::clearWriteQueue() {
  std::queue<std::tuple<byte, std::vector<byte>>> _;
  std::swap(m_writeQueue, _);
}

void RFID::readAccounts() {
  bool doneRead = false;
  std::vector<byte> avail;

  Serial.println("Read Accounts");
  while (doneRead == false) {
    byte bufferATQA[2];
    byte bufferSize = sizeof(bufferATQA);
    reader->PICC_WakeupA(bufferATQA, &bufferSize);
    if (reader->PICC_ReadCardSerial()) {
      Serial.println("Read Accounts > if");
      try {
        avail = getAvailableSectors();
        doneRead = true;
      } catch (MFRC522::StatusCode e) {
        WebSerial.print("Exception in read: ");
        WebSerial.println(MFRC522::GetStatusCodeName(e));
      }
    }
  }

  reader->PICC_HaltA();
  reader->PCD_StopCrypto1();

  const byte possible[4] = {10, 7, 4, 1};

  Serial.println("Read Accounts > outside for");
  for (int i = 0; i < 4; i++) {
    Serial.println("Read Accounts > inside for");
    // Checks if there is data on a given sector
    // if there is, push it to read queue
    bool isPresent = false;
    for (int j = 0; j < avail.size(); j++) {
      if (possible[i] == avail[j]) {
        isPresent = true;
      }
    }

    if (!isPresent) {
      m_readQueue.push(possible[i]);
    }
  }

}

int RFID::getItemsInReadQueue() {
  size_t out = m_readQueue.size();
  Serial.printf("Items in read queue: %d", out);
  return out;
}

int RFID::getItemsInWriteQueue() {
  return m_writeQueue.size();
}

void RFID::updateAvailableSectors(byte occupiedSector) {
  if (occupiedSector > 14) {
    std::string e = "Sector must be less than 14, got " + occupiedSector;
    throw std::out_of_range(e);
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

  for (int i = 0; i < currAvail.size(); i++) {
    if (currAvail[i] == occupiedSector) {
      currAvail.erase(currAvail.begin() + i);
    }
  }

  currAvail.resize(48);
  std::sort(currAvail.begin(), currAvail.end(), std::greater<byte>());

  // Write to sector 15 the newly occupied sector
  writeToSector(resAddr, currAvail);
}

// void RFID::decrypt() {
//   mbedtls_aes_context context;

//   unsigned char key[32];
//   unsigned char iv[16];

//   unsigned char input[512];
//   unsigned char output[512];

//   size_t input_len = 40;
//   size_t output_len = 0;

//   size_t* adjustedInput = &input_len;

//   // Generate key
//   mbedtls_ctr_drbg_context ctr_drbg;
//   mbedtls_entropy_context entropy;

//   char *pers = "Test personalization string";
//   int ret;

//   mbedtls_entropy_init(&entropy);
//   mbedtls_ctr_drbg_init(&ctr_drbg);

//   if( ( ret = mbedtls_ctr_drbg_seed( &ctr_drbg, mbedtls_entropy_func, &entropy,
//     (unsigned char *) pers, strlen( pers ) ) ) != 0 )
//   {
//     printf( " failed\n ! mbedtls_ctr_drbg_init returned -0x%04x\n", -ret );
//   }

//   if( ( ret = mbedtls_ctr_drbg_random( &ctr_drbg, key, 32 ) ) != 0 )
//   {
//     printf( " failed\n ! mbedtls_ctr_drbg_random returned -0x%04x\n", -ret );
//   }
//   // End generate key

//   mbedtls_aes_init(&context);
//   mbedtls_aes_setkey_enc(&context, key, 256);
//   mbedtls_aes_crypt_cbc(&context, MBEDTLS_AES_ENCRYPT, 48, iv, input, output);
// }

bool RFID::isBufferAllZeroes(std::vector<byte> buffer) {
  bool isAll0 = true;
  for (int i = 0; i < buffer.size(); i++) {
    if (buffer[i] != 0) {
      isAll0 = false;
    }
  }
  return isAll0;
}