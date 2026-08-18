#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace esphome {

inline std::string format_hex_pretty(const uint8_t *data, size_t length) {
  std::string result;
  char byte[3];
  for (size_t i = 0; i < length; i++) {
    if (i != 0) {
      result += '.';
    }
    snprintf(byte, sizeof(byte), "%02X", data[i]);
    result += byte;
  }
  return result;
}

}  // namespace esphome
