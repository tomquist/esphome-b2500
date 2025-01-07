#pragma once

#include "../b2500_state.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace b2500 {

class B2500Sensor : public Component {
 public:
  B2500Sensor(B2500State *state) : state_(state) {}
  void dump_config() override;
  void setup() override;

  SUB_SENSOR(soc)
  SUB_SENSOR(in1_power)
  SUB_SENSOR(in2_power)
  SUB_SENSOR(in_total_power)
  SUB_SENSOR(out1_power)
  SUB_SENSOR(out2_power)
  SUB_SENSOR(out_total_power)
  SUB_SENSOR(capacity)
  SUB_SENSOR(wifi_rssi)
  SUB_SENSOR(adaptive_power_out);
  SUB_SENSOR(smart_meter_reading);
  SUB_SENSOR(temperature_low);
  SUB_SENSOR(temperature_high);
  SUB_SENSOR(daily_total_battery_charge);
  SUB_SENSOR(daily_total_battery_discharge);
  SUB_SENSOR(daily_total_load_charge);
  SUB_SENSOR(daily_total_load_discharge);

 protected:
  B2500State *state_;
  void on_message(B2500Message message);
};

}  // namespace b2500
}  // namespace esphome
