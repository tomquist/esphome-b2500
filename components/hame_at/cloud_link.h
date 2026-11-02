#pragma once

// Cloud forwarding: a real MQTT session to the broker the battery MCU asked
// for, with the MCU's own host, client id, credentials, TLS settings and
// certificates. It mirrors the MCU's subscriptions, forwards the MCU's
// publishes upstream and hands broker messages back for delivery to the MCU.
//
// The MCU's own session always ends on the module (see Fc41dEmulator), so this
// link has its own reconnect loop and never changes what the MCU sees. A cloud
// outage therefore cannot disturb local control.
//
// Platform independent: the socket/TLS side is a Transport, and service() is
// driven by the caller. On the device both run in a worker task, because a TLS
// connect blocks for seconds; the host tests use a fake transport.

#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "fc41d_emulator.h"
#include "mqtt_codec.h"

namespace esphome::hame_at {

struct TlsCredentials {
  std::string ca;
  std::string client_cert;
  std::string client_key;
  bool complete() const { return !this->client_cert.empty() && !this->client_key.empty(); }
};

class Transport {
 public:
  virtual ~Transport() = default;
  /// Blocking connect. `tls` null means plain TCP.
  virtual bool connect(const std::string &host, uint16_t port, const TlsCredentials *tls) = 0;
  virtual bool connected() = 0;
  /// Non-blocking read: bytes read, 0 if nothing is available, -1 on a closed link.
  virtual int read(uint8_t *buf, size_t len) = 0;
  virtual bool write(const uint8_t *data, size_t len) = 0;
  virtual void close() = 0;
  /// Why the last connect() failed, for the log.
  virtual std::string last_error() const { return ""; }
};

class CloudLink {
 public:
  enum class State : uint8_t {
    DISABLED,      // forwarding switched off
    NO_SESSION,    // the MCU has not configured a broker session yet
    BACKOFF,       // waiting before the next connect attempt
    WAIT_CONNACK,  // TCP/TLS up, CONNECT sent
    CONNECTED,
  };

  static constexpr uint32_t CONNACK_TIMEOUT_MS = 10000;
  static constexpr uint32_t BACKOFF_MIN_MS = 5000;
  static constexpr uint32_t BACKOFF_MAX_MS = 300000;
  static constexpr size_t MAX_QUEUED = 16;

  using MessageCallback = std::function<void(const std::string &topic, const std::string &payload)>;
  using LogCallback = std::function<void(const std::string &msg)>;

  explicit CloudLink(Transport *transport) : transport_(transport) {}

  void set_on_message(MessageCallback cb) { this->on_message_ = std::move(cb); }
  void set_logger(LogCallback cb) { this->log_ = std::move(cb); }

  void set_enabled(bool enabled, uint32_t now_ms);
  bool enabled() const { return this->enabled_; }

  /// The MCU (re)configured its session. Starts connecting once the session is
  /// complete (TLS sessions need the client certificate and key).
  void set_session(const MqttSession &session, uint32_t now_ms);
  void set_credentials(const TlsCredentials &creds, uint32_t now_ms);
  /// The MCU closed its session: drop ours too.
  void clear_session();
  /// The station lost or regained Wi-Fi.
  void set_network(bool up, uint32_t now_ms);

  void subscribe(const std::string &topic);
  void publish(const std::string &topic, const std::string &payload);

  /// Drive the link: connect, read, keep alive. Blocks inside
  /// Transport::connect() when a connect is due.
  void service(uint32_t now_ms);

  State state() const { return this->state_; }
  const char *state_str() const;
  uint32_t connect_count() const { return this->connect_count_; }
  uint32_t dropped_count() const { return this->dropped_; }

 protected:
  void evaluate_(uint32_t now_ms);
  void attempt_(uint32_t now_ms);
  void fail_(const std::string &why, uint32_t now_ms);
  void send_(const std::vector<uint8_t> &pkt);
  void subscribe_now_(const std::string &topic);
  void log_line_(const std::string &msg) {
    if (this->log_)
      this->log_(msg);
  }

  Transport *transport_;
  MessageCallback on_message_;
  LogCallback log_;

  bool enabled_{true};
  bool network_up_{false};
  bool have_session_{false};
  MqttSession session_;
  TlsCredentials creds_;
  std::vector<std::string> topics_;
  std::deque<std::pair<std::string, std::string>> queue_;

  mqtt::Decoder decoder_;
  State state_{State::NO_SESSION};
  uint32_t next_attempt_ms_{0};
  uint32_t backoff_ms_{BACKOFF_MIN_MS};
  uint32_t state_since_ms_{0};
  uint32_t now_ms_{0};
  uint32_t last_tx_ms_{0};
  uint32_t last_rx_ms_{0};
  bool ping_outstanding_{false};
  uint16_t next_packet_id_{1};
  uint32_t connect_count_{0};
  uint32_t dropped_{0};
};

}  // namespace esphome::hame_at
