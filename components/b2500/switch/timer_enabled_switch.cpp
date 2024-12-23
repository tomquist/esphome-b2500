#include "timer_enabled_switch.h"
#include "../b2500_state.h"

namespace esphome {
namespace b2500 {

TimerEnabledSwitch::TimerEnabledSwitch(int timer) : timer_(timer) {}

void TimerEnabledSwitch::setup() {
  this->parent_->get_state()->add_on_message_callback([this](B2500Message message) {
    if (message == B2500_MSG_TIMER_INFO) {
      auto timer = this->parent_->get_state()->get_timer(this->timer_);
      if (this->state != timer.enabled) {
        this->publish_state(timer.enabled);
      }
    }
  });
}

void TimerEnabledSwitch::write_state(bool state) {
  if (this->parent_->set_timer_enabled(this->timer_, state)) {
    this->publish_state(state);
  }
}

}  // namespace b2500
}  // namespace esphome
