#pragma once

#include "b2500_binary_sensor_base.h"
#include "../b2500_state.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace b2500 {

class B2500BinarySensorV1 : public B2500BinarySensorBase {
 public:
  B2500BinarySensorV1(B2500State *state) : B2500BinarySensorBase(state) {}
};

}  // namespace b2500
}  // namespace esphome
