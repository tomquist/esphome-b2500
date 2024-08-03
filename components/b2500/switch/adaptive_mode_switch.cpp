#include "adaptive_mode_switch.h"

namespace esphome {
namespace b2500 {

void AdaptiveModeSwitch::write_state(bool state) {
  if (this->parent_->set_adaptive_mode_enabled(state)) {
    this->publish_state(state);
  }
}

}  // namespace b2500
}  // namespace esphome
