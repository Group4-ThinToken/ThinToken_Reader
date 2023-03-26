#include "statuses.h"

uint8_t ST_Ready = 0x00;
uint8_t ST_WriteFlowRequested = 0xF1;
uint8_t ST_WriteFlowReady = 0x01;
uint8_t ST_WriteFlowRequestFailed = 0x10;
uint8_t ST_WriteFlowEndRequest = 0x11;
uint8_t ST_WriteTagReady = 0x21;
uint8_t ST_TagRead = 0x02;
uint8_t ST_WriteSuccess = 0x03;
uint8_t ST_WriteFailed = 0x04;