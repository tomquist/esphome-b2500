#pragma once

// hame_at: ESPHome firmware for the Quectel FC41D inside Hame/Marstek batteries.
// Emulates the module's AT firmware towards the battery MCU on the UART
// (Fc41dEmulator) and backs it with ESPHome:
//   - Wi-Fi status from the ESPHome station,
//   - BLE through bk72xx_ble_server (the phone app keeps working),
//   - MQTT terminated locally, optionally forwarded to the Hame cloud with
//     the MCU's own broker, credentials and certificates (on by default),
//   - HTTP: local answers for the Hame endpoints when the cloud is off, real
//     requests otherwise and always for LAN devices (CT/P1 meters),
//   - automation triggers/actions for the MCU's messages.

#include "esphome/core/defines.h"

#include <map>
#include <string>
#include <vector>

#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif
#ifdef USE_BK72XX_BLE_SERVER
#include "esphome/components/bk72xx_ble_server/bk72xx_ble_server.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include "cloud_link.h"
#include "fc41d_emulator.h"
#include "hame_cloud.h"
#include "net_worker.h"

namespace esphome::hame_at {

class HameAtComponent : public Component, public uart::UARTDevice, public Fc41dBackend {
 public:
  // --- configuration ---
  void set_module_version(const std::string &v) { this->emulator_.set_module_version(v); }
  void set_ble_mac(const std::string &mac) { this->ble_mac_ = mac; }
  void set_cloud_enabled(bool enabled);
  void set_allow_mcu_ota(bool allow) { this->allow_mcu_ota_ = allow; }
  /// What reaches the real cloud while cloud access is on (the master switch);
  /// with it off everything is answered locally.
  void set_mqtt_mode(CloudMode mode) { this->mqtt_mode_ = mode; }
  void set_http_mode(HameEndpoint endpoint, CloudMode mode);
#ifdef USE_TIME
  void set_time(time::RealTimeClock *t) { this->time_ = t; }
#endif
#ifdef USE_BK72XX_BLE_SERVER
  void set_ble_server(bk72xx_ble_server::BK72xxBLEServer *server) { this->ble_ = server; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_cloud_status_sensor(text_sensor::TextSensor *s) { this->cloud_status_ = s; }
#endif

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  bool is_cloud_enabled() const { return this->cloud_enabled_; }
  Fc41dEmulator &emulator() { return this->emulator_; }

  /// Deliver a message to the MCU as if it came from its broker. Empty topic:
  /// the MCU's first subscription (its App control topic).
  void send_to_mcu(const std::string &payload, const std::string &topic = "");
  /// Write to the MCU as a BLE central would.
  void ble_write_to_mcu(const std::vector<uint8_t> &data, uint16_t uuid = 0xFF01);

  Trigger<std::string, std::string> *get_mcu_publish_trigger() { return &this->on_mcu_publish_; }
  /// C++ consumers (e.g. hame_device) of the MCU's MQTT publishes.
  void add_on_mcu_publish_callback(std::function<void(const std::string &, const std::string &)> &&cb) {
    this->publish_callbacks_.add(std::move(cb));
  }
  /// The MCU has an MQTT session with a subscription, so send_to_mcu() works.
  /// The MCU listens for commands: it has subscribed, a few seconds ago, and
  /// is not waiting for an HTTP answer. A B2500 (HMA V232) ignores commands
  /// for ~3 s after subscribing and while its HTTP request is in flight.
  bool mcu_ready() {
    return !this->emulator_.mqtt_topics().empty() && millis() - this->mcu_subscribed_ms_ >= MCU_SETTLE_MS &&
           !this->emulator_.http_busy() && this->emulator_.engine().get_mode() == AtEngine::Mode::LINE;
  }
  Trigger<std::vector<uint8_t>> *get_ble_notify_trigger() { return &this->on_ble_notify_; }
  /// C++ consumers (e.g. a B2500 decoder) of MCU notifications.
  void add_on_ble_notify_callback(std::function<void(uint16_t, const std::vector<uint8_t> &)> &&cb) {
    this->notify_callbacks_.add(std::move(cb));
  }

  // --- Fc41dBackend ---
  WifiStatus wifi_status() override;
  void wifi_credentials(const std::string &ssid, const std::string &password) override;
  void ble_advertise(const std::string &name, uint16_t service, const std::vector<uint16_t> &chars) override;
  void ble_stop_advertising() override;
  void ble_notify(uint16_t uuid, const std::vector<uint8_t> &data) override;
  void mqtt_connected(const MqttSession &session) override;
  void mqtt_closed() override;
  void mqtt_subscribed(const std::string &topic) override;
  void mqtt_publish(const std::string &topic, const std::string &payload, bool retain) override;
  bool mqtt_broker_echoes() override;
  void http_request(const HttpRequest &request, std::function<void(HttpResult)> done) override;
  void socket_open(const SocketSpec &spec, std::function<void(int)> done) override;
  void socket_send(int id, const std::string &data, const std::string &ip, int port) override;
  void socket_close(int id) override;
  void ssl_cert(const std::string &kind, const std::vector<uint8_t> &data) override;
  void module_restart() override;

 protected:
  static constexpr uint32_t MCU_SETTLE_MS = 5000;
  /// cloud_access "auto": wait this long for the cloud before answering locally.
  static constexpr uint32_t AUTO_HTTP_WAIT_MS = 3000;
  uint32_t mcu_subscribed_ms_{0};
  struct LiveSocket {
    SocketSpec spec;
    int fd{-1};
    bool connecting{false};
    uint32_t started{0};
    std::function<void(int)> open_done;
  };

  bool local_time_(LocalTime &out);
  CloudMode http_mode_(HameEndpoint endpoint) const;
  bool mqtt_forwarding_() const { return this->cloud_enabled_ && this->mqtt_mode_ != CloudMode::LOCAL; }
  void answer_locally_(const HttpRequest &request, std::function<void(HttpResult)> done);
  void forward_http_(const HttpRequest &request, std::function<void(HttpResult)> done);
  void service_sockets_();
  void publish_cloud_status_();
#ifdef USE_BK72XX
  void kick_uart_fifo_(uint32_t now);
  uint32_t fifo_pending_since_{0};
  uint32_t fifo_kicks_{0};
#endif

  Fc41dEmulator emulator_{this};
  NetWorker worker_;
  std::string ble_mac_;
  bool cloud_enabled_{true};
  bool allow_mcu_ota_{false};
  CloudMode mqtt_mode_{CloudMode::FORWARD};
  CloudMode time_mode_{CloudMode::AUTO};
  CloudMode config_mode_{CloudMode::AUTO};
  CloudMode reports_mode_{CloudMode::FORWARD};
  bool worker_started_{false};
  bool network_up_{false};
  uint32_t next_http_id_{1};
  std::map<uint32_t, std::function<void(HttpResult)>> http_pending_;
  std::map<int, LiveSocket> sockets_;
  std::string last_cloud_status_;
  uint32_t last_status_ms_{0};
  uint8_t rx_buf_[256];

#ifdef USE_TIME
  time::RealTimeClock *time_{nullptr};
#endif
#ifdef USE_BK72XX_BLE_SERVER
  bk72xx_ble_server::BK72xxBLEServer *ble_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *cloud_status_{nullptr};
#endif

  Trigger<std::string, std::string> on_mcu_publish_;
  Trigger<std::vector<uint8_t>> on_ble_notify_;
  CallbackManager<void(uint16_t, const std::vector<uint8_t> &)> notify_callbacks_;
  CallbackManager<void(const std::string &, const std::string &)> publish_callbacks_;
};

#ifdef USE_SWITCH
class CloudSwitch : public switch_::Switch, public Parented<HameAtComponent> {
 protected:
  void write_state(bool state) override {
    this->parent_->set_cloud_enabled(state);
    this->publish_state(state);
  }
};
#endif

template<typename... Ts> class SendToMcuAction : public Action<Ts...>, public Parented<HameAtComponent> {
 public:
  TEMPLATABLE_VALUE(std::string, payload)
  TEMPLATABLE_VALUE(std::string, topic)

  void play(const Ts &...x) override {
    this->parent_->send_to_mcu(this->payload_.value(x...), this->topic_.has_value() ? this->topic_.value(x...) : "");
  }
};

template<typename... Ts> class BleWriteAction : public Action<Ts...>, public Parented<HameAtComponent> {
 public:
  TEMPLATABLE_VALUE(std::vector<uint8_t>, data)

  void play(const Ts &...x) override { this->parent_->ble_write_to_mcu(this->data_.value(x...)); }
};

}  // namespace esphome::hame_at
