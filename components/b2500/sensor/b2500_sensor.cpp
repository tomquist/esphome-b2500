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
    if (this->in1_power_sensor_ != nullptr && this->in1_power_sensor_->state != payload.in1_power) {
      this->in1_power_sensor_->publish_state(payload.in1_power);
    }
    if (this->in2_power_sensor_ != nullptr && this->in2_power_sensor_->state != payload.in2_power) {
      this->in2_power_sensor_->publish_state(payload.in2_power);
    }
    if (this->in_total_power_sensor_ != nullptr &&
        this->in_total_power_sensor_->state != payload.in1_power + payload.in2_power) {
      this->in_total_power_sensor_->publish_state(payload.in1_power + payload.in2_power);
    }
    if (this->out1_power_sensor_ != nullptr && this->out1_power_sensor_->state != payload.out1_power) {
      this->out1_power_sensor_->publish_state(payload.out1_power);
    }
    if (this->out2_power_sensor_ != nullptr && this->out2_power_sensor_->state != payload.out2_power) {
      this->out2_power_sensor_->publish_state(payload.out2_power);
    }
    if (this->out_total_power_sensor_ != nullptr &&
        this->out_total_power_sensor_->state != payload.out1_power + payload.out2_power) {
      this->out_total_power_sensor_->publish_state(payload.out1_power + payload.out2_power);
    }
    if (this->capacity_sensor_ != nullptr && this->capacity_sensor_->state != payload.remaining_capacity) {
      this->capacity_sensor_->publish_state(payload.remaining_capacity);
    }
    if (this->soc_sensor_ != nullptr) {
      float soc = payload.soc / 10.0;
      if (this->soc_sensor_->state != soc) {
        this->soc_sensor_->publish_state(soc);
      }
    }
    if (this->temperature_low_sensor_ != nullptr && this->temperature_low_sensor_->state != payload.temperature_low) {
      this->temperature_low_sensor_->publish_state(payload.temperature_low);
    }
    if (this->temperature_high_sensor_ != nullptr && this->temperature_high_sensor_->state != payload.temperature_high) {
      this->temperature_high_sensor_->publish_state(payload.temperature_high);
    }
  } else if (message == B2500_MSG_WIFI_INFO) {
    auto wifi_info = this->state_->get_wifi_info();
    if (this->wifi_rssi_sensor_ != nullptr && this->wifi_rssi_sensor_->state != wifi_info.signal) {
      this->wifi_rssi_sensor_->publish_state(wifi_info.signal);
    }
  } else if (message == B2500_MSG_TIMER_INFO) {
    auto timer_info = this->state_->get_timer_info();
    if (this->adaptive_power_out_sensor_ != nullptr &&
        this->adaptive_power_out_sensor_->state != timer_info.base.smart_meter.power_out) {
      this->adaptive_power_out_sensor_->publish_state(timer_info.base.smart_meter.power_out);
    }
    if (this->smart_meter_reading_sensor_ != nullptr &&
        this->smart_meter_reading_sensor_->state != timer_info.base.smart_meter.meter_reading) {
      this->smart_meter_reading_sensor_->publish_state(timer_info.base.smart_meter.meter_reading);
    }
  }
}

}  // namespace b2500
}  // namespace esphome
