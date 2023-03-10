#include "RFID.h"

RFID::RFID(MFRC522* t_reader) {
  reader = t_reader;

  for (byte i = 0; i < 6; ++i) {
    MiKey.keyByte[i] = 0xFF;
  }
}

std::vector<byte> RFID::readTag(byte sector) {

  byte blockStart = 4;
  byte blockEnd = 7;

  MFRC522::StatusCode status;
  byte bufferRaw[18];  // Each block read is 16 bytes + 2 CRC bytes
  byte bufferRawSize = sizeof(bufferRaw);
  std::vector<byte> buffer;

  // Authenticate via key A
  status = reader->PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockEnd, &MiKey, &(reader->uid));

  if (status != MFRC522::STATUS_OK) {
    // Failed read
    // return buffer;
    throw status;
  }

  status = reader->MIFARE_Read(blockStart, bufferRaw, &bufferRawSize);

  if (status != MFRC522::STATUS_OK) {
    throw status;
  }

  for (byte i = 0; i < 16; ++i) {
    buffer.push_back(bufferRaw[i]);
  }

  reader->PICC_HaltA();
  reader->PCD_StopCrypto1();

  Serial.println(status);
  return buffer;
}