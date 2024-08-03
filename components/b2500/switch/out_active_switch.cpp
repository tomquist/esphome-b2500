#include "out_active_switch.h"

namespace esphome {
namespace b2500 {

OutActiveSwitch::OutActiveSwitch(int out) : out_(out) {}
void OutActiveSwitch::write_state(bool state) {
  if (this->parent_->set_out_active(out_, state)) {
    this->publish_state(state);
  }
}

}  // namespace b2500
}  // namespace esphome
