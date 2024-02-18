
#include "ebus_sensor.h"

#define GET_BYTE(CMD, I) ((uint8_t) ((CMD >> 8 * I) & 0XFF))

namespace esphome {
namespace ebus {

void EbusSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "EbusSensor");
  ESP_LOGCONFIG(TAG, "  message:");
  ESP_LOGCONFIG(TAG, "    send_poll: %", this->send_poll_ ? "true" : "false");
  ESP_LOGCONFIG(TAG, "    source: 0x%02x", this->source_);
  ESP_LOGCONFIG(TAG, "    destination: 0x%02x", this->destination_);
  ESP_LOGCONFIG(TAG, "    command: 0x%04x", this->command_);
};

void EbusSensor::set_send_poll(bool send_poll) {
  this->send_poll_ = send_poll;
}
void EbusSensor::set_master_address(uint8_t master_address) {
  this->master_address_ = master_address;
}
bool EbusSensor::is_send_poll() {
  return this->send_poll_;
}
void EbusSensor::set_source(uint8_t source) {
  this->source_ = source;
}
void EbusSensor::set_destination(uint8_t destination) {
  this->destination_ = destination;
}
void EbusSensor::set_command(uint16_t command) {
  this->command_ = command;
}
void EbusSensor::set_payload(const std::vector<uint8_t> &payload) {
  this->payload_ = payload;
}

Ebus::SendCommand EbusSensor::prepare_command() {
  return Ebus::SendCommand(  //
       this->master_address_,
       Ebus::Elf::to_slave(this->destination_),
       GET_BYTE(this->command_, 1),
       GET_BYTE(this->command_, 0),
       this->payload_.size(),
       &this->payload_[0]);
}

void EbusSensor::process_received(Ebus::Telegram telegram) {
  if (!is_mine(telegram)) {
    return;
  }
  this->publish_state(to_float(telegram, 0, 2, 1000.0));
}

uint32_t EbusSensor::get_response_bytes(Ebus::Telegram &telegram, uint8_t start, uint8_t length) {
  uint32_t result = 0;
  for (uint8_t i = 0; i < 4 && i < length; i++) {
    result = result | (telegram.get_response_byte(start + i) << (i * 8));
  }
  return result;
}

float EbusSensor::to_float(Ebus::Telegram &telegram, uint8_t start, uint8_t length, float divider) {
  return get_response_bytes(telegram, start, length) / divider;
}

bool EbusSensor::is_mine(Ebus::Telegram &telegram) {
  if (telegram.getCommand() != this->command_) {
    ESP_LOGD(TAG, "Message receive. command: %02X, expected::0x%02X", telegram.getCommand(), this->command_);
    return false;
  }
  for (int i = 0; i < this->payload_.size(); i++) {
    if (this->payload_[i] != telegram.get_request_byte(i)) {
      return false;
    }
  }
  return true;
}

}  // namespace ebus
}  // namespace esphome
