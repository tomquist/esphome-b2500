// Tests for b2500::hex_dump, the packet formatter for log lines.
//
// ESPHome 2026.11.0 moved the std::string format_hex_pretty() the component
// used to log packets with out of helpers.h (issue #335); hex_dump keeps the
// same output on top of format_hex_pretty_to(). Compiled and run on the host by test_codec.py,
// using the ESPHome stubs in stubs/.

#include "b2500_codec.h"

#include <cstdio>
#include <string>
#include <vector>

using esphome::b2500::hex_dump;

static int failures = 0;

static void expect(const char *name, const std::vector<uint8_t> &data, const std::string &expected) {
  std::string got = hex_dump(data.data(), data.size());
  printf("--- %s: %s\n", name, got.c_str());
  if (got != expected) {
    printf("FAIL: %s: expected '%s', got '%s'\n", name, expected.c_str(), got.c_str());
    failures++;
  }
  if (got.size() != strlen(got.c_str())) {
    printf("FAIL: %s: embedded NUL in the formatted string\n", name);
    failures++;
  }
}

int main() {
  expect("empty", {}, "");
  expect("one byte", {0x0A}, "0A (1)");
  expect("header", {0x73, 0x04, 0x23, 0x09}, "73.04.23.09 (4)");
  expect("full range", {0x00, 0x7F, 0x80, 0xFF}, "00.7F.80.FF (4)");

  // A whole BLE packet is formatted, not truncated.
  std::vector<uint8_t> large(250);
  for (size_t i = 0; i < large.size(); i++)
    large[i] = static_cast<uint8_t>(i);
  std::string expected;
  char byte[4];
  for (size_t i = 0; i < large.size(); i++) {
    snprintf(byte, sizeof(byte), i == 0 ? "%02X" : ".%02X", large[i]);
    expected += byte;
  }
  expected += " (250)";
  expect("250 bytes", large, expected);

  if (failures != 0) {
    printf("%d assertion(s) failed\n", failures);
    return 1;
  }
  printf("all assertions passed\n");
  return 0;
}
