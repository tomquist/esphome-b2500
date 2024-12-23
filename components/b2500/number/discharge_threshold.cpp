#include "discharge_threshold.h"
#include "../b2500_state.h"

namespace esphome {
namespace b2500 {

void DischargeThresholdNumber::setup() {
  this->parent_->get_state()->add_on_message_callback([this](B2500Message message) {
    if (message == B2500_MSG_RUNTIME_INFO) {
      auto runtime_info = this->parent_->get_state()->get_runtime_info();
      if (this->state != runtime_info.discharge_threshold) {
        this->publish_state(runtime_info.discharge_threshold);
      }
    }
  });
}

void DischargeThresholdNumber::control(float value) {
  this->parent_->set_discharge_threshold(value);
  this->publish_state(value);
}

}  // namespace b2500
}  // namespace esphome
