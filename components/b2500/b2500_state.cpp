#include "b2500_state.h"

#include "esphome/core/application.h"
#include "esphome/core/time.h"

namespace esphome {
namespace b2500 {

void B2500State::add_on_message_callback(std::function<void(B2500Message)> &&callback) {
  this->message_callback_.add(std::move(callback));
}

void B2500State::message_received(B2500Message message, time_t timestamp) {
  info_timestamps_[message] = timestamp;
  this->last_message_received_timestamp_ = timestamp;
  this->message_callback_.call(message);
}

bool B2500State::is_message_received(B2500Message message) const {
  auto entry = info_timestamps_.find(message);
  if (entry == info_timestamps_.end()) {
    return false;
  }
  return entry->second > 0;
}

bool B2500State::set_load_first_enabled(bool state, std::vector<uint8_t> &payload) {
  if (!this->is_message_received(B2500_MSG_RUNTIME_INFO)) {
    return false;
  }
  this->runtime_info_.charge_mode.byte = state ? 0x01 : 0x00;
  if (!this->codec_->encode_load_first_enabled(this->runtime_info_.charge_mode, payload)) {
    return false;
  }
  return true;
}

bool B2500State::set_out_active(int out, bool state, std::vector<uint8_t> &payload) {
  if (!this->is_message_received(B2500_MSG_RUNTIME_INFO)) {
    return false;
  }
  if (out == 0) {
    this->runtime_info_.discharge_setting.out1_enable = state;
  } else {
    this->runtime_info_.discharge_setting.out2_enable = state;
  }
  if (!this->codec_->encode_out_active(this->runtime_info_.discharge_setting, payload)) {
    return false;
  }
  return true;
}

bool B2500State::set_discharge_threshold(int threshold, std::vector<uint8_t> &payload) {
  if (!this->is_message_received(B2500_MSG_RUNTIME_INFO)) {
    return false;
  }
  this->runtime_info_.discharge_threshold = threshold;
  if (!this->codec_->encode_discharge_threshold(this->runtime_info_.discharge_threshold, payload)) {
    return false;
  }
  return true;
}

bool B2500State::set_dod(int dod, std::vector<uint8_t> &payload) {
  if (!this->is_message_received(B2500_MSG_RUNTIME_INFO)) {
    return false;
  }
  this->runtime_info_.dod = dod;
  if (!this->codec_->encode_dod(this->runtime_info_.dod, payload)) {
    return false;
  }
  return true;
}

uint8_t B2500State::get_number_of_timers() const {
  if (this->runtime_info_.dev_version < 218) {
    return 3;
  }
  return 5;
}

bool B2500State::set_timer_enabled(int timer, bool enabled, std::vector<uint8_t> &payload) {
  if (!this->is_message_received(B2500_MSG_TIMER_INFO)) {
    return false;
  }
  if (timer < 0 || timer >= 5) {
    return false;
  }
  if (timer < 3) {
    this->timer_info_.base.timer[timer].enabled = enabled;
  } else {
    this->timer_info_.additional_timers[timer - 3].enabled = enabled;
  }
  return this->encode_timers(payload);
}

bool B2500State::set_timer_output_power(int timer, int output_power, std::vector<uint8_t> &payload) {
  if (!this->is_message_received(B2500_MSG_TIMER_INFO)) {
    return false;
  }
  if (timer < 0 || timer >= 5) {
    return false;
  }
  if (timer < 3) {
    this->timer_info_.base.timer[timer].output_power = output_power;
  } else {
    this->timer_info_.additional_timers[timer - 3].output_power = output_power;
  }
  return this->encode_timers(payload);
}

bool B2500State::set_timer_start(int timer, uint8_t hour, uint8_t minute, std::vector<uint8_t> &payload) {
  if (!this->is_message_received(B2500_MSG_TIMER_INFO)) {
    return false;
  }
  if (timer < 0 || timer >= 5) {
    return false;
  }
  hour = hour % 24;
  minute = minute % 60;
  if (timer < 3) {
    this->timer_info_.base.timer[timer].start.hour = hour;
    this->timer_info_.base.timer[timer].start.minute = minute;
  } else {
    this->timer_info_.additional_timers[timer - 3].start.hour = hour;
    this->timer_info_.additional_timers[timer - 3].start.minute = minute;
  }
  return this->encode_timers(payload);
}

bool B2500State::set_timer_end(int timer, uint8_t hour, uint8_t minute, std::vector<uint8_t> &payload) {
  if (!this->is_message_received(B2500_MSG_TIMER_INFO)) {
    return false;
  }
  if (timer < 0 || timer >= 5) {
    return false;
  }
  // The end time must be after the start time
  hour = hour % 24;
  if (hour == 0 && minute == 0) {
    hour = 24;
  }
  minute = minute % 60;
  if (timer < 3) {
    this->timer_info_.base.timer[timer].end.hour = hour;
    this->timer_info_.base.timer[timer].end.minute = minute;
  } else {
    this->timer_info_.additional_timers[timer - 3].end.hour = hour;
    this->timer_info_.additional_timers[timer - 3].end.minute = minute;
  }
  return this->encode_timers(payload);
}

bool B2500State::encode_timers(std::vector<uint8_t> &payload) {
  if (this->is_message_received(B2500_MSG_RUNTIME_INFO) && this->runtime_info_.dev_version < 218) {
    return this->codec_->encode_timers(this->timer_info_.base.timer, 3, payload);
  } else {
    TimerInfo timer[5];
    std::memcpy(timer, this->timer_info_.base.timer, sizeof(TimerInfo) * 3);
    std::memcpy(timer + 3, this->timer_info_.additional_timers, sizeof(TimerInfo) * 2);
    return this->codec_->encode_timers(timer, 5, payload);
  }
}

bool B2500State::set_adaptive_mode_enabled(bool enabled, std::vector<uint8_t> &payload) {
  if (!this->is_message_received(B2500_MSG_TIMER_INFO)) {
    return false;
  }
  this->timer_info_.base.adaptive_mode_enabled = enabled;
  if (enabled) {
    if (!this->codec_->encode_simple_command(CMD_ENABLE_ADAPTIVE_MODE, payload)) {
      return false;
    }
  } else {
    if (!this->encode_timers(payload)) {
      return false;
    }
  }
  return true;
}

bool B2500State::set_datetime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second,
                              std::vector<uint8_t> &payload) {
  DateTimePacket time_packet;
  time_packet.year = year - 1900;
  time_packet.month = month;
  time_packet.day = day;
  time_packet.hour = hour;
  time_packet.minute = minute;
  time_packet.second = second;
  return this->codec_->encode_set_datetime(time_packet, payload);
}

bool B2500State::set_wifi(const std::string &ssid, const std::string &password, std::vector<uint8_t> &payload) {
  return this->codec_->encode_set_wifi(ssid, password, payload);
}

bool B2500State::set_mqtt(bool ssl, const std::string &host, uint16_t port, const std::string &username,
                          const std::string &password, std::vector<uint8_t> &payload) {
  return this->codec_->encode_set_mqtt(ssl, host, port, username, password, payload);
}

bool B2500State::get_simple_command(B2500Command command, std::vector<uint8_t> &payload) {
  return this->codec_->encode_simple_command(command, payload);
}

const TimerInfo B2500State::get_timer(int timer) const {
  if (timer < 3) {
    return this->timer_info_.base.timer[timer];
  } else {
    return this->timer_info_.additional_timers[timer - 3];
  }
}

bool B2500State::receive_packet(uint8_t *data, uint16_t data_len, time_t timestamp) {
  if (this->codec_->is_valid_cell_info(data, data_len)) {
    if (!this->codec_->parse_cell_info(data, data_len, this->cell_info_)) {
      ESP_LOGW(TAG, "Failed to parse cell info");
      return false;
    }
    this->message_received(B2500_MSG_CELL_INFO, timestamp);
    return true;
  }

  B2500Command command;
  if (!this->codec_->parse_command(data, data_len, command)) {
    ESP_LOGW(TAG, "Failed to parse command");
    return false;
  }

  ESP_LOGD(TAG, "Command: %d", command);
  switch (command) {
    case CMD_DEVICE_INFO: {
      if (!this->codec_->parse_device_info(data, data_len, this->device_info_)) {
        ESP_LOGW(TAG, "Failed to parse device info");
        return false;
      }
      this->message_received(B2500_MSG_DEVICE_INFO, timestamp);
      break;
    }
    case CMD_RUNTIME_INFO: {
      if (!this->codec_->parse_runtime_info(data, data_len, this->runtime_info_)) {
        ESP_LOGW(TAG, "Failed to parse runtime info");
        return false;
      }
      this->message_received(B2500_MSG_RUNTIME_INFO, timestamp);
      break;
    }
    case CMD_WIFI_INFO: {
      if (!this->codec_->parse_wifi_info(data, data_len, this->wifi_info_)) {
        ESP_LOGW(TAG, "Failed to parse wifi info");
        return false;
      }
      this->message_received(B2500_MSG_WIFI_INFO, timestamp);
      break;
    }
    case CMD_FC41D_INFO: {
      if (!this->codec_->parse_fc41d_info(data, data_len, this->fc41d_info_)) {
        ESP_LOGW(TAG, "Failed to parse fc41d info");
        return false;
      }
      this->message_received(B2500_MSG_FC41D_INFO, timestamp);
      break;
    }
    case CMD_GET_TIMERS: {
      if (!this->codec_->parse_timer_info(data, data_len, this->timer_info_)) {
        ESP_LOGW(TAG, "Failed to parse timer info");
        return false;
      }
      this->message_received(B2500_MSG_TIMER_INFO, timestamp);
      break;
    }
    default:
      ESP_LOGW(TAG, "Unknown command");
      return false;
      break;
  }
  return true;
}

B2500State::B2500State() : codec_(new B2500Codec()) {}

}  // namespace b2500
}  // namespace esphome
