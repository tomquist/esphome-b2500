#pragma once

#include <unordered_map>
#include <functional>

#include "b2500_codec.h"

namespace esphome {
namespace b2500 {

enum B2500Message : uint8_t {
  B2500_MSG_DEVICE_INFO = 0x01,
  B2500_MSG_RUNTIME_INFO = 0x02,
  B2500_MSG_CELL_INFO = 0x03,
  B2500_MSG_WIFI_INFO = 0x04,
  B2500_MSG_FC41D_INFO = 0x05,
  B2500_MSG_TIMER_INFO = 0x06,
};

class B2500State {
 public:
  B2500State();

  // Receiving Packets
  bool receive_packet(uint8_t *data, uint16_t data_len, time_t timestamp);
  void add_on_message_callback(std::function<void(B2500Message)> &&callback);
  bool is_message_received(B2500Message message) const;

  time_t get_last_message_received_timestamp() const { return this->last_message_received_timestamp_; }

  // Commands
  bool set_load_first_enabled(bool state, std::vector<uint8_t> &payload);
  bool set_out_active(int out, bool state, std::vector<uint8_t> &payload);
  bool set_discharge_threshold(int threshold, std::vector<uint8_t> &payload);
  bool set_dod(int dod, std::vector<uint8_t> &payload);
  bool get_simple_command(B2500Command command, std::vector<uint8_t> &payload);
  bool set_timer_enabled(int timer, bool enabled, std::vector<uint8_t> &payload);
  bool set_timer_output_power(int timer, int output_power, std::vector<uint8_t> &payload);
  bool set_timer_start(int timer, uint8_t hour, uint8_t minute, std::vector<uint8_t> &payload);
  bool set_timer_end(int timer, uint8_t hour, uint8_t minute, std::vector<uint8_t> &payload);
  bool set_adaptive_mode_enabled(bool enabled, std::vector<uint8_t> &payload);
  bool set_datetime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second,
                    std::vector<uint8_t> &payload);
  bool set_wifi(const std::string &ssid, const std::string &password, std::vector<uint8_t> &payload);
  bool set_mqtt(bool ssl, const std::string &host, uint16_t port, const std::string &username,
                const std::string &password, std::vector<uint8_t> &payload);

  // State Accessors
  const DeviceInfoPacket &get_device_info() const { return this->device_info_; }
  const RuntimeInfoPacket &get_runtime_info() const { return this->runtime_info_; }
  const CellInfoPacket &get_cell_info() const { return this->cell_info_; }
  const WifiInfoPacket &get_wifi_info() const { return this->wifi_info_; }
  const FC41DInfoPacket &get_fc41d_info() const { return this->fc41d_info_; }
  const TimerInfoPacket &get_timer_info() const { return this->timer_info_; }

 protected:
  void message_received(B2500Message message, time_t timestamp);

  B2500Codec *codec_;
  std::unordered_map<B2500Message, time_t> info_timestamps_ = {{B2500_MSG_DEVICE_INFO, 0}, {B2500_MSG_RUNTIME_INFO, 0},
                                                               {B2500_MSG_CELL_INFO, 0},   {B2500_MSG_WIFI_INFO, 0},
                                                               {B2500_MSG_FC41D_INFO, 0},  {B2500_MSG_TIMER_INFO, 0}};

  DeviceInfoPacket device_info_;
  RuntimeInfoPacket runtime_info_;
  CellInfoPacket cell_info_;
  WifiInfoPacket wifi_info_;
  FC41DInfoPacket fc41d_info_;
  TimerInfoPacket timer_info_;

  time_t last_message_received_timestamp_ = 0;

  CallbackManager<void(B2500Message)> message_callback_{};
};

}  // namespace b2500
}  // namespace esphome
