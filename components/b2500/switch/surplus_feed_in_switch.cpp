#include "surplus_feed_in_switch.h"
#include "../b2500_state.h"

namespace esphome {
namespace b2500 {

void SurplusFeedInSwitch::setup() {
  this->parent_->get_state()->add_on_message_callback([this](B2500Message message) {
    if (message != B2500_MSG_RUNTIME_INFO) {
      return;
    }

    auto *state = this->parent_->get_state();
    const auto runtime_info = state->get_runtime_info();

    // Only available on newer v2 firmware; keep switch command-only otherwise.
    constexpr uint8_t kMinFirmwareSurplusFeedIn = 226;
    if (runtime_info.dev_version < kMinFirmwareSurplusFeedIn) {
      return;
    }

    // Surplus flag is the last byte of the 56-byte runtime payload (payload index 55):
    // 0x00 = enabled, 0x01 = disabled.
    constexpr uint16_t kRuntimeSurplusFlagPayloadIndex = 55;
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

void SurplusFeedInSwitch::write_state(bool state) {
  if (this->parent_->set_surplus_feed_in_enabled(state)) {
    this->publish_state(state);
  }
}

}  // namespace b2500
}  // namespace esphome
