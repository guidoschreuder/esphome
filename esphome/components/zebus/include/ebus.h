#ifndef EBUS_H
#define EBUS_H

#include <stdint.h>
#include <functional>
#include <list>

#include "telegram.h"
#include "ebus-enums.h"

typedef struct {
  uint8_t master_address;
  uint8_t max_tries;
  uint8_t max_lock_counter;
} ebus_config_t;


namespace Ebus {

class Elf {
public:
  static unsigned char crc8_calc(unsigned char data, unsigned char crc_init);
  static unsigned char crc8_array(unsigned char data[], unsigned int length);
  static bool is_master(uint8_t address);
  static int is_master_nibble(uint8_t nibble);
  static uint8_t get_priority_class(uint8_t address);
  static uint8_t get_sub_address(uint8_t address);
  static uint8_t to_slave(uint8_t address);
};


class Ebus {
public:
  explicit Ebus(ebus_config_t &config);
  void set_uart_send_function(std::function<void(const char *, int16_t)> uart_send);
  //void set_queue_received_telegram_function(void (*queue_received_telegram)(Telegram &telegram));
  void set_queue_received_telegram_function(std::function<void(Telegram &telegram)> queue_received_telegram);
  //void set_deueue_command_function(bool (*dequeue_command)(void *const command));
  void set_deueue_command_function(std::function<bool(void *const)> dequeue_command);
  void process_received_char(unsigned char receivedByte);
  //void add_send_response_handler(send_response_handler);
  void add_send_response_handler(std::function<uint8_t(Telegram &, uint8_t *)>);


protected:
  uint8_t masterAddress;
  uint8_t maxTries;
  uint8_t maxLockCounter;
  uint8_t lockCounter = 0;
  uint8_t charCountSinceLastSyn = 0;
  EbusState state = EbusState::arbitration;
  Telegram receivingTelegram;
  SendCommand activeCommand;
  //std::list<send_response_handler> send_response_handlers_;
  std::list<std::function<uint8_t(Telegram &, uint8_t *)>> send_response_handlers_;

  std::function<void(const char *, int16_t)> uartSend;
  std::function<void(Telegram &)> queue_received_telegram_;
  std::function<bool(void *const&)> dequeue_command_;
  uint8_t uart_send_char(uint8_t cr, bool esc, bool runCrc, uint8_t crc_init);
  void uart_send_char(uint8_t cr, bool esc = true);
  void uart_send_remaining_request_part(SendCommand &command);
  void handle_response(Telegram &telegram);

#ifdef UNIT_TEST
  Telegram getReceivingTelegram();
#endif
};

}  // namespace Ebus

#endif
