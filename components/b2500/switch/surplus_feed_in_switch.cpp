#include "surplus_feed_in_switch.h"
#include "../b2500_state.h"

namespace esphome {
namespace b2500 {

void SurplusFeedInSwitch::setup() {
  this->parent_->get_state()->add_on_message_callback([this](B2500Message message) {
    if (message == B2500_MSG_RUNTIME_INFO) {
      auto *state = this->parent_->get_state();
      if (state->has_surplus_feed_in_enabled()) {
        const bool enabled = state->get_surplus_feed_in_enabled();
        if (this->state != enabled) {
          this->publish_state(enabled);
        }
      }
    }
  });
}

void SurplusFeedInSwitch::write_state(bool state) {
  if (this->parent_->set_surplus_feed_in_enabled(state)) {
    this->publish_state(state);
  }
}

}  // namespace b2500
}  // namespace esphome
