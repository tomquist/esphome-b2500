#pragma once

// The text protocol Hame/Marstek devices speak over MQTT (and that the MCU
// publishes through its FC41D): status as "key=value,key=value,...", commands
// as "cd=<n>,key=value,...". Plain C++, host-tested in tests/hame_device.

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace esphome::hame_device {

using KvMap = std::map<std::string, std::string>;

/// Parse "k1=v1,k2=v2". Whitespace around keys/values is dropped, empty items
/// and items without '=' are skipped. Returns false if nothing was parsed.
bool parse_kv(const std::string &payload, KvMap &out);

/// Looks up the current value of a device key; nullptr when unknown.
using KeyLookup = std::function<const std::string *(const std::string &key)>;

/// Fill a command template:
///   {value}        the value being set (number, select code, ...)
///   {key}          the device's current value of `key`, e.g. {a1}
///   {key|default}  the same, or `default` while the key is unknown
///   [ ... ]        optional section: left out when a key inside is unknown
///                  (e.g. schedule periods the device does not report)
///   {{ }} [[ ]]    literal braces / brackets
/// Returns false (and names the key in *missing) if a key without default is
/// unknown, so no command goes out with a hole in it.
bool render_template(const std::string &tpl, const std::string &value, const KeyLookup &lookup, std::string &out,
                     std::string *missing = nullptr);

/// Which messages are of a type: all of `required` present, none of
/// `excluded`, and exactly `key_count` keys if non-zero.
struct MessageRule {
  std::vector<std::string> required;
  std::vector<std::string> excluded;
  size_t key_count{0};
  bool matches(const KvMap &msg) const;
};

/// "8:5" -> "08:05" (device report to display); false if not H:M.
bool pad_time(const std::string &in, std::string &out);
/// "08:05" -> "8:5" (display to device command); false if not a valid time.
bool unpad_time(const std::string &in, std::string &out);

/// How an entity reads a number out of a message.
struct NumberField {
  /// How several keys combine. SUM needs every key; the others use the keys
  /// that are present (and non-zero with ignore_zero), at least one.
  enum Aggregate : uint8_t { SUM, MIN, MAX, AVERAGE, SPREAD };

  std::vector<const char *> keys;  ///< more than one: combined by `aggregate` (literals, kept in flash)
  Aggregate aggregate{SUM};
  bool ignore_zero{false};
  int bit{-1};                    ///< >= 0: take bits [bit, bit + bits) of the integer value
  int bits{1};
  bool use_min{false}, use_max{false};
  double min{0}, max{0};          ///< outside: no value (the devices' "no reading" sentinels)
  double multiply{1};

  void add_key(const char *k) { this->keys.push_back(k); }
  void set_bit(int b, int n) {
    this->bit = b;
    this->bits = n;
  }
  void set_valid_min(double v) {
    this->use_min = true;
    this->min = v;
  }
  void set_valid_max(double v) {
    this->use_max = true;
    this->max = v;
  }
  void set_multiply(double m) { this->multiply = m; }
  void set_aggregate(Aggregate a, bool skip_zero) {
    this->aggregate = a;
    this->ignore_zero = skip_zero;
  }

  /// True if every key is in `msg`, parses and is in range.
  bool extract(const KvMap &msg, double &out) const;
  /// True if `msg` carries any of the keys.
  bool touches(const KvMap &msg) const;
};

/// Strict number parse ("12", "-3.5"); false for empty or trailing garbage.
bool parse_number(const std::string &s, double &out);

/// Format a number the way the devices expect it: no decimals when integral.
std::string format_number(double v);

}  // namespace esphome::hame_device
