#include "b2500_binary_sensor_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace b2500 {

static const char *const TAG = "b2500.binary_sensor";

void B2500BinarySensorBase::dump_config() {
  ESP_LOGCONFIG(TAG, "B2500 Sensor:");
  LOG_BINARY_SENSOR("  ", "WiFi Connected", this->wifi_connected_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "MQTT Connected", this->mqtt_connected_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "IN1 Active", this->in1_active_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "IN2 Active", this->in2_active_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "IN1 Transparent", this->in1_transparent_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "IN2 Transparent", this->in2_transparent_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "OUT1 Active", this->out1_active_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "OUT2 Active", this->out2_active_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Extern1 Connected", this->extern1_connected_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Extern2 Connected", this->extern2_connected_binary_sensor_);
}

void B2500BinarySensorBase::setup() {
  ESP_LOGD(TAG, "Setting up B2500 Binary Sensor...");
  this->state_->add_on_message_callback([this](B2500Message message) { this->on_message(message); });
}

void B2500BinarySensorBase::on_message(B2500Message message) {
  if (message == B2500_MSG_RUNTIME_INFO) {
    auto payload = this->state_->get_runtime_info();
    if (this->in1_active_binary_sensor_ != nullptr && this->in1_active_binary_sensor_->state != payload.in1_active.active) {
      this->in1_active_binary_sensor_->publish_state(payload.in1_active.active);
    }
    if (this->in2_active_binary_sensor_ != nullptr && this->in2_active_binary_sensor_->state != payload.in2_active.active) {
      this->in2_active_binary_sensor_->publish_state(payload.in2_active.active);
    }
    if (this->in1_transparent_binary_sensor_ != nullptr &&
        this->in1_transparent_binary_sensor_->state != payload.in1_active.transparent) {
      this->in1_transparent_binary_sensor_->publish_state(payload.in1_active.transparent);
    }
    if (this->in2_transparent_binary_sensor_ != nullptr &&
        this->in2_transparent_binary_sensor_->state != payload.in2_active.transparent) {
      this->in2_transparent_binary_sensor_->publish_state(payload.in2_active.transparent);
    }
    if (this->out1_active_binary_sensor_ != nullptr && this->out1_active_binary_sensor_->state != payload.out1_active) {
      this->out1_active_binary_sensor_->publish_state(payload.out1_active);
    }
    if (this->out2_active_binary_sensor_ != nullptr && this->out2_active_binary_sensor_->state != payload.out2_active) {
      this->out2_active_binary_sensor_->publish_state(payload.out2_active);
    }
    if (this->wifi_connected_binary_sensor_ != nullptr &&
        this->wifi_connected_binary_sensor_->state != payload.wifi_mqtt_state.wifi_connected) {
      this->wifi_connected_binary_sensor_->publish_state(payload.wifi_mqtt_state.wifi_connected);
    }
    if (this->mqtt_connected_binary_sensor_ != nullptr &&
        this->mqtt_connected_binary_sensor_->state != payload.wifi_mqtt_state.mqtt_connected) {
      this->mqtt_connected_binary_sensor_->publish_state(payload.wifi_mqtt_state.mqtt_connected);
    }
    if (this->extern1_connected_binary_sensor_ != nullptr &&
        this->extern1_connected_binary_sensor_->state != payload.extern1_connected) {
      this->extern1_connected_binary_sensor_->publish_state(payload.extern1_connected);
    }
    if (this->extern2_connected_binary_sensor_ != nullptr &&
        this->extern2_connected_binary_sensor_->state != payload.extern2_connected) {
      this->extern2_connected_binary_sensor_->publish_state(payload.extern2_connected);
    }
  }
}

}  // namespace b2500
}  // namespace esphome
