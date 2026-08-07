#include "ebus_uart_isr.h"
#include "soc/interrupts.h"  // voor ETS_UARTx_INTR_SOURCE
#include "telegram.h"        // voor de SYN-constante (dezelfde die ebus.h ook gebruikt)
#include "esp_log.h"

namespace esphome {
namespace ebus {

static int uart_intr_source_for_(uart_port_t num) {
  switch (num) {
    case UART_NUM_0: return ETS_UART0_INTR_SOURCE;
    case UART_NUM_1: return ETS_UART1_INTR_SOURCE;
    default: return ETS_UART2_INTR_SOURCE;
  }
}

void EbusUartIsr::setup(uart_port_t uart_num, uint8_t tx_pin, uint8_t rx_pin) {
  this->uart_num_ = uart_num;
  this->hw_ = UART_LL_GET_HW(uart_num);

  uart_config_t uart_config = {
      .baud_rate = 2400,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_APB,
  };

  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  portENTER_CRITICAL(&mux);

  ESP_ERROR_CHECK(uart_param_config(this->uart_num_, &uart_config));

  esp_err_t err = uart_set_pin(this->uart_num_, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  ESP_LOGI("ebus_uart_isr", "uart_set_pin(uart_num=%d, tx=%d, rx=%d) -> %s",
          (int) this->uart_num_, (int) tx_pin, (int) rx_pin, esp_err_to_name(err));
  ESP_ERROR_CHECK(err);

  ESP_ERROR_CHECK(esp_intr_alloc(uart_intr_source_for_(uart_num), ESP_INTR_FLAG_IRAM,
                                  &EbusUartIsr::isr_handler_, this, &this->isr_handle_));

  uart_ll_set_rxfifo_full_thr(this->hw_, 1);
  uart_ll_set_rx_tout(this->hw_, 2);
  uart_ll_ena_intr_mask(this->hw_, UART_INTR_RXFIFO_FULL | UART_INTR_RXFIFO_TOUT);

  portEXIT_CRITICAL(&mux);

}

void EbusUartIsr::arm_arbitration_byte(uint8_t byte_to_send) {
  this->byte_to_send_ = byte_to_send;
  this->arbitration_armed_ = true;
}

void EbusUartIsr::write_byte(uint8_t byte) { uart_ll_write_txfifo(this->hw_, &byte, 1); }

bool EbusUartIsr::pop_byte(uint8_t *out_byte) {
  if (this->ring_tail_ == this->ring_head_) return false;
  *out_byte = this->ring_buf_[this->ring_tail_];
  this->ring_tail_ = (this->ring_tail_ + 1) % RING_SIZE;
  return true;
}

void EbusUartIsr::isr_handler_(void *arg) {
  static_cast<EbusUartIsr *>(arg)->handle_isr_();
}

void IRAM_ATTR EbusUartIsr::handle_isr_() {
  uint8_t byte;
  while (uart_ll_get_rxfifo_len(this->hw_) > 0) {
    uart_ll_read_rxfifo(this->hw_, &byte, 1);
    this->ring_buf_[this->ring_head_] = byte;
    this->ring_head_ = (this->ring_head_ + 1) % RING_SIZE;
    if (this->arbitration_armed_ && byte == SYN) {
      uint8_t to_send = this->byte_to_send_;
      uart_ll_write_txfifo(this->hw_, &to_send, 1);
      this->arbitration_armed_ = false;
    }
  }
  uart_ll_clr_intsts_mask(this->hw_, UART_INTR_RXFIFO_FULL | UART_INTR_RXFIFO_TOUT);
}

}  // namespace ebus
}  // namespace esphome