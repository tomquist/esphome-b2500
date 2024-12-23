#include "adaptive_mode_switch.h"
#include "../b2500_state.h"

namespace esphome {
namespace b2500 {

void AdaptiveModeSwitch::setup() {
  this->parent_->get_state()->add_on_message_callback([this](B2500Message message) {
    if (message == B2500_MSG_TIMER_INFO) {
      auto timer_info = this->parent_->get_state()->get_timer_info();
      if (this->state != timer_info.base.adaptive_mode_enabled) {
        this->publish_state(timer_info.base.adaptive_mode_enabled);
      }
    }
  });
}

void AdaptiveModeSwitch::write_state(bool state) {
  if (this->parent_->set_adaptive_mode_enabled(state)) {
    this->publish_state(state);
  }
}

}  // namespace b2500
}  // namespace esphome
