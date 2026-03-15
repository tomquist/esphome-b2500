#include "b2500_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace b2500 {

static const char *const TAG = "b2500.sensor";

void B2500Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "B2500 Sensor:");
  LOG_SENSOR("  ", "SoC", this->soc_sensor_);
  LOG_SENSOR("  ", "Capacity", this->capacity_sensor_);
  LOG_SENSOR("  ", "IN1 Power", this->in1_power_sensor_);
  LOG_SENSOR("  ", "IN2 Power", this->in2_power_sensor_);
  LOG_SENSOR("  ", "OUT1 Power", this->out1_power_sensor_);
  LOG_SENSOR("  ", "OUT2 Power", this->out2_power_sensor_);
  LOG_SENSOR("  ", "WiFi RSSI", this->wifi_rssi_sensor_);
}

void B2500Sensor::setup() {
  ESP_LOGD(TAG, "Setting up B2500 Sensor...");
  this->state_->add_on_message_callback([this](B2500Message message) { this->on_message(message); });
}

void B2500Sensor::on_message(B2500Message message) {
  if (message == B2500_MSG_RUNTIME_INFO) {
    auto payload = this->state_->get_runtime_info();
    this->publish_sensor_(this->in1_power_sensor_, payload.in1_power);
    this->publish_sensor_(this->in2_power_sensor_, payload.in2_power);
    this->publish_sensor_(this->in_total_power_sensor_, payload.in1_power + payload.in2_power);
    this->publish_sensor_(this->out1_power_sensor_, payload.out1_power);
    this->publish_sensor_(this->out2_power_sensor_, payload.out2_power);
    this->publish_sensor_(this->out_total_power_sensor_, payload.out1_power + payload.out2_power);
    this->publish_sensor_(this->capacity_sensor_, payload.remaining_capacity);
    this->publish_sensor_(this->soc_sensor_, payload.soc / 10.0);
    this->publish_sensor_(this->temperature_low_sensor_, payload.temperature_low);
    this->publish_sensor_(this->temperature_high_sensor_, payload.temperature_high);
    this->publish_sensor_(this->daily_total_battery_charge_sensor_, payload.daily_total_battery_charge);
    this->publish_sensor_(this->daily_total_battery_discharge_sensor_, payload.daily_total_battery_discharge);
    this->publish_sensor_(this->daily_total_load_charge_sensor_, payload.daily_total_load_charge);
    this->publish_sensor_(this->daily_total_load_discharge_sensor_, payload.daily_total_load_discharge);
  } else if (message == B2500_MSG_WIFI_INFO) {
    auto wifi_info = this->state_->get_wifi_info();
    this->publish_sensor_(this->wifi_rssi_sensor_, wifi_info.signal);
  } else if (message == B2500_MSG_TIMER_INFO) {
    auto timer_info = this->state_->get_timer_info();
    this->publish_sensor_(this->adaptive_power_out_sensor_, timer_info.base.smart_meter.power_out);
    this->publish_sensor_(this->smart_meter_reading_sensor_, timer_info.base.smart_meter.meter_reading);
  }
}

}  // namespace b2500
}  // namespace esphome
