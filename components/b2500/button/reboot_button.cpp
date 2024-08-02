#include "reboot_button.h"

namespace esphome {
namespace b2500 {

void RebootButton::press_action() { this->parent_->reboot(); }

}  // namespace b2500
}  // namespace esphome
