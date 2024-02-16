#include <ebus.h>

namespace Ebus {

Ebus::Ebus(ebus_config_t &config) {
  masterAddress = config.master_address;
  maxTries = config.max_tries;
  maxLockCounter = config.max_lock_counter;
}

void Ebus::set_uart_send_function(std::function<void(const char *, int16_t)> uart_send) {
  uartSend = uart_send;
}

void Ebus::set_queue_received_telegram_function(std::function<void(Telegram &telegram)> queue_received_telegram) {
  queue_received_telegram_ = queue_received_telegram;
}

void Ebus::set_deueue_command_function(std::function<bool(void *const)> dequeue_command) {
  dequeue_command_ = dequeue_command;
}

uint8_t Ebus::uart_send_char(uint8_t cr, bool esc, bool runCrc, uint8_t crc_init) {
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
  crc = Elf::crc8_calc(buffer[0], crc_init);
  if (len == 1) {
    return crc;
  }
  return Elf::crc8_calc(buffer[1], crc);
}

void Ebus::uart_send_char(uint8_t cr, bool esc) {
  uart_send_char(cr, esc, false, 0);
}

void Ebus::uart_send_remaining_request_part(SendCommand &command) {
  uart_send_char(command.getZZ());
  uart_send_char(command.getPB());
  uart_send_char(command.getSB());
  uart_send_char(command.getNN());
  // NOTE: use <= in loop, so we also get CRC
  for (int i = 0; i < command.getNN(); i++) {
    uart_send_char((uint8_t)command.getRequestByte(i));
  }
  uart_send_char(command.getCRC());
}

void Ebus::process_received_char(unsigned char receivedByte) {
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
    if (queue_received_telegram_) {
      queue_received_telegram_(receivingTelegram);
    }
    receivingTelegram = Telegram();
  }

  if (activeCommand.isFinished() && dequeue_command_) {
    SendCommand dequeued;
    if (dequeue_command_(&dequeued)) {
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
      uart_send_char(activeCommand.getQQ());
    }
    break;
  case TelegramState::waitForArbitration:
    if (receivedByte == activeCommand.getQQ()) {
      // we won arbitration
      uart_send_remaining_request_part(activeCommand);
      if (activeCommand.isAckExpected()) {
        activeCommand.setState(TelegramState::waitForCommandAck);
      } else {
        activeCommand.setState(TelegramState::endCompleted);
        lockCounter = maxLockCounter;
      }
    } else if (Elf::get_priority_class(receivedByte) == Elf::get_priority_class(activeCommand.getQQ())) {
      // eligible for round 2
      activeCommand.setState(TelegramState::waitForArbitration2nd);
    } else {
      // lost arbitration, try again later if retries left
      activeCommand.setState(activeCommand.canRetry(maxTries) ? TelegramState::waitForSend : TelegramState::endSendFailed);
    }
    break;
  case TelegramState::waitForArbitration2nd:
    if (receivedByte == SYN) {
      uart_send_char(activeCommand.getQQ());
    } else if (receivedByte == activeCommand.getQQ()) {
      // won round 2
      uart_send_remaining_request_part(activeCommand);
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
      uart_send_char(ACK);
      uart_send_char(SYN, false);
    } else {
      uart_send_char(NACK);
    }
  }

  // Handle our responses
  handle_response(receivingTelegram);

}

#ifdef UNIT_TEST
Telegram Ebus::getReceivingTelegram() {
  return receivingTelegram;
}
#endif

void Ebus::add_send_response_handler(std::function<uint8_t(Telegram &, uint8_t *)> sendResponseHandler) {
  send_response_handlers_.push_back(sendResponseHandler);
}

void Ebus::handle_response(Telegram &telegram) {
  if (telegram.getState() != TelegramState::waitForRequestAck ||
      telegram.getZZ() != Elf::to_slave(masterAddress)) {
    return;
  }
  if (!telegram.isRequestValid()) {
    uart_send_char(NACK);
    return;
  }

  // response buffer
  uint8_t buf[RESPONSE_BUFFER_SIZE] = {0};
  int len = 0;

  // find response
  for (auto const& handler : send_response_handlers_) {
    len = handler(telegram, buf);
    if (len != 0) {
      break;
    }
  }

  // we found no reponse to send
  if (len == 0) {
    uart_send_char(NACK);
    return;
  }

  uart_send_char(ACK);
  uint8_t crc = Elf::crc8_calc(len, 0);
  uart_send_char(len);
  for (int i = 0; i < len; i++) {
    crc = uart_send_char(buf[i], true, true, crc);
  }
  uart_send_char(crc);
}

unsigned char Elf::crc8_calc(unsigned char data, unsigned char crc_init) {
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

unsigned char Elf::crc8_array(unsigned char data[], unsigned int length) {
  unsigned char uc_crc;
  uc_crc = (unsigned char)0;
  for (int i = 0; i < length; i++) {
    uc_crc = crc8_calc(data[i], uc_crc);
  }
  return (uc_crc);
}

bool Elf::is_master(uint8_t address) {
  return is_master_nibble(get_priority_class(address)) &&  //
         is_master_nibble(get_sub_address(address));
}

int Elf::is_master_nibble(uint8_t nibble) {
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

uint8_t Elf::get_priority_class(uint8_t address) {
  return (address & 0x0F);
}

uint8_t Elf::get_sub_address(uint8_t address) {
  return (address >> 4);
}

uint8_t Elf::to_slave(uint8_t address) {
  if (is_master(address)) {
    return (address + 5) % 0xFF;
  }
  return address;
}

}  // namespace Ebus
