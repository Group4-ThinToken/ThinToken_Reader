#ifndef crypto_h
#define crypto_h

#include <stdint.h>
#include <vector>

class Crypto {
private:
  /* data */
  std::vector<uint8_t> m_lastCryptoKey;
  std::vector<uint8_t> m_lastIv;

  void reset(); // Clear CryptoKey and IV
public:
  Crypto();
  ~Crypto();

  std::vector<uint8_t> decrypt(std::vector<uint8_t> input);

  void setCryptoKey(std::vector<uint8_t> t_cryptoKey);
  void setIv(std::vector<uint8_t> t_lastIv);
};

#endif