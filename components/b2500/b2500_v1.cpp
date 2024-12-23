#ifdef USE_ESP32

#include "b2500_v1.h"
#include "esphome/core/log.h"

const std::string CHARGE_MODE_LOAD_FIRST = "LoadFirst";
const std::string CHARGE_MODE_PV2_PASSTHROUGH = "PV2Passthrough";

namespace esphome {
namespace b2500 {

std::vector<std::string> B2500ComponentV1::get_valid_charge_modes() {
  return {CHARGE_MODE_LOAD_FIRST, CHARGE_MODE_PV2_PASSTHROUGH};
}

bool B2500ComponentV1::set_out_active(int out, bool active) {
  std::vector<uint8_t> payload;
  if (!this->state_->set_out_active(out, active, payload)) {
    ESP_LOGW(TAG, "Failed to set out active");
    return false;
  }
  this->send_command(payload);
  return true;
}

bool B2500ComponentV1::set_discharge_threshold(float threshold) {
  std::vector<uint8_t> payload;
  if (!this->state_->set_discharge_threshold(threshold, payload)) {
    ESP_LOGW(TAG, "Failed to set discharge threshold");
    return false;
  }
  this->send_command(payload);
  return true;
}

bool B2500ComponentV1::set_charge_mode(const std::string &charge_mode) {
  std::vector<uint8_t> payload;
  if (charge_mode == "LoadFirst") {
    if (!this->state_->set_load_first_enabled(true, payload)) {
      ESP_LOGW(TAG, "Failed to set charge mode");
      return false;
    }
  } else if (charge_mode == "PV2Passthrough") {
    if (!this->state_->set_load_first_enabled(false, payload)) {
      ESP_LOGW(TAG, "Failed to set charge mode");
      return false;
    }
  } else {
    ESP_LOGW(TAG, "Invalid charge mode: %s", charge_mode.c_str());
    return false;
  }
  this->send_command(payload);
  return true;
}

std::string B2500ComponentV1::get_charge_mode() {
  auto runtime_info = this->state_->get_runtime_info();
  if (runtime_info.charge_mode.load_first) {
    return CHARGE_MODE_LOAD_FIRST;
  } else {
    return CHARGE_MODE_PV2_PASSTHROUGH;
  }
}

void B2500ComponentV1::interpret_message(B2500Message message) {
  B2500ComponentBase::interpret_message(message);
  if (message == B2500_MSG_RUNTIME_INFO) {
    auto runtime_info = this->state_->get_runtime_info();
    if (this->out1_switch_ != nullptr && this->out1_switch_->state != runtime_info.discharge_setting.out1_enable) {
      this->out1_switch_->publish_state(runtime_info.discharge_setting.out1_enable);
    }
    if (this->out2_switch_ != nullptr && this->out2_switch_->state != runtime_info.discharge_setting.out2_enable) {
      this->out2_switch_->publish_state(runtime_info.discharge_setting.out2_enable);
    }
    if (this->discharge_threshold_number_ != nullptr &&
        this->discharge_threshold_number_->state != runtime_info.discharge_threshold) {
      this->discharge_threshold_number_->publish_state(runtime_info.discharge_threshold);
    }
  }
}

}  // namespace b2500
}  // namespace esphome
#endif  // USE_ESP32
