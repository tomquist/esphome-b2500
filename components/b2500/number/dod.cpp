#include "dod.h"

namespace esphome {
namespace b2500 {

void DodNumber::control(float value) {
  this->parent_->set_dod(value);
  this->publish_state(value);
}

}  // namespace b2500
}  // namespace esphome
