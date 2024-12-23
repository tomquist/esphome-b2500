#pragma once

#include "esphome/components/switch/switch.h"
#include "../b2500_v2.h"

namespace esphome {
namespace b2500 {

class TimerEnabledSwitch : public Component, public switch_::Switch, public Parented<B2500ComponentV2> {
 public:
  TimerEnabledSwitch(int timer);
  void setup() override;

 protected:
  void write_state(bool state) override;

  int timer_;
};

}  // namespace b2500
}  // namespace esphome
