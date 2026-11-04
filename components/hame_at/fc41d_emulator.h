#pragma once

// Emulates the Quectel FC41D AT firmware (Hame build) towards the battery MCU.
//
// Command set: what the B2500 MCU images send (hm2500 research repo,
// doc/esphome-fc41d-plan.md §4). Formats and timing: the FC41D AT manual,
// corrected by the hm2500 firmware emulator (branch claude/clever-fermi-m8nfb5,
// emulator/marstek_emu/fc41d/module.py), which runs the real MCU images
// against a module model for minutes at a time. Where that model found the
// firmware to depend on a detail, the detail is noted here as "seen in the
// firmware emulator". Nothing has been checked against a UART capture of a
// stock module yet.
//
// Platform independent (host-tested in tests/hame_at). The ESPHome component
// implements Fc41dBackend with the real Wi-Fi, BLE server, HTTP, sockets and
// clock.

#include <cstdint>
#include <functional>
#include <map>
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
  std::string mac;  // station MAC, "aa:bb:cc:dd:ee:ff"
};

/// What the MCU configured for its broker session (AT+QMTCFG/QMTOPEN/QMTCONN).
/// Everything a real connection to that broker needs, apart from the
/// certificates, which arrive separately through ssl_cert().
struct MqttSession {
  std::string host;
  int port{0};
  std::string client_id;
  std::string username;
  std::string password;
  bool ssl{false};
  int ssl_ctx{0};
  int keepalive{120};  // Quectel default
  int version{4};      // MQTT protocol level: AT+QMTCFG "version" 3 -> 3.1, 4 -> 3.1.1
};

/// An HTTP request as the MCU configured it (AT+QHTTPCFG, AT+QHTTPGET/POST).
struct HttpRequest {
  std::string method;  // "GET" / "POST"
  std::string url;
  std::string content_type;
  std::vector<std::pair<std::string, std::string>> headers;  // AT+QHTTPCFG="header"
  std::vector<uint8_t> body;
};

struct HttpResult {
  int error{0};    // 0 = ok, otherwise a Quectel HTTP error code
  int status{0};   // HTTP status
  std::string body;
};

/// A TCP/UDP socket the MCU opened (AT+QIOPEN), e.g. to a CT meter.
struct SocketSpec {
  int id{0};
  std::string kind;  // "TCP", "UDP", "UDP SERVICE", "TCP LISTENER"
  std::string host;  // remote host
  int port{0};       // remote port
  int local_port{0};
  int mode{0};       // 0 buffer (+QIRD), 1 push (+QIURC with data), 2 transparent
};

class Fc41dBackend {
 public:
  virtual ~Fc41dBackend() = default;

  virtual WifiStatus wifi_status() = 0;
  /// The MCU stored station credentials (app provisioning). Default: ignore.
  virtual void wifi_credentials(const std::string & /*ssid*/, const std::string & /*password*/) {}

  /// The MCU finished its BLE setup and asked to advertise.
  virtual void ble_advertise(const std::string & /*name*/, uint16_t /*service*/,
                             const std::vector<uint16_t> & /*characteristics*/) {}
  virtual void ble_stop_advertising() {}
  /// A notification the MCU wants sent on `uuid`.
  virtual void ble_notify(uint16_t uuid, const std::vector<uint8_t> &data) = 0;

  /// The MCU "connected" to its MQTT broker (local termination: always succeeds).
  virtual void mqtt_connected(const MqttSession & /*session*/) {}
  /// The MCU closed or lost its broker session.
  virtual void mqtt_closed() {}
  virtual void mqtt_subscribed(const std::string & /*topic*/) {}
  virtual void mqtt_publish(const std::string &topic, const std::string &payload, bool retain) = 0;
  /// Whether a real broker currently delivers the MCU's publishes back to its
  /// own matching subscriptions (cloud forwarding connected). If not, the
  /// emulator does it, as a broker would: Venus firmware publishes to its own
  /// ctrl topic and takes the echo as proof that the connection works.
  virtual bool mqtt_broker_echoes() { return false; }

  /// An HTTP request. Call `done` exactly once, now or later (from the main loop).
  virtual void http_request(const HttpRequest &request, std::function<void(HttpResult)> done) = 0;

  /// Open a socket; call `done` once with 0 or a Quectel socket error code.
  /// Received data goes back through Fc41dEmulator::socket_received().
  virtual void socket_open(const SocketSpec & /*spec*/, std::function<void(int)> done) { done(566); }
  /// Send on a socket; `ip`/`port` are set for AT+QISEND to a UDP target.
  virtual void socket_send(int /*id*/, const std::string & /*data*/, const std::string & /*ip*/, int /*port*/) {}
  virtual void socket_close(int /*id*/) {}

  /// A certificate/key the MCU uploaded with AT+QSSLCERT. Default: discard.
  virtual void ssl_cert(const std::string & /*kind*/, const std::vector<uint8_t> & /*data*/) {}

  /// The MCU asked the module to restart.
  virtual void module_restart() {}
};

class Fc41dEmulator {
 public:
  // Timing seen in the firmware emulator.
  static constexpr uint32_t BOOT_MS = 1000;            // "ready" after power-up / AT+QRST (stock: ~1.1 s)
  static constexpr uint32_t WIFI_IDLE_MS = 2500;       // announce Wi-Fi only once the MCU is quiet...
  static constexpr uint32_t WIFI_MAX_MS = 20000;       // ...but no later than this after boot
  static constexpr uint32_t DHCP_MS = 100;             // GOT_IP after WLAN_CONNECTED (stock: 100 ms)
  static constexpr uint32_t WIFI_JOIN_MS = 3000;       // after AT+QSTAAPINFO[DEF] (stock: 3.2 s)
  static constexpr uint32_t REANNOUNCE_MS = 8000;      // re-announce if the MCU ignored GOT_IP
  static constexpr uint8_t REANNOUNCE_MAX = 3;
  static constexpr size_t TRAN_CHUNK = 244;            // notification size in transparent mode (MTU 247)
  static constexpr uint32_t TRAN_GAP_MS = 20;          // send a partial chunk after this pause
  static constexpr uint32_t HTTP_RESULT_MIN_MS = 300;  // +QHTTPGET/POST at least this long after OK

  explicit Fc41dEmulator(Fc41dBackend *backend) : backend_(backend) { this->register_handlers_(); }

  AtEngine &engine() { return this->engine_; }

  /// Module identity reported to the MCU. Must match the stock module: the MCU
  /// derives the device id (topics, BLE name, cloud identity) from the BLE MAC,
  /// and reports the module version to the cloud (`fcv`).
  void set_module_version(const std::string &v) { this->version_ = v; }
  void set_ble_mac(const std::string &mac) { this->ble_mac_ = mac; }

  /// Feed MCU bytes / advance timers. Call from the main loop.
  void feed(const uint8_t *data, size_t len, uint32_t now_ms);
  void loop(uint32_t now_ms);

  /// Module power-up: "ready" after BOOT_MS, then Wi-Fi once the MCU is idle.
  void power_on(uint32_t now_ms);

  // --- Events towards the MCU ---
  /// A central wrote `data` to a characteristic (phone or local poller).
  /// Without a BLE session one is opened first (+QBLESTAT:CONNECTED).
  void ble_write(uint16_t uuid, const std::vector<uint8_t> &data);
  void ble_connected(bool connected);
  /// A message from the broker; delivered if the MCU subscribed to a matching
  /// filter. Returns whether it was delivered.
  bool mqtt_receive(const std::string &topic, const std::string &payload);
  /// Data arrived on socket `id` (`ip`/`port`: sender, for UDP services).
  void socket_received(int id, const std::string &data, const std::string &ip = "", int port = 0);
  void socket_closed(int id);

  // --- State, for diagnostics ---
  const std::string &ble_name() const { return this->ble_name_; }
  bool ble_advertising() const { return this->ble_advertising_; }
  bool ble_session() const { return this->ble_session_; }
  bool mqtt_connected() const { return this->mqtt_conn_; }
  const std::vector<std::string> &mqtt_topics() const { return this->mqtt_topics_; }
  const MqttSession &mqtt_session() const { return this->mqtt_session_; }
  bool booting() const { return !this->booted_; }
  bool wifi_announced() const { return this->got_ip_; }
  size_t socket_count() const { return this->sockets_.size(); }

  static bool topic_matches(const std::string &filter, const std::string &topic);
  /// Checksum later Hame module builds append to AT+QSSLCERT? lines. The real
  /// algorithm is unknown; CRC-16/CCITT-FALSE as in the firmware emulator. A
  /// mismatch only makes the MCU delete and upload its certificate again.
  static uint16_t cert_checksum(const std::string &data);

 protected:
  struct Timer {
    uint32_t due;
    uint32_t epoch;
    std::function<void()> fn;
  };
  struct Socket {
    SocketSpec spec;
    std::string rx;
    std::string rx_ip;  // sender of the buffered data (UDP SERVICE)
    int rx_port{0};
    bool closed{false};  // "+QIURC: closed" already sent
    uint32_t rx_total{0};
    uint32_t tx_total{0};
  };

  void register_handlers_();
  void later_(uint32_t ms, std::function<void()> fn);
  void reset_state_();
  void wifi_tick_();
  void announce_wifi_();
  void wifi_lost_();
  void net_used_() { this->net_used_ms_ = this->now_; }
  void socket_sent_(Socket &s, const std::string &data, const std::string &ip, int port);
  static uint16_t parse_uuid16_(const std::string &s);
  /// Whether AT+QSSLCERT? appends checksums: not for module builds
  /// 202406141323 and 202409090159, yes for later ones.
  bool cert_checksums_() const;
  void flush_tran_(bool all);

  Fc41dBackend *backend_;
  AtEngine engine_;
  uint32_t now_{0};
  uint32_t epoch_{0};
  std::vector<Timer> timers_;

  std::string version_{"FC41DAAR18A04M02_HM.bin_202409090159"};
  std::string ble_mac_{"00:00:00:00:00:00"};

  // Power/Wi-Fi state, as announced to the MCU.
  bool booted_{false};
  uint32_t boot_ms_{0};
  uint32_t last_rx_ms_{0};
  bool wifi_up_{false};
  bool got_ip_{false};
  bool wifi_asked_{false};
  bool join_pending_{false};
  uint32_t ip_ms_{0};
  uint32_t net_used_ms_{0};
  uint8_t reannounced_{0};

  // BLE
  std::string ble_name_;
  uint16_t ble_service_{0};
  std::vector<uint16_t> ble_chars_;
  uint16_t ble_tran_uuid_{0};
  bool ble_tran_auto_{false};
  std::vector<uint8_t> tran_buf_;  // MCU bytes in BLE transparent mode, not yet notified
  uint32_t tran_last_ms_{0};
  bool ble_init_{false};
  bool ble_advertising_{false};
  bool ble_session_{false};

  // MQTT
  MqttSession mqtt_session_;
  bool mqtt_open_{false};
  bool mqtt_conn_{false};
  std::vector<std::string> mqtt_topics_;
  uint16_t mqtt_msgid_{0};

  // HTTP
  std::string http_url_;
  std::string http_content_type_{"application/x-www-form-urlencoded"};
  std::vector<std::pair<std::string, std::string>> http_headers_;
  std::string http_body_;
  bool http_busy_{false};

  // TLS certificates as the "module" stores them (RAM only).
  std::map<std::string, std::string> certs_;

  // Sockets by id.
  std::map<int, Socket> sockets_;
};

}  // namespace esphome::hame_at
