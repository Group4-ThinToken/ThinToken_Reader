#ifndef rfid_h
#define rfid_h

#include <MFRC522.h>
#include <vector>
#include <queue>
#include <tuple>

class RFID {
public:
  bool mutexLock;
  bool immediateOpRequested;

  RFID(MFRC522* t_reader);
  MFRC522* getReader();
  std::vector<byte> readSector(byte sector);
  int writeToSector(byte sector, std::vector<byte> data);
  int writeToTagUsingQueue();
  std::vector<byte> readOnceFromQueue();
  int appendAccount(std::vector<byte> data);
  int queueRead(byte sector);
  void setPreviousUid(byte *uid, size_t size);
  bool sameAsPreviousUid(byte *uid, size_t size);
  std::vector<byte> getAvailableSectors();
  void clearThinToken();
  void clearWriteQueue();
  void readAccounts();
  int getItemsInReadQueue();

  // AES Related methods
  void generateKey(char* personalString);
  void decrypt(char* personalString);

private:
  MFRC522* reader;
  MFRC522::MIFARE_Key MiKey;
  std::vector<byte> m_previousUid;
  std::queue<std::tuple<byte, std::vector<byte>>> m_writeQueue;
  std::queue<byte> m_readQueue;

  void updateAvailableSectors(byte occupiedSector);
  static bool isBufferAllZeroes(std::vector<byte> buffer);
  static void decrypt();
};

#endif