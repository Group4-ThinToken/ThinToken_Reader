#include "Authenticator.h"
#include <cmath>
#include <cstring>
#include <ctime>
#include <HardwareSerial.h>
#include <mbedtls/md.h>
#include <WebSerialLite.h>

uint32_t Authenticator::generateOtp(uint8_t *secret, size_t secretLength) {
  // Serial.println("Getting time");
  uint64_t currTime = (time_t)floor(time(nullptr) / 30.0);

  // Serial.println("Detecting Endianness");
  // Detect whether device is big endian or small endian
  // For ESP32-CAM we are not sure, so better be safe and
  // check instead of hard code
  uint32_t endianness = 0xDEADBEEF;
  if ((*(const uint8_t *)&endianness) == 0xEF) {
    // If small endian, reverse the time bytes
    currTime = ((currTime & 0x00000000FFFFFFFFU) << 32U) |
               ((currTime & 0xFFFFFFFF00000000U) >> 32U);
    currTime = ((currTime & 0x0000FFFF0000FFFFU) << 16U) |
               ((currTime & 0xFFFF0000FFFF0000U) >> 16U);
    currTime = ((currTime & 0x00FF00FF00FF00FFU) << 8U) |
               ((currTime & 0xFF00FF00FF00FF00U) >> 8U);
  }

  unsigned char hmacResult[512];

  mbedtls_md_context_t context;
  mbedtls_md_init(&context);
  mbedtls_md_setup(&context, mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), 1);
  mbedtls_md_hmac_starts(&context, (const unsigned char *)secret, secretLength);
  mbedtls_md_hmac_update(&context, (const unsigned char *)&currTime,
                         sizeof(currTime));
  mbedtls_md_hmac_finish(&context, hmacResult);
  // Serial.println((const char *)hmacResult);
  mbedtls_md_free(&context);

  // Serial.println("Generating DT");
  uint32_t dynamicTruncation = 0;
  uint64_t offset = hmacResult[19] & 0xFU;
  // I don't understand how this part works actually
  // but this is in the RFC doc
  uint32_t binCode = (hmacResult[offset] & 0x7FU) << 24 |
                     (hmacResult[offset + 1] & 0xFFU) << 16 |
                     (hmacResult[offset + 2] & 0xFFU) << 8 |
                     (hmacResult[offset + 3] & 0xFFU);

  return binCode % (int)pow(10, 6); // 6 denotes the number of digits of the otp
}

/**
 * Base32 decoder
 * From
 * https://github.com/google/google-authenticator-libpam/blob/master/src/base32.c
 * @param encoded Encoded text
 * @param result Bytes output
 * @param buf_len Bytes length
 * @return -1 if failed, or length decoded
 */
int Authenticator::base32Decode(const char *encoded, uint8_t *result, int bufferLength) {
  Serial.println("Inside base32Decode");
  if (encoded == nullptr || result == nullptr) {
    Serial.println("String or buffer are null");
    return -1;
  }

  // Base32's overhead must be at least 1.4x than the decoded bytes, so the
  // result output must be bigger than this
  size_t expectedLength = ceil(std::strlen(encoded) / 1.6);
  if (bufferLength < expectedLength) {
    Serial.printf("Buffer length is too short, only %d, need %u\n", bufferLength, expectedLength);
    return -1;
  }

  int buffer = 0;
  int bits_left = 0;
  int count = 0;
  // Serial.println("Before loop");
  for (const char *ptr = encoded; count < bufferLength && *ptr; ++ptr) {
    // Serial.println("Loop");
    uint8_t ch = *ptr;
    if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '-') {
      continue;
    }
    buffer <<= 5;

    // Serial.println("Mistyped");
    // Deal with commonly mistyped characters
    if (ch == '0') {
      ch = 'O';
    } else if (ch == '1') {
      ch = 'L';
    } else if (ch == '8') {
      ch = 'B';
    }

    // Serial.println("Mistyped");
    // Look up one base32 digit
    if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
      ch = (ch & 0x1F) - 1;
    } else if (ch >= '2' && ch <= '7') {
      ch -= '2' - 26;
    } else {
      // ESP_LOGE(TAG, "Invalid Base32!");
      return -1;
    }

    // Serial.println("Store");
    buffer |= ch;
    bits_left += 5;
    if (bits_left >= 8) {
      result[count++] = buffer >> (bits_left - 8);
      bits_left -= 8;
    }
  }

  // Serial.println("Pad");
  if (count < bufferLength) {
    result[count] = '\000';
  }
  return count;
}

// Get the TOTP secret from <EMAIL>,<SECRET> vector
std::vector<uint8_t> Authenticator::getSecret(std::vector<uint8_t> *in) {
  const uint8_t DELIMITER = 0x2C;
  int beginIdx;
  for (beginIdx = 0; beginIdx < in->size(); beginIdx++) {
    if (in->at(beginIdx) == DELIMITER) {
      beginIdx++;
      break;
    }
  }

  if (beginIdx == in->size()) {
    throw std::logic_error("Secret not found");
  }

  auto out = std::vector<uint8_t>(in->data() + beginIdx, in->data() + in->size());
  _log("Secret", out.data(), out.size());
  return out;
}

time_t Authenticator::getCurrTime() { return time(nullptr); }

void Authenticator::_log(String display, uint8_t* buffer, size_t size) {
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