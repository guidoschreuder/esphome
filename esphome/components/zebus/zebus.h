#pragma once

#include "esphome.h"
#include "ebus.h"

#include <driver/uart.h>
#include <soc/uart_reg.h>

namespace esphome {
namespace ebus {


#define BYTES_TO_WORD(HIGH_BYTE, LOW_BYTE) ((((uint16_t)HIGH_BYTE) << 8) | LOW_BYTE)
#define GET_BYTE(CMD, I) ((uint8_t) ((CMD >> 8 * I) & 0XFF))

  const uint8_t MASTER_HEATER = 0x03;
  const uint8_t MASTER_EXA = 0x10;

  const uint16_t CMD_IDENTIFICATION = 0x0704;
  const uint16_t CMD_DEVICE_CONFIG  = 0xB509;

  const uint8_t DEVICE_CONFIG_SUBCOMMAND_READ = 0x0D;

  const uint16_t DEVICE_CONFIG_WATER_PRESSURE = 0x0200;
  const uint16_t DEVICE_CONFIG_DISP_ROOM_TEMP = 0x3E00;


typedef struct {
  uint32_t interval_ms;
  struct uart {
    uint8_t num;
    uint8_t pin_rx;
    uint8_t pin_tx;
  } uart;
  struct queues {
    uint8_t history_size;
    uint8_t command_size;
  } queues;
  struct ebus {
    ebus_config_t ebus_config;
    struct identification {
      const char* device_name;
      uint8_t     vendor_id;
      uint16_t    software_version;
      uint16_t    hardware_version;
    } identification;
  } ebus;
} zebus_config_t;


class Zebus : public PollingComponent, public Sensor {

  zebus_config_t zebus_config;

public:

  Zebus(zebus_config_t zebus_config) : PollingComponent(), zebus_config(zebus_config) {
  }

  float get_setup_priority() const override {
    return esphome::setup_priority::LATE;
  }

  Sensor *water_pressure = new Sensor();
  Sensor *flow_temp = new Sensor();
  Sensor *return_temp = new Sensor();
  Sensor *storage_temp = new Sensor();
  Sensor *hot_water_heating = new Sensor();
  Sensor *disp_room_temp = new Sensor();


  void setup() override {

    set_update_interval(zebus_config.interval_ms);

    setup_queues();
    setup_ebus();
    setup_uart();

    xTaskCreate(&ebus_process_received_bytes, "ebus_process_received_bytes", 2048, (void*) this, 10, NULL);
    xTaskCreate(&ebus_process_received_messages, "ebus_process_received_messages", 2560, (void*) this, 5, NULL);


    add_message_handler( [&](Ebus::Telegram &telegram) {
      uint16_t command = BYTES_TO_WORD(telegram.getPB(), telegram.getSB());
      if (command != CMD_DEVICE_CONFIG) {
        return;
      }
      if (telegram.getRequestByte(0) != DEVICE_CONFIG_SUBCOMMAND_READ) {
        return;
      }
      uint16_t configReadCommand = BYTES_TO_WORD(telegram.getRequestByte(1), telegram.getRequestByte(2));
      if (configReadCommand != DEVICE_CONFIG_WATER_PRESSURE) {
        return;
      }

      water_pressure->publish_state(to_float(telegram, 0, 2, 1000.0));

    } );

    add_message_handler( [&](Ebus::Telegram &telegram) {
      uint16_t command = BYTES_TO_WORD(telegram.getPB(), telegram.getSB());
      if (command != CMD_DEVICE_CONFIG) {
        return;
      }
      if (telegram.getRequestByte(0) != DEVICE_CONFIG_SUBCOMMAND_READ) {
        return;
      }
      uint16_t configReadCommand = BYTES_TO_WORD(telegram.getRequestByte(1), telegram.getRequestByte(2));
      if (configReadCommand != DEVICE_CONFIG_DISP_ROOM_TEMP) {
        return;
      }

      disp_room_temp->publish_state(to_float(telegram, 0, 2, 16.0));

    } );

    add_message_handler( [&](Ebus::Telegram &telegram) {
      uint16_t command = BYTES_TO_WORD(telegram.getPB(), telegram.getSB());
      if (command != 0xB511) {
        return;
      }
      if (telegram.getRequestByte(0) != 0x01) {
        return;
      }

      flow_temp->publish_state(to_float(telegram, 0, 1, 2.0));
      return_temp->publish_state(to_float(telegram, 1, 1, 2.0));
      storage_temp->publish_state(to_float(telegram, 5, 1, 2.0));
      hot_water_heating->publish_state(telegram.getResponseByte(6) && 0x01);
    } );


  }

  void update() override {

    ebus_enqueue_command(createReadConfigCommand(MASTER_HEATER, DEVICE_CONFIG_WATER_PRESSURE));
    ebus_enqueue_command(createReadConfigCommand(MASTER_EXA, DEVICE_CONFIG_DISP_ROOM_TEMP));

  }

  static void ebus_process_received_bytes(void *pvParameter) {
    Zebus* instance = static_cast<Zebus*>(pvParameter);

    while (1) {
      uint8_t receivedByte;
      int len = uart_read_bytes(instance->zebus_config.uart.num, &receivedByte, 1, 20 / portTICK_PERIOD_MS);
      if (len) {
        instance->ebus->process_received_char(receivedByte);
        taskYIELD();
      }
    }
  }

  static void ebus_process_received_messages(void *pvParameter) {
    Zebus* instance = static_cast<Zebus*>(pvParameter);

    Ebus::Telegram telegram;
    while (1) {
      if (xQueueReceive(instance->telegramHistoryQueue, &telegram, pdMS_TO_TICKS(1000))) {
        instance->handleMessage(telegram);
        // TODO: this comment is kept as reference on how to debug stack overflows. Could be generalized.
        // ESP_LOGD(ZEBUS_LOG_TAG, "Task: %s, Stack Highwater Mark: %d", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL));
        taskYIELD();
      }
    }
  }

  void ebus_enqueue_command(const Ebus::SendCommand &command) {
    xQueueSendToBack(telegramCommandQueue, &command, portMAX_DELAY);
  }

  Ebus::SendCommand createReadConfigCommand(uint8_t target, unsigned short config_element) {
    uint8_t data[] = { DEVICE_CONFIG_SUBCOMMAND_READ, GET_BYTE(config_element, 1), GET_BYTE(config_element, 0)};
    return Ebus::SendCommand(  //
        zebus_config.ebus.ebus_config.master_address,
        Ebus::Elf::to_slave(target),
        GET_BYTE(CMD_DEVICE_CONFIG, 1),
        GET_BYTE(CMD_DEVICE_CONFIG, 0),
        3,
        data);
  }

  void add_message_handler(std::function<void(Ebus::Telegram &)> message_handler) {
    message_handlers.push_back(message_handler);
  }

  void handleMessage(Ebus::Telegram &telegram) {
    if (telegram.getState() != Ebus::TelegramState::endCompleted) {
      ESP_LOGD("Zebus", "Message received with invalid state: %s, QQ:%02X, ZZ:%02X, Command:%02X%02X",
               telegram.getStateString(),
               telegram.getQQ(),
               telegram.getZZ(),
               telegram.getPB(),
               telegram.getSB());
      return;
    }

    for (auto const& message_handler : message_handlers) {
      message_handler(telegram);
    }
  }

  static uint32_t get_response_bytes(Ebus::Telegram &telegram, uint8_t start, uint8_t length) {
    uint32_t result = 0;
    for (uint8_t i = 0; i < 4 && i < length; i++) {
      result = result | (telegram.getResponseByte(start + i) << (i * 8));
    }
    return result;
  }

  static float to_float(Ebus::Telegram &telegram, uint8_t start, uint8_t length, float divider) {
    return get_response_bytes(telegram, start, length) / divider;
  }

protected:

  Ebus::Ebus* ebus;

  QueueHandle_t telegramHistoryQueue;
  QueueHandle_t telegramCommandQueue;

  std::list<std::function<void(Ebus::Telegram &telegram)>> message_handlers;

  void setup_queues() {
    telegramHistoryQueue = xQueueCreate(zebus_config.queues.history_size, sizeof(Ebus::Telegram));
    telegramCommandQueue = xQueueCreate(zebus_config.queues.command_size, sizeof(Ebus::Telegram));
  }

  void setup_ebus() {
    ebus = new Ebus::Ebus(zebus_config.ebus.ebus_config);
    ebus->set_uart_send_function( [&](const char * buffer, int16_t length) { return uart_write_bytes(zebus_config.uart.num, buffer, length); } );

    //ebus->set_queue_received_telegram_function(esphome::zebus::ebus_queue_telegram);
    ebus->set_queue_received_telegram_function( [&](Ebus::Telegram &telegram) {
      BaseType_t xHigherPriorityTaskWoken;
      xHigherPriorityTaskWoken = pdFALSE;
      xQueueSendToBackFromISR(telegramHistoryQueue, &telegram, &xHigherPriorityTaskWoken);
      if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
      }
    } );

    //ebus->set_deueue_command_function(esphome::zebus::ebus_dequeue_command);
    ebus->set_deueue_command_function( [&](void *const command) {
      BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      if (xQueueReceiveFromISR(telegramCommandQueue, command, &xHigherPriorityTaskWoken)) {
        uint8_t queue_size = (uint8_t)uxQueueMessagesWaiting(telegramCommandQueue);
        if (xHigherPriorityTaskWoken) {
          portYIELD_FROM_ISR();
        }
        return true;
      }
      return false;
    } );

    ebus->add_send_response_handler( [&](Ebus::Telegram &telegram, uint8_t *buffer) {
      if (BYTES_TO_WORD(telegram.getPB(), telegram.getSB()) != CMD_IDENTIFICATION) {
        return 0;
      }
      int response_size = 10;
      uint8_t fixedIdentificationResponse[response_size] = {0x00};
      fixedIdentificationResponse[0] = zebus_config.ebus.identification.vendor_id;
      for (uint8_t i = 0; i < 5; i++) {
        fixedIdentificationResponse[i + 1] = strlen(zebus_config.ebus.identification.device_name) > i ? zebus_config.ebus.identification.device_name[i] : '-';
      }
      fixedIdentificationResponse[6] = GET_BYTE(zebus_config.ebus.identification.software_version, 1);
      fixedIdentificationResponse[7] = GET_BYTE(zebus_config.ebus.identification.software_version, 0);
      fixedIdentificationResponse[8] = GET_BYTE(zebus_config.ebus.identification.hardware_version, 1);
      fixedIdentificationResponse[9] = GET_BYTE(zebus_config.ebus.identification.hardware_version, 0);

      memcpy(buffer, fixedIdentificationResponse, response_size);
      return response_size;
    } );


  }

  void setup_uart() {
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&mux);

    uart_config_t uart_config = {
      .baud_rate = 2400,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 2,
      .use_ref_tick = true,
    };

    ESP_ERROR_CHECK(uart_param_config(zebus_config.uart.num, &uart_config));

    ESP_ERROR_CHECK(
        uart_set_pin(
            zebus_config.uart.num,
            zebus_config.uart.pin_tx,
            zebus_config.uart.pin_rx,
            UART_PIN_NO_CHANGE,
            UART_PIN_NO_CHANGE));

    ESP_ERROR_CHECK(uart_driver_install(zebus_config.uart.num, 256, 0, 0, NULL, 0));

    portEXIT_CRITICAL(&mux);
  }

};

} // namespace ebus
} // namespace esphome
