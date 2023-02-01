#include <Arduino.h>
#include <WebSerialLite.h>

String readRfidBytes(byte *buffer, byte size);
void dumpToSerial(byte *buffer, byte size);