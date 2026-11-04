#pragma once

// Entities of a hame_device. Each reads one or more keys of the device's
// status messages and, for controls, sends a command template.

#include "esphome/core/defines.h"

#include <string>
#include <utility>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "hame_device.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
#ifdef USE_TEXT
#include "esphome/components/text/text.h"
#endif

namespace esphome::hame_device {

/// Reads a boolean: bit `bit` of the value (if set), else "on" when the value
/// is one of `on_values` (if given), else when it is a non-zero number.
class BoolField {
 public:
  void set_key(const char *k) { this->key_ = k; }
  void set_bit(int bit) { this->bit_ = bit; }
  void add_on_value(const char *v) { this->on_values_.push_back(v); }
  bool extract(const KvMap &msg, bool &out) const {
    auto it = msg.find(this->key_);
    if (it == msg.end())
      return false;
    if (!this->on_values_.empty()) {
      out = false;
      for (const auto &v : this->on_values_)
        out |= it->second == v;
      return true;
    }
    double v;
    if (!parse_number(it->second, v))
      return false;
    const long long raw = static_cast<long long>(v);
    out = this->bit_ >= 0 ? ((raw >> this->bit_) & 1) != 0 : raw != 0;
    return true;
  }

 protected:
  const char *key_{""};
  int bit_{-1};
  std::vector<const char *> on_values_;  // literals from the config, kept in flash
};

#ifdef USE_SENSOR
class HameDeviceSensor : public sensor::Sensor, public HameDeviceListener {
 public:
  NumberField &field() { return this->field_; }
  /// For daily counters: a single lower reading is a glitch and ignored; the
  /// second one in a row is the daily reset.
  void set_monotonic(bool m) { this->monotonic_ = m; }
  void on_message(const KvMap &msg) override {
    double v;
    if (!this->field_.touches(msg) || !this->field_.extract(msg, v))
      return;
    if (this->monotonic_ && this->has_last_ && v < this->last_ && !this->dropped_once_) {
      this->dropped_once_ = true;
      return;
    }
    this->dropped_once_ = false;
    const bool changed = !this->has_last_ || v != this->last_;
    this->has_last_ = true;
    this->last_ = v;
    // Only changes: a status message updates dozens of entities at once.
    if (changed)
      this->publish_state(static_cast<float>(v));
  }

 protected:
  NumberField field_;
  bool monotonic_{false};
  bool has_last_{false};
  bool dropped_once_{false};
  double last_{0};
};
#endif

#ifdef USE_BINARY_SENSOR
class HameDeviceBinarySensor : public binary_sensor::BinarySensor, public HameDeviceListener {
 public:
  BoolField &field() { return this->field_; }
  void on_message(const KvMap &msg) override {
    bool v;
    if (this->field_.extract(msg, v) && (!this->has_state() || v != this->state))
      this->publish_state(v);
  }

 protected:
  BoolField field_;
};
#endif

#ifdef USE_TEXT_SENSOR
/// The raw value, or its label from `map`.
class HameDeviceTextSensor : public text_sensor::TextSensor, public HameDeviceListener {
 public:
  void set_key(const char *k) { this->key_ = k; }
  void add_mapping(const char *raw, const char *label) { this->map_.emplace_back(raw, label); }
  /// Shown for values `map` does not list (default: "unknown (<raw>)").
  void set_default(const char *d) { this->default_ = d; }
  /// Show "H:M" values as "HH:MM".
  void set_time(bool t) { this->time_ = t; }
  void on_message(const KvMap &msg) override {
    auto it = msg.find(this->key_);
    if (it == msg.end())
      return;
    std::string out;
    bool mapped = false;
    for (const auto &m : this->map_) {
      if (m.first == it->second) {
        out = m.second;
        mapped = true;
        break;
      }
    }
    if (!mapped) {
      std::string padded;
      if (this->time_ && pad_time(it->second, padded)) {
        out = padded;
      } else if (this->default_ != nullptr) {
        out = this->default_;
      } else {
        out = this->map_.empty() ? it->second : "unknown (" + it->second + ")";
      }
    }
    if (!this->published_ || out != this->state) {
      this->published_ = true;
      this->publish_state(out);
    }
  }

 protected:
  const char *key_{""};
  std::vector<std::pair<const char *, const char *>> map_;
  const char *default_{nullptr};
  bool time_{false};
  bool published_{false};
};
#endif

#ifdef USE_NUMBER
/// State from `key` (times `multiply`); setting sends `command` with {value}
/// = the new value divided by `multiply`.
class HameDeviceNumber : public number::Number, public HameDeviceListener, public Parented<HameDevice> {
 public:
  NumberField &field() { return this->field_; }
  void set_command(const char *c) { this->command_ = c; }
  void on_message(const KvMap &msg) override {
    double v;
    if (this->field_.touches(msg) && this->field_.extract(msg, v) && (!this->published_ || v != this->last_)) {
      this->published_ = true;
      this->last_ = v;
      this->publish_state(static_cast<float>(v));
    }
  }

 protected:
  bool published_{false};
  double last_{0};
  void control(float value) override {
    const double raw = value / (this->field_.multiply != 0 ? this->field_.multiply : 1.0);
    if (this->parent_->send_command(this->command_, format_number(raw), this->message_)) {
      this->published_ = true;
      this->last_ = value;
      this->publish_state(value);
    }
  }
  NumberField field_;
  const char *command_{""};
};
#endif

#ifdef USE_SELECT
/// Options map device codes to labels; selecting sends `command` with
/// {value} = the code.
class HameDeviceSelect : public select::Select, public HameDeviceListener, public Parented<HameDevice> {
 public:
  void set_key(const char *k) { this->key_ = k; }
  void set_command(const char *c) { this->command_ = c; }
  void add_option_code(const char *code, const char *label) { this->codes_.emplace_back(code, label); }
  void on_message(const KvMap &msg) override {
    auto it = msg.find(this->key_);
    if (it == msg.end())
      return;
    for (const auto &c : this->codes_) {
      if (c.first == it->second) {
        if (c.second != this->last_) {
          this->last_ = c.second;
          this->publish_state(c.second);
        }
        return;
      }
    }
  }

 protected:
  void control(const std::string &label) override {
    for (const auto &c : this->codes_) {
      if (c.second == label) {
        if (this->parent_->send_command(this->command_, c.first, this->message_)) {
          this->last_ = c.second;
          this->publish_state(label);
        }
        return;
      }
    }
  }
  const char *key_{""};
  const char *command_{""};
  const char *last_{nullptr};  ///< last published label
  std::vector<std::pair<const char *, const char *>> codes_;
};
#endif

#ifdef USE_SWITCH
class HameDeviceSwitch : public switch_::Switch, public HameDeviceListener, public Parented<HameDevice> {
 public:
  BoolField &field() { return this->field_; }
  void set_commands(const char *on, const char *off) {
    this->command_on_ = on;
    this->command_off_ = off;
  }
  void on_message(const KvMap &msg) override {
    bool v;
    if (this->field_.extract(msg, v) && (!this->published_ || v != this->state)) {
      this->published_ = true;
      this->publish_state(v);
    }
  }

 protected:
  bool published_{false};
  void write_state(bool state) override {
    if (this->parent_->send_command(state ? this->command_on_ : this->command_off_, state ? "1" : "0", this->message_))
      this->publish_state(state);
  }
  BoolField field_;
  const char *command_on_{""};
  const char *command_off_{""};
};
#endif

#ifdef USE_BUTTON
class HameDeviceButton : public button::Button, public Parented<HameDevice> {
 public:
  void set_command(const char *c) { this->command_ = c; }
  void set_message(const std::string &m) { this->message_ = m; }

 protected:
  void press_action() override { this->parent_->send_command(this->command_, "", this->message_); }
  const char *command_{""};
  std::string message_;
};
#endif

#ifdef USE_TEXT
/// A settable text, e.g. a schedule time. With `time`, the device's "H:M"
/// shows as "HH:MM" and an entered "HH:MM" goes out as "H:M".
class HameDeviceText : public text::Text, public HameDeviceListener, public Parented<HameDevice> {
 public:
  void set_key(const char *k) { this->key_ = k; }
  void set_command(const char *c) { this->command_ = c; }
  void set_time(bool t) { this->time_ = t; }
  void on_message(const KvMap &msg) override {
    auto it = msg.find(this->key_);
    if (it == msg.end())
      return;
    std::string padded;
    std::string out = this->time_ && pad_time(it->second, padded) ? padded : it->second;
    if (!this->published_ || out != this->state) {
      this->published_ = true;
      this->publish_state(out);
    }
  }

 protected:
  void control(const std::string &value) override {
    std::string out = value;
    if (this->time_ && !unpad_time(value, out))
      return;  // not a time of day: refused
    if (this->parent_->send_command(this->command_, out, this->message_))
      this->publish_state(value);
  }
  const char *key_{""};
  const char *command_{""};
  bool time_{false};
  bool published_{false};
};
#endif

}  // namespace esphome::hame_device
