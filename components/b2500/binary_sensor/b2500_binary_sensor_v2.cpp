#include "b2500_binary_sensor_v2.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace b2500 {

void B2500BinarySensorV2::dump_config() {
  B2500BinarySensorBase::dump_config();
  LOG_BINARY_SENSOR("  ", "Smart Meter Connected", this->smart_meter_connected_binary_sensor_);
}

void B2500BinarySensorV2::on_message(B2500Message message) {
  B2500BinarySensorBase::on_message(message);
  if (message == B2500_MSG_TIMER_INFO) {
    auto payload = this->state_->get_timer_info();
    if (this->smart_meter_connected_binary_sensor_ != nullptr &&
        this->smart_meter_connected_binary_sensor_->state != payload.smart_meter.connected) {
      this->smart_meter_connected_binary_sensor_->publish_state(payload.smart_meter.connected);
    }
  }
}

}  // namespace b2500
}  // namespace esphome
