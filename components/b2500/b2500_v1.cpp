#ifdef USE_ESP32

#include "b2500_v1.h"
#include "esphome/core/log.h"

constexpr const char *CHARGE_MODE_LOAD_FIRST = "LoadFirst";
constexpr const char *CHARGE_MODE_PV2_PASSTHROUGH = "PV2Passthrough";

namespace esphome {
namespace b2500 {

void B2500ComponentV1::set_charge_mode_traits(select::SelectTraits &traits) const {
  traits.set_options({CHARGE_MODE_LOAD_FIRST, CHARGE_MODE_PV2_PASSTHROUGH});
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

}  // namespace b2500
}  // namespace esphome
#endif  // USE_ESP32
