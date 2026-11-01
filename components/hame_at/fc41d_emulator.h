#pragma once

// Emulates the Quectel FC41D AT firmware (Hame build) towards the battery MCU.
//
// Command set: what the B2500 MCU images send (hm2500 research repo,
// doc/esphome-fc41d-plan.md §4). Response formats: the FC41D AT manual,
// corrected where the MCU firmware's own parse strings disagree
// (QGETWIFISTATE uses "ssid=", "rssi="). Every format lives in this class so
// UART captures from a real module can correct it in one place; anything not
// yet confirmed by a capture is marked "unverified".
//
// Platform independent (host-tested in tests/hame_at). The ESPHome component
// implements Fc41dBackend with the real Wi-Fi, BLE server, HTTP and clock.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "at_engine.h"

namespace esphome::hame_at {

struct WifiStatus {
  bool connected{false};
  std::string ssid;
  std::string bssid;  // "aa:bb:cc:dd:ee:ff"
  int rssi{0};
  int channel{0};
  std::string ip;
  std::string gateway;
  std::string netmask;
  std::string dns;
};

struct HttpResult {
  int error{0};    // 0 = ok, otherwise a Quectel HTTP error code
  int status{0};   // HTTP status
  std::string body;
};

class Fc41dBackend {
 public:
  virtual ~Fc41dBackend() = default;

  virtual WifiStatus wifi_status() = 0;
  /// The MCU stored station credentials (app provisioning). Default: ignore.
  virtual void wifi_credentials(const std::string &/*ssid*/, const std::string &/*password*/) {}

  /// The MCU finished its BLE setup and asked to advertise.
  virtual void ble_advertise(const std::string &/*name*/, uint16_t /*service*/, const std::vector<uint16_t> &/*characteristics*/) {}
  virtual void ble_stop_advertising() {}
  /// A notification the MCU wants sent on `uuid`.
  virtual void ble_notify(uint16_t uuid, const std::vector<uint8_t> &data) = 0;

  /// The MCU "connected" to its MQTT broker (local termination: always succeeds).
  virtual void mqtt_connected(const std::string &/*host*/, int /*port*/, const std::string &/*client_id*/) {}
  virtual void mqtt_subscribed(const std::string &/*topic*/) {}
  virtual void mqtt_publish(const std::string &topic, const std::string &payload) = 0;

  /// An HTTP request. Call `done` exactly once, now or later (from the main loop).
  virtual void http_request(const std::string &method, const std::string &url, const std::string &content_type,
                            const std::vector<uint8_t> &body, std::function<void(HttpResult)> done) = 0;

  /// A certificate/key the MCU uploaded with AT+QSSLCERT. Default: discard.
  virtual void ssl_cert(const std::string &/*kind*/, const std::vector<uint8_t> &/*data*/) {}

  /// The MCU asked the module to restart.
  virtual void module_restart() {}
};

class Fc41dEmulator {
 public:
  explicit Fc41dEmulator(Fc41dBackend *backend) : backend_(backend) { this->register_handlers_(); }

  AtEngine &engine() { return this->engine_; }

  /// Module identity reported to the MCU. Must match the stock module: the MCU
  /// derives the device id (topics, BLE name, cloud identity) from the BLE MAC.
  void set_module_version(const std::string &v) { this->version_ = v; }
  void set_ble_mac(const std::string &mac) { this->ble_mac_ = mac; }

  /// Feed MCU bytes / advance timers. Call from the main loop.
  void feed(const uint8_t *data, size_t len, uint32_t now_ms) {
    this->now_ = now_ms;
    this->engine_.feed(data, len, now_ms);
  }
  void loop(uint32_t now_ms);

  /// Send the boot banner ("ready"), as the module does after power-up/reset.
  void announce_ready();

  // --- Events towards the MCU ---
  /// A central wrote `data` to a characteristic (phone or local poller).
  void ble_write(uint16_t uuid, const std::vector<uint8_t> &data);
  void ble_connected(bool connected);
  /// A message for a topic the MCU subscribed to.
  void mqtt_receive(const std::string &topic, const std::string &payload);

  // --- State, for diagnostics ---
  const std::string &ble_name() const { return this->ble_name_; }
  bool ble_advertising() const { return this->ble_advertising_; }
  bool ble_session() const { return this->ble_session_; }
  bool mqtt_connected() const { return this->mqtt_conn_; }
  const std::vector<std::string> &mqtt_topics() const { return this->mqtt_topics_; }
  bool rebooting() const { return this->ready_at_ != 0; }

 protected:
  void register_handlers_();
  void wifi_urcs_(const WifiStatus &st);
  static uint16_t parse_uuid16_(const std::string &s);
  static bool decode_hex_(const std::string &hex, std::vector<uint8_t> &out);

  Fc41dBackend *backend_;
  AtEngine engine_;
  uint32_t now_{0};

  std::string version_{"FC41DAAR18A04M02_HM"};  // unverified: stock string from a capture
  std::string ble_mac_{"00:00:00:00:00:00"};
  uint32_t ready_at_{0};  // pending "ready" banner after AT+QRST

  // Wi-Fi URC edge detection
  bool wifi_reported_{false};
  bool wifi_last_connected_{false};

  // BLE
  std::string ble_name_;
  uint16_t ble_service_{0};
  std::vector<uint16_t> ble_chars_;
  uint16_t ble_tran_uuid_{0};
  bool ble_tran_auto_{false};
  bool ble_advertising_{false};
  bool ble_session_{false};

  // MQTT
  std::string mqtt_host_;
  int mqtt_port_{0};
  bool mqtt_open_{false};
  bool mqtt_conn_{false};
  std::vector<std::string> mqtt_topics_;

  // HTTP
  std::string http_url_;
  std::string http_content_type_{"application/x-www-form-urlencoded"};
  std::string http_body_;
  bool http_busy_{false};
};

}  // namespace esphome::hame_at
