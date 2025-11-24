#pragma once

#ifdef USE_ESP32

#include "b2500_base.h"

namespace esphome {
namespace b2500 {

class B2500ComponentV1 : public B2500ComponentBase {
 public:
  B2500ComponentV1() : B2500ComponentBase(1) {}

  std::vector<std::string> get_valid_charge_modes() override;
  void set_charge_mode_traits(select::SelectTraits &traits) const override;
  std::string get_charge_mode() override;

  // Actions
  bool set_out_active(int out, bool active);
  bool set_discharge_threshold(float threshold);
  bool set_charge_mode(const std::string &charge_mode) override;
};

}  // namespace b2500
}  // namespace esphome

#endif  // USE_ESP32
