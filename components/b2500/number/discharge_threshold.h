#pragma once

#include "esphome/components/number/number.h"
#include "../b2500_v1.h"

namespace esphome {
namespace b2500 {

class DischargeThresholdNumber : public number::Number, public Parented<B2500ComponentV1> {
 protected:
  void control(float value) override;
};

}  // namespace b2500
}  // namespace esphome
