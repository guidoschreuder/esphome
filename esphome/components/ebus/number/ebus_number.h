#pragma once

#include "../ebus_component.h"
#include "../sensor/ebus_sensor.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace ebus {

class EbusNumber : public EbusItem, public number::Number {
 public:
  EbusNumber() {}

 protected:
  void control(float value) override;

};

}  // namespace ebus
}  // namespace esphome
