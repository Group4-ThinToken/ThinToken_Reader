#ifndef rfid_h
#define rfid_h

#include <MFRC522.h>
#include <vector>

class RFID {
public:
  RFID(MFRC522* t_reader);
  MFRC522* getReader();
  std::vector<byte> readTag(byte sector);
  int writeTag(byte sector, std::vector<byte> data);

private:
  MFRC522* reader;
  MFRC522::MIFARE_Key MiKey;
};

#endif