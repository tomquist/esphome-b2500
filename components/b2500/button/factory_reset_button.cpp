#include "factory_reset_button.h"

namespace esphome {
namespace b2500 {

void FactoryResetButton::press_action() { this->parent_->factory_reset(); }

}  // namespace b2500
}  // namespace esphome
