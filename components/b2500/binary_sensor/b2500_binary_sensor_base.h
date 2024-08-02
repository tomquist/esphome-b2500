#pragma once

#include "../b2500_state.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace b2500 {

class B2500BinarySensorBase : public Component {
 public:
  B2500BinarySensorBase(B2500State *state) : state_(state) {}
  virtual void dump_config() override;
  void setup() override;

  SUB_BINARY_SENSOR(wifi_connected)
  SUB_BINARY_SENSOR(mqtt_connected)
  SUB_BINARY_SENSOR(in1_active)
  SUB_BINARY_SENSOR(in2_active)
  SUB_BINARY_SENSOR(in1_transparent)
  SUB_BINARY_SENSOR(in2_transparent)
  SUB_BINARY_SENSOR(out1_active)
  SUB_BINARY_SENSOR(out2_active)
  SUB_BINARY_SENSOR(extern1_connected)
  SUB_BINARY_SENSOR(extern2_connected)

 protected:
  B2500State *state_;
  virtual void on_message(B2500Message message);
};

}  // namespace b2500
}  // namespace esphome
