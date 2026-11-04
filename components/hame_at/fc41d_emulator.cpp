#include "fc41d_emulator.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace esphome::hame_at {

static constexpr uint32_t QRST_MS = 50;

uint16_t Fc41dEmulator::parse_uuid16_(const std::string &s) {
  return static_cast<uint16_t>(strtoul(s.c_str(), nullptr, 16));
}

uint16_t Fc41dEmulator::cert_checksum(const std::string &data) {
  uint16_t crc = 0xFFFF;
  for (unsigned char b : data) {
    crc ^= static_cast<uint16_t>(b << 8);
    for (int i = 0; i < 8; i++)
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
  }
  return crc;
}

// The stock module prints MACs in lower case; the MCU builds its device id
// (topics, cloud identity) from the BLE one.
static std::string lower_mac(std::string mac) {
  for (auto &c : mac)
    c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return mac;
}

bool Fc41dEmulator::cert_checksums_() const {
  auto ends = [this](const char *build) {
    size_t n = strlen(build);
    return this->version_.size() >= n && this->version_.compare(this->version_.size() - n, n, build) == 0;
  };
  return !(ends("202406141323") || ends("202409090159"));
}

bool Fc41dEmulator::topic_matches(const std::string &filter, const std::string &topic) {
  size_t f = 0, t = 0;
  for (;;) {
    size_t fe = filter.find('/', f);
    size_t te = topic.find('/', t);
    std::string fp = filter.substr(f, fe == std::string::npos ? std::string::npos : fe - f);
    if (fp == "#")
      return true;
    if (t > topic.size())
      return false;
    std::string tp = topic.substr(t, te == std::string::npos ? std::string::npos : te - t);
    if (fp != "+" && fp != tp)
      return false;
    if (fe == std::string::npos || te == std::string::npos)
      return fe == std::string::npos && te == std::string::npos;
    f = fe + 1;
    t = te + 1;
  }
}

// ---------------------------------------------------------------------------
// Timers, power and Wi-Fi
// ---------------------------------------------------------------------------

void Fc41dEmulator::later_(uint32_t ms, std::function<void()> fn) {
  this->timers_.push_back({this->now_ + ms, this->epoch_, std::move(fn)});
}

void Fc41dEmulator::feed(const uint8_t *data, size_t len, uint32_t now_ms) {
  this->now_ = now_ms;
  this->last_rx_ms_ = now_ms;
  this->engine_.feed(data, len, now_ms);
}

void Fc41dEmulator::reset_state_() {
  this->epoch_++;  // drops pending timers: a module reset discards queued output
  this->timers_.clear();
  this->booted_ = false;
  this->wifi_up_ = this->got_ip_ = false;
  this->wifi_asked_ = this->join_pending_ = false;
  this->reannounced_ = 0;
  this->net_used_ms_ = 0;
  this->ble_init_ = this->ble_advertising_ = false;
  if (this->engine_.get_mode() == AtEngine::Mode::TRANSPARENT)
    this->engine_.exit_transparent();
  this->tran_buf_.clear();
  this->ble_tran_uuid_ = 0;
  this->ble_chars_.clear();
  if (this->ble_session_) {
    this->ble_session_ = false;
    this->backend_->ble_stop_advertising();
  }
  if (this->mqtt_conn_)
    this->backend_->mqtt_closed();
  this->mqtt_open_ = this->mqtt_conn_ = false;
  this->mqtt_topics_.clear();
  this->mqtt_session_ = MqttSession{};
  this->http_busy_ = false;
  for (auto &kv : this->sockets_)
    this->backend_->socket_close(kv.first);
  this->sockets_.clear();
  // Certificates live in the module's flash: they survive a reset.
}

void Fc41dEmulator::power_on(uint32_t now_ms) {
  this->now_ = now_ms;
  this->reset_state_();
  this->boot_ms_ = now_ms;
  this->last_rx_ms_ = now_ms;
  this->later_(BOOT_MS, [this]() {
    this->booted_ = true;
    this->engine_.send_line("ready");
  });
}

void Fc41dEmulator::loop(uint32_t now_ms) {
  this->now_ = now_ms;
  this->engine_.poll(now_ms);
  // Run due timers; a timer may add timers or reset the module.
  for (size_t i = 0; i < this->timers_.size();) {
    if (static_cast<int32_t>(now_ms - this->timers_[i].due) < 0) {
      i++;
      continue;
    }
    Timer t = std::move(this->timers_[i]);
    this->timers_.erase(this->timers_.begin() + i);
    if (t.epoch == this->epoch_)
      t.fn();
    i = 0;  // the list may have changed
  }
  this->flush_tran_(false);
  this->wifi_tick_();
}

void Fc41dEmulator::flush_tran_(bool all) {
  // Bytes written in transparent mode go out in notification-sized chunks,
  // once the MCU pauses (or a chunk is full).
  while (!this->tran_buf_.empty()) {
    // One notification carries MTU - 3 bytes.
    const size_t per_ntf = this->ble_mtu_ > 23 ? this->ble_mtu_ - 3u : 20u;
    bool full = this->tran_buf_.size() >= per_ntf;
    if (!all && !full && this->now_ - this->tran_last_ms_ < TRAN_GAP_MS)
      return;
    size_t n = std::min(this->tran_buf_.size(), per_ntf);
    std::vector<uint8_t> chunk(this->tran_buf_.begin(), this->tran_buf_.begin() + n);
    this->tran_buf_.erase(this->tran_buf_.begin(), this->tran_buf_.begin() + n);
    if (this->ble_session_)
      this->backend_->ble_notify(this->ble_tran_uuid_, chunk);
  }
}

void Fc41dEmulator::wifi_tick_() {
  if (!this->booted_)
    return;
  const bool quiet = this->engine_.get_mode() == AtEngine::Mode::LINE;
  const uint32_t idle = this->now_ - this->last_rx_ms_;
  if (!this->backend_->wifi_status().connected) {
    if (this->wifi_up_)
      this->wifi_lost_();
    return;
  }
  if (!this->wifi_up_ && !this->join_pending_) {
    // Several firmwares drop URCs that arrive while they wait for a reply, so
    // announce the station once the MCU is quiet (seen in the firmware emulator).
    if (quiet && (idle >= WIFI_IDLE_MS || this->now_ - this->boot_ms_ >= WIFI_MAX_MS || this->wifi_asked_))
      this->announce_wifi_();
    return;
  }
  // The MCU never used the network after GOT_IP: it probably missed the URC.
  // Announce again, as a module does after a reconnect.
  if (this->got_ip_ && this->reannounced_ < REANNOUNCE_MAX && static_cast<int32_t>(this->net_used_ms_ - this->ip_ms_) < 0 &&
      this->now_ - this->ip_ms_ >= REANNOUNCE_MS && quiet && idle >= WIFI_IDLE_MS) {
    this->reannounced_++;
    this->got_ip_ = false;
    this->engine_.send_bare_line("+QSTASTAT:WLAN_CONNECTED");
    this->later_(DHCP_MS, [this]() {
      this->got_ip_ = true;
      this->ip_ms_ = this->now_;
      this->engine_.send_bare_line("+QSTASTAT:GOT_IP");
    });
  }
}

void Fc41dEmulator::announce_wifi_() {
  this->wifi_up_ = true;
  this->engine_.send_bare_line("+QSTASTAT:WLAN_CONNECTED");
  this->later_(DHCP_MS, [this]() {
    if (!this->wifi_up_)
      return;
    this->got_ip_ = true;
    this->ip_ms_ = this->now_;
    this->engine_.send_bare_line("+QSTASTAT:GOT_IP");
  });
}

void Fc41dEmulator::wifi_lost_() {
  this->wifi_up_ = this->got_ip_ = false;
  // The families disagree on the spelling: Venus and Jupiter match
  // "WLAN DISCONNECTED", the CT002/CT003 "WLAN_DISCONNECTED", the B2500
  // neither (it polls AT+QSTAST). Each ignores the other form, so send both.
  this->engine_.send_bare_line("+QSTASTAT:WLAN DISCONNECTED");
  this->engine_.send_bare_line("+QSTASTAT:WLAN_DISCONNECTED");
  if (this->mqtt_conn_ || this->mqtt_open_) {
    // A real module loses its MQTT session with the station.
    bool was = this->mqtt_conn_;
    this->mqtt_conn_ = this->mqtt_open_ = false;
    this->mqtt_topics_.clear();
    this->engine_.send_line("+QMTSTAT: 0,1");
    if (was)
      this->backend_->mqtt_closed();
  }
}

// ---------------------------------------------------------------------------
// Events from the outside
// ---------------------------------------------------------------------------

void Fc41dEmulator::ble_write(uint16_t uuid, const std::vector<uint8_t> &data) {
  // The stock module forwards every peer write as raw bytes, no URC, ~45 ms
  // later, in AT mode and in transparent mode alike, whatever the
  // characteristic (ff01, ff06). (It also forwards the CCCD write that enables
  // notifications, "01 00"; the B2500 ignores it, so that is not emulated.)
  (void) uuid;
  if (!this->ble_session_)
    this->ble_connected(true);
  this->engine_.send_raw(data.data(), data.size());
}

void Fc41dEmulator::ble_connected(bool connected) {
  if (connected == this->ble_session_)
    return;
  this->ble_session_ = connected;
  if (connected) {
    this->ble_advertising_ = false;
    this->ble_mtu_ = 23;
    // Stock module: no leading CRLF; "+QBLEMTU:<512>" follows once the
    // central has negotiated (see ble_mtu()).
    this->engine_.send_bare_line("+QBLESTAT:CONNECTED");
    // The MCU sends notifications as AT+QBLEGATTSNTFY commands (every image),
    // so the UART stays in command mode while a central is connected.
  } else {
    this->engine_.send_bare_line("+QBLESTAT:DISCONNECTED");
    if (this->engine_.get_mode() == AtEngine::Mode::TRANSPARENT)
      this->engine_.exit_transparent();
  }
}

void Fc41dEmulator::ble_mtu(uint16_t mtu) {
  // The stock module never reports more than 512 (the largest attribute
  // value); the BDK 3.0.78 stack negotiates up to 527 with the same central.
  mtu = std::min<uint16_t>(mtu, 512);
  this->ble_mtu_ = mtu;
  if (this->ble_session_)
    this->engine_.send_bare_line("+QBLEMTU:<" + std::to_string(mtu) + ">");  // brackets and all
}

bool Fc41dEmulator::mqtt_receive(const std::string &topic, const std::string &payload) {
  if (!this->mqtt_conn_)
    return false;
  bool subscribed = false;
  for (auto &f : this->mqtt_topics_) {
    if (topic_matches(f, topic)) {
      subscribed = true;
      break;
    }
  }
  if (!subscribed)
    return false;
  // QoS 0 deliveries carry message id 0 (stock module).
  char hdr[48];
  snprintf(hdr, sizeof(hdr), "+QMTRECV: 0,%u,\"", 0u);
  this->engine_.send_line(std::string(hdr) + topic + "\"," + std::to_string(payload.size()) + ",\"" + payload + "\"");
  return true;
}

void Fc41dEmulator::socket_received(int id, const std::string &data, const std::string &ip, int port) {
  auto it = this->sockets_.find(id);
  if (it == this->sockets_.end())
    return;
  Socket &s = it->second;
  s.rx_total += data.size();
  if (s.spec.mode == 1) {
    std::string hdr = "+QIURC: \"recv\"," + std::to_string(id) + "," + std::to_string(data.size());
    if (s.spec.kind == "UDP SERVICE" && !ip.empty())
      // Hame's build: '%s+QIURC: "recv",%d,%d,"%s",%d%s' (Quectel's generic
      // build puts a comma after the port).
      hdr += ",\"" + ip + "\"," + std::to_string(port);
    this->engine_.send_raw("\r\n" + hdr + "\r\n" + data + "\r\n");
  } else if (s.spec.mode == 2) {
    this->engine_.send_raw(data);
  } else {
    s.rx += data;
    s.rx_ip = ip;
    s.rx_port = port;
    this->engine_.send_line("+QIURC: \"recv\"," + std::to_string(id));
  }
}

void Fc41dEmulator::socket_closed(int id) {
  auto it = this->sockets_.find(id);
  if (it != this->sockets_.end() && !it->second.closed) {
    it->second.closed = true;
    this->engine_.send_line("+QIURC: \"closed\"," + std::to_string(id));
  }
}

void Fc41dEmulator::socket_sent_(Socket &s, const std::string &data, const std::string &ip, int port) {
  s.tx_total += data.size();
  this->backend_->socket_send(s.spec.id, data, ip, port);
  // "+QISEND: <n>" then OK, as the firmware emulator answers (unverified).
  this->engine_.send_line("+QISEND: " + std::to_string(data.size()));
  this->engine_.ok();
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

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
  e.on("QECHO", [](AtEngine &e, const AtCommand &cmd) {
    e.set_echo(cmd.arg_int(0, 0) != 0);
    e.ok();
  });
  e.on("QVERSION", [this](AtEngine &e, const AtCommand &) {
    // The MCU reports the part after "_" to the cloud as fcv (e.g. 202409090159).
    e.send_line("+QVERSION:" + this->version_);
    e.ok();
  });
  e.on("QRST", [this](AtEngine &e, const AtCommand &) {
    e.ok();
    // The module stops at once: output it had queued is lost.
    this->epoch_++;
    this->timers_.clear();
    this->backend_->module_restart();
    this->later_(QRST_MS, [this]() { this->power_on(this->now_); });
  });
  // Module firmware update: never; the cloud must not replace ESPHome. (A real
  // module answers OK and reboots into the new image.)
  e.on("QWLANOTA", [](AtEngine &e, const AtCommand &) { e.error(); });
  for (const char *name : {"QURCCFG", "QSETBAUD", "QSETBAND", "QSTADHCP", "QSTADHCPDEF", "QSOFTAP",
                           "QSOFTAPSTOP", "QICFG", "QSSLCFG"})
    e.on(name, [](AtEngine &e, const AtCommand &) { e.ok(); });
  e.on("QWLMAC", [this](AtEngine &e, const AtCommand &) {
    e.send_line("+QWLMAC:" + lower_mac(this->backend_->wifi_status().mac));
    e.ok();
  });
  // Hame's TCP tunnel extension: not supported (no B2500 image uses it).
  e.on("HMIOPEN", [this](AtEngine &e, const AtCommand &) {
    e.ok();
    this->later_(50, [this]() { this->engine_.send_line("+HMIOPEN FAIL"); });
  });

  // ---------------------------------------------------------------- Wi-Fi
  auto creds = [this](AtEngine &e, const AtCommand &cmd) {
    if (cmd.kind != AtCommand::Kind::SET) {
      e.send_line("+QSTAAPINFO: \"" + this->backend_->wifi_status().ssid + "\"");
      e.ok();
      return;
    }
    this->backend_->wifi_credentials(cmd.arg(0), cmd.arg(1));
    e.ok();
    // ESPHome owns the radio: "rejoin" the station it already holds.
    this->wifi_up_ = this->got_ip_ = false;
    this->join_pending_ = true;
    this->later_(WIFI_JOIN_MS, [this]() {
      this->join_pending_ = false;
      if (this->backend_->wifi_status().connected)
        this->announce_wifi_();
    });
  };
  e.on("QSTAAPINFODEF", creds);
  e.on("QSTAAPINFO", creds);
  e.on("QSTASTOP", [this](AtEngine &e, const AtCommand &) {
    e.ok();
    if (this->wifi_up_)
      this->wifi_lost_();
  });
  e.on("QSTAST", [this](AtEngine &e, const AtCommand &) {
    this->wifi_asked_ = true;
    e.send_line(std::string("+QSTAST:") + (this->wifi_up_ ? "STATION_UP" : "STATION_DOWN"));
    e.ok();
  });
  e.on("QGETWIFISTATE", [this](AtEngine &e, const AtCommand &) {
    this->wifi_asked_ = true;
    if (!this->wifi_up_) {
      e.error();  // stock module, station down
      return;
    }
    // key=value, not the manual's positional form (stock module:
    // "+QGETWIFISTATE:ssid=x,bssid=44:4e:..,rssi=-79,channel=1,cipher_type=CCMP"):
    // every family searches for "ssid=" (most also "rssi="); a B2500 that
    // misses it polls every 80 ms from its 10-minute Wi-Fi check on and starves
    // its CT polls (seen in the firmware emulator).
    WifiStatus st = this->backend_->wifi_status();
    char buf[192];
    snprintf(buf, sizeof(buf), "+QGETWIFISTATE:ssid=%s,bssid=%s,rssi=%d,channel=%d,cipher_type=CCMP", st.ssid.c_str(),
             lower_mac(st.bssid).c_str(), st.rssi, st.channel);
    e.send_line(buf);
    e.ok();
  });
  e.on("QGETIP", [this](AtEngine &e, const AtCommand &) {
    this->wifi_asked_ = true;
    if (!this->got_ip_) {
      e.send_line("+QGETIP:ip:0.0.0.0,gate:0.0.0.0,mask:0.0.0.0,dns:0.0.0.0");  // stock module
      e.ok();
      return;
    }
    WifiStatus st = this->backend_->wifi_status();
    e.send_line("+QGETIP:ip:" + st.ip + ",gate:" + st.gateway + ",mask:" + st.netmask + ",dns:" + st.dns);
    e.ok();
  });
  e.on("QWSCAN", [this](AtEngine &e, const AtCommand &) {
    WifiStatus st = this->backend_->wifi_status();
    if (st.connected)
      e.send_line("+QWSCAN:\"" + st.ssid + "\",3," + std::to_string(st.rssi) + ",\"" + st.bssid + "\"," +
                  std::to_string(st.channel));
    e.ok();
  });

  // ---------------------------------------------------------------- BLE
  e.on("QBLEINIT", [this](AtEngine &e, const AtCommand &cmd) {
    if (cmd.kind == AtCommand::Kind::QUERY) {
      e.send_line(std::string("+QBLEINIT: ") + (this->ble_init_ ? "2" : "0"));
      e.ok();
      return;
    }
    this->ble_init_ = true;
    e.ok();
  });
  e.on("QBLEADDR", [this](AtEngine &e, const AtCommand &) {
    e.send_line("+QBLEADDR:" + lower_mac(this->ble_mac_));
    e.ok();
  });
  e.on("QBLENAME", [this](AtEngine &e, const AtCommand &cmd) {
    if (cmd.kind == AtCommand::Kind::QUERY) {
      e.send_line("+QBLENAME:" + this->ble_name_);
      e.ok();
      return;
    }
    std::string name = cmd.raw_args;
    if (name.size() >= 2 && name.front() == '"' && name.back() == '"')
      name = name.substr(1, name.size() - 2);
    this->ble_name_ = name;
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
  // The stock module enters transparent mode at once, connected or not: it
  // stops answering AT, sends what the MCU writes as notifications on <uuid>
  // and leaves on "+++". The B2500 uses it for bulk transfers from the app
  // and sends "+++" after 30 s without data.
  e.on("QBLETRANMODE", [this](AtEngine &e, const AtCommand &cmd) {
    this->ble_tran_uuid_ = parse_uuid16_(cmd.arg(0));
    this->ble_tran_auto_ = cmd.arg_int(1, 0) != 0;
    e.ok();
    if (this->ble_tran_uuid_ == 0)
      return;
    this->tran_buf_.clear();
    e.enter_transparent(
        [this](const uint8_t *data, size_t len) {
          this->tran_buf_.insert(this->tran_buf_.end(), data, data + len);
          this->tran_last_ms_ = this->now_;
        },
        [this]() { this->flush_tran_(true); });
  });
  // AT+QBLEGATTSNTFY=<uuid>,<n>,<payload>: n raw bytes or 2n hex digits (the
  // Hame build converts hex to binary). Without a length (plain line) the
  // data is text: the B2500's cell-info frame "soc_t1_t2_mv1_..._mv14".
  e.on_inline("QBLEGATTSNTFY", 2, true, [this](AtEngine &e, const AtCommand &cmd) {
    uint16_t uuid = parse_uuid16_(cmd.arg(0));
    std::vector<uint8_t> data;
    if (cmd.has_payload) {
      data.assign(cmd.payload.begin(), cmd.payload.end());
    } else if (cmd.args.size() >= 2) {
      const std::string &text = cmd.args.back();
      data.assign(text.begin(), text.end());
    } else {
      e.error();
      return;
    }
    if (!this->ble_session_) {
      e.error();  // stock module: no central connected
      return;
    }
    e.ok();
    this->backend_->ble_notify(uuid, data);
  });
  e.on("QBLESTAT", [this](AtEngine &e, const AtCommand &) {
    const char *st = this->ble_session_ ? "CONNECTED"
                     : this->ble_advertising_ ? "ADVERTISING"
                     : this->ble_init_ ? "INIT"
                                       : "NOINIT";
    e.send_line(std::string("+QBLESTAT:") + st);
    e.ok();
  });
  e.on("QBLECFGMTU", [](AtEngine &e, const AtCommand &cmd) {
    if (cmd.kind == AtCommand::Kind::QUERY)
      e.send_line("+QBLECFGMTU: 247");
    e.ok();
  });
  e.on("QBLEDISCONN", [this](AtEngine &e, const AtCommand &) {
    e.ok();
    this->ble_connected(false);
  });

  // ---------------------------------------------------------------- MQTT
  // The MCU's session ends here: open, connect and subscribe succeed locally
  // (cloud forwarding, if any, runs beside it in the backend).
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
    this->net_used_();
    if (cmd.kind == AtCommand::Kind::QUERY) {
      if (this->mqtt_open_)
        e.send_line("+QMTOPEN: 0,\"" + this->mqtt_session_.host + "\"," + std::to_string(this->mqtt_session_.port));
      e.ok();
      return;
    }
    e.ok();
    std::string idx = cmd.arg(0, "0");
    if (!this->got_ip_) {
      this->later_(100, [this, idx]() { this->engine_.send_line("+QMTOPEN: " + idx + ",-1"); });
      return;
    }
    this->mqtt_open_ = true;
    this->mqtt_session_.host = cmd.arg(1);
    this->mqtt_session_.port = static_cast<int>(cmd.arg_int(2, 0));
    this->later_(30, [this, idx]() { this->engine_.send_line("+QMTOPEN: " + idx + ",0"); });
  });
  e.on("QMTCONN", [this](AtEngine &e, const AtCommand &cmd) {
    this->net_used_();
    if (cmd.kind == AtCommand::Kind::QUERY) {
      if (this->mqtt_open_)
        e.send_line(std::string("+QMTCONN: 0,") + (this->mqtt_conn_ ? "3" : "1"));  // 3 = connected
      e.ok();
      return;
    }
    if (!this->mqtt_open_) {
      e.error();  // stock module: no network connection open
      return;
    }
    e.ok();
    std::string idx = cmd.arg(0, "0");
    this->mqtt_conn_ = true;
    // A new (clean) session has no subscriptions; the MCU subscribes again.
    // Otherwise commands kept going to a previous session's topic (e.g. the
    // simulator's, or the one before the app gave the device a new id).
    this->mqtt_topics_.clear();
    this->mqtt_session_.client_id = cmd.arg(1);
    this->mqtt_session_.username = cmd.arg(2);
    this->mqtt_session_.password = cmd.arg(3);
    this->backend_->mqtt_connected(this->mqtt_session_);
    this->later_(20, [this, idx]() { this->engine_.send_line("+QMTCONN: " + idx + ",0,0"); });
  });
  e.on("QMTSUB", [this](AtEngine &e, const AtCommand &cmd) {
    // AT+QMTSUB=<id>,<msgid>,"topic",<qos>[,"topic",<qos>...]
    if (!this->mqtt_conn_) {
      e.error();  // stock module
      return;
    }
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
    std::string urc = "+QMTSUB: " + cmd.arg(0, "0") + "," + std::to_string(cmd.arg_int(1, 0)) + ",0," +
                      std::to_string(cmd.arg_int(3, 0));
    this->later_(20, [this, urc]() { this->engine_.send_line(urc); });
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
    std::string urc = "+QMTUNS: " + cmd.arg(0, "0") + "," + std::to_string(cmd.arg_int(1, 0)) + ",0";
    this->later_(20, [this, urc]() { this->engine_.send_line(urc); });
  });
  auto publish = [this](AtEngine &e, const std::string &idx, long msgid, bool retain, const std::string &topic,
                        const std::string &payload) {
    if (!this->mqtt_conn_) {
      e.error();  // stock module
      return;
    }
    e.ok();
    this->backend_->mqtt_publish(topic, payload, retain);
    std::string urc = "+QMTPUB: " + idx + "," + std::to_string(msgid) + ",0";
    this->later_(10, [this, urc]() { this->engine_.send_line(urc); });
    // A broker delivers a message to every matching subscription, the
    // publisher's own included. Do it here when no real broker will.
    if (!this->backend_->mqtt_broker_echoes()) {
      for (auto &f : this->mqtt_topics_) {
        if (topic_matches(f, topic)) {
          this->later_(60, [this, topic, payload]() { this->mqtt_receive(topic, payload); });
          break;
        }
      }
    }
  };
  // AT+QMTPUB=<id>,<msgid>,<qos>,<retain>,"topic",<len>,"payload" (B2500), or
  // without <len>: "> " prompt, payload, Ctrl-Z.
  e.on_inline("QMTPUB", 6, false, [this, publish](AtEngine &e, const AtCommand &cmd) {
    std::string idx = cmd.arg(0, "0");
    long msgid = cmd.arg_int(1, 0);
    bool retain = cmd.arg_int(3, 0) != 0;
    std::string topic = cmd.arg(4);
    if (cmd.has_payload) {
      publish(e, idx, msgid, retain, topic, cmd.payload);
      return;
    }
    if (cmd.args.size() == 5) {
      e.send_raw("\r\n>");
      e.expect_until(0x1A, [publish, idx, msgid, retain, topic](AtEngine &e, std::vector<uint8_t> data) {
        publish(e, idx, msgid, retain, topic, std::string(data.begin(), data.end()));
      });
      return;
    }
    e.error();
  });
  e.on("QMTCLOSE", [this](AtEngine &e, const AtCommand &cmd) {
    if (!this->mqtt_open_) {
      e.error();  // stock module: also after QMTDISC, which closes the connection
      return;
    }
    e.ok();
    bool was = this->mqtt_conn_;
    this->mqtt_open_ = this->mqtt_conn_ = false;
    this->mqtt_topics_.clear();
    std::string urc = "+QMTCLOSE: " + cmd.arg(0, "0") + ",0";
    this->later_(10, [this, urc]() { this->engine_.send_line(urc); });
    if (was)
      this->backend_->mqtt_closed();
  });
  e.on("QMTDISC", [this](AtEngine &e, const AtCommand &cmd) {
    if (!this->mqtt_open_) {
      e.error();  // stock module
      return;
    }
    e.ok();
    bool was = this->mqtt_conn_;
    this->mqtt_conn_ = this->mqtt_open_ = false;
    this->mqtt_topics_.clear();
    std::string urc = "+QMTDISC: " + cmd.arg(0, "0") + ",0";
    this->later_(10, [this, urc]() { this->engine_.send_line(urc); });
    if (was)
      this->backend_->mqtt_closed();
  });

  // ---------------------------------------------------------------- TLS
  e.on("QSSLCERT", [this](AtEngine &e, const AtCommand &cmd) {
    if (cmd.kind == AtCommand::Kind::QUERY) {
      // '"<kind>",<exists>' per kind, all three lines in one block (single
      // CRLFs between them): some firmwares (CT00x) compare the whole reply
      // with a string. Module builds 202406141323 and 202409090159 send no
      // checksum, later builds append one (seen in the firmware emulator).
      // The B2500 looks for '"User Key",1'. Certificates are kept in RAM only,
      // so after a module boot the MCU uploads them again.
      const bool with_checksum = this->cert_checksums_();
      std::string block = "\r\n";
      for (const char *kind : {"CA", "User Cert", "User Key"}) {
        auto it = this->certs_.find(kind);
        bool have = it != this->certs_.end();
        block += std::string("+QSSLCERT: \"") + kind + "\"," + (have ? "1" : "0");
        if (with_checksum) {
          char ck[8] = "0";
          if (have)
            snprintf(ck, sizeof(ck), "%x", cert_checksum(it->second));
          block += std::string(",") + ck;
        }
        block += "\r\n";
      }
      e.send_raw(block);
      e.ok();
      return;
    }
    std::string kind = cmd.arg(0);
    long op = cmd.arg_int(1, 1);
    if (op == 0) {  // delete
      this->certs_.erase(kind);
      e.ok();
    } else if (op == 1) {  // read back
      auto it = this->certs_.find(kind);
      if (it == this->certs_.end()) {
        e.error();
        return;
      }
      e.send_line("CONNECT");
      e.send_raw(it->second);
      e.ok();
    } else if (op == 2) {  // upload: CONNECT, <len> raw bytes, OK
      long len = cmd.arg_int(2, 0);
      e.send_line("CONNECT");
      e.expect_text_data(static_cast<size_t>(len), [this, kind](AtEngine &e, std::vector<uint8_t> data) {
        this->certs_[kind] = std::string(data.begin(), data.end());
        this->backend_->ssl_cert(kind, data);
        e.ok();
      });
    } else {
      e.error();
    }
  });

  // ---------------------------------------------------------------- HTTP
  e.on("QHTTPCFG", [this](AtEngine &e, const AtCommand &cmd) {
    std::string key = cmd.arg(0);
    if (key == "url") {
      // The URL may contain commas: everything after the key.
      std::string url = raw_arg_tail(cmd.raw_args, 1);
      if (url.size() >= 2 && url.front() == '"' && url.back() == '"')
        url = url.substr(1, url.size() - 2);
      this->http_url_ = url;
    } else if (key == "contenttype" || key == "content_type") {
      this->http_content_type_ = cmd.arg(1);
    } else if (key == "header" && cmd.args.size() >= 2) {
      // AT+QHTTPCFG="header","<name>","<value>" sets, without a value removes
      // (Venus E 3.0/D/A).
      const std::string &name = cmd.arg(1);
      for (auto it = this->http_headers_.begin(); it != this->http_headers_.end(); ++it) {
        if (it->first == name) {
          this->http_headers_.erase(it);
          break;
        }
      }
      if (cmd.args.size() > 2) {
        std::string value = raw_arg_tail(cmd.raw_args, 2);
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
          value = value.substr(1, value.size() - 2);
        this->http_headers_.emplace_back(name, value);
      }
    }
    e.ok();
  });
  // The result URC comes a network round trip after OK. Sent at once it is
  // lost: a B2500 then retries its time request every 15 s, each try blocking
  // the module for 10 s (seen in the firmware emulator).
  auto request = [this](const std::string &method, std::vector<uint8_t> body) {
    const std::string urc = "+QHTTP" + method + ": ";
    const uint32_t ok_ms = this->now_;
    this->http_busy_ = true;
    this->http_body_.clear();
    this->net_used_();
    if (!this->got_ip_) {
      this->later_(50, [this, urc]() {
        this->http_busy_ = false;
        this->engine_.send_line(urc + "1");
      });
      return;
    }
    const uint32_t epoch = this->epoch_;
    HttpRequest req{method, this->http_url_, this->http_content_type_, this->http_headers_, std::move(body)};
    this->backend_->http_request(req, [this, urc, ok_ms, epoch](HttpResult r) {
                                   if (epoch != this->epoch_)
                                     return;  // the module was reset meanwhile
                                   uint32_t due = ok_ms + HTTP_RESULT_MIN_MS;
                                   uint32_t wait =
                                       static_cast<int32_t>(due - this->now_) > 0 ? due - this->now_ : 0;
                                   this->later_(wait, [this, urc, r]() {
                                     this->http_busy_ = false;
                                     this->http_body_ = r.body;
                                     if (r.error != 0) {
                                       this->engine_.send_line(urc + std::to_string(r.error));
                                       return;
                                     }
                                     this->engine_.send_line(urc + "0," + std::to_string(r.status) + "," +
                                                             std::to_string(r.body.size()));
                                   });
                                 });
  };
  e.on("QHTTPGET", [this, request](AtEngine &e, const AtCommand &) {
    if (this->http_busy_ || this->http_url_.empty()) {
      e.error();
      return;
    }
    e.ok();
    request("GET", {});
  });
  e.on("QHTTPPOST", [this, request](AtEngine &e, const AtCommand &cmd) {
    if (this->http_busy_ || this->http_url_.empty()) {
      e.error();
      return;
    }
    long len = cmd.arg_int(0, 0);
    e.send_line("CONNECT");
    e.expect_data(static_cast<size_t>(len), [request](AtEngine &e, std::vector<uint8_t> body) {
      e.ok();
      request("POST", std::move(body));
    });
  });
  e.on("QHTTPREAD", [this](AtEngine &e, const AtCommand &) {
    e.send_line("CONNECT");
    e.send_raw(this->http_body_);
    e.ok();
    this->later_(5, [this]() { this->engine_.send_line("+QHTTPREAD: 0"); });
  });

  // ---------------------------------------------------------------- sockets
  // B2500 v2/v3 reach their CT meter (UDP) and Shelly meters (TCP) this way.
  e.on("QIOPEN", [this](AtEngine &e, const AtCommand &cmd) {
    // AT+QIOPEN=<id>,"<kind>","<host>",<port>,<local port>,<access mode>
    this->net_used_();
    SocketSpec spec;
    spec.id = static_cast<int>(cmd.arg_int(0, 0));
    spec.kind = cmd.arg(1);
    for (auto &c : spec.kind)
      c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    spec.host = cmd.arg(2);
    spec.port = static_cast<int>(cmd.arg_int(3, 0));
    spec.local_port = static_cast<int>(cmd.arg_int(4, 0));
    spec.mode = static_cast<int>(cmd.arg_int(5, 0));
    if (this->sockets_.count(spec.id))
      this->backend_->socket_close(spec.id);
    Socket sock;
    sock.spec = spec;
    this->sockets_[spec.id] = sock;
    e.ok();
    std::string id = std::to_string(spec.id);
    if (!this->got_ip_) {
      this->later_(50, [this, id]() { this->engine_.send_line("+QIOPEN: " + id + ",553"); });
      return;
    }
    const uint32_t epoch = this->epoch_;
    this->backend_->socket_open(spec, [this, id, epoch](int err) {
      if (epoch != this->epoch_)
        return;
      this->later_(20, [this, id, err]() { this->engine_.send_line("+QIOPEN: " + id + "," + std::to_string(err)); });
    });
  });
  // AT+QISEND=<id>,<len>,"<data>"[,"<ip>",<port>] inline (B2500), or
  // AT+QISEND=<id>,<len> then "> " and <len> raw bytes, or AT+QISEND=<id>,0 (query).
  e.on_inline("QISEND", 2, false, [this](AtEngine &e, const AtCommand &cmd) {
    int id = static_cast<int>(cmd.arg_int(0, -1));
    auto it = this->sockets_.find(id);
    if (cmd.has_payload) {
      if (it == this->sockets_.end()) {
        e.error();
        return;
      }
      this->socket_sent_(it->second, cmd.payload, cmd.arg(2), static_cast<int>(cmd.arg_int(3, 0)));
      return;
    }
    long len = cmd.arg_int(1, -1);
    if (len == 0) {
      uint32_t tx = it != this->sockets_.end() ? it->second.tx_total : 0;
      e.send_line("+QISEND: " + std::to_string(tx) + "," + std::to_string(tx) + ",0");
      e.ok();
      return;
    }
    if (it == this->sockets_.end() || len < 0) {
      e.error();
      return;
    }
    std::string ip = cmd.arg(2);
    int port = static_cast<int>(cmd.arg_int(3, 0));
    e.send_raw("\r\n> ");
    e.expect_data(static_cast<size_t>(len), [this, id, ip, port](AtEngine &e, std::vector<uint8_t> data) {
      auto it = this->sockets_.find(id);
      if (it == this->sockets_.end()) {
        e.error();
        return;
      }
      this->socket_sent_(it->second, std::string(data.begin(), data.end()), ip, port);
    });
  });
  e.on("QIRD", [this](AtEngine &e, const AtCommand &cmd) {
    int id = static_cast<int>(cmd.arg_int(0, -1));
    auto it = this->sockets_.find(id);
    if (it == this->sockets_.end()) {
      e.error();
      return;
    }
    Socket &s = it->second;
    long n = cmd.arg_int(1, 1500);
    if (n == 0) {
      e.send_line("+QIRD: " + std::to_string(s.rx_total) + "," + std::to_string(s.rx_total - s.rx.size()) + "," +
                  std::to_string(s.rx.size()));
      e.ok();
      return;
    }
    std::string data = s.rx.substr(0, static_cast<size_t>(n));
    s.rx.erase(0, data.size());
    // Stock module, UDP SERVICE: '+QIRD: 5,"192.168.178.96",51494'.
    std::string hdr = "+QIRD: " + std::to_string(data.size());
    if (s.spec.kind == "UDP SERVICE" && !s.rx_ip.empty())
      hdr += ",\"" + s.rx_ip + "\"," + std::to_string(s.rx_port);
    e.send_raw("\r\n" + hdr + "\r\n" + data + "\r\nOK\r\n");
  });
  e.on("QICLOSE", [this](AtEngine &e, const AtCommand &cmd) {
    int id = static_cast<int>(cmd.arg_int(0, -1));
    auto it = this->sockets_.find(id);
    bool open = it != this->sockets_.end();
    bool announce = open && !it->second.closed;
    if (open) {
      this->sockets_.erase(it);
      this->backend_->socket_close(id);
    }
    e.ok();
    if (announce)  // stock module: right after OK
      e.send_line("+QIURC: \"closed\"," + std::to_string(id));
  });
  e.on("QISTATE", [this](AtEngine &e, const AtCommand &) {
    for (auto &kv : this->sockets_) {
      const SocketSpec &sp = kv.second.spec;
      // Stock module: '+QISTATE: 1,"UDP SERVICE","<own ip>",0,12345,2'.
      std::string host = sp.kind == "UDP SERVICE" ? this->backend_->wifi_status().ip : sp.host;
      e.send_line("+QISTATE: " + std::to_string(sp.id) + ",\"" + sp.kind + "\",\"" + host + "\"," +
                  std::to_string(sp.port) + "," + std::to_string(sp.local_port) + ",2");
    }
    e.ok();
  });
}

}  // namespace esphome::hame_at
