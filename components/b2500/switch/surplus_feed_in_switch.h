#pragma once

#include "esphome/components/switch/switch.h"
#include "../b2500_v2.h"
#include "../b2500_state.h"

namespace esphome {
namespace b2500 {

class SurplusFeedInSwitch : public Component, public switch_::Switch, public Parented<B2500ComponentV2> {
 public:
  void setup() override {
    this->parent_->get_state()->add_on_message_callback([this](B2500Message message) {
      if (message != B2500_MSG_RUNTIME_INFO) {
        return;
      }

      constexpr uint8_t kMinFirmwareSurplusFeedIn = 226;
      constexpr uint8_t kMinFirmwareSurplusFeedInHMJ = 110;
      constexpr uint16_t kRuntimeSurplusFlagPayloadIndex = 55;

      auto *state = this->parent_->get_state();
      const auto runtime_info = state->get_runtime_info();
      const auto device_info = state->get_device_info();

      const bool is_hmj = device_info.type.rfind("HMJ", 0) == 0;
      const uint8_t required_fw = is_hmj ? kMinFirmwareSurplusFeedInHMJ : kMinFirmwareSurplusFeedIn;
      if (runtime_info.dev_version < required_fw) {
        return;
      }
      if (state->get_last_runtime_payload_size() <= kRuntimeSurplusFlagPayloadIndex) {
        return;
      }

      const uint8_t disabled = static_cast<uint8_t>((runtime_info.daily_total_load_discharge >> 24) & 0xFF);
      const bool enabled = (disabled == 0x00);
      if (this->state != enabled) {
        this->publish_state(enabled);
      }
    });
  }

  void write_state(bool state) override {
    if (this->parent_->set_surplus_feed_in_enabled(state)) {
      this->publish_state(state);
    }
  }
};

}  // namespace b2500
}  // namespace esphome
