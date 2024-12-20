#ifdef USE_ESP32

#include "b2500_v2.h"
#include "esphome/core/log.h"

const std::string CHARGE_MODE_LOAD_FIRST = "LoadFirst";
const std::string CHARGE_MODE_SIMULTANEOUS_CHARGE_AND_DISCHARGE = "SimultaneousChargeAndDischarge";

namespace esphome {
namespace b2500 {

std::vector<std::string> B2500ComponentV2::get_valid_charge_modes() {
  return {CHARGE_MODE_LOAD_FIRST, CHARGE_MODE_SIMULTANEOUS_CHARGE_AND_DISCHARGE};
}

void B2500ComponentV2::poll_runtime_info_() {
  B2500ComponentBase::poll_runtime_info_();
  this->enqueue_simple_command(CMD_GET_TIMERS);
}

bool B2500ComponentV2::set_timer_enabled(int timer, bool enabled) {
  std::vector<uint8_t> payload;
  if (!this->state_->set_timer_enabled(timer, enabled, payload)) {
    ESP_LOGW(TAG, "Failed to set timer enabled");
    return false;
  }
  this->send_command(payload);
  return true;
}

bool B2500ComponentV2::set_timer_output_power(int timer, float power) {
  std::vector<uint8_t> payload;
  if (!this->state_->set_timer_output_power(timer, power, payload)) {
    ESP_LOGW(TAG, "Failed to set timer output power");
    return false;
  }
  this->send_command(payload);
  return true;
}

bool B2500ComponentV2::set_timer_start(int timer, uint8_t hour, uint8_t minute) {
  std::vector<uint8_t> payload;
  if (!this->state_->set_timer_start(timer, hour, minute, payload)) {
    ESP_LOGW(TAG, "Failed to set timer start");
    return false;
  }
  this->send_command(payload);
  return true;
}

bool B2500ComponentV2::set_timer_end(int timer, uint8_t hour, uint8_t minute) {
  std::vector<uint8_t> payload;
  if (!this->state_->set_timer_end(timer, hour, minute, payload)) {
    ESP_LOGW(TAG, "Failed to set timer end");
    return false;
  }
  this->send_command(payload);
  return true;
}

bool B2500ComponentV2::set_timer(int timer, bool enabled, float output_power, uint8_t start_hour, uint8_t start_minute,
                                 uint8_t end_hour, uint8_t end_minute) {
  std::vector<uint8_t> payload;
  if (!this->state_->set_timer(timer, enabled, output_power, start_hour, start_minute, end_hour, end_minute, payload)) {
    ESP_LOGW(TAG, "Failed to set timer");
    return false;
  }
  this->send_command(payload);
  return true;
}

bool B2500ComponentV2::set_charge_mode(const std::string &charge_mode) {
  std::vector<uint8_t> payload;
  if (charge_mode == "LoadFirst") {
    if (!this->state_->set_load_first_enabled(true, payload)) {
      ESP_LOGW(TAG, "Failed to set charge mode");
      return false;
    }
  } else if (charge_mode == "SimultaneousChargeAndDischarge") {
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

bool B2500ComponentV2::set_adaptive_mode_enabled(bool enabled) {
  std::vector<uint8_t> payload;
  if (!this->state_->set_adaptive_mode_enabled(enabled, payload)) {
    ESP_LOGW(TAG, "Failed to set adaptive mode enabled");
    return false;
  }
  this->send_command(payload);
  return true;
}

void B2500ComponentV2::interpret_message(B2500Message message) {
  B2500ComponentBase::interpret_message(message);
  if (message == B2500_MSG_RUNTIME_INFO) {
    auto runtime_info = this->state_->get_runtime_info();
    std::string charge_mode;
    if (runtime_info.charge_mode.load_first) {
      charge_mode = CHARGE_MODE_LOAD_FIRST;
    } else {
      charge_mode = CHARGE_MODE_SIMULTANEOUS_CHARGE_AND_DISCHARGE;
    }
    if (this->charge_mode_select_ != nullptr && this->charge_mode_select_->state != charge_mode) {
      this->charge_mode_select_->publish_state(charge_mode);
    }
  } else if (message == B2500_MSG_TIMER_INFO) {
    for (int i = 0; i < this->state_->get_number_of_timers(); i++) {
      auto timer = this->state_->get_timer(i);
      if (this->timer_enabled_switch_[i] != nullptr &&
          this->timer_enabled_switch_[i]->state != timer.enabled) {
        this->timer_enabled_switch_[i]->publish_state(timer.enabled);
      }
      if (this->timer_output_power_number_[i] != nullptr &&
          this->timer_output_power_number_[i]->state != timer.output_power) {
        this->timer_output_power_number_[i]->publish_state(timer.output_power);
      }
    }
    auto timers = this->state_->get_timer_info();
    if (this->adaptive_mode_switch_ != nullptr &&
        this->adaptive_mode_switch_->state != timers.base.adaptive_mode_enabled) {
      this->adaptive_mode_switch_->publish_state(timers.base.adaptive_mode_enabled);
    }
  }
}

}  // namespace b2500
}  // namespace esphome
#endif  // USE_ESP32
