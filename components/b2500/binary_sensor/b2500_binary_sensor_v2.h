#pragma once

#include "b2500_binary_sensor_base.h"
#include "../b2500_state.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace b2500 {

class B2500BinarySensorV2 : public B2500BinarySensorBase {
  SUB_BINARY_SENSOR(smart_meter_connected)
 public:
  B2500BinarySensorV2(B2500State *state) : B2500BinarySensorBase(state) {}
  void dump_config() override;

 protected:
  void on_message(B2500Message message) override;
};

}  // namespace b2500
}  // namespace esphome
