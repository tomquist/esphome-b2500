#include "timer_enabled_switch.h"

namespace esphome {
namespace b2500 {

TimerEnabledSwitch::TimerEnabledSwitch(int timer) : timer_(timer) {}
void TimerEnabledSwitch::write_state(bool state) {
  if (this->parent_->set_timer_enabled(this->timer_, state)) {
    this->publish_state(state);
  }
}

}  // namespace b2500
}  // namespace esphome
