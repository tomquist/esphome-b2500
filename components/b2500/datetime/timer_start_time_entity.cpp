#include "timer_start_time_entity.h"

namespace esphome {
namespace b2500 {

void TimerStartTimeEntity::control(const datetime::TimeCall &call) {
  this->parent_->set_timer_start(this->timer_, *call.get_hour(), *call.get_minute());

  this->hour_ = *call.get_hour();
  this->minute_ = *call.get_minute();
  this->publish_state();
}


void TimerStartTimeEntity::setup() {
  this->parent_->get_state()->add_on_message_callback([this](B2500Message message) { this->on_message(message); });
}

void TimerStartTimeEntity::on_message(B2500Message message) {
  if (message == B2500_MSG_TIMER_INFO) {
    auto timer = this->parent_->get_state()->get_timer(this->timer_);
    uint8_t start_hour = timer.start.hour % 24;
    uint8_t start_minute = timer.start.minute % 60;
    if (this->hour != start_hour || this->minute != start_minute) {
      this->hour_ = start_hour;
      this->minute_ = start_minute;
      this->publish_state();
    }
  }
}

}  // namespace b2500
}  // namespace esphome
