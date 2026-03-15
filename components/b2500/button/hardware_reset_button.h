#pragma once

#include "esphome/core/component.h"
#include "esphome/components/button/button.h"

#include "../b2500_base.h"

namespace esphome {
namespace b2500 {

class HardwareResetButton : public button::Button, public Component, public Parented<B2500ComponentBase> {
 public:
  void press_action() override;
};

}  // namespace b2500
}  // namespace esphome
