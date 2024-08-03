#include "discharge_threshold.h"

namespace esphome {
namespace b2500 {

void DischargeThresholdNumber::control(float value) {
  this->parent_->set_discharge_threshold(value);
  this->publish_state(value);
}

}  // namespace b2500
}  // namespace esphome
