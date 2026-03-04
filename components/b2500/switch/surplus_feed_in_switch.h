#pragma once

#include "esphome/components/switch/switch.h"
#include "../b2500_v2.h"

namespace esphome {
namespace b2500 {

class SurplusFeedInSwitch : public Component, public switch_::Switch, public Parented<B2500ComponentV2> {
 public:
  void write_state(bool state) override;
};

}  // namespace b2500
}  // namespace esphome
