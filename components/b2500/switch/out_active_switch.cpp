#include "out_active_switch.h"
#include "../b2500_state.h"

namespace esphome {
namespace b2500 {

OutActiveSwitch::OutActiveSwitch(int out) : out_(out) {}

void OutActiveSwitch::setup() {
  this->parent_->get_state()->add_on_message_callback([this](B2500Message message) {
    if (message == B2500_MSG_RUNTIME_INFO) {
      auto runtime_info = this->parent_->get_state()->get_runtime_info();
      bool out_enable = this->out_ == 0 ? runtime_info.discharge_setting.out1_enable : runtime_info.discharge_setting.out2_enable;
      if (this->state != out_enable) {
        this->publish_state(out_enable);
      }
    }
  });
}

void OutActiveSwitch::write_state(bool state) {
  if (this->parent_->set_out_active(out_, state)) {
    this->publish_state(state);
  }
}

}  // namespace b2500
}  // namespace esphome
