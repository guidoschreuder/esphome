#pragma once

#include <cstdint>

#ifndef USE_ESP8266
#include "driver/uart.h"
#include "hal/uart_ll.h"
#include "esp_intr_alloc.h"
#endif

#include "esp_attr.h"

namespace esphome {
namespace ebus {

class EbusUartIsr {
 public:
  void setup(uart_port_t uart_num, uint8_t tx_pin, uint8_t rx_pin);

  // door de Ebus-state machine aangeroepen tijdens arbitrage
  void arm_arbitration_byte(uint8_t byte_to_send);

  // door de task aangeroepen om binnengekomen bytes op te halen
  bool pop_byte(uint8_t *out_byte);

  // direct schrijven buiten arbitrage om (rest van het telegram, na gewonnen arbitrage)
  void write_byte(uint8_t byte);

 private:
  static void isr_handler_(void *arg);
  void handle_isr_();

  static constexpr size_t RING_SIZE = 64;
  volatile uint8_t ring_buf_[RING_SIZE];
  volatile uint8_t ring_head_ = 0;
  volatile uint8_t ring_tail_ = 0;
  volatile bool arbitration_armed_ = false;
  volatile uint8_t byte_to_send_ = 0;

  uart_port_t uart_num_;
  uart_dev_t *hw_ = nullptr;
  uart_isr_handle_t isr_handle_ = nullptr;
};

}  // namespace ebus
}  // namespace esphome