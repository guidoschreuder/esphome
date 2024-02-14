#ifndef EBUS_H
#define EBUS_H

#include <stdint.h>
#include <functional>
#include <list>

#include "Telegram.h"
#include "ebus-enums.h"

typedef struct {
  uint8_t master_address;
  uint8_t max_tries;
  uint8_t max_lock_counter;
} ebus_config_t;


namespace Ebus {

class Elf {
public:
  static unsigned char crc8Calc(unsigned char data, unsigned char crc_init);
  static unsigned char crc8Array(unsigned char data[], unsigned int length);
  static bool isMaster(uint8_t address);
  static int isMasterNibble(uint8_t nibble);
  static uint8_t getPriorityClass(uint8_t address);
  static uint8_t getSubAddress(uint8_t address);
  static uint8_t toSlave(uint8_t address);
};


class Ebus {
public:
  explicit Ebus(ebus_config_t &config);
  void setUartSendFunction(std::function<void(const char *, int16_t)> uart_send);
  //void setQueueReceivedTelegramFunction(void (*queue_received_telegram)(Telegram &telegram));
  void setQueueReceivedTelegramFunction(std::function<void(Telegram &telegram)> queue_received_telegram);
  //void setDeueueCommandFunction(bool (*dequeue_command)(void *const command));
  void setDeueueCommandFunction(std::function<bool(void *const)> dequeue_command);
  void processReceivedChar(unsigned char receivedByte);
  //void addSendResponseHandler(send_response_handler);
  void addSendResponseHandler(std::function<uint8_t(Telegram &, uint8_t *)>);


protected:
  uint8_t masterAddress;
  uint8_t maxTries;
  uint8_t maxLockCounter;
  uint8_t lockCounter = 0;
  uint8_t charCountSinceLastSyn = 0;
  EbusState state = EbusState::arbitration;
  Telegram receivingTelegram;
  SendCommand activeCommand;
  //std::list<send_response_handler> sendResponseHandlers;
  std::list<std::function<uint8_t(Telegram &, uint8_t *)>> sendResponseHandlers;

  std::function<void(const char *, int16_t)> uartSend;
  std::function<void(Telegram &)> queueReceivedTelegram;
  std::function<bool(void *const&)> dequeueCommand;
  uint8_t uartSendChar(uint8_t cr, bool esc, bool runCrc, uint8_t crc_init);
  void uartSendChar(uint8_t cr, bool esc = true);
  void uartSendRemainingRequestPart(SendCommand &command);
  void handleResponse(Telegram &telegram);

#ifdef UNIT_TEST
  Telegram getReceivingTelegram();
#endif
};

}  // namespace Ebus

#endif
