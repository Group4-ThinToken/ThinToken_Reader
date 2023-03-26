#include "RFID.h"

#include <mbedtls/aes.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>

RFID::RFID(MFRC522* t_reader) {
  reader = t_reader;

  for (byte i = 0; i < 6; ++i) {
    MiKey.keyByte[i] = 0xFF;
  }
}

std::vector<byte> RFID::readTag(byte sector) {
  byte blockStart = 4 * sector;
  byte blockEnd = 7 * sector;

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

int RFID::writeTag(byte sector, std::vector<byte> data) {
  Serial.println("Write tag");
  MFRC522::StatusCode status;
  byte blockStart = 4 * sector;
  byte blockEnd = 7 * sector;

  Serial.println("Auth");
  // Authenticate via key B
  status = reader->PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_B, blockEnd, &MiKey, &(reader->uid));

  Serial.println(status);
  if (status != MFRC522::STATUS_OK) {
    throw status;
  }

  status = reader->MIFARE_Write(blockStart, data.data(), 16);

  Serial.println(status);
  if (status != MFRC522::STATUS_OK) {
    throw status;
  }

  return data.size();
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