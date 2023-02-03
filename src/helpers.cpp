#include <helpers.h>

String readRfidBytes(byte *buffer, byte size) {
    String d = "";
    // for (byte i = 0; i < size; ++i) {
    //     d += char(buffer[i] < 0x10 ? " 0" : " ");
    //     d += char(buffer[i]);
    // }

    return d;
}

void dumpToSerial(byte *buffer, byte size) {
  for (byte i = 0; i < size; ++i) {
    WebSerial.print(i);
    WebSerial.print(": ");
    WebSerial.println(buffer[i]);
  }
}

// void dumpToSerial(byte *buffer, byte size) {
//   for (byte i = 0; i < size; ++i) {
//     Serial.print(buffer[i] < 0x10 ? " 0" : " ");
//     Serial.print(buffer[i], HEX);
//   }
// }
