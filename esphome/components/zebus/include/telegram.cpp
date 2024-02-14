#include "ebus.h"

namespace Ebus {

TelegramBase::TelegramBase() {
}

TelegramState TelegramBase::getState() {
  return state;
}

#define X(name, int) case int: return ""#name"";
const char * TelegramBase::getStateString() {
  switch((int8_t) state) {
    TELEGRAM_STATE_TABLE
    default:
      return "[INVALID STATE]";
  }
}
#undef X

void TelegramBase::setState(TelegramState newState) {
  state = newState;
}


void TelegramBase::pushBuffer(uint8_t cr, uint8_t *buffer, uint8_t *pos, uint8_t *crc, int max_pos) {
  if (waitForEscaped) {
    if (*pos < max_pos) {
      *crc = Elf::crc8Calc(cr, *crc);
    }
    buffer[(*pos)] = (cr == 0x0 ? ESC : SYN);
    waitForEscaped = false;
  } else {
    if (*pos < max_pos) {
      *crc = Elf::crc8Calc(cr, *crc);
    }
    buffer[(*pos)++] = cr;
    waitForEscaped = (cr == ESC);
  }
}

TelegramType TelegramBase::getType() {
  if (getZZ() == ESC) {
    return TelegramType::Unknown;
  }
  if (getZZ() == BROADCAST_ADDRESS) {
    return TelegramType::Broadcast;
  }
  if (Elf::isMaster(getZZ())) {
    return TelegramType::MasterMaster;
  }
  return TelegramType::MasterSlave;
}

int16_t TelegramBase::getRequestByte(uint8_t pos) {
  if (pos > getNN() || pos >= MAX_DATA_LENGTH) {
    return -1;
  }
  return requestBuffer[OFFSET_DATA + pos];
}

uint8_t TelegramBase::getRequestCRC() {
  return requestBuffer[OFFSET_DATA + getNN()];
}

void TelegramBase::pushReqData(uint8_t cr) {
  pushBuffer(cr, requestBuffer, &requestBufferPos, &requestRollingCRC, OFFSET_DATA + getNN());
}

bool TelegramBase::isAckExpected() {
  return (getType() != TelegramType::Broadcast);
}

bool TelegramBase::isResponseExpected() {
  return (getType() == TelegramType::MasterSlave);
}

bool TelegramBase::isFinished() {
  return state < TelegramState::unknown;
}


Telegram::Telegram() {
  state = TelegramState::waitForSyn;
}

int16_t Telegram::getResponseByte(uint8_t pos) {
  if (pos > getResponseNN() || pos >= MAX_DATA_LENGTH) {
    return INVALID_RESPONSE_BYTE;
  }
  return responseBuffer[RESPONSE_OFFSET + pos];
}

uint8_t Telegram::getResponseCRC() {
  return responseBuffer[RESPONSE_OFFSET + getResponseNN()];
}

void Telegram::pushRespData(uint8_t cr) {
  pushBuffer(cr, responseBuffer, &responseBufferPos, &responseRollingCRC, RESPONSE_OFFSET + getResponseNN());
}

bool Telegram::isResponseComplete() {
  return (state > TelegramState::waitForSyn || state == TelegramState::endCompleted) &&
         (responseBufferPos > RESPONSE_OFFSET) &&
         (responseBufferPos == (RESPONSE_OFFSET + getResponseNN() + 1)) &&
         !waitForEscaped;
}

bool Telegram::isResponseValid() {
  return isResponseComplete() && getResponseCRC() == responseRollingCRC;
}

bool Telegram::isRequestComplete() {
  return (state > TelegramState::waitForSyn || state == TelegramState::endCompleted) &&
         (requestBufferPos > OFFSET_DATA) &&
         (requestBufferPos == (OFFSET_DATA + getNN() + 1)) && !waitForEscaped;
}
bool Telegram::isRequestValid() {
  return isRequestComplete() && getRequestCRC() == requestRollingCRC;
}


SendCommand::SendCommand() {
  state = TelegramState::endCompleted;
}

SendCommand::SendCommand(uint8_t QQ, uint8_t ZZ, uint8_t PB, uint8_t SB, uint8_t NN, uint8_t *data) {
  state = TelegramState::waitForSend;
  pushReqData(QQ);
  pushReqData(ZZ);
  pushReqData(PB);
  pushReqData(SB);
  pushReqData(NN);
  for (int i = 0; i < NN; i++) {
    pushReqData(data[i]);
  }
  pushReqData(requestRollingCRC);
}

bool SendCommand::canRetry(int8_t max_tries) {
  return numTries++ < max_tries;
}

uint8_t SendCommand::getCRC() {
  return requestRollingCRC;
}

}  // namespace Ebus
