#include "hardware_reset_button.h"

namespace esphome {
namespace b2500 {

void HardwareResetButton::press_action() { this->parent_->hardware_reset(); }

}  // namespace b2500
}  // namespace esphome
