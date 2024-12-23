#pragma once

#include "esphome/components/switch/switch.h"
#include "../b2500_v1.h"

namespace esphome {
namespace b2500 {

class OutActiveSwitch : public Component, public switch_::Switch, public Parented<B2500ComponentV1> {
 public:
  OutActiveSwitch(int out);
  void setup() override;

 protected:
  void write_state(bool state) override;

  int out_;
};

}  // namespace b2500
}  // namespace esphome
