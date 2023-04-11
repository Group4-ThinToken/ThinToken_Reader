#ifndef crypto_h
#define crypto_h

#include <stdint.h>
#include <vector>
#include <WString.h>

class Crypto {
public:
  Crypto();
  ~Crypto();

  std::vector<uint8_t> decrypt(std::vector<uint8_t> input);

  void setCryptoKeyAndIv(std::vector<uint8_t> t_cryptoKeyAndIv);
private:
  /* data */
  std::vector<uint8_t> m_lastCryptoKey;
  std::vector<uint8_t> m_lastIv;

  void reset(); // Clear CryptoKey and IV
  void _log(String display, uint8_t* buffer, size_t size);
  std::vector<uint8_t> truncateToStopByte(std::vector<uint8_t>* in);
};

#endif