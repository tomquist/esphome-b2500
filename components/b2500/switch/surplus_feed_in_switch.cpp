#include "surplus_feed_in_switch.h"

namespace esphome {
namespace b2500 {

void SurplusFeedInSwitch::write_state(bool state) {
  if (this->parent_->set_surplus_feed_in_enabled(state)) {
    this->publish_state(state);
  }
}

}  // namespace b2500
}  // namespace esphome
