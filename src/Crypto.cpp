#include "Crypto.h"

#include <algorithm>
#include <stdexcept>
#include <WebSerialLite.h>
#include <mbedtls/gcm.h>

Crypto::Crypto() {}

Crypto::~Crypto() {}

std::vector<uint8_t> Crypto::decrypt(std::vector<uint8_t> input) {
  _log("Decrypt in", input.data(), input.size());
  mbedtls_gcm_context aes;
  uint8_t outBuffer[256];

  mbedtls_gcm_init(&aes);
  mbedtls_gcm_setkey(&aes, MBEDTLS_CIPHER_ID_AES, m_lastCryptoKey.data(), 256);
  mbedtls_gcm_starts(&aes, MBEDTLS_GCM_DECRYPT, m_lastIv.data(), m_lastIv.size(), NULL, 0);
  // Note: Update takes only inputs in 16 byte multiples
  mbedtls_gcm_update(&aes, input.size(), input.data(), outBuffer);
  mbedtls_gcm_free(&aes);

  _log("Decrypt out", outBuffer, 64);

  std::vector<uint8_t> output(outBuffer, outBuffer + 256);
  output = truncateToStopByte(&output);
  _log("Truncate out", output.data(), output.size()); // Print out only 64 bytes
  return output;
}

void Crypto::setCryptoKeyAndIv(std::vector<uint8_t> t_cryptoKeyAndIv) {
  if (t_cryptoKeyAndIv.size() == 0) {
    throw std::logic_error("Crypto key and IV is empty");
  }

  // The secret payload will be structured this way
  // DATA: [0:12) - IV
  // DATA: [12:44) - Key

  m_lastIv = std::vector<uint8_t>(t_cryptoKeyAndIv.begin(),
                                  t_cryptoKeyAndIv.begin() + 12);

  m_lastCryptoKey = std::vector<uint8_t>(t_cryptoKeyAndIv.begin() + 12,
                                         t_cryptoKeyAndIv.end());

  WebSerial.println("Crypto IV and Key set.");
  _log("IV", m_lastIv.data(), m_lastIv.size());
  _log("Key", m_lastCryptoKey.data(), m_lastCryptoKey.size());
}

void Crypto::_log(String display, uint8_t* buffer, size_t size) {
  String a = "";
  a += display;
  a += " bytes: ";
  a += size;

  a += "\nData:\n";
  for (unsigned int i = 0; i < size; i++) {
    a += buffer[i];
    a += ' ';
  }
  WebSerial.println(a);
  Serial.println(a);
}

std::vector<uint8_t> Crypto::truncateToStopByte(std::vector<uint8_t>* in) {
  const uint8_t STOP_BYTE = 0x20; // ' ' is the stop byte defined in JS repo
  const size_t oldSize = in->size();
  size_t newSize;
  for (newSize = 0; newSize < oldSize; newSize++) {
    if (in->at(newSize) == STOP_BYTE) {
      break;
    }
  }

  if (oldSize == newSize) throw std::logic_error("Stop byte not found.");

  return std::vector<uint8_t>(in->data(), in->data() + newSize);
}
