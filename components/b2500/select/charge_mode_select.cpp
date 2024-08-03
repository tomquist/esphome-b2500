#include "charge_mode_select.h"

namespace esphome {
namespace b2500 {

void ChargeModeSelect::setup() {
  auto charge_modes = this->parent_->get_valid_charge_modes();
  this->traits.set_options(charge_modes);
}

void ChargeModeSelect::control(const std::string &value) {
  if (this->parent_->set_charge_mode(value)) {
    this->publish_state(state);
  }
  // TODO: Implement this
}

}  // namespace b2500
}  // namespace esphome
