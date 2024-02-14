#include <ebus.h>

namespace Ebus {

Ebus::Ebus(ebus_config_t &config) {
  masterAddress = config.master_address;
  maxTries = config.max_tries;
  maxLockCounter = config.max_lock_counter;
}

void Ebus::setUartSendFunction(std::function<void(const char *, int16_t)> uart_send) {
  uartSend = uart_send;
}

void Ebus::setQueueReceivedTelegramFunction(std::function<void(Telegram &telegram)> queue_received_telegram) {
  queueReceivedTelegram = queue_received_telegram;
}

void Ebus::setDeueueCommandFunction(std::function<bool(void *const)> dequeue_command) {
  dequeueCommand = dequeue_command;
}

uint8_t Ebus::uartSendChar(uint8_t cr, bool esc, bool runCrc, uint8_t crc_init) {
  char buffer[2];
  uint8_t crc = 0;
  uint8_t len = 1;
  if (esc && cr == ESC) {
    buffer[0] = ESC;
    buffer[1] = 0x00;
    len = 2;
  } else if (esc && cr == SYN) {
    buffer[0] = ESC;
    buffer[1] = 0x01;
    len = 2;
  } else {
    buffer[0] = cr;
  }
  uartSend(buffer, len);
  if (!runCrc) {
    return 0;
  }
  crc = Elf::crc8Calc(buffer[0], crc_init);
  if (len == 1) {
    return crc;
  }
  return Elf::crc8Calc(buffer[1], crc);
}

void Ebus::uartSendChar(uint8_t cr, bool esc) {
  uartSendChar(cr, esc, false, 0);
}

void Ebus::uartSendRemainingRequestPart(SendCommand &command) {
  uartSendChar(command.getZZ());
  uartSendChar(command.getPB());
  uartSendChar(command.getSB());
  uartSendChar(command.getNN());
  // NOTE: use <= in loop, so we also get CRC
  for (int i = 0; i < command.getNN(); i++) {
    uartSendChar((uint8_t)command.getRequestByte(i));
  }
  uartSendChar(command.getCRC());
}

void Ebus::processReceivedChar(unsigned char receivedByte) {
  // keep track of number of character between last 2 SYN chars
  // this is needed in case of arbitration
  if (receivedByte == SYN) {
    state = charCountSinceLastSyn == 1 ? EbusState::arbitration : EbusState::normal;
    charCountSinceLastSyn = 0;

    if (lockCounter > 0 && state == EbusState::normal) {
      lockCounter--;
    }

  } else {
    charCountSinceLastSyn++;
  }

  if (receivingTelegram.isFinished()) {
    if (queueReceivedTelegram) {
      queueReceivedTelegram(receivingTelegram);
    }
    receivingTelegram = Telegram();
  }

  if (activeCommand.isFinished() && dequeueCommand) {
    SendCommand dequeued;
    if (dequeueCommand(&dequeued)) {
      activeCommand = dequeued;
    }
  }

  switch (receivingTelegram.getState()) {
  case TelegramState::waitForSyn:
    if (receivedByte == SYN) {
      receivingTelegram.setState(TelegramState::waitForArbitration);
    }
    break;
  case TelegramState::waitForArbitration:
    if (receivedByte != SYN) {
      receivingTelegram.pushReqData(receivedByte);
      receivingTelegram.setState(TelegramState::waitForRequestData);
    }
    break;
  case TelegramState::waitForRequestData:
    if (receivedByte == SYN) {
      if (receivingTelegram.getZZ() == ESC) {
        receivingTelegram.setState(TelegramState::endArbitration);
      } else {
        receivingTelegram.setState(TelegramState::endErrorUnexpectedSyn);
      }
    } else {
      receivingTelegram.pushReqData(receivedByte);
      if (receivingTelegram.isRequestComplete()) {
        receivingTelegram.setState(receivingTelegram.isAckExpected() ? TelegramState::waitForRequestAck : TelegramState::endCompleted);
      }
    }
    break;
  case TelegramState::waitForRequestAck:
    switch (receivedByte) {
    case ACK:
      receivingTelegram.setState(receivingTelegram.isResponseExpected() ? TelegramState::waitForResponseData : TelegramState::endCompleted);
      break;
    case NACK:
      receivingTelegram.setState(TelegramState::endErrorRequestNackReceived);
      break;
    default:
      receivingTelegram.setState(TelegramState::endErrorRequestNoAck);
    }
    break;
  case TelegramState::waitForResponseData:
    if (receivedByte == SYN) {
      receivingTelegram.setState(TelegramState::endErrorUnexpectedSyn);
    } else {
      receivingTelegram.pushRespData(receivedByte);
      if (receivingTelegram.isResponseComplete()) {
        receivingTelegram.setState(TelegramState::waitForResponseAck);
      }
    }
    break;
  case TelegramState::waitForResponseAck:
    switch (receivedByte) {
    case ACK:
      receivingTelegram.setState(TelegramState::endCompleted);
      break;
    case NACK:
      receivingTelegram.setState(TelegramState::endErrorResponseNackReceived);
      break;
    default:
      receivingTelegram.setState(TelegramState::endErrorResponseNoAck);
    }
    break;
  default:
    break;
  }

  switch (activeCommand.getState()) {
  case TelegramState::waitForSend:
    if (receivedByte == SYN && state == EbusState::normal && lockCounter == 0) {
      activeCommand.setState(TelegramState::waitForArbitration);
      uartSendChar(activeCommand.getQQ());
    }
    break;
  case TelegramState::waitForArbitration:
    if (receivedByte == activeCommand.getQQ()) {
      // we won arbitration
      uartSendRemainingRequestPart(activeCommand);
      if (activeCommand.isAckExpected()) {
        activeCommand.setState(TelegramState::waitForCommandAck);
      } else {
        activeCommand.setState(TelegramState::endCompleted);
        lockCounter = maxLockCounter;
      }
    } else if (Elf::getPriorityClass(receivedByte) == Elf::getPriorityClass(activeCommand.getQQ())) {
      // eligible for round 2
      activeCommand.setState(TelegramState::waitForArbitration2nd);
    } else {
      // lost arbitration, try again later if retries left
      activeCommand.setState(activeCommand.canRetry(maxTries) ? TelegramState::waitForSend : TelegramState::endSendFailed);
    }
    break;
  case TelegramState::waitForArbitration2nd:
    if (receivedByte == SYN) {
      uartSendChar(activeCommand.getQQ());
    } else if (receivedByte == activeCommand.getQQ()) {
      // won round 2
      uartSendRemainingRequestPart(activeCommand);
      if (activeCommand.isAckExpected()) {
        activeCommand.setState(TelegramState::waitForCommandAck);
      } else {
        activeCommand.setState(TelegramState::endCompleted);
        lockCounter = maxLockCounter;
      }
    } else {
      // try again later if retries left
      activeCommand.setState(activeCommand.canRetry(maxTries) ? TelegramState::waitForSend : TelegramState::endSendFailed);
    }
    break;
  case TelegramState::waitForCommandAck:
    if (receivedByte == ACK) {
      activeCommand.setState(TelegramState::endCompleted);
      lockCounter = maxLockCounter;
    } else if (receivedByte == SYN) { // timeout waiting for ACK signaled by AUTO-SYN
      activeCommand.setState(activeCommand.canRetry(maxTries) ? TelegramState::waitForSend : TelegramState::endSendFailed);
    }
    break;
  default:
    break;
  }

  // responses to our commands are stored in receivingTelegram
  // when response is completed send ACK or NACK when we were the master
  if (receivingTelegram.getState() == TelegramState::waitForResponseAck &&
      receivingTelegram.getQQ() == masterAddress) {
    if (receivingTelegram.isResponseValid()) {
      uartSendChar(ACK);
      uartSendChar(SYN, false);
    } else {
      uartSendChar(NACK);
    }
  }

  // Handle our responses
  handleResponse(receivingTelegram);

}

#ifdef UNIT_TEST
Telegram Ebus::getReceivingTelegram() {
  return receivingTelegram;
}
#endif

void Ebus::addSendResponseHandler(std::function<uint8_t(Telegram &, uint8_t *)> sendResponseHandler) {
  sendResponseHandlers.push_back(sendResponseHandler);
}

void Ebus::handleResponse(Telegram &telegram) {
  if (telegram.getState() != TelegramState::waitForRequestAck ||
      telegram.getZZ() != Elf::toSlave(masterAddress)) {
    return;
  }
  if (!telegram.isRequestValid()) {
    uartSendChar(NACK);
    return;
  }

  // response buffer
  uint8_t buf[RESPONSE_BUFFER_SIZE] = {0};
  int len = 0;

  // find response
  for (auto const& handler : sendResponseHandlers) {
    len = handler(telegram, buf);
    if (len != 0) {
      break;
    }
  }

  // we found no reponse to send
  if (len == 0) {
    uartSendChar(NACK);
    return;
  }

  uartSendChar(ACK);
  uint8_t crc = Elf::crc8Calc(len, 0);
  uartSendChar(len);
  for (int i = 0; i < len; i++) {
    crc = uartSendChar(buf[i], true, true, crc);
  }
  uartSendChar(crc);
}

unsigned char Elf::crc8Calc(unsigned char data, unsigned char crc_init) {
  unsigned char crc;
  unsigned char polynom;

  crc = crc_init;
  for (int i = 0; i < 8; i++) {
    if (crc & 0x80) {
      polynom = (unsigned char)0x9B;
    } else {
      polynom = (unsigned char)0;
    }
    crc = (unsigned char)((crc & ~0x80) << 1);
    if (data & 0x80) {
      crc = (unsigned char)(crc | 1);
    }
    crc = (unsigned char)(crc ^ polynom);
    data = (unsigned char)(data << 1);
  }
  return (crc);
}

unsigned char Elf::crc8Array(unsigned char data[], unsigned int length) {
  unsigned char uc_crc;
  uc_crc = (unsigned char)0;
  for (int i = 0; i < length; i++) {
    uc_crc = crc8Calc(data[i], uc_crc);
  }
  return (uc_crc);
}

bool Elf::isMaster(uint8_t address) {
  return isMasterNibble(getPriorityClass(address)) &&  //
         isMasterNibble(getSubAddress(address));
}

int Elf::isMasterNibble(uint8_t nibble) {
  switch (nibble) {
  case 0b0000:
  case 0b0001:
  case 0b0011:
  case 0b0111:
  case 0b1111:
    return true;
  default:
    return false;
  }
}

uint8_t Elf::getPriorityClass(uint8_t address) {
  return (address & 0x0F);
}

uint8_t Elf::getSubAddress(uint8_t address) {
  return (address >> 4);
}

uint8_t Elf::toSlave(uint8_t address) {
  if (isMaster(address)) {
    return (address + 5) % 0xFF;
  }
  return address;
}

}  // namespace Ebus