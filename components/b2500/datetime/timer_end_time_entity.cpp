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

void TimerEndTimeEntity::setup() {
  this->parent_->get_state()->add_on_message_callback([this](B2500Message message) { this->on_message(message); });
}

void TimerEndTimeEntity::on_message(B2500Message message) {
  if (message == B2500_MSG_TIMER_INFO) {
    auto timer = this->parent_->get_state()->get_timer(this->timer_);
    uint8_t end_hour = timer.end.hour % 24;
    uint8_t end_minute = timer.end.minute % 60;
    if (this->hour != end_hour || this->minute != end_minute) {
      auto call = this->make_call();
      call.set_hour(end_hour);
      call.set_minute(end_minute);
      call.perform();
    }
  }
}


}  // namespace b2500
}  // namespace esphome
