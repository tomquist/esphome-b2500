#pragma once

#include "../b2500_state.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace b2500 {

class B2500TextSensor : public Component {
 public:
  B2500TextSensor(B2500State *state) : state_(state) {}
  void dump_config() override;
  void setup() override;

  SUB_TEXT_SENSOR(firmware_version)
  SUB_TEXT_SENSOR(device_type)
  SUB_TEXT_SENSOR(device_id)
  SUB_TEXT_SENSOR(mac_address)
  SUB_TEXT_SENSOR(fc41d_version)
  SUB_TEXT_SENSOR(wifi_ssid)
  SUB_TEXT_SENSOR(scene)
  SUB_TEXT_SENSOR(region)
  SUB_TEXT_SENSOR(cell_voltage)

  SUB_TEXT_SENSOR(device_time)
  SUB_TEXT_SENSOR(last_response)

 protected:
  B2500State *state_;
  void on_message(B2500Message message);
};

}  // namespace b2500
}  // namespace esphome
