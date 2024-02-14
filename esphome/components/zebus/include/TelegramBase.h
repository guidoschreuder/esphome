#ifndef EBUS_COMMAND_H
#define EBUS_COMMAND_H

#include "ebus-enums.h"

namespace Ebus {

  const uint8_t SYN = 0xAA;
  const uint8_t ESC = 0xA9;
  const uint8_t ACK = 0x00;
  const uint8_t NACK = 0xFF;

  const uint8_t BROADCAST_ADDRESS = 0xFE;

  /* Specification says:
     1. In master and slave telegram part, standardised commands must be limited to 10 used data bytes.
     2. In master and slave telegram part, the sum of mfr.-specific telegram used data bytes must not exceed 14.
     We use 16 to be on the safe side for now.
  */
  const uint8_t MAX_DATA_LENGTH = 16;
  const uint8_t OFFSET_QQ = 0;
  const uint8_t OFFSET_ZZ = 1;
  const uint8_t OFFSET_PB = 2;
  const uint8_t OFFSET_SB = 3;
  const uint8_t OFFSET_NN = 4;
  const uint8_t OFFSET_DATA = 5;
  const uint8_t REQUEST_BUFFER_SIZE = (OFFSET_DATA + MAX_DATA_LENGTH + 1);
  const uint8_t RESPONSE_BUFFER_SIZE = (MAX_DATA_LENGTH + 2);
  const uint8_t RESPONSE_OFFSET = 1;
  const uint8_t INVALID_RESPONSE_BYTE = -1;


class TelegramBase {
public:
  TelegramBase();

  uint8_t getQQ() {
    return requestBuffer[OFFSET_QQ];
  }
  uint8_t getZZ() {
    return requestBuffer[OFFSET_ZZ];
  }
  uint8_t getPB() {
    return requestBuffer[OFFSET_PB];
  }
  uint8_t getSB() {
    return requestBuffer[OFFSET_SB];
  }
  uint8_t getNN() {
    uint8_t nn = requestBuffer[OFFSET_NN];
    if (nn >= MAX_DATA_LENGTH) {
      return 0;
    }
    return nn;
  }

  TelegramState getState();
  const char * getStateString();

  void setState(TelegramState newState);
  TelegramType getType();
  int16_t getRequestByte(uint8_t pos);
  uint8_t getRequestCRC();
  void pushReqData(uint8_t cr);
  bool isAckExpected();
  bool isResponseExpected();
  bool isFinished();

protected:
  TelegramState state;
  uint8_t requestBuffer[REQUEST_BUFFER_SIZE] = {ESC, ESC};  // initialize QQ and ZZ with ESC char to distinguish from valid master 0
  uint8_t requestBufferPos = 0;
  uint8_t requestRollingCRC = 0;
  bool waitForEscaped = false;
  void pushBuffer(uint8_t cr, uint8_t *buffer, uint8_t *pos, uint8_t *crc, int max_pos);

};

}

#endif
