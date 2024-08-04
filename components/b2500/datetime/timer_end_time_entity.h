#pragma once

#include "esphome/components/datetime/time_entity.h"
#include "../b2500_v2.h"

namespace esphome {
namespace b2500 {

class TimerEndTimeEntity : public datetime::TimeEntity, public Component, public Parented<B2500ComponentV2> {
 public:
  TimerEndTimeEntity(int timer) : timer_(timer) { this->second_ = 0; }

 protected:
  void control(const datetime::TimeCall &call) override;
  void setup() override;
  void on_message(B2500Message message);

  int timer_{0};
};

}  // namespace b2500
}  // namespace esphome
