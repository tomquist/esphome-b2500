// Regression tests for B2500Codec::parse_wifi_info.
//
// Compiled and run on the host by test_codec.py, using the ESPHome stubs in
// stubs/.

#include "b2500_codec.h"

#include <cstdio>
#include <string>
#include <vector>

using esphome::b2500::B2500Codec;
using esphome::b2500::WifiInfoPacket;

static int failures = 0;

static void expect_parsed(const char *name, std::vector<uint8_t> packet, uint8_t signal, const std::string &ssid) {
  B2500Codec codec;
  WifiInfoPacket payload;
  payload.signal = 0xEE;
  payload.ssid = "previous";

  printf("--- %s\n", name);
  if (!codec.parse_wifi_info(packet.data(), packet.size(), payload)) {
    printf("FAIL: %s: expected the packet to parse\n", name);
    failures++;
    return;
  }
  if (payload.signal != signal) {
    printf("FAIL: %s: expected signal %d, got %d\n", name, signal, payload.signal);
    failures++;
  }
  if (payload.ssid != ssid) {
    printf("FAIL: %s: expected ssid '%s', got '%s'\n", name, ssid.c_str(), payload.ssid.c_str());
    failures++;
  }
}

static void expect_rejected(const char *name, std::vector<uint8_t> packet) {
  B2500Codec codec;
  WifiInfoPacket payload;

  printf("--- %s\n", name);
  if (codec.parse_wifi_info(packet.data(), packet.size(), payload)) {
    printf("FAIL: %s: expected the packet to be rejected\n", name);
    failures++;
  }
}

int main() {
  // A device which is not connected to a Wi-Fi network answers with a
  // header-only packet (see issue #291).
  expect_parsed("header only", {0x73, 0x04, 0x23, 0x09}, 0, "");
  expect_parsed("signal only", {0x73, 0x05, 0x23, 0x09, 0x40}, 0x40, "");
  expect_parsed("signal and separator, no ssid", {0x73, 0x06, 0x23, 0x09, 0x40, 0x00}, 0x40, "");
  expect_parsed("signal and ssid", {0x73, 0x0A, 0x23, 0x09, 0x40, 0x00, 'h', 'o', 'm', 'e'}, 0x40, "home");
  expect_parsed("non-printable ssid", {0x73, 0x08, 0x23, 0x09, 0x40, 0x00, 0x01, 0x02}, 0x40, "");

  expect_rejected("truncated header", {0x73, 0x04, 0x23});
  expect_rejected("wrong command", {0x73, 0x05, 0x23, 0x03, 0x40});

  if (failures != 0) {
    printf("%d assertion(s) failed\n", failures);
    return 1;
  }
  printf("all assertions passed\n");
  return 0;
}
