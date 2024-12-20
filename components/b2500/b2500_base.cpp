#ifdef USE_ESP32

#include <inttypes.h>
#include "b2500_base.h"
#include "esphome/core/log.h"

namespace esphome {
namespace b2500 {

void B2500ComponentBase::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                             esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status == ESP_GATT_OK) {
        ESP_LOGI(TAG, "Connected successfully!");
      }
      break;
    }

    case ESP_GATTC_DISCONNECT_EVT: {
      this->read_handle_ = 0;
      this->write_handle_ = 0;
      ESP_LOGW(TAG, "Disconnected!");
      break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      this->read_handle_ = 0;
      auto *chr = this->parent()->get_characteristic(B2500_SERVICE_UUID, B2500_STATUS_UUID);
      if (chr == nullptr) {
        ESP_LOGW(TAG, "No sensor read characteristic found at service %s char %s",
                 B2500_SERVICE_UUID.to_string().c_str(), B2500_STATUS_UUID.to_string().c_str());
        break;
      }
      this->read_handle_ = chr->handle;

      auto status = esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(), this->parent()->get_remote_bda(),
                                                      chr->handle);
      if (status) {
        ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify failed, status=%d", status);
      }

      auto *write_chr = this->parent()->get_characteristic(B2500_SERVICE_UUID, B2500_COMMAND_UUID);
      if (write_chr == nullptr) {
        ESP_LOGW(TAG, "No sensor write characteristic found at service %s char %s",
                 B2500_SERVICE_UUID.to_string().c_str(), B2500_COMMAND_UUID.to_string().c_str());
        break;
      }
      this->write_handle_ = write_chr->handle;

      break;
    }

    case ESP_GATTC_READ_CHAR_EVT: {
      if (param->read.conn_id != this->parent()->get_conn_id())
        break;
      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Error reading char at handle %d, status=%d", param->read.handle, param->read.status);
        break;
      }
      if (param->read.handle == this->read_handle_) {
        this->process_message_(param->read.value, param->read.value_len);
      }
      break;
    }

    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      this->node_state = espbt::ClientState::ESTABLISHED;
      std::queue<std::vector<uint8_t>>().swap(this->requests_);
      this->defer([this]() { this->poll_once(); });
      break;
    }

    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.conn_id != this->parent()->get_conn_id())
        break;
      this->process_message_(param->notify.value, param->notify.value_len);
      break;
    }

    default:
      break;
  }
}

void B2500ComponentBase::setup() { ESP_LOGCONFIG(TAG, "Setting up B2500..."); }

bool B2500ComponentBase::reboot() {
  std::vector<uint8_t> payload;
  if (!this->state_->get_simple_command(CMD_REBOOT, payload)) {
    ESP_LOGW(TAG, "Failed to get reboot command");
    return false;
  }
  this->send_command(payload);
  return true;
}

bool B2500ComponentBase::factory_reset() {
  std::vector<uint8_t> payload;
  if (!this->state_->get_simple_command(CMD_FACTORY_RESET, payload)) {
    ESP_LOGW(TAG, "Failed to get factory reset command");
    return false;
  }
  this->send_command(payload);
  return true;
}

bool B2500ComponentBase::set_dod(float dod) {
  std::vector<uint8_t> payload;
  if (!this->state_->set_dod(dod, payload)) {
    ESP_LOGW(TAG, "Failed to set dod");
    return false;
  }
  this->send_command(payload);
  return true;
}

bool B2500ComponentBase::set_wifi(const std::string &ssid, const std::string &password) {
  std::vector<uint8_t> payload;
  if (!this->state_->set_wifi(ssid, password, payload)) {
    ESP_LOGW(TAG, "Failed to set wifi");
    return false;
  }
  this->send_command(payload);
  return true;
}

bool B2500ComponentBase::set_mqtt(bool ssl, const std::string &host, uint16_t port, const std::string &username,
                                  const std::string &password) {
  std::vector<uint8_t> payload;
  if (!this->state_->set_mqtt(ssl, host, port, username, password, payload)) {
    ESP_LOGW(TAG, "Failed to set mqtt");
    return false;
  }
  this->send_command(payload);
  return true;
}

bool B2500ComponentBase::reset_mqtt() {
  std::vector<uint8_t> payload;
  if (!this->state_->reset_mqtt(payload)) {
    ESP_LOGW(TAG, "Failed to reset mqtt");
    return false;
  }
  this->send_command(payload);
  return true;
}

bool B2500ComponentBase::set_datetime(ESPTime datetime) {
  std::vector<uint8_t> payload;
  if (!this->state_->set_datetime(datetime.year, datetime.month, datetime.day_of_month, datetime.hour, datetime.minute,
                                  datetime.second, payload)) {
    ESP_LOGW(TAG, "Failed to set datetime");
    return false;
  }
  this->send_command(payload);
  return true;
}

void B2500ComponentBase::update() {
  if (this->node_state == esp32_ble_tracker::ClientState::ESTABLISHED) {
    this->poll_once();
  }
}

void B2500ComponentBase::poll_once() {
  if (!this->requests_.empty()) {
    ESP_LOGD(TAG, "Request queue not empty. Clearing queue");
    std::queue<std::vector<uint8_t>>().swap(this->requests_);
  }
  this->poll_runtime_info_();
  this->process_requests_();
}

void B2500ComponentBase::poll_runtime_info_() {
  if (!this->state_->is_message_received(B2500_MSG_DEVICE_INFO)) {
    this->enqueue_simple_command(CMD_DEVICE_INFO);
  }
  if (!this->state_->is_message_received(B2500_MSG_FC41D_INFO)) {
    this->enqueue_simple_command(CMD_FC41D_INFO);
  }
  this->enqueue_simple_command(CMD_RUNTIME_INFO);
  this->enqueue_simple_command(CMD_WIFI_INFO);
  this->enqueue_simple_command(CMD_CELL_INFO);
}

bool B2500ComponentBase::process_requests_() {
  if (this->requests_.empty()) {
    return false;
  }

  auto request = this->requests_.front();
  this->requests_.pop();
  ESP_LOGD(TAG, "Dequeued command. New queue size: %d", this->requests_.size());
  this->send_command(request);
  return true;
}

void B2500ComponentBase::enqueue_simple_command(B2500Command cmd) {
  std::vector<uint8_t> payload;
  if (!this->state_->get_simple_command(cmd, payload)) {
    ESP_LOGW(TAG, "Failed to get simple command");
    return;
  }
  this->enqueue_command(payload);
}

void B2500ComponentBase::enqueue_command(std::vector<uint8_t> data) {
  this->requests_.push(data);
  ESP_LOGD(TAG, "Command enqueued. New queue size: %d", this->requests_.size());
}

void B2500ComponentBase::send_command(std::vector<uint8_t> payload) {
  // if (!this->is_connected()) {
  //   ESP_LOGW(TAG, "BLE client not connected");
  //   return;
  // }

  auto status = esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                         this->write_handle_, payload.size(), const_cast<uint8_t *>(payload.data()),
                                         ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);

  if (status) {
    ESP_LOGW(TAG, "Error sending command, status=%d", status);
    return;
  }

  ESP_LOGD(TAG, "Command sent: %s", format_hex_pretty(payload.data(), payload.size()).c_str());
}

void B2500ComponentBase::process_message_(uint8_t *data, uint16_t data_len) {
  ESP_LOGD(TAG, "Received data: %s", format_hex_pretty(data, data_len).c_str());
  auto now = this->time_->now().timestamp;
  if (!this->state_->receive_packet(data, data_len, now)) {
    ESP_LOGW(TAG, "Failed to receive packet");
  }
  this->defer([this]() {
    if (!this->process_requests_()) {
      ESP_LOGD(TAG, "No more requests pending.");
    }
  });
}

void B2500ComponentBase::interpret_message(B2500Message message) {
  switch (message) {
    case B2500_MSG_DEVICE_INFO:
      this->poll_once();
      break;
    case B2500_MSG_RUNTIME_INFO:
      this->interpret_runtime_info();
      break;
    case B2500_MSG_CELL_INFO:
      this->interpret_cell_info();
      break;
    default:
      break;
  }
}

void B2500ComponentBase::interpret_cell_info() {
  auto cell_info = this->state_->get_cell_info();
  char log_buffer[512];
  int offset = snprintf(log_buffer, sizeof(log_buffer), "SoC: %d, Temperature1: %d, Temperature2: %d", cell_info.soc,
                        cell_info.temperature1, cell_info.temperature2);

  for (int i = 0; i < 14; i++) {
    offset +=
        snprintf(log_buffer + offset, sizeof(log_buffer) - offset, ", Cell %d: %d", i, cell_info.cell_voltages[i]);
  }

  ESP_LOGD(TAG, "%s", log_buffer);
}

void B2500ComponentBase::interpret_runtime_info() {
  auto payload = this->state_->get_runtime_info();
  // For nor just dump the values
  ESP_LOGD(TAG,
           "in1_active: %d, pv_in2_state: %d, in1_power: %d, in2_power: %d, soc: %d, dev_version: %d, "
           "charge_mode: %d, wifi_connected: %d, mqtt_connected: %d, out1_active: %d, out2_active: %d, out1_enabled: "
           "%d, out2_enabled: %d, "
           "discharge_threshold: %d, dod: %d, remaining_capacity: %d, device_scene: %d, out1_power: %d, "
           "out2_power: %d, "
           "device_region: %d, extern1_connected: %d, extern2_connected: %d, "
           "hour: %d, minute: %d",
           payload.in1_active.byte, payload.in2_active.byte, payload.in1_power, payload.in2_power, payload.soc,
           payload.dev_version, payload.charge_mode.byte, payload.wifi_mqtt_state.wifi_connected,
           payload.wifi_mqtt_state.mqtt_connected, payload.out1_active, payload.out2_active,
           payload.discharge_setting.out1_enable, payload.discharge_setting.out2_enable, payload.discharge_threshold,
           payload.dod, payload.remaining_capacity, payload.device_scene, payload.out1_power, payload.out2_power,
           payload.device_region, payload.extern1_connected, payload.extern2_connected, payload.time.hour, payload.time.minute);

  if (this->dod_number_ != nullptr && this->dod_number_->state != payload.dod) {
    this->dod_number_->publish_state(payload.dod);
  }
}

void B2500ComponentBase::dump_config() {
  ESP_LOGCONFIG(TAG, "B2500:");
  ESP_LOGCONFIG(TAG, "  Update Interval: %" PRIu32 " ms", this->update_interval_);
}

B2500ComponentBase::B2500ComponentBase(int generation) {
  this->generation_ = generation;
  this->state_ = new B2500State();
  this->state_->add_on_message_callback([this](B2500Message message) { this->interpret_message(message); });
}

}  // namespace b2500
}  // namespace esphome
#endif  // USE_ESP32
