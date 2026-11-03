#pragma once

// Local stand-in for the Hame (hamedata.com) HTTP endpoints, used when cloud
// forwarding is off. Answers what Marsrelay (github.com/tomquist/marsrelay)
// and the hm2500 firmware emulator's offline cloud answer: the battery needs
// some of these to work properly. Without the clock reply's trailing fields a
// B2500 rejects the date and keeps asking; getDeviceFire carries its output
// limits. Platform independent and host-tested.

#include <cstdint>
#include <string>

namespace esphome::hame_at {

struct LocalTime {
  int year, month, day, hour, minute, second;
};

/// True if `url` points at a Hame cloud host (*.hamedata.com, marstekcloud.com).
bool is_hame_cloud_url(const std::string &url);

/// What a Hame cloud request is for; each kind can be forwarded or answered
/// locally on its own.
enum class HameEndpoint : uint8_t {
  TIME,           // getDateInfo*.php: the battery's clock
  DEVICE_CONFIG,  // getDeviceFire (output limits), getRealtimeSoc, schedules, weather, ...
  REPORTS,        // statistics and error uploads (put_tenmins_v2, puterrinfo, set...Report[ing])
  FIRMWARE,       // MCU firmware images
};

/// How requests of one kind are handled.
enum class CloudMode : uint8_t {
  FORWARD,  // to the real cloud
  LOCAL,    // answered locally (Marsrelay-compatible)
  AUTO,     // to the real cloud; answered locally when that fails
};

HameEndpoint classify_hame_url(const std::string &url);
const char *hame_endpoint_name(HameEndpoint e);

/// True if `url` downloads a firmware image for the MCU.
bool is_mcu_firmware_url(const std::string &url);

/// Answer a request to the Hame cloud locally. `now` is the local time for the
/// clock reply (nullptr: unknown). Returns false if no answer can be given
/// (the clock reply without a valid time); `status` 404 for unknown paths.
bool local_hame_answer(const std::string &method, const std::string &url, const std::string &body,
                       const LocalTime *now, int &status, std::string &response);

}  // namespace esphome::hame_at
