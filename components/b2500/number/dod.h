#pragma once

#include "esphome/components/number/number.h"
#include "../b2500_base.h"

namespace esphome {
namespace b2500 {

class DodNumber : public Component, public number::Number, public Parented<B2500ComponentBase> {
 public:
  void setup() override;

 protected:
  void control(float value) override;
};
}  // namespace b2500
}  // namespace esphome
