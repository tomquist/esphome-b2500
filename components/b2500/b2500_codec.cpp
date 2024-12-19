#include "b2500_codec.h"

namespace esphome {
namespace b2500 {

bool B2500Codec::parse_command(uint8_t *data, uint16_t data_len, B2500Command &command) {
  if (data_len < sizeof(B2500PacketHeader)) {
    ESP_LOGW(TAG, "Packet too short for B2500PacketHeader");
    return false;
  }
  const B2500PacketHeader *header = reinterpret_cast<const B2500PacketHeader *>(data);
  if (header->head != 0x73 || header->cntl != 0x23) {
    ESP_LOGW(TAG, "Invalid B2500PacketHeader");
    return false;
  }
  command = header->command;
  return true;
}

bool B2500Codec::parse_device_info(uint8_t *data, uint16_t data_len, DeviceInfoPacket &info) {
  B2500Command command;
  if (!this->parse_command(data, data_len, command)) {
    return false;
  }
  if (command != CMD_DEVICE_INFO) {
    ESP_LOGW(TAG, "Not a CMD_DEVICE_INFO packet");
    return false;
  }
  // Data looks like this:
  // type=TypeValue,id=IdValue,mac=MacValue
  // Parse the data into a c++ string, ignore last byte

  const char *start = reinterpret_cast<const char *>(data + sizeof(B2500PacketHeader));
  const char *end = reinterpret_cast<const char *>(data + data_len - 1);
  std::string device_info(start, end);
  ESP_LOGD(TAG, "Device info: %s", device_info.c_str());

  // Use a stringstream to split the string by commas
  std::stringstream ss(device_info);
  std::string item;

  while (std::getline(ss, item, ',')) {
    // Find the position of the '=' character
    size_t pos = item.find('=');
    if (pos != std::string::npos) {
      // Extract the key and value
      std::string key = item.substr(0, pos);
      std::string value = item.substr(pos + 1);

      // Assign the value to the corresponding property
      if (key == "type") {
        info.type = value;
      } else if (key == "id") {
        info.id = value;
      } else if (key == "mac") {
        info.mac = value;
      }
    }
  }
  return true;
}

bool B2500Codec::parse_runtime_info(uint8_t *data, uint16_t data_len, RuntimeInfoPacket &payload) {
  B2500Command command;
  if (!this->parse_command(data, data_len, command)) {
    return false;
  }
  if (command != CMD_RUNTIME_INFO) {
    ESP_LOGW(TAG, "Not a CMD_RUNTIME_INFO packet");
    return false;
  }
  const size_t header_size = sizeof(B2500PacketHeader);
  const size_t payload_size = sizeof(RuntimeInfoPacket);

  if (data_len < header_size + payload_size) {
    ESP_LOGW(TAG, "Packet too short for CMD_RUNTIME_INFO, expected %d, got %d", header_size + payload_size, data_len);
    ESP_LOGW(TAG, "data: %s", format_hex_pretty(data, data_len).c_str());
    return false;
  }

  std::memcpy(&payload, data + header_size, payload_size);

  return true;
}

bool B2500Codec::is_valid_cell_info(uint8_t *data, uint16_t data_len) {
  if (data_len < 10) {
    return false;
  }
  const char *start = reinterpret_cast<const char *>(data);
  const char *end = reinterpret_cast<const char *>(data + data_len);
  return ((std::count(start, end, '_') == 16) || (std::count(start, start + 10, '_') == 3));
}

bool B2500Codec::parse_cell_info(uint8_t *data, uint16_t data_len, CellInfoPacket &payload) {
  if (!is_valid_cell_info(data, data_len)) {
    ESP_LOGW(TAG, "Not a valid cell info packet");
    return false;
  }

  const char *start = reinterpret_cast<const char *>(data);
  const char *end = reinterpret_cast<const char *>(data + data_len);
  std::string cellInfo(start, end);

  // Example:
  // 10_24_25_3162_3161_3156_3156_3152_3162_3156_3156_3151_3153_3153_3153_3147_3155

  // Define constants for readability
  const int SOC_INDEX = 0;
  const int TEMP1_INDEX = 1;
  const int TEMP2_INDEX = 2;
  const int FIRST_VOLTAGE_INDEX = 3;
  const int MAX_CELLS = sizeof(payload.cell_voltages) / sizeof(payload.cell_voltages[0]);

  std::stringstream ss(cellInfo);
  std::string item;
  int i = 0;

  while (std::getline(ss, item, '_')) {
    // Convert the string to an integer
    int value = std::stoi(item);
    // Assign the value to the corresponding property
    if (i == SOC_INDEX) {
      payload.soc = value;
    } else if (i == TEMP1_INDEX) {
      payload.temperature1 = value;
    } else if (i == TEMP2_INDEX) {
      payload.temperature2 = value;
    } else {
      int voltageIndex = i - FIRST_VOLTAGE_INDEX;
      if (voltageIndex < MAX_CELLS) {
        payload.cell_voltages[voltageIndex] = value;
      } else {
        ESP_LOGW(TAG, "Too many cell voltages, expected maximum %d", MAX_CELLS);
        return false;
      }
    }
    i++;
  }

  return true;
}

bool B2500Codec::parse_wifi_info(uint8_t *data, uint16_t data_len, WifiInfoPacket &payload) {
  B2500Command command;
  if (!this->parse_command(data, data_len, command)) {
    return false;
  }
  if (command != CMD_WIFI_INFO) {
    ESP_LOGW(TAG, "Not a CMD_WIFI_INFO packet");
    return false;
  }
  if (data_len < sizeof(B2500PacketHeader) + 2) {
    ESP_LOGW(TAG, "Packet too short for CMD_WIFI_INFO, expected at least %d, got %d", sizeof(B2500PacketHeader) + 2,
             data_len);
    ESP_LOGW(TAG, "data: %s", format_hex_pretty(data, data_len).c_str());
    return false;
  }

  // Extract the signal strength
  payload.signal = data[sizeof(B2500PacketHeader)];

  // Calculate the start and end pointers for the SSID string
  const char *start = reinterpret_cast<const char *>(data + sizeof(B2500PacketHeader) + 2);
  const char *end = reinterpret_cast<const char *>(data + data_len);

  // Ensure SSID is within a reasonable length
  if (start >= end) {
    ESP_LOGW(TAG, "SSID is empty or invalid");
    return false;
  }

  // Assign SSID to the payload
  auto ssid = std::string(start, end);

  // Optional: Validate the SSID string (e.g., check for non-printable characters)
  if (ssid.empty() || !std::all_of(ssid.begin(), ssid.end(), ::isprint)) {
    ESP_LOGW(TAG, "SSID contains non-printable characters or is empty");
    return false;
  }

  payload.ssid = ssid;

  return true;
}

bool B2500Codec::parse_fc41d_info(uint8_t *data, uint16_t data_len, FC41DInfoPacket &payload) {
  B2500Command command;
  if (!this->parse_command(data, data_len, command)) {
    return false;
  }
  if (command != CMD_FC41D_INFO) {
    ESP_LOGW(TAG, "Not a CMD_RUNTIME_INFO packet");
    return false;
  }
  if (data_len < sizeof(B2500PacketHeader) + 1) {
    ESP_LOGW(TAG, "Packet too short for CMD_FC41D_INFO, expected %d, got %d", sizeof(B2500PacketHeader) + 1, data_len);
    ESP_LOGW(TAG, "data: %s", format_hex_pretty(data, data_len).c_str());
    return false;
  }
  const char *start = reinterpret_cast<const char *>(data + sizeof(B2500PacketHeader));
  const char *end = reinterpret_cast<const char *>(data + data_len - 1);
  payload.firmware = std::string(start, end);
  return true;
}

bool B2500Codec::parse_timer_info_base(uint8_t *data, uint16_t data_len, void* payload, size_t payload_size) {
  B2500Command command;
  if (!this->parse_command(data, data_len, command)) {
    return false;
  }
  if (command != CMD_GET_TIMERS) {
    ESP_LOGW(TAG, "Not a CMD_TIMER_INFO packet");
    return false;
  }
  const size_t header_size = sizeof(B2500PacketHeader);

  if (data_len < header_size + payload_size) {
    ESP_LOGW(TAG, "Packet too short for CMD_TIMER_INFO, expected %d, got %d", header_size + payload_size, data_len);
    ESP_LOGW(TAG, "data: %s", format_hex_pretty(data, data_len).c_str());
    return false;
  }

  // Ensure the memory is properly aligned
  uint8_t aligned_buffer[payload_size];
  std::memcpy(aligned_buffer, data + header_size, payload_size);
  std::memcpy(payload, aligned_buffer, payload_size);

  return true;
}

bool B2500Codec::parse_timer_info3(uint8_t *data, uint16_t data_len, TimerInfoPacket3 &payload) {
  return this->parse_timer_info_base(data, data_len, &payload, sizeof(TimerInfoPacket3));
}

bool B2500Codec::parse_timer_info5(uint8_t *data, uint16_t data_len, TimerInfoPacket &payload) {
  return this->parse_timer_info_base(data, data_len, &payload, sizeof(TimerInfoPacket));
}

bool B2500Codec::parse_timer_info(uint8_t *data, uint16_t data_len, TimerInfoPacket &payload) {
  const size_t size_timer_info_packet3 = sizeof(TimerInfoPacket3);
  const size_t size_timer_info_packet5 = sizeof(TimerInfoPacket);

  if (data_len < sizeof(B2500PacketHeader) + size_timer_info_packet3) {
    ESP_LOGW(TAG, "Packet too short to be a valid TimerInfoPacket");
    ESP_LOGW(TAG, "data: %s", format_hex_pretty(data, data_len).c_str());
    return false;
  }

  if (data_len >= sizeof(B2500PacketHeader) + size_timer_info_packet5) {
    // Handle TimerInfoPacket
    return this->parse_timer_info5(data, data_len, payload);
  } else if (data_len >= sizeof(B2500PacketHeader) + size_timer_info_packet3) {
    // Handle TimerInfoPacket3 and populate TimerInfoPacket
    if (!this->parse_timer_info3(data, data_len, payload.base)) {
      return false;
    }

    // Zero out additional timers
    std::memset(payload.additional_timers, 0, sizeof(payload.additional_timers));
    for (size_t i = 0; i < sizeof(payload.additional_timers) / sizeof(payload.additional_timers[0]); i++) {
      // Set End time to 23:59 and power to 800W (default values)
      payload.additional_timers[i].end.hour = 23;
      payload.additional_timers[i].end.minute = 59;
      payload.additional_timers[i].output_power = 800;
    }
    return true;
  }

  ESP_LOGW(TAG, "Unexpected packet size: %d", data_len);
  return false;
}

bool B2500Codec::encode_command(B2500Command command, const uint8_t *data, uint16_t data_len,
                                std::vector<uint8_t> &payload) {
  // Payload format:
  // 0x73 | <len> | 0x23 | <command> | <data> | <sum>
  if (data == nullptr && data_len > 0) {
    ESP_LOGW(TAG, "Data pointer is null with non-zero data length");
    return false;
  }

  constexpr size_t header_len = 4;
  constexpr size_t checksum_len = 1;
  size_t len = header_len + data_len + checksum_len;

  if (len > UINT16_MAX) {
    ESP_LOGW(TAG, "Encoded command length exceeds maximum limit");
    return false;
  }

  payload.resize(len);
  payload[0] = 0x73;
  payload[1] = static_cast<uint8_t>(len);
  payload[2] = 0x23;
  payload[3] = static_cast<uint8_t>(command);
  if (data_len > 0) {
    std::memcpy(payload.data() + header_len, data, data_len);
  }

  int8_t rxor = 0;
  for (size_t i = 0; i < len - 1; i++) {
    rxor ^= payload[i];
  }
  payload[len - 1] = rxor;

  return true;
}

bool B2500Codec::encode_simple_command(B2500Command command, std::vector<uint8_t> &payload) {
  uint8_t data = 0x01;
  if (command == CMD_GET_TIMERS || command == CMD_ENABLE_ADAPTIVE_MODE) {
    data = 0x00;
  }
  return this->encode_command(command, &data, sizeof(data), payload);
}

bool B2500Codec::encode_load_first_enabled(const ChargeMode &charge_mode, std::vector<uint8_t> &payload) {
  return this->encode_command(CMD_LOAD_FIRST_ENABLED, &charge_mode.byte, sizeof(charge_mode.byte), payload);
}

bool B2500Codec::encode_out_active(const DisChargeSetting &discharge_setting, std::vector<uint8_t> &payload) {
  return this->encode_command(CMD_POWER_OUT, &discharge_setting.byte, sizeof(discharge_setting.byte), payload);
}

bool B2500Codec::encode_discharge_threshold(uint16_t threshold, std::vector<uint8_t> &payload) {
  return this->encode_command(CMD_SET_DISCHARGE_THRESHOLD, reinterpret_cast<uint8_t *>(&threshold), sizeof(threshold),
                              payload);
}

bool B2500Codec::encode_dod(uint8_t dod, std::vector<uint8_t> &payload) {
  return this->encode_command(CMD_SET_DOD, &dod, sizeof(dod), payload);
}

bool B2500Codec::encode_timers(const TimerInfo *timers, size_t count, std::vector<uint8_t> &payload) {
    return this->encode_command(CMD_SET_TIMERS, reinterpret_cast<const uint8_t *>(timers), sizeof(TimerInfo) * count, payload);
}

bool B2500Codec::encode_set_datetime(const DateTimePacket &datetime, std::vector<uint8_t> &payload) {
  return this->encode_command(CMD_SET_DATETIME, reinterpret_cast<const uint8_t *>(&datetime), sizeof(DateTimePacket),
                              payload);
}

bool B2500Codec::encode_set_wifi(const std::string &ssid, const std::string &password, std::vector<uint8_t> &payload) {
  // Validate the SSID and password
  if (ssid.empty() || password.empty()) {
    ESP_LOGW(TAG, "SSID or password is empty");
    return false;
  }
  // Calculate string to send:
  std::string wifi_info = ssid + "<.,.>" + password;
  return this->encode_command(CMD_SET_WIFI, reinterpret_cast<const uint8_t *>(wifi_info.c_str()), wifi_info.size(),
                              payload);
}

bool B2500Codec::encode_set_mqtt(bool ssl, const std::string &host, uint16_t port, const std::string &username,
                                 const std::string &password, std::vector<uint8_t> &payload) {
  // Validate the host, username, and password
  if (host.empty() || username.empty() || password.empty()) {
    ESP_LOGW(TAG, "Host, username, or password is empty");
    return false;
  }
  if (port == 0) {
    ESP_LOGW(TAG, "Invalid port number");
    return false;
  }

  std::string mqtt_info =
      std::to_string(ssl) + "<.,.>" + host + "<.,.>" + std::to_string(port) + "<.,.>" + username + "<.,.>" + password;
  return this->encode_command(CMD_SET_MQTT, reinterpret_cast<const uint8_t *>(mqtt_info.c_str()), mqtt_info.size(),
                              payload);
}

}  // namespace b2500
}  // namespace esphome
