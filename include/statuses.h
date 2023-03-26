#ifndef statuses_h
#define statuses_h

#include <stdint.h>

extern uint8_t ST_Ready;
extern uint8_t ST_WriteFlowRequested;
extern uint8_t ST_WriteFlowReady;
extern uint8_t ST_WriteFlowRequestFailed;
extern uint8_t ST_WriteFlowEndRequest;
extern uint8_t ST_WriteTagReady;
extern uint8_t ST_TagRead;
extern uint8_t ST_WriteSuccess;
extern uint8_t ST_WriteFailed;

#endif