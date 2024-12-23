#include "timer_output_power.h"
#include "../b2500_state.h"

namespace esphome {
namespace b2500 {

void TimerOutputPowerNumber::setup() {
  this->parent_->get_state()->add_on_message_callback([this](B2500Message message) {
    if (message == B2500_MSG_TIMER_INFO) {
      auto timer = this->parent_->get_state()->get_timer(this->timer_);
      if (this->state != timer.output_power) {
        this->publish_state(timer.output_power);
      }
    }
  });
}

void TimerOutputPowerNumber::control(float value) {
  this->parent_->set_timer_output_power(this->timer_, value);
  this->publish_state(value);
}

}  // namespace b2500
}  // namespace esphome
