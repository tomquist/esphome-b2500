#pragma once

#include "esphome/components/datetime/time_entity.h"
#include "../b2500_v2.h"

namespace esphome {
namespace b2500 {

class TimerStartTimeEntity : public datetime::TimeEntity, public Parented<B2500ComponentV2> {
 public:
  TimerStartTimeEntity(int timer) : timer_(timer) { this->second_ = 0; }

 protected:
  void control(const datetime::TimeCall &call) override;

  int timer_{0};
};

}  // namespace b2500
}  // namespace esphome
