#pragma once

#include "b2500_const.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <sstream>
#include <map>

namespace esphome {
namespace b2500 {

struct B2500PacketHeader {
  uint8_t head;  // 0x73
  uint8_t data_length;
  uint8_t cntl;  // 0x23
  B2500Command command;
} __attribute__((packed));

union DisChargeSetting {
  struct {
    uint8_t out1_enable : 1;
    uint8_t out2_enable : 1;
  };
  uint8_t byte;
} __attribute__((packed));

union ChargeMode {
  struct {
    uint8_t load_first : 1;
  };
  uint8_t byte;
} __attribute__((packed));

union WifiMqttState {
  struct {
    uint8_t wifi_connected : 1;
    uint8_t mqtt_connected : 1;
  };
  uint8_t byte;
} __attribute__((packed));

union InActive {
  struct {
    uint8_t active : 1;
    uint8_t transparent : 1;
  };
  uint8_t byte;
} __attribute__((packed));

struct TimeInfo {
  uint8_t hour;
  uint8_t minute;
} __attribute__((packed));

struct RuntimeInfoPacket {
  InActive in1_active;                  // 4
  InActive in2_active;                  // 5
  uint16_t in1_power;                  // 6-7
  uint16_t in2_power;                  // 8-9
  uint16_t soc;                        // 10-11
  uint8_t dev_version;                 // 12
  ChargeMode charge_mode;              // 13
  DisChargeSetting discharge_setting;  // 14
  WifiMqttState wifi_mqtt_state;       // 15
  uint8_t out1_active;                 // 16
  uint8_t out2_active;                 // 17
  uint8_t dod;                         // 18
  uint16_t discharge_threshold;        // 19-20
  enum DeviceScene device_scene;       // 21
  uint16_t remaining_capacity;         // 22-23
  uint16_t out1_power;                 // 24-25
  uint16_t out2_power;                 // 26-27
  uint8_t extern1_connected;           // 28
  uint8_t extern2_connected;           // 29
  enum DeviceRegion device_region;     // 30
  TimeInfo time;                      // 31 - 32
  int16_t temperature_low;            // 33 - 34
  int16_t temperature_high;           // 35 - 36
} __attribute__((packed));

struct DeviceInfoPacket {
  std::string type;
  std::string id;
  std::string mac;
};

struct CellInfoPacket {
  uint16_t soc;
  int16_t temperature1;
  int16_t temperature2;
  uint16_t cell_voltages[14];
};

struct WifiInfoPacket {
  uint8_t signal;
  std::string ssid;
};

struct FC41DInfoPacket {
  std::string firmware;
};

struct TimerInfo {
  uint8_t enabled;
  TimeInfo start;
  TimeInfo end;
  uint16_t output_power;
} __attribute__((packed));

struct SmartMeterInfo {
  uint8_t connected;
  uint16_t power_out;
  uint16_t meter_reading;
  uint16_t unknown;
} __attribute__((packed));

struct TimerInfoPacket3 {
  uint8_t adaptive_mode_enabled;
  TimerInfo timer[3];
  SmartMeterInfo smart_meter;
} __attribute__((packed));

struct TimerInfoPacket {
    struct TimerInfoPacket3 base;
    TimerInfo additional_timers[2];
} __attribute__((packed));

struct DateTimePacket {
  uint8_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
} __attribute__((packed));

class B2500Codec {
 public:
  bool parse_command(uint8_t *data, uint16_t data_len, B2500Command &command);
  bool parse_device_info(uint8_t *data, uint16_t data_len, DeviceInfoPacket &info);
  bool parse_runtime_info(uint8_t *data, uint16_t data_len, RuntimeInfoPacket &payload);
  bool is_valid_cell_info(uint8_t *data, uint16_t data_len);
  bool parse_cell_info(uint8_t *data, uint16_t data_len, CellInfoPacket &payload);
  bool parse_wifi_info(uint8_t *data, uint16_t data_len, WifiInfoPacket &payload);
  bool parse_fc41d_info(uint8_t *data, uint16_t data_len, FC41DInfoPacket &payload);
  bool parse_timer_info(uint8_t *data, uint16_t data_len, TimerInfoPacket &payload);

  bool encode_simple_command(B2500Command command, std::vector<uint8_t> &payload);
  bool encode_load_first_enabled(const ChargeMode &chargeMode, std::vector<uint8_t> &payload);
  bool encode_out_active(const DisChargeSetting &discharge_setting, std::vector<uint8_t> &payload);
  bool encode_discharge_threshold(uint16_t threshold, std::vector<uint8_t> &payload);
  bool encode_dod(uint8_t dod, std::vector<uint8_t> &payload);
  bool encode_timers(const TimerInfo *timers, size_t count, std::vector<uint8_t> &payload);
  bool encode_set_datetime(const DateTimePacket &datetime, std::vector<uint8_t> &payload);
  bool encode_set_wifi(const std::string &ssid, const std::string &password, std::vector<uint8_t> &payload);
  bool encode_set_mqtt(bool ssl, const std::string &host, uint16_t port, const std::string &username,
                       const std::string &password, std::vector<uint8_t> &payload);

 protected:
  bool encode_command(B2500Command command, const uint8_t *data, uint16_t data_len, std::vector<uint8_t> &payload);
  bool parse_timer_info_base(uint8_t *data, uint16_t data_len, void* payload, size_t payload_size);
  bool parse_timer_info3(uint8_t *data, uint16_t data_len, TimerInfoPacket3 &payload);
  bool parse_timer_info5(uint8_t *data, uint16_t data_len, TimerInfoPacket &payload);
};

}  // namespace b2500
}  // namespace esphome
