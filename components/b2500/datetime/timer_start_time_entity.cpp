#include "timer_start_time_entity.h"

namespace esphome {
namespace b2500 {

void TimerStartTimeEntity::control(const datetime::TimeCall &call) {
  this->parent_->set_timer_start(this->timer_, *call.get_hour(), *call.get_minute());

  this->hour_ = *call.get_hour();
  this->minute_ = *call.get_minute();
  this->publish_state();
}

}  // namespace b2500
}  // namespace esphome
