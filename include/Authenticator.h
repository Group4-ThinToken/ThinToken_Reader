#ifndef authenticator_h
#define authenticator_h

#include <sys/types.h>

class Authenticator {
public:
  static uint32_t generateOtp(uint8_t* secret, size_t secretLength);
  static int base32Decode(const char* encoded, uint8_t* result, int bufferLength);
  static void syncTime();
  static time_t getCurrTime();
private:
};

#endif