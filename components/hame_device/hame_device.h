#pragma once

// hame_device: a Hame/Marstek device described in YAML.
//
// The hub knows nothing about a particular device. It is fed the device's
// status messages ("key=value,..."), keeps the latest value of every key,
// hands each message to its entities and sends commands built from templates
// ("cd=4,md={value}", "cd=20,md=0,a1={a1},...,v1={value}"). Which keys mean
// what lives in YAML (examples/fc41d/devices/*.yaml), so a new device family
// is a new YAML package, not new C++.
//
// Transport-agnostic: feed() and set_sender() connect it to anything. With
// `hame_at_id` it is wired to the module emulator (the battery MCU's MQTT
// session on the FC41D).

#include "esphome/core/defines.h"

#include <functional>
#include <string>
#include <vector>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"

#include "kv_message.h"

#ifdef USE_HAME_AT
#include "esphome/components/hame_at/hame_at.h"
#endif

namespace esphome::hame_device {

class HameDevice;

/// Something that reads its state from one type of the device's messages.
class HameDeviceListener {
 public:
  virtual ~HameDeviceListener() = default;
  virtual void on_message(const KvMap &msg) = 0;
  /// The message type this entity reads ("" = the hub's first type). Device
  /// families reuse keys across message types, so entities are bound to one.
  void set_message(const char *m) { this->message_ = m; }
  const char *get_message() const { return this->message_; }

 protected:
  const char *message_{""};  // literal from the config, kept in flash
};

class HameDevice : public Component {
 public:
  using Sender = std::function<bool(const std::string &payload)>;

  // --- configuration ---
  void set_sender(Sender sender) { this->sender_ = std::move(sender); }
  void set_topic_filter(const std::string &f) { this->topic_filter_ = f; }
  /// A message type: recognised by `rule`, requested with `poll` (if not
  /// empty) every `interval_ms`. Without any, every message is one type.
  void add_message(const std::string &name, const MessageRule &rule, const std::string &poll, uint32_t interval_ms) {
    this->types_.push_back({name, rule, poll, interval_ms, 0, false, {}});
  }
  void add_message(const std::string &name, const std::vector<std::string> &required,
                   const std::vector<std::string> &excluded, size_t key_count, const std::string &poll,
                   uint32_t interval_ms) {
    MessageRule rule;
    rule.required = required;
    rule.excluded = excluded;
    rule.key_count = key_count;
    this->add_message(name, rule, poll, interval_ms);
  }
  void set_poll_after_command(uint32_t ms) { this->poll_after_command_ms_ = ms; }
#ifdef USE_HAME_AT
  void set_hame_at(hame_at::HameAtComponent *at);
#endif

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  /// A status message from the device (with its topic, if the transport has one).
  void feed(const std::string &payload, const std::string &topic = "");
  /// Send a command built from `tpl` ({value}, {key}, {key|default}, [...]).
  /// Keys are looked up in `message` first (default: the first type).
  bool send_command(const std::string &tpl, const std::string &value = "", const std::string &message = "");
  /// Latest value of a key (of `message` first, then of any type).
  const std::string *get(const std::string &key, const std::string &message = "") const;
  /// Request every message type (one after another, spaced out).
  void poll();

  void add_listener(HameDeviceListener *l) { this->listeners_.push_back(l); }
  Trigger<std::string> *get_message_trigger() { return &this->on_message_; }
  Trigger<std::string> *get_command_trigger() { return &this->on_command_; }

 protected:
  struct MessageType {
    std::string name;
    MessageRule rule;
    std::string poll;
    uint32_t interval_ms;
    uint32_t last_ms;
    bool due;      ///< request it at the next chance
    KvMap values;  ///< latest value of every key of this type
  };

  MessageType *type_(const std::string &name);
  const MessageType *type_(const std::string &name) const;

  Sender sender_;
  std::string topic_filter_;
  std::vector<MessageType> types_;
  MessageType any_{"", {}, "", 0, 0, false, {}};  ///< used when no type is configured
  static constexpr uint32_t POLL_SPACING_MS = 300;
  static constexpr uint32_t RETRY_MS = 3000;
  uint32_t poll_after_command_ms_{1500};
  uint32_t next_send_ms_{0};
  uint32_t messages_{0};
  std::vector<HameDeviceListener *> listeners_;
  Trigger<std::string> on_message_;
  Trigger<std::string> on_command_;
};

template<typename... Ts> class SendCommandAction : public Action<Ts...>, public Parented<HameDevice> {
 public:
  TEMPLATABLE_VALUE(std::string, command)
  TEMPLATABLE_VALUE(std::string, value)
  void play(const Ts &...x) override {
    this->parent_->send_command(this->command_.value(x...), this->value_.has_value() ? this->value_.value(x...) : "");
  }
};

template<typename... Ts> class PollAction : public Action<Ts...>, public Parented<HameDevice> {
 public:
  void play(const Ts &...x) override { this->parent_->poll(); }
};

}  // namespace esphome::hame_device
