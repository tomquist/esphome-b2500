#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace esphome {

// Mirrors esphome/core/helpers.{h,cpp} as of 2026.11.0, which moved the
// std::string format_hex_pretty() out to alloc_helpers.h. Only the buffer-based
// API is stubbed, so the host build fails the same way the firmware build would
// if the component goes back to calling it through helpers.h.

constexpr size_t format_hex_pretty_size(size_t byte_count) { return byte_count * 3; }

inline char *format_hex_pretty_to(char *buffer, size_t buffer_size, const uint8_t *data, size_t length,
                                  char separator = ':') {
  static const char HEX_CHARS[] = "0123456789ABCDEF";
  if (length == 0 || buffer_size == 0) {
    if (buffer_size > 0)
      buffer[0] = '\0';
    return buffer;
  }
  size_t stride = separator ? 3 : 2;
  size_t max_bytes = separator ? (buffer_size / 3) : ((buffer_size - 1) / 2);
  if (max_bytes == 0) {
    buffer[0] = '\0';
    return buffer;
  }
  length = std::min(length, max_bytes);
  for (size_t i = 0; i < length; i++) {
    size_t pos = i * stride;
    buffer[pos] = HEX_CHARS[data[i] >> 4];
    buffer[pos + 1] = HEX_CHARS[data[i] & 0x0F];
    if (separator)
      buffer[pos + 2] = separator;
  }
  buffer[length * stride - (separator ? 1 : 0)] = '\0';
  return buffer;
}

}  // namespace esphome
