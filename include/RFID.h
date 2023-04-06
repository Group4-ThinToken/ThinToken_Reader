#ifndef rfid_h
#define rfid_h

#include <MFRC522.h>
#include <vector>
#include <queue>
#include <tuple>

class RFID {
public:
  bool mutexLock;

  RFID(MFRC522* t_reader);
  MFRC522* getReader();
  std::vector<byte> readSector(byte sector);
  int writeToSector(byte sector, std::vector<byte> data);
  int writeToTagUsingQueue();
  int appendAccount(std::vector<byte> data);
  void setPreviousUid(byte *uid, size_t size);
  bool sameAsPreviousUid(byte *uid, size_t size);
  std::vector<byte> getAvailableSectors();

  // AES Related methods
  void generateKey(char* personalString);
  void decrypt(char* personalString);

private:
  MFRC522* reader;
  MFRC522::MIFARE_Key MiKey;
  std::vector<byte> m_previousUid;
  std::queue<std::tuple<byte, std::vector<byte>>> m_writeQueue;

  void updateAvailableSectors(byte occupiedSector);
  static bool isBufferAllZeroes(std::vector<byte> buffer);
  static void decrypt();
};

#endif