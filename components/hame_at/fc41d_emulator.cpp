#include "fc41d_emulator.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace esphome::hame_at {

static constexpr uint32_t RESTART_MS = 300;

uint16_t Fc41dEmulator::parse_uuid16_(const std::string &s) {
  return static_cast<uint16_t>(strtoul(s.c_str(), nullptr, 16));
}

bool Fc41dEmulator::decode_hex_(const std::string &hex, std::vector<uint8_t> &out) {
  if (hex.size() % 2 != 0)
    return false;
  out.clear();
  out.reserve(hex.size() / 2);
  for (size_t i = 0; i < hex.size(); i += 2) {
    char buf[3] = {hex[i], hex[i + 1], 0};
    if (!isxdigit(static_cast<unsigned char>(buf[0])) || !isxdigit(static_cast<unsigned char>(buf[1])))
      return false;
    out.push_back(static_cast<uint8_t>(strtoul(buf, nullptr, 16)));
  }
  return true;
}

void Fc41dEmulator::announce_ready() {
  // unverified: banner text and whether more URCs follow it
  this->engine_.send_line("ready");
}

void Fc41dEmulator::loop(uint32_t now_ms) {
  this->now_ = now_ms;
  this->engine_.poll(now_ms);
  if (this->ready_at_ != 0 && static_cast<int32_t>(now_ms - this->ready_at_) >= 0) {
    this->ready_at_ = 0;
    this->announce_ready();
    this->wifi_reported_ = false;  // re-announce the station state after a "reboot"
  }
  if (this->ready_at_ == 0) {
    WifiStatus st = this->backend_->wifi_status();
    if (!this->wifi_reported_ || st.connected != this->wifi_last_connected_) {
      // Only report edges after the first look; at boot report "connected" only.
      if (this->wifi_reported_ || st.connected)
        this->wifi_urcs_(st);
      this->wifi_reported_ = true;
      this->wifi_last_connected_ = st.connected;
    }
  }
}

void Fc41dEmulator::wifi_urcs_(const WifiStatus &st) {
  // URC spelling per the manual's URC table and Rippleon's capture (no space,
  // no quotes); the manual contradicts itself, so: unverified.
  if (st.connected) {
    this->engine_.send_line("+QSTASTAT:WLAN_CONNECTED");
    this->engine_.send_line("+QSTASTAT:GOT_IP");
  } else {
    this->engine_.send_line("+QSTASTAT:WLAN_DISCONNECTED");
    if (this->mqtt_conn_ || this->mqtt_open_) {
      // A real module loses its MQTT session with the station.
      this->mqtt_conn_ = false;
      this->mqtt_open_ = false;
      this->mqtt_topics_.clear();
      this->engine_.send_line("+QMTSTAT: 0,1");
      this->backend_->mqtt_closed();
    }
  }
}

void Fc41dEmulator::ble_write(uint16_t uuid, const std::vector<uint8_t> &data) {
  // The stock module forwards peer writes as raw bytes (no URC) once the MCU
  // set AT+QBLETRANMODE; with AT+QBLEURC off it does the same before that.
  // Writes to other characteristics (ff06) arrive the same way.
  (void) uuid;
  this->engine_.send_raw(data.data(), data.size());
}

void Fc41dEmulator::ble_connected(bool connected) {
  if (connected == this->ble_session_)
    return;
  this->ble_session_ = connected;
  if (connected) {
    this->ble_advertising_ = false;
    this->engine_.send_line("+QBLESTAT:CONNECTED");
    // The MCU sends notifications as AT+QBLEGATTSNTFY lines (seen in every
    // image), so transparent UART mode is not entered here; whether the stock
    // module does that after QBLETRANMODE needs a capture (plan, open question 2).
  } else {
    this->engine_.send_line("+QBLESTAT:DISCONNECTED");
    if (this->engine_.get_mode() == AtEngine::Mode::TRANSPARENT)
      this->engine_.exit_transparent();
  }
}

void Fc41dEmulator::mqtt_receive(const std::string &topic, const std::string &payload) {
  if (!this->mqtt_conn_)
    return;
  bool subscribed = false;
  for (auto &t : this->mqtt_topics_) {
    if (t == topic) {
      subscribed = true;
      break;
    }
  }
  if (!subscribed)
    return;
  char hdr[32];
  snprintf(hdr, sizeof(hdr), "\",%u,\"", static_cast<unsigned>(payload.size()));
  this->engine_.send_line("+QMTRECV: 0,0,\"" + topic + hdr + payload + "\"");
}

void Fc41dEmulator::register_handlers_() {
  AtEngine &e = this->engine_;

  // ---------------------------------------------------------------- system
  e.on("", [](AtEngine &e, const AtCommand &) { e.ok(); });
  e.on("E0", [](AtEngine &e, const AtCommand &) {
    e.set_echo(false);
    e.ok();
  });
  e.on("E1", [](AtEngine &e, const AtCommand &) {
    e.set_echo(true);
    e.ok();
  });
  e.on("QVERSION", [this](AtEngine &e, const AtCommand &) {
    e.send_line("+QVERSION: " + this->version_);
    e.ok();
  });
  e.on("QRST", [this](AtEngine &e, const AtCommand &) {
    e.ok();
    // Forget everything the MCU configured, as a real reboot would.
    this->ble_advertising_ = false;
    this->ble_session_ = false;
    this->ble_tran_uuid_ = 0;
    this->ble_chars_.clear();
    if (this->mqtt_conn_)
      this->backend_->mqtt_closed();
    this->mqtt_open_ = false;
    this->mqtt_conn_ = false;
    this->mqtt_topics_.clear();
    this->mqtt_session_ = MqttSession{};
    this->http_busy_ = false;
    if (e.get_mode() == AtEngine::Mode::TRANSPARENT)
      e.exit_transparent();
    this->backend_->module_restart();
    this->ready_at_ = this->now_ + RESTART_MS;
    if (this->ready_at_ == 0)
      this->ready_at_ = 1;
  });
  // Module firmware update: never. The cloud must not overwrite ESPHome.
  e.on("QWLANOTA", [](AtEngine &e, const AtCommand &) { e.error(); });
  e.on("QECHO", [](AtEngine &e, const AtCommand &cmd) {
    e.set_echo(cmd.arg_int(0, 0) != 0);
    e.ok();
  });

  // ---------------------------------------------------------------- Wi-Fi
  auto creds = [this](AtEngine &e, const AtCommand &cmd) {
    if (cmd.kind == AtCommand::Kind::SET && !cmd.args.empty())
      this->backend_->wifi_credentials(cmd.arg(0), cmd.arg(1));
    e.ok();
    // ESPHome owns the radio: confirm the station that is already up, as a
    // module that just joined would.
    WifiStatus st = this->backend_->wifi_status();
    if (st.connected)
      this->wifi_urcs_(st);
  };
  e.on("QSTAAPINFODEF", creds);
  e.on("QSTAAPINFO", creds);
  e.on("QSTAST", [this](AtEngine &e, const AtCommand &) {
    e.send_line(std::string("+QSTAST: ") + (this->backend_->wifi_status().connected ? "STATION_UP" : "STATION_DOWN"));
    e.ok();
  });
  e.on("QGETWIFISTATE", [this](AtEngine &e, const AtCommand &) {
    WifiStatus st = this->backend_->wifi_status();
    if (!st.connected) {
      e.error();
      return;
    }
    // key=value form: the MCU images search for "ssid=" and "rssi=".
    char buf[160];
    snprintf(buf, sizeof(buf), "+QGETWIFISTATE: ssid=%s,bssid=%s,rssi=%d,channel=%d,cipher_type=%d", st.ssid.c_str(),
             st.bssid.c_str(), st.rssi, st.channel, 4);
    e.send_line(buf);
    e.ok();
  });
  e.on("QGETIP", [this](AtEngine &e, const AtCommand &) {
    WifiStatus st = this->backend_->wifi_status();
    if (!st.connected) {
      e.error();
      return;
    }
    e.send_line("+QGETIP: ip:" + st.ip + ",gate:" + st.gateway + ",mask:" + st.netmask + ",dns:" + st.dns);
    e.ok();
  });

  // ---------------------------------------------------------------- BLE
  e.on("QBLEINIT", [](AtEngine &e, const AtCommand &) { e.ok(); });
  e.on("QBLEADDR", [this](AtEngine &e, const AtCommand &) {
    // unverified: the MCU reads 12 characters after "+QBLEADDR:"
    e.send_line("+QBLEADDR:" + this->ble_mac_);
    e.ok();
  });
  e.on("QBLENAME", [this](AtEngine &e, const AtCommand &cmd) {
    this->ble_name_ = cmd.arg(0);
    e.ok();
  });
  e.on("QBLEGATTSSRV", [this](AtEngine &e, const AtCommand &cmd) {
    this->ble_service_ = parse_uuid16_(cmd.arg(0));
    this->ble_chars_.clear();
    e.ok();
  });
  e.on("QBLEGATTSCHAR", [this](AtEngine &e, const AtCommand &cmd) {
    this->ble_chars_.push_back(parse_uuid16_(cmd.arg(0)));
    e.ok();
  });
  e.on("QBLEADVPARAM", [](AtEngine &e, const AtCommand &) { e.ok(); });
  e.on("QBLEADVDATA", [](AtEngine &e, const AtCommand &) { e.ok(); });
  e.on("QBLEADVSTART", [this](AtEngine &e, const AtCommand &) {
    e.ok();
    this->ble_advertising_ = true;
    this->backend_->ble_advertise(this->ble_name_, this->ble_service_, this->ble_chars_);
  });
  e.on("QBLEADVSTOP", [this](AtEngine &e, const AtCommand &) {
    e.ok();
    this->ble_advertising_ = false;
    this->backend_->ble_stop_advertising();
  });
  e.on("QBLETRANMODE", [this](AtEngine &e, const AtCommand &cmd) {
    this->ble_tran_uuid_ = parse_uuid16_(cmd.arg(0));
    this->ble_tran_auto_ = cmd.arg_int(1, 0) != 0;
    e.ok();
  });
  e.on("QBLEGATTSNTFY", [this](AtEngine &e, const AtCommand &cmd) {
    // Two forms are in the MCU images:
    //   ff02,<len>,<hex>   binary app frames, hex encoded
    //   ff02,<text>        the cell-info frame "soc_t1_t2_mv1_..._mv14" as text
    uint16_t uuid = parse_uuid16_(cmd.arg(0));
    std::vector<uint8_t> data;
    if (cmd.args.size() >= 3) {
      if (!decode_hex_(cmd.arg(2), data)) {
        e.error();
        return;
      }
    } else if (cmd.args.size() == 2) {
      const std::string &text = cmd.arg(1);
      data.assign(text.begin(), text.end());
    } else {
      e.error();
      return;
    }
    e.ok();
    this->backend_->ble_notify(uuid, data);
  });
  e.on("QBLESTAT", [this](AtEngine &e, const AtCommand &) {
    e.send_line(std::string("+QBLESTAT: ") +
                (this->ble_session_ ? "CONNECTED" : (this->ble_advertising_ ? "ADVERTISING" : "NOADVERTISING")));
    e.ok();
  });
  e.on("QBLEDISCONN", [this](AtEngine &e, const AtCommand &) {
    e.ok();
    this->ble_connected(false);
  });

  // ---------------------------------------------------------------- MQTT
  // Terminated locally: every operation succeeds at once.
  e.on("QMTCFG", [this](AtEngine &e, const AtCommand &cmd) {
    // AT+QMTCFG="<key>",<client>,<values...>
    const std::string &key = cmd.arg(0);
    if (key == "ssl") {
      this->mqtt_session_.ssl = cmd.arg_int(2, 0) != 0;
      this->mqtt_session_.ssl_ctx = static_cast<int>(cmd.arg_int(3, 0));
    } else if (key == "keepalive") {
      this->mqtt_session_.keepalive = static_cast<int>(cmd.arg_int(2, 120));
    } else if (key == "version") {
      this->mqtt_session_.version = cmd.arg_int(2, 4) == 3 ? 3 : 4;
    }
    e.ok();
  });
  e.on("QMTOPEN", [this](AtEngine &e, const AtCommand &cmd) {
    if (cmd.kind == AtCommand::Kind::QUERY) {
      if (this->mqtt_open_)
        e.send_line("+QMTOPEN: 0,\"local\",1883");
      e.ok();
      return;
    }
    e.ok();
    this->mqtt_open_ = true;
    e.send_line("+QMTOPEN: 0,0");
    this->mqtt_session_.host = cmd.arg(1);
    this->mqtt_session_.port = static_cast<int>(cmd.arg_int(2, 0));
  });
  e.on("QMTCONN", [this](AtEngine &e, const AtCommand &cmd) {
    if (cmd.kind == AtCommand::Kind::QUERY) {
      // 3 = connected (manual)
      e.send_line(std::string("+QMTCONN: 0,") + (this->mqtt_conn_ ? "3" : "1"));
      e.ok();
      return;
    }
    e.ok();
    if (!this->mqtt_open_) {
      e.send_line("+QMTCONN: 0,1");
      return;
    }
    this->mqtt_conn_ = true;
    e.send_line("+QMTCONN: 0,0,0");
    this->mqtt_session_.client_id = cmd.arg(1);
    this->mqtt_session_.username = cmd.arg(2);
    this->mqtt_session_.password = cmd.arg(3);
    this->backend_->mqtt_connected(this->mqtt_session_);
  });
  e.on("QMTSUB", [this](AtEngine &e, const AtCommand &cmd) {
    // AT+QMTSUB=<id>,<msgid>,"topic",<qos>[,"topic",<qos>...]
    e.ok();
    for (size_t i = 2; i < cmd.args.size(); i += 2) {
      const std::string &topic = cmd.arg(i);
      if (topic.empty())
        break;
      bool known = false;
      for (auto &t : this->mqtt_topics_)
        known |= t == topic;
      if (!known)
        this->mqtt_topics_.push_back(topic);
      this->backend_->mqtt_subscribed(topic);
    }
    e.send_line("+QMTSUB: 0," + std::to_string(cmd.arg_int(1, 0)) + ",0,0");
  });
  e.on("QMTUNS", [this](AtEngine &e, const AtCommand &cmd) {
    e.ok();
    for (size_t i = 2; i < cmd.args.size(); i++) {
      for (auto it = this->mqtt_topics_.begin(); it != this->mqtt_topics_.end(); ++it) {
        if (*it == cmd.arg(i)) {
          this->mqtt_topics_.erase(it);
          break;
        }
      }
    }
    e.send_line("+QMTUNS: 0," + std::to_string(cmd.arg_int(1, 0)) + ",0");
  });
  e.on("QMTPUB", [this](AtEngine &e, const AtCommand &cmd) {
    // AT+QMTPUB=<id>,<msgid>,<qos>,<retain>,"topic",<len>,"payload"
    const std::string &topic = cmd.arg(4);
    long len = cmd.arg_int(5, -1);
    if (cmd.args.size() < 7 || len < 0) {
      // unverified: the prompt form ("> " then payload + Ctrl-Z) seen on
      // other MCUs is not used by the B2500 images.
      e.error();
      return;
    }
    // The payload may contain quotes and commas of its own: take everything
    // after the sixth top-level comma and strip one pair of outer quotes.
    std::string payload = raw_arg_tail(cmd.raw_args, 6);
    if (payload.size() >= 2 && payload.front() == '"' && payload.back() == '"')
      payload = payload.substr(1, payload.size() - 2);
    if (!this->mqtt_conn_) {
      e.ok();
      e.send_line("+QMTPUB: 0," + std::to_string(cmd.arg_int(1, 0)) + ",2");
      return;
    }
    e.ok();
    e.send_line("+QMTPUB: 0," + std::to_string(cmd.arg_int(1, 0)) + ",0");
    this->backend_->mqtt_publish(topic, payload);
  });
  e.on("QMTCLOSE", [this](AtEngine &e, const AtCommand &) {
    e.ok();
    bool was = this->mqtt_conn_;
    this->mqtt_open_ = false;
    this->mqtt_conn_ = false;
    this->mqtt_topics_.clear();
    e.send_line("+QMTCLOSE: 0,0");
    if (was)
      this->backend_->mqtt_closed();
  });
  e.on("QMTDISC", [this](AtEngine &e, const AtCommand &) {
    e.ok();
    bool was = this->mqtt_conn_;
    this->mqtt_conn_ = false;
    this->mqtt_topics_.clear();
    e.send_line("+QMTDISC: 0,0");
    if (was)
      this->backend_->mqtt_closed();
  });

  // ---------------------------------------------------------------- TLS
  e.on("QSSLCFG", [](AtEngine &e, const AtCommand &) { e.ok(); });
  e.on("QSSLCERT", [this](AtEngine &e, const AtCommand &cmd) {
    if (cmd.kind == AtCommand::Kind::QUERY) {
      // Report an empty store so the MCU uploads its certificates on every
      // boot: they are kept in RAM only (cloud forwarding needs them).
      // unverified: the listing format and how the MCU reads an empty one.
      e.ok();
      return;
    }
    std::string kind = cmd.arg(0);
    long mode = cmd.arg_int(1, -1);
    if (mode == 2) {
      // Upload: "CONNECT", then exactly <len> raw bytes, then OK.
      long len = cmd.arg_int(2, 0);
      e.send_line("CONNECT");
      e.expect_data(static_cast<size_t>(len), [this, kind](AtEngine &e, std::vector<uint8_t> data) {
        this->backend_->ssl_cert(kind, data);
        e.ok();
      });
      return;
    }
    e.ok();  // delete / other: nothing stored
  });

  // ---------------------------------------------------------------- HTTP
  e.on("QHTTPCFG", [this](AtEngine &e, const AtCommand &cmd) {
    std::string key = cmd.arg(0);
    if (key == "url")
      this->http_url_ = cmd.arg(1);
    else if (key == "contenttype" || key == "content_type")
      this->http_content_type_ = cmd.arg(1);
    e.ok();
  });
  auto finish = [this](const char *urc) {
    return [this, urc](HttpResult r) {
      this->http_busy_ = false;
      this->http_body_ = r.body;
      if (r.error != 0) {
        this->engine_.send_line(std::string(urc) + std::to_string(r.error));
        return;
      }
      this->engine_.send_line(std::string(urc) + "0," + std::to_string(r.status) + "," +
                              std::to_string(r.body.size()));
    };
  };
  e.on("QHTTPGET", [this, finish](AtEngine &e, const AtCommand &) {
    if (this->http_busy_ || this->http_url_.empty()) {
      e.error();
      return;
    }
    e.ok();
    this->http_busy_ = true;
    this->http_body_.clear();
    this->backend_->http_request("GET", this->http_url_, "", {}, finish("+QHTTPGET: "));
  });
  e.on("QHTTPPOST", [this, finish](AtEngine &e, const AtCommand &cmd) {
    if (this->http_busy_ || this->http_url_.empty()) {
      e.error();
      return;
    }
    long len = cmd.arg_int(0, 0);
    e.send_line("CONNECT");
    e.expect_data(static_cast<size_t>(len), [this, finish](AtEngine &e, std::vector<uint8_t> body) {
      e.ok();
      this->http_busy_ = true;
      this->http_body_.clear();
      this->backend_->http_request("POST", this->http_url_, this->http_content_type_, body, finish("+QHTTPPOST: "));
    });
  });
  e.on("QHTTPREAD", [this](AtEngine &e, const AtCommand &) {
    // "CONNECT", body, "OK", "+QHTTPREAD: 0" (manual; unverified)
    e.send_line("CONNECT");
    e.send_raw(this->http_body_);
    e.ok();
    e.send_line("+QHTTPREAD: 0");
  });
}

}  // namespace esphome::hame_at
