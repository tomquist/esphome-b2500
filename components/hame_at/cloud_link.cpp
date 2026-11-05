#include "cloud_link.h"

#include <algorithm>

namespace esphome::hame_at {

const char *CloudLink::state_str() const {
  switch (this->state_) {
    case State::DISABLED:
      return "disabled";
    case State::NO_SESSION:
      return "waiting for battery";
    case State::BACKOFF:
      return "reconnecting";
    case State::WAIT_CONNACK:
      return "connecting";
    case State::CONNECTED:
      return "connected";
  }
  return "unknown";
}

void CloudLink::set_enabled(bool enabled, uint32_t now_ms) {
  if (enabled == this->enabled_)
    return;
  this->enabled_ = enabled;
  if (!enabled) {
    if (this->state_ == State::CONNECTED)
      this->send_(mqtt::encode_disconnect());
    if (this->state_ == State::CONNECTED || this->state_ == State::WAIT_CONNACK)
      this->transport_->close();
    this->queue_.clear();
    this->state_ = State::DISABLED;
    return;
  }
  this->state_ = State::NO_SESSION;
  this->backoff_ms_ = BACKOFF_MIN_MS;
  this->next_attempt_ms_ = now_ms;
  this->evaluate_(now_ms);
}

void CloudLink::set_session(const MqttSession &session, uint32_t now_ms) {
  bool changed = !this->have_session_ || session.host != this->session_.host || session.port != this->session_.port ||
                 session.client_id != this->session_.client_id || session.username != this->session_.username ||
                 session.password != this->session_.password || session.ssl != this->session_.ssl;
  this->session_ = session;
  this->have_session_ = !session.host.empty() && session.port > 0;
  // The MCU subscribes again after every connect; don't carry old topics.
  this->topics_.clear();
  if (changed && (this->state_ == State::CONNECTED || this->state_ == State::WAIT_CONNACK)) {
    this->transport_->close();
    this->state_ = State::BACKOFF;
  }
  this->backoff_ms_ = BACKOFF_MIN_MS;
  this->next_attempt_ms_ = now_ms;
  this->evaluate_(now_ms);
}

void CloudLink::set_credentials(const TlsCredentials &creds, uint32_t now_ms) {
  this->creds_ = creds;
  this->evaluate_(now_ms);
}

void CloudLink::set_credential(const std::string &kind, std::string &&pem, uint32_t now_ms) {
  if (kind == "CA") {
    this->creds_.ca = std::move(pem);
  } else if (kind == "User Cert") {
    this->creds_.client_cert = std::move(pem);
  } else if (kind == "User Key") {
    this->creds_.client_key = std::move(pem);
  } else {
    return;
  }
  this->evaluate_(now_ms);
}

void CloudLink::clear_session() {
  if (this->state_ == State::CONNECTED)
    this->send_(mqtt::encode_disconnect());
  if (this->state_ == State::CONNECTED || this->state_ == State::WAIT_CONNACK)
    this->transport_->close();
  this->have_session_ = false;
  this->topics_.clear();
  this->queue_.clear();
  this->state_ = this->enabled_ ? State::NO_SESSION : State::DISABLED;
}

void CloudLink::set_network(bool up, uint32_t now_ms) {
  if (up == this->network_up_)
    return;
  this->network_up_ = up;
  if (!up && (this->state_ == State::CONNECTED || this->state_ == State::WAIT_CONNACK)) {
    this->transport_->close();
    this->state_ = State::BACKOFF;
  }
  if (up) {
    this->backoff_ms_ = BACKOFF_MIN_MS;
    this->next_attempt_ms_ = now_ms;
  }
  this->evaluate_(now_ms);
}

void CloudLink::subscribe(const std::string &topic) {
  if (std::find(this->topics_.begin(), this->topics_.end(), topic) != this->topics_.end())
    return;
  this->topics_.push_back(topic);
  if (this->state_ == State::CONNECTED)
    this->subscribe_now_(topic);
}

void CloudLink::publish(const std::string &topic, const std::string &payload, bool retain) {
  if (!this->enabled_ || !this->have_session_)
    return;
  if (this->state_ == State::CONNECTED) {
    this->send_(mqtt::encode_publish(topic, payload, 0, retain));
    return;
  }
  // Hold a few messages across a reconnect; status payloads are periodic, so
  // dropping the oldest loses nothing that will not be sent again.
  if (this->queue_.size() >= MAX_QUEUED) {
    this->queue_.pop_front();
    this->dropped_++;
  }
  this->queue_.push_back({topic, payload, retain});
}

void CloudLink::evaluate_(uint32_t now_ms) {
  if (!this->enabled_) {
    this->state_ = State::DISABLED;
    return;
  }
  if (this->state_ == State::CONNECTED || this->state_ == State::WAIT_CONNACK)
    return;
  bool ready = this->have_session_ && (!this->session_.ssl || this->creds_.complete());
  if (!ready) {
    this->state_ = State::NO_SESSION;
    return;
  }
  if (this->state_ != State::BACKOFF) {
    this->state_ = State::BACKOFF;
    this->next_attempt_ms_ = now_ms;
  }
}

void CloudLink::fail_(const std::string &why, uint32_t now_ms) {
  this->log_line_("cloud: " + why + "; retrying in " + std::to_string(this->backoff_ms_ / 1000) + " s");
  this->transport_->close();
  this->state_ = State::BACKOFF;
  this->next_attempt_ms_ = now_ms + this->backoff_ms_;
  this->backoff_ms_ = std::min(this->backoff_ms_ * 2, BACKOFF_MAX_MS);
}

void CloudLink::send_(const std::vector<uint8_t> &pkt) {
  // A failed write shows up as a lost connection on the next service().
  if (this->transport_->write(pkt.data(), pkt.size()))
    this->last_tx_ms_ = this->now_ms_;
}

void CloudLink::subscribe_now_(const std::string &topic) {
  uint16_t id = this->next_packet_id_++;
  if (this->next_packet_id_ == 0)
    this->next_packet_id_ = 1;
  this->send_(mqtt::encode_subscribe(id, topic, 0));
}

void CloudLink::attempt_(uint32_t now_ms) {
  this->now_ms_ = now_ms;
  const TlsCredentials *tls = this->session_.ssl ? &this->creds_ : nullptr;
  this->log_line_("cloud: connecting to " + this->session_.host + ":" + std::to_string(this->session_.port) +
                  (tls ? " (TLS)" : ""));
  if (!this->transport_->connect(this->session_.host, static_cast<uint16_t>(this->session_.port), tls)) {
    std::string err = this->transport_->last_error();
    this->fail_(err.empty() ? "connect failed" : "connect failed (" + err + ")", now_ms);
    return;
  }
  this->decoder_.reset();
  uint16_t keepalive = static_cast<uint16_t>(std::max(10, this->session_.keepalive));
  this->send_(mqtt::encode_connect(this->session_.client_id, this->session_.username, this->session_.password,
                                   keepalive, static_cast<uint8_t>(this->session_.version)));
  this->state_ = State::WAIT_CONNACK;
  // connect() blocks (a TLS handshake with a client key takes ~12 s on the
  // BK7231N), so `now_ms` is stale: the CONNACK wait starts at the next
  // service().
  this->connack_clock_started_ = false;
  this->last_tx_ms_ = now_ms;
  this->last_rx_ms_ = now_ms;
  this->ping_outstanding_ = false;
}

void CloudLink::service(uint32_t now_ms) {
  this->now_ms_ = now_ms;
  switch (this->state_) {
    case State::DISABLED:
    case State::NO_SESSION:
      return;
    case State::BACKOFF:
      if (!this->network_up_ || static_cast<int32_t>(now_ms - this->next_attempt_ms_) < 0)
        return;
      this->attempt_(now_ms);
      return;
    case State::WAIT_CONNACK:
    case State::CONNECTED:
      break;
  }

  if (!this->transport_->connected()) {
    std::string err = this->transport_->last_error();
    this->fail_(err.empty() ? "connection lost" : "connection lost (" + err + ")", now_ms);
    return;
  }

  uint8_t buf[512];
  for (;;) {
    int n = this->transport_->read(buf, sizeof(buf));
    if (n < 0) {
      std::string err = this->transport_->last_error();
      this->fail_(err.empty() ? "connection closed" : "connection closed (" + err + ")", now_ms);
      return;
    }
    if (n == 0)
      break;
    this->last_rx_ms_ = now_ms;
    this->decoder_.feed(buf, static_cast<size_t>(n));
  }

  mqtt::Packet pkt;
  while (this->decoder_.next(pkt)) {
    switch (pkt.type) {
      case mqtt::CONNACK:
        if (this->state_ != State::WAIT_CONNACK)
          break;
        if (pkt.code != 0) {
          this->fail_("broker refused the connection (code " + std::to_string(pkt.code) + ")", now_ms);
          return;
        }
        this->state_ = State::CONNECTED;
        this->connect_count_++;
        this->backoff_ms_ = BACKOFF_MIN_MS;
        {
          const std::string info = this->transport_->connect_info();
          this->log_line_(info.empty() ? "cloud: connected" : "cloud: connected; " + info);
        }
        for (auto &t : this->topics_)
          this->subscribe_now_(t);
        while (!this->queue_.empty()) {
          auto &m = this->queue_.front();
          this->send_(mqtt::encode_publish(m.topic, m.payload, 0, m.retain));
          this->queue_.pop_front();
        }
        break;
      case mqtt::PUBLISH:
        if (pkt.qos == 1)
          this->send_(mqtt::encode_puback(pkt.packet_id));
        if (this->on_message_)
          this->on_message_(pkt.topic, pkt.payload);
        break;
      case mqtt::PINGRESP:
        this->ping_outstanding_ = false;
        break;
      case mqtt::SUBACK:
        if (pkt.code == 0x80)
          this->log_line_("cloud: subscription refused");
        break;
      default:
        break;
    }
  }
  if (this->decoder_.error()) {
    this->fail_("malformed packet from broker", now_ms);
    return;
  }

  if (this->state_ == State::WAIT_CONNACK) {
    if (!this->connack_clock_started_) {
      this->connack_clock_started_ = true;
      this->state_since_ms_ = now_ms;
    }
    if (now_ms - this->state_since_ms_ > CONNACK_TIMEOUT_MS)
      this->fail_("no CONNACK", now_ms);
    return;
  }

  // Keepalive: ping at half the interval; give up after a full interval of silence.
  uint32_t ka_ms = static_cast<uint32_t>(std::max(10, this->session_.keepalive)) * 1000;
  if (now_ms - this->last_rx_ms_ > ka_ms + ka_ms / 2) {
    this->fail_("keepalive timeout", now_ms);
    return;
  }
  if (!this->ping_outstanding_ && now_ms - this->last_tx_ms_ >= ka_ms / 2 && now_ms - this->last_rx_ms_ >= ka_ms / 2) {
    this->send_(mqtt::encode_pingreq());
    this->ping_outstanding_ = true;
  }
}

}  // namespace esphome::hame_at
