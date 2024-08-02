#pragma once

#include "esphome/components/switch/switch.h"
#include "../b2500_v1.h"

namespace esphome {
namespace b2500 {

class OutActiveSwitch : public switch_::Switch, public Parented<B2500ComponentV1> {
 public:
  OutActiveSwitch(int out);

 protected:
  void write_state(bool state) override;

  int out_{0};
};

}  // namespace b2500
}  // namespace esphome
