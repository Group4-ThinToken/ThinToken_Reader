#ifndef authenticator_h
#define authenticator_h

#include <sys/types.h>
#include <vector>
#include <WString.h>

class Authenticator {
public:
  static uint32_t generateOtp(uint8_t* secret, size_t secretLength);
  static int base32Decode(const char* encoded, uint8_t* result, int bufferLength);
  static std::vector<uint8_t> getSecret(std::vector<uint8_t>* in);
  static void syncTime();
  static time_t getCurrTime();
private:
  static void _log(String display, uint8_t* buffer, size_t size);
};

#endif