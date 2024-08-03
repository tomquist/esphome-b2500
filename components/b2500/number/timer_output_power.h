#pragma once

#include "esphome/components/number/number.h"
#include "../b2500_v2.h"

namespace esphome {
namespace b2500 {

class TimerOutputPowerNumber : public number::Number, public Parented<B2500ComponentV2> {
 public:
  TimerOutputPowerNumber(int timer) : timer_(timer) {}

 protected:
  void control(float value) override;

  int timer_{0};
};

}  // namespace b2500
}  // namespace esphome
