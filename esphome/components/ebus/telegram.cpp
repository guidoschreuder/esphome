#include "ebus.h"

namespace Ebus {

TelegramBase::TelegramBase() {
}

void TelegramBase::setState(TelegramState new_state) {
  this->state = new_state;
}

TelegramState TelegramBase::getState() {
  return this->state;
}

#define X(name, int) case int: return ""#name"";
const char * TelegramBase::getStateString() {
  switch((int8_t) this->state) {
    TELEGRAM_STATE_TABLE
    default:
      return "[INVALID STATE]";
  }
}
#undef X


void TelegramBase::push_buffer(uint8_t cr, uint8_t *buffer, uint8_t *pos, uint8_t *crc, int max_pos) {
  if (this->wait_for_escaped_char_) {
    if (*pos < max_pos) {
      *crc = Elf::crc8_calc(cr, *crc);
    }
    buffer[(*pos)] = (cr == 0x0 ? ESC : SYN);
    this->wait_for_escaped_char_ = false;
  } else {
    if (*pos < max_pos) {
      *crc = Elf::crc8_calc(cr, *crc);
    }
    buffer[(*pos)++] = cr;
    this->wait_for_escaped_char_ = (cr == ESC);
  }
}

TelegramType TelegramBase::getType() {
  if (this->getZZ() == ESC) {
    return TelegramType::Unknown;
  }
  if (this->getZZ() == BROADCAST_ADDRESS) {
    return TelegramType::Broadcast;
  }
  if (Elf::is_master(this->getZZ())) {
    return TelegramType::MasterMaster;
  }
  return TelegramType::MasterSlave;
}

int16_t TelegramBase::get_request_byte(uint8_t pos) {
  if (pos > this->getNN() || pos >= MAX_DATA_LENGTH) {
    return -1;
  }
  return this->requestBuffer[OFFSET_DATA + pos];
}

uint8_t TelegramBase::get_request_crc() {
  return this->requestBuffer[OFFSET_DATA + this->getNN()];
}

void TelegramBase::push_req_data(uint8_t cr) {
  this->push_buffer(cr, requestBuffer, &requestBufferPos, &requestRollingCRC, OFFSET_DATA + getNN());
}

bool TelegramBase::isAckExpected() {
  return (this->getType() != TelegramType::Broadcast);
}

bool TelegramBase::isResponseExpected() {
  return (this->getType() == TelegramType::MasterSlave);
}

bool TelegramBase::isFinished() {
  return this->state < TelegramState::unknown;
}


Telegram::Telegram() {
  this->state = TelegramState::waitForSyn;
}

int16_t Telegram::getResponseByte(uint8_t pos) {
  if (pos > this->getResponseNN() || pos >= MAX_DATA_LENGTH) {
    return INVALID_RESPONSE_BYTE;
  }
  return this->responseBuffer[RESPONSE_OFFSET + pos];
}

uint8_t Telegram::getResponseCRC() {
  return this->responseBuffer[RESPONSE_OFFSET + this->getResponseNN()];
}

void Telegram::pushRespData(uint8_t cr) {
  this->push_buffer(cr, responseBuffer, &responseBufferPos, &responseRollingCRC, RESPONSE_OFFSET + getResponseNN());
}

bool Telegram::isResponseComplete() {
  return (this->state > TelegramState::waitForSyn || this->state == TelegramState::endCompleted) &&
         (this->responseBufferPos > RESPONSE_OFFSET) &&
         (this->responseBufferPos == (RESPONSE_OFFSET + this->getResponseNN() + 1)) &&
         !this->wait_for_escaped_char_;
}

bool Telegram::isResponseValid() {
  return this->isResponseComplete() && this->getResponseCRC() == responseRollingCRC;
}

bool Telegram::is_request_complete() {
  return (this->state > TelegramState::waitForSyn || this->state == TelegramState::endCompleted) &&
         (this->requestBufferPos > OFFSET_DATA) &&
         (this->requestBufferPos == (OFFSET_DATA + this->getNN() + 1)) && !this->wait_for_escaped_char_;
}
bool Telegram::is_request_valid() {
  return this->is_request_complete() && this->get_request_crc() == this->requestRollingCRC;
}


SendCommand::SendCommand() {
  this->state = TelegramState::endCompleted;
}

SendCommand::SendCommand(uint8_t QQ, uint8_t ZZ, uint8_t PB, uint8_t SB, uint8_t NN, uint8_t *data) {
  this->state = TelegramState::waitForSend;
  this->push_req_data(QQ);
  this->push_req_data(ZZ);
  this->push_req_data(PB);
  this->push_req_data(SB);
  this->push_req_data(NN);
  for (int i = 0; i < NN; i++) {
    this->push_req_data(data[i]);
  }
  this->push_req_data(this->requestRollingCRC);
}

bool SendCommand::can_retry(int8_t max_tries) {
  return this->tries_count_++ < max_tries;
}

uint8_t SendCommand::get_crc() {
  return this->requestRollingCRC;
}

}  // namespace Ebus
