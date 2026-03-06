#include "surplus_feed_in_switch.h"
#include "../b2500_state.h"

namespace esphome {
namespace b2500 {

constexpr uint8_t kMinFirmwareSurplusFeedIn = 226;
constexpr uint8_t kMinFirmwareSurplusFeedInHMJ = 110;

static bool supports_surplus_feed_in(const DeviceInfoPacket &device_info, uint8_t firmware) {
  const bool is_hmj = device_info.type.rfind("HMJ", 0) == 0;
  return firmware >= (is_hmj ? kMinFirmwareSurplusFeedInHMJ : kMinFirmwareSurplusFeedIn);
}

void SurplusFeedInSwitch::setup() {
  this->parent_->get_state()->add_on_message_callback([this](B2500Message message) {
    if (message != B2500_MSG_RUNTIME_INFO) {
      return;
    }

    auto *state = this->parent_->get_state();
    const auto runtime_info = state->get_runtime_info();
    const auto device_info = state->get_device_info();

    // Firmware gate mirrors command path: HMJ >= 110, others >= 226.
    if (!supports_surplus_feed_in(device_info, runtime_info.dev_version)) {
      return;
    }

    // Extension byte is payload index 59 in full packet (= payload index 55).
    constexpr uint16_t kRuntimeSurplusFlagPayloadIndex = 55;
    if (state->get_last_runtime_payload_size() <= kRuntimeSurplusFlagPayloadIndex) {
      return;
    }

    const uint8_t disabled = runtime_info.surplus_feed_in_disabled;
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
