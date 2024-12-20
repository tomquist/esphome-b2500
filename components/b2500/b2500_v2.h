#pragma once

#ifdef USE_ESP32

#include "b2500_base.h"

namespace esphome {
namespace b2500 {

class B2500ComponentV2 : public B2500ComponentBase {
 public:
  B2500ComponentV2() : B2500ComponentBase(2) {}

  std::vector<std::string> get_valid_charge_modes() override;

  void set_timer_enabled_switch(int timer, switch_::Switch *switch_) { this->timer_enabled_switch_[timer] = switch_; }
  void set_adaptive_mode_switch(switch_::Switch *switch_) { this->adaptive_mode_switch_ = switch_; }
  void set_timer_output_power_number(int timer, number::Number *number) {
    this->timer_output_power_number_[timer] = number;
  }

  // Actions
  bool set_timer_enabled(int timer, bool enabled);
  bool set_timer_output_power(int timer, float power);
  bool set_charge_mode(const std::string &charge_mode) override;
  bool set_timer_start(int timer, uint8_t hour, uint8_t minute);
  bool set_timer_end(int timer, uint8_t hour, uint8_t minute);
  bool set_timer(int timer, bool enabled, float output_power, uint8_t start_hour, uint8_t start_minute, uint8_t end_hour,
                 uint8_t end_minute);
  bool set_adaptive_mode_enabled(bool enabled);

 protected:
  switch_::Switch *timer_enabled_switch_[5]{nullptr};
  switch_::Switch *adaptive_mode_switch_{nullptr};

  number::Number *timer_output_power_number_[5]{nullptr};

  void poll_runtime_info_() override;
  void interpret_message(B2500Message message) override;
};

}  // namespace b2500
}  // namespace esphome

#endif  // USE_ESP32
