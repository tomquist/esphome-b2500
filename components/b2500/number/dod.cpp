#include "dod.h"
#include "../b2500_state.h"

namespace esphome {
namespace b2500 {

void DodNumber::setup() {
  this->parent_->get_state()->add_on_message_callback([this](B2500Message message) {
    if (message == B2500_MSG_RUNTIME_INFO) {
      auto runtime_info = this->parent_->get_state()->get_runtime_info();
      if (this->state != runtime_info.dod) {
        this->publish_state(runtime_info.dod);
      }
    }
  });
}

void DodNumber::control(float value) {
  this->parent_->set_dod(value);
  this->publish_state(value);
}

}  // namespace b2500
}  // namespace esphome
