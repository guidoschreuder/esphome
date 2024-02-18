#pragma once

#include "../ebus_component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace ebus {

class EbusSensor : public EbusReceiver, public EbusSender, public sensor::Sensor, public Component {
public:
  EbusSensor() {
  }

  void dump_config() override;

  bool is_send_poll();

  void set_send_poll(bool);
  void set_master_address(uint8_t) override;
  void set_source(uint8_t);
  void set_destination(uint8_t);
  void set_command(uint16_t);
  void set_payload(const std::vector<uint8_t> &);

  void process_received(Ebus::Telegram) override;
  Ebus::SendCommand prepare_command() override;

  // TODO: refactor these
  uint32_t get_response_bytes(Ebus::Telegram &telegram, uint8_t start, uint8_t length);
  float to_float(Ebus::Telegram &telegram, uint8_t start, uint8_t length, float divider);
  bool is_mine(Ebus::Telegram &telegram);

protected:
  bool send_poll_;
  uint8_t master_address_;
  uint8_t source_;
  uint8_t destination_;
  uint16_t command_;
  std::vector<uint8_t> payload_{};


};

}  // namespace ebus
}  // namespace esphome
