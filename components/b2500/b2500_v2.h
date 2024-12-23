#pragma once

#ifdef USE_ESP32

#include "b2500_base.h"

namespace esphome {
namespace b2500 {

class B2500ComponentV2 : public B2500ComponentBase {
 public:
  B2500ComponentV2() : B2500ComponentBase(2) {}

  std::vector<std::string> get_valid_charge_modes() override;

  std::string get_charge_mode() override;

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
  void poll_runtime_info_() override;
};

}  // namespace b2500
}  // namespace esphome

#endif  // USE_ESP32
