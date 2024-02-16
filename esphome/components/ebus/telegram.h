#ifndef EBUS_TELEGRAM_H
#define EBUS_TELEGRAM_H

namespace Ebus {

enum EbusState : int8_t {
  normal,
  arbitration,
};

enum TelegramType : int8_t {
  Unknown = -1,
  Broadcast = 0,
  MasterMaster = 1,
  MasterSlave = 2,
};

#define TELEGRAM_STATE_TABLE \
X(waitForSyn, 1)                    \
X(waitForSend, 2)                   \
X(waitForRequestData, 3)            \
X(waitForRequestAck, 4)             \
X(waitForResponseData, 5)           \
X(waitForResponseAck, 6)            \
X(waitForArbitration, 7)            \
X(waitForArbitration2nd, 8)         \
X(waitForCommandAck, 9)             \
X(unknown, 0)                       \
X(endErrorUnexpectedSyn, -1)        \
X(endErrorRequestNackReceived, -2)  \
X(endErrorResponseNackReceived, -3) \
X(endErrorResponseNoAck, -4)        \
X(endErrorRequestNoAck, -5)         \
X(endArbitration, -6)               \
X(endCompleted, -16)                \
X(endSendFailed, -17)

#define X(name, int) name = int,
enum TelegramState : int8_t {
  TELEGRAM_STATE_TABLE
};
#undef X

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


class Telegram : public TelegramBase {
public:
  Telegram();

  uint8_t getResponseNN() {
    uint8_t nn = responseBuffer[0];
    if (nn >= MAX_DATA_LENGTH) {
      return 0;
    }
    return nn;
  }

  int16_t getResponseByte(uint8_t pos);
  uint8_t getResponseCRC();

  void pushRespData(uint8_t cr);
  bool isResponseComplete();
  bool isResponseValid();
  bool isRequestComplete();
  bool isRequestValid();

protected:
  uint8_t responseBuffer[RESPONSE_BUFFER_SIZE] = {0};
  uint8_t responseBufferPos = 0;
  uint8_t responseRollingCRC = 0;

};

class SendCommand : public TelegramBase {
public:
  SendCommand();
  SendCommand(uint8_t QQ, uint8_t ZZ, uint8_t PB, uint8_t SB, uint8_t NN, uint8_t *data);
  bool canRetry(int8_t max_tries);
  uint8_t getCRC();

protected:
  uint8_t numTries = 0;

};


}  // namespace Ebus

#endif
