#pragma once

#include "ebus.h"

#include "esphome.h"
#include "esphome/core/component.h"

#include <driver/uart.h>
#include <soc/uart_reg.h>

namespace esphome {
namespace ebus {

class EbusComponent : public Component {
public:
  EbusComponent() {
  }

  void dump_config() override;
  void setup() override;

  void set_master_address(uint8_t);
  void set_max_tries(uint8_t);
  void set_max_lock_counter(uint8_t);
  void set_uart_num(uint8_t);
  void set_uart_tx_pin(uint8_t);
  void set_uart_rx_pin(uint8_t);
  void set_history_queue_size(uint8_t);
  void set_command_queue_size(uint8_t);

protected:
  uint8_t master_address_;
  uint8_t max_tries_;
  uint8_t max_lock_counter_;
  uint8_t history_queue_size_;
  uint8_t command_queue_size_;
  uint8_t uart_num_;
  uint8_t uart_tx_pin_;
  uint8_t uart_rx_pin_;

  QueueHandle_t history_queue_;
  QueueHandle_t command_queue_;

  std::list<std::function<void(Ebus::Telegram &telegram)>> message_handlers;

  Ebus::Ebus* ebus;

  void setup_queues();
  void setup_ebus();
  void setup_uart();
  void setup_tasks();

  static void process_received_bytes(void *);
  static void process_received_messages(void *);
  void handle_message(Ebus::Telegram &);

};

} // namespace ebus
} // namespace esphome
