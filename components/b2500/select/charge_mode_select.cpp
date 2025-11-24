#include "charge_mode_select.h"
#include "../b2500_state.h"

namespace esphome {
namespace b2500 {

void ChargeModeSelect::setup() {
  auto charge_modes = this->parent_->get_valid_charge_modes();
  FixedVector<const char *> options;
  options.init(charge_modes.size());
  for (auto &mode : charge_modes) {
    options.push_back(mode.c_str());
  }
  this->traits.set_options(options);
  this->parent_->get_state()->add_on_message_callback([this](B2500Message message) {
    if (message == B2500_MSG_RUNTIME_INFO) {
      auto charge_mode = this->parent_->get_charge_mode();
      if (this->state != charge_mode) {
        this->publish_state(charge_mode);
      }
    }
  });
}

void ChargeModeSelect::control(const std::string &value) {
  if (this->parent_->set_charge_mode(value)) {
    this->publish_state(value);
  }
}

}  // namespace b2500
}  // namespace esphome
