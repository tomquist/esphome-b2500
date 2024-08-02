#include "timer_end_time_entity.h"

namespace esphome {
namespace b2500 {

void TimerEndTimeEntity::control(const datetime::TimeCall &call) {
  uint8_t hour = *call.get_hour();
  uint8_t minute = *call.get_minute();
  if (hour == 0 && minute == 0) {
    hour = 23;
    minute = 59;
  }
  this->parent_->set_timer_end(this->timer_, hour, minute);

  this->hour_ = hour;
  this->minute_ = minute;
  this->publish_state();
}

}  // namespace b2500
}  // namespace esphome
