#include "hame_device.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::hame_device {

static const char *const TAG = "hame_device";

#ifdef USE_HAME_AT
void HameDevice::set_hame_at(hame_at::HameAtComponent *at) {
  // Status the MCU publishes reaches us; commands go to it as if from its
  // broker, on its first subscription (the App control topic).
  at->add_on_mcu_publish_callback(
      [this](const std::string &topic, const std::string &payload) { this->feed(payload, topic); });
  this->sender_ = [at](const std::string &payload) {
    if (!at->mcu_ready())
      return false;
    at->send_to_mcu(payload);
    return true;
  };
  if (this->topic_filter_.empty())
    this->topic_filter_ = "/device/";  // the MCU's status topic: <prefix>/<type>/device/<id>/ctrl
}
#endif

HameDevice::MessageType *HameDevice::type_(const std::string &name) {
  if (this->types_.empty())
    return &this->any_;
  if (name.empty())
    return &this->types_.front();
  for (auto &t : this->types_) {
    if (t.name == name)
      return &t;
  }
  return nullptr;
}

const HameDevice::MessageType *HameDevice::type_(const std::string &name) const {
  return const_cast<HameDevice *>(this)->type_(name);
}

void HameDevice::setup() {
  for (auto *l : this->listeners_) {
    if (this->type_(l->get_message()) == nullptr)
      ESP_LOGE(TAG, "An entity uses message type '%s', which is not configured", l->get_message());
  }
  // First poll soon after boot; until the transport is ready it is retried.
  this->poll();
  this->next_send_ms_ = millis() + 5000;
}

void HameDevice::loop() {
  const uint32_t now = millis();
  if (static_cast<int32_t>(now - this->next_send_ms_) < 0)
    return;
  // At most one request at a time, spaced out, so the replies do not collide.
  for (auto &t : this->types_) {
    if (t.poll.empty())
      continue;
    const bool interval_due = t.interval_ms > 0 && now - t.last_ms >= t.interval_ms;
    if (!t.due && !interval_due)
      continue;
    if (this->sender_ && this->sender_(t.poll)) {
      t.due = false;
      t.last_ms = now;
      this->next_send_ms_ = now + POLL_SPACING_MS;
    } else {
      t.due = true;  // transport not ready: try again shortly
      this->next_send_ms_ = now + RETRY_MS;
      ESP_LOGV(TAG, "Poll '%s' not sent: transport not ready", t.poll.c_str());
    }
    return;
  }
}

void HameDevice::poll() {
  for (auto &t : this->types_)
    t.due = !t.poll.empty();
}

void HameDevice::feed(const std::string &payload, const std::string &topic) {
  if (!this->topic_filter_.empty() && !topic.empty() && topic.find(this->topic_filter_) == std::string::npos)
    return;
  KvMap msg;
  if (!parse_kv(payload, msg))
    return;
  MessageType *type = nullptr;
  if (this->types_.empty()) {
    type = &this->any_;
  } else {
    for (auto &t : this->types_) {
      if (t.rule.matches(msg)) {
        type = &t;
        break;
      }
    }
  }
  if (type == nullptr) {
    ESP_LOGV(TAG, "Message of no configured type (%u keys)", static_cast<unsigned>(msg.size()));
    return;
  }
  this->messages_++;
  for (const auto &kv : msg) {
    if (this->kept_(kv.first))
      type->values[kv.first] = kv.second;
  }
  ESP_LOGV(TAG, "'%s' message, %u keys", type->name.c_str(), static_cast<unsigned>(msg.size()));
  for (auto *l : this->listeners_) {
    if (this->type_(l->get_message()) == type)
      l->on_message(msg);
  }
  this->on_message_.trigger(payload);
}

const std::string *HameDevice::get(const std::string &key, const std::string &message) const {
  const MessageType *first = this->type_(message);
  if (first != nullptr) {
    auto it = first->values.find(key);
    if (it != first->values.end())
      return &it->second;
  }
  for (const auto &t : this->types_) {
    auto it = t.values.find(key);
    if (it != t.values.end())
      return &it->second;
  }
  return nullptr;
}

bool HameDevice::send_command(const std::string &tpl, const std::string &value, const std::string &message) {
  std::string payload, missing;
  if (!render_template(
          tpl, value, [this, &message](const std::string &key) { return this->get(key, message); }, payload,
          &missing)) {
    ESP_LOGW(TAG, "Command '%s' not sent: the device has not reported '%s' yet", tpl.c_str(), missing.c_str());
    return false;
  }
  if (!this->sender_ || !this->sender_(payload)) {
    ESP_LOGW(TAG, "Command '%s' not sent: no connection to the device", payload.c_str());
    return false;
  }
  ESP_LOGD(TAG, "Command: %s", payload.c_str());
  this->on_command_.trigger(payload);
  MessageType *t = this->type_(message);
  if (this->poll_after_command_ms_ > 0 && t != nullptr && !t->poll.empty()) {
    // Read the result back.
    t->due = true;
    this->next_send_ms_ = millis() + this->poll_after_command_ms_;
  }
  return true;
}

void HameDevice::dump_config() {
  ESP_LOGCONFIG(TAG, "Hame device:");
  for (const auto &t : this->types_) {
    ESP_LOGCONFIG(TAG, "  Message '%s': poll '%s' every %u s", t.name.c_str(), t.poll.c_str(),
                  static_cast<unsigned>(t.interval_ms / 1000));
  }
  if (!this->topic_filter_.empty())
    ESP_LOGCONFIG(TAG, "  Status topic contains: '%s'", this->topic_filter_.c_str());
  ESP_LOGCONFIG(TAG, "  Entities: %u", static_cast<unsigned>(this->listeners_.size()));
  if (this->keep_all_keys_) {
    ESP_LOGCONFIG(TAG, "  Keeps the values of all keys");
  } else {
    ESP_LOGCONFIG(TAG, "  Keeps the values of %u keys (for commands)", static_cast<unsigned>(this->kept_keys_.size()));
  }
}

}  // namespace esphome::hame_device
