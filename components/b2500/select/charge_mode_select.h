#pragma once

#include "esphome/components/select/select.h"
#include "../b2500_base.h"

namespace esphome {
namespace b2500 {

class ChargeModeSelect : public Component, public select::Select, public Parented<B2500ComponentBase> {
 public:
  void setup() override;

 protected:
  void control(const std::string &value) override;
};

}  // namespace b2500
}  // namespace esphome
