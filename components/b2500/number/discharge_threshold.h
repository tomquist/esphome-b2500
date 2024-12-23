#pragma once

#include "esphome/components/number/number.h"
#include "../b2500_v1.h"

namespace esphome {
namespace b2500 {

class DischargeThresholdNumber : public Component, public number::Number, public Parented<B2500ComponentV1> {
 public:
  void setup() override;

 protected:
  void control(float value) override;
};

}  // namespace b2500
}  // namespace esphome
