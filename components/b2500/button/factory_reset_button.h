#pragma once

#include "esphome/components/button/button.h"
#include "../b2500_base.h"

namespace esphome {
namespace b2500 {

class FactoryResetButton : public button::Button, public Parented<B2500ComponentBase> {
 public:
  FactoryResetButton() = default;

 protected:
  void press_action() override;
};

}  // namespace b2500
}  // namespace esphome
