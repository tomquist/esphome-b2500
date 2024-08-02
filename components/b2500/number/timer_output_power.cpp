#include "timer_output_power.h"

namespace esphome {
namespace b2500 {

void TimerOutputPowerNumber::control(float value) {
  this->parent_->set_timer_output_power(this->timer_, value);
  this->publish_state(value);
}

}  // namespace b2500
}  // namespace esphome
