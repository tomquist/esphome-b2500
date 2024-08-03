#include "b2500_text_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/time.h"
#include "esphome/components/json/json_util.h"

namespace esphome {
namespace b2500 {

static const char *const TAG = "b2500.text_sensor";

void B2500TextSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "B2500 Text Sensor:");
  LOG_TEXT_SENSOR("  ", "Firmware Version", this->firmware_version_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Device Type", this->device_type_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Device ID", this->device_id_text_sensor_);
  LOG_TEXT_SENSOR("  ", "MAC Address", this->mac_address_text_sensor_);
  LOG_TEXT_SENSOR("  ", "FC41D Version", this->fc41d_version_text_sensor_);
  LOG_TEXT_SENSOR("  ", "WiFi SSID", this->wifi_ssid_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Scene", this->scene_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Region", this->region_text_sensor_);
}

void B2500TextSensor::setup() {
  ESP_LOGD(TAG, "Setting up B2500 Text Sensor...");
  this->state_->add_on_message_callback([this](B2500Message message) { this->on_message(message); });
}

void B2500TextSensor::on_message(B2500Message message) {
  if (this->last_response_text_sensor_ != nullptr) {
    auto last_response = this->state_->get_last_message_received_timestamp();
    ESPTime time = ESPTime::from_epoch_local(last_response);
    auto time_str = time.strftime("%Y-%m-%dT%H:%M:%S");
    this->last_response_text_sensor_->publish_state(time_str);
  }

  if (message == B2500_MSG_RUNTIME_INFO) {
    auto runtime_info = this->state_->get_runtime_info();
    if (this->firmware_version_text_sensor_ != nullptr) {
      std::string firmware = std::to_string(runtime_info.dev_version);
      if (this->firmware_version_text_sensor_->state != firmware) {
        this->firmware_version_text_sensor_->publish_state(firmware);
      }
    }
    if (this->scene_text_sensor_ != nullptr) {
      std::string scene;
      switch (runtime_info.device_scene) {
        case DeviceScene::SCENE_DAY:
          scene = "Day";
          break;
        case DeviceScene::SCENE_NIGHT:
          scene = "Night";
          break;
        case DeviceScene::SCENE_DUSK_DAWN:
          scene = "Dusk/Dawn";
          break;
        case DeviceScene::SCENE_UNKNOWN:
          // fallthrough
        default:
          scene = "Unknown";
          break;
      }
      if (this->scene_text_sensor_->state != scene) {
        this->scene_text_sensor_->publish_state(scene);
      }
    }
    if (this->region_text_sensor_ != nullptr) {
      std::string region;
      switch (runtime_info.device_region) {
        case DeviceRegion::REGION_EU:
          region = "EU";
          break;
        case DeviceRegion::REGION_CHINA:
          region = "China";
          break;
        case DeviceRegion::REGION_NON_EU:
          region = "Non-EU";
          break;
        case DeviceRegion::REGION_NOT_SET:
          region = "Not Set";
          break;
        case DeviceRegion::REGION_UNKNOWN:
          // fallthrough
        default:
          region = "Unknown";
          break;
      }
      if (this->region_text_sensor_->state != region) {
        this->region_text_sensor_->publish_state(region);
      }
    }
  } else if (message == B2500_MSG_DEVICE_INFO) {
    auto device_info = this->state_->get_device_info();
    if (this->device_type_text_sensor_ != nullptr && this->device_type_text_sensor_->state != device_info.type) {
      this->device_type_text_sensor_->publish_state(device_info.type);
    }
    if (this->device_id_text_sensor_ != nullptr && this->device_id_text_sensor_->state != device_info.id) {
      this->device_id_text_sensor_->publish_state(device_info.id);
    }
    if (this->mac_address_text_sensor_ != nullptr && this->mac_address_text_sensor_->state != device_info.mac) {
      this->mac_address_text_sensor_->publish_state(device_info.mac);
    }
  } else if (message == B2500_MSG_WIFI_INFO) {
    auto wifi_info = this->state_->get_wifi_info();
    if (this->wifi_ssid_text_sensor_ != nullptr && this->wifi_ssid_text_sensor_->state != wifi_info.ssid) {
      this->wifi_ssid_text_sensor_->publish_state(wifi_info.ssid);
    }
  } else if (message == B2500_MSG_FC41D_INFO) {
    auto fc41d_info = this->state_->get_fc41d_info();
    if (this->fc41d_version_text_sensor_ != nullptr && this->fc41d_version_text_sensor_->state != fc41d_info.firmware) {
      this->fc41d_version_text_sensor_->publish_state(fc41d_info.firmware);
    }
  } else if (message == B2500_MSG_CELL_INFO) {
    if (this->cell_voltage_text_sensor_ != nullptr) {
      auto cellInfo = json::build_json([this](JsonObject root) {
        auto cell_info = this->state_->get_cell_info();
        auto min = cell_info.cell_voltages[0];
        auto max = cell_info.cell_voltages[0];
        int sum = 0;
        JsonArray cells = root.createNestedArray("cells");
        for (int i = 0; i < 14; i++) {
          cells.add(cell_info.cell_voltages[i] / 1000.0);
          if (cell_info.cell_voltages[i] < min) {
            min = cell_info.cell_voltages[i];
          }
          if (cell_info.cell_voltages[i] > max) {
            max = cell_info.cell_voltages[i];
          }
          sum += cell_info.cell_voltages[i];
        }
        root["min"] = (min / 1000.0);
        root["max"] = (max / 1000.0);
        root["avg"] = ((sum / 14.0) / 1000.0);
      });
      this->cell_voltage_text_sensor_->publish_state(cellInfo);
    }
  }
}

}  // namespace b2500
}  // namespace esphome
