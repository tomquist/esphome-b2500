#include "charge_mode_select.h"
#include "../b2500_state.h"

#include <algorithm>

namespace esphome {
namespace b2500 {

void ChargeModeSelect::setup() {
  constexpr const char *CHARGE_MODE_OPTIONS_V1[] = {"LoadFirst", "PV2Passthrough"};
  constexpr const char *CHARGE_MODE_OPTIONS_V2[] = {"LoadFirst", "SimultaneousChargeAndDischarge"};

  auto charge_modes = this->parent_->get_valid_charge_modes();
  if (std::find(charge_modes.begin(), charge_modes.end(), "SimultaneousChargeAndDischarge") != charge_modes.end()) {
    this->traits.set_options({CHARGE_MODE_OPTIONS_V2[0], CHARGE_MODE_OPTIONS_V2[1]});
  } else {
    this->traits.set_options({CHARGE_MODE_OPTIONS_V1[0], CHARGE_MODE_OPTIONS_V1[1]});
  }
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
