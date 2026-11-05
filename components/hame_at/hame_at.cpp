#include "hame_at.h"

#include <WiFi.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>

#include "esphome/components/wifi/wifi_component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "hame_cloud.h"

namespace esphome::hame_at {

static const char *const TAG = "hame_at";

static std::string ip_str(const IPAddress &ip) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  return buf;
}

// Quectel socket error codes (unverified for the FC41D build).
static constexpr int SOCK_ERR_CREATE = 554;
static constexpr int SOCK_ERR_BIND = 556;
static constexpr int SOCK_ERR_DNS = 565;
static constexpr int SOCK_ERR_CONNECT = 566;
static constexpr uint32_t SOCK_CONNECT_TIMEOUT_MS = 10000;

static bool resolve(const std::string &host, struct in_addr &out) {
  if (lwip_inet_pton(AF_INET, host.c_str(), &out) == 1)
    return true;
  // LAN meters are addressed by IP; a name means a (short) blocking lookup.
  struct hostent *he = lwip_gethostbyname(host.c_str());
  if (he == nullptr || he->h_addr_list[0] == nullptr)
    return false;
  memcpy(&out, he->h_addr_list[0], sizeof(out));
  return true;
}

void HameAtComponent::setup() {
  this->emulator_.engine().set_output([this](const uint8_t *data, size_t len) { this->write_array(data, len); });
  this->emulator_.engine().set_line_logger([](bool from_mcu, const std::string &line) {
    // Payloads can carry credentials (QMTCONN) and keys (QSSLCERT data is
    // never a line); keep the transcript at VERBOSE. Bytes that were not a
    // command (lost on the UART) are worth a warning: the MCU gets no answer.
    if (from_mcu && (line.rfind("<garbage>", 0) == 0 || line.rfind("<incomplete>", 0) == 0 ||
                     line.rfind("<repeated>", 0) == 0)) {
      ESP_LOGW(TAG, "MCU sent %s", line.substr(0, 48).c_str());
      return;
    }
    ESP_LOGV(TAG, "%s %s", from_mcu ? "MCU>" : "<MOD", line.c_str());
  });

  if (this->ble_mac_.empty()) {
    uint8_t mac[6];
    get_mac_address_raw(mac);
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    this->ble_mac_ = buf;
    ESP_LOGW(TAG, "ble_mac not set, using %s; set the stock module's BLE MAC to keep the battery's identity",
             buf);
  }
  this->emulator_.set_ble_mac(this->ble_mac_);

#ifdef USE_BK72XX_BLE_SERVER
  if (this->ble_ != nullptr) {
    // Phone writes go to the MCU (the stock module makes all three
    // characteristics writable); connection state is reported as +QBLESTAT.
    for (uint16_t uuid : {0xFF01, 0xFF02, 0xFF06}) {
      auto *chr = this->ble_->get_characteristic(uuid);
      if (chr != nullptr)
        chr->add_on_write_callback([this, uuid](const std::vector<uint8_t> &v) { this->emulator_.ble_write(uuid, v); });
    }
    this->ble_->add_on_connection_callback([this](bool connected) { this->emulator_.ble_connected(connected); });
    this->ble_->add_on_mtu_callback([this](uint16_t mtu) { this->emulator_.ble_mtu(mtu); });
  }
#endif

  this->worker_started_ = this->worker_.start(this->mqtt_forwarding_());
  if (!this->worker_started_)
    ESP_LOGE(TAG, "Could not start the network task; cloud forwarding and HTTP are unavailable");

  // Power-up: "ready", then the station once the MCU is quiet.
  this->emulator_.power_on(millis());
}

#ifdef USE_BK72XX
// BK7231 UART1 (P10/P11, the MCU's side), registers from the BDK's
// driver/uart/uart.h. The RX interrupt fires at 16 bytes in the FIFO or when
// the line has been idle; on the eval kit the last bytes of long MCU lines
// were seen arriving only with the next line, i.e. the idle interrupt did not
// come. When bytes sit in the FIFO with nothing arriving, lower the threshold
// to 1 for a moment: the interrupt fires and LibreTiny's handler drains them.
static volatile uint32_t *const BK_UART1_FIFO_CONFIG = reinterpret_cast<volatile uint32_t *>(0x802100 + 4 * 1);
static volatile uint32_t *const BK_UART1_FIFO_STATUS = reinterpret_cast<volatile uint32_t *>(0x802100 + 4 * 2);
static volatile uint32_t *const BK_UART1_INTR_ENABLE = reinterpret_cast<volatile uint32_t *>(0x802100 + 4 * 4);

void HameAtComponent::kick_uart_fifo_(uint32_t now) {
  const uint32_t pending = (*BK_UART1_FIFO_STATUS >> 8) & 0xFF;
  if (pending == 0) {
    this->fifo_pending_since_ = 0;
    return;
  }
  if (this->fifo_pending_since_ == 0) {
    this->fifo_pending_since_ = now == 0 ? 1 : now;
    return;
  }
  if (now - this->fifo_pending_since_ < 5)
    return;
  this->fifo_kicks_++;
  if (this->fifo_kicks_ <= 5 || this->fifo_kicks_ % 100 == 0) {
    ESP_LOGW(TAG, "%u bytes stuck in the UART FIFO, draining (interrupts 0x%02X, %u times so far)",
             static_cast<unsigned>(pending), static_cast<unsigned>(*BK_UART1_INTR_ENABLE & 0xFF),
             static_cast<unsigned>(this->fifo_kicks_));
  }
  const uint32_t cfg = *BK_UART1_FIFO_CONFIG;
  *BK_UART1_FIFO_CONFIG = (cfg & ~(0xFFu << 8)) | (1u << 8);  // RX threshold 1
  delayMicroseconds(20);                                       // the interrupt runs now
  *BK_UART1_FIFO_CONFIG = cfg;
  this->fifo_pending_since_ = 0;
}
#endif

void HameAtComponent::loop() {
  const uint32_t now = millis();

  // UART from the MCU.
  int avail = this->available();
#ifdef USE_BK72XX
  if (avail <= 0)
    this->kick_uart_fifo_(now);
#endif
  while (avail > 0) {
    size_t n = std::min<size_t>(avail, sizeof(this->rx_buf_));
    if (!this->read_array(this->rx_buf_, n))
      break;
    this->emulator_.feed(this->rx_buf_, n, now);
    avail = this->available();
  }
  this->emulator_.loop(now);
  this->service_sockets_();

  if (!this->worker_started_)
    return;

  bool up = wifi::global_wifi_component != nullptr && wifi::global_wifi_component->is_connected();
  if (up != this->network_up_) {
    this->network_up_ = up;
    this->worker_.set_network(up);
  }

  std::string topic, payload;
  while (this->worker_.pop_message(topic, payload)) {
    ESP_LOGD(TAG, "cloud -> MCU %s: %s", topic.c_str(), payload.c_str());
    this->emulator_.mqtt_receive(topic, payload);
  }
  uint32_t id;
  HttpResult result;
  while (this->worker_.pop_http_result(id, result)) {
    auto it = this->http_pending_.find(id);
    if (it == this->http_pending_.end())
      continue;
    auto done = std::move(it->second);
    this->http_pending_.erase(it);
    done(std::move(result));
  }
  std::string line;
  while (this->worker_.pop_log(line))
    ESP_LOGI(TAG, "%s", line.c_str());

  if (now - this->last_status_ms_ > 1000) {
    this->last_status_ms_ = now;
    this->publish_cloud_status_();
  }
}

void HameAtComponent::publish_cloud_status_() {
  std::string st = this->worker_.cloud_state();
  if (st == this->last_cloud_status_)
    return;
  this->last_cloud_status_ = st;
  ESP_LOGD(TAG, "Cloud: %s", st.c_str());
#ifdef USE_TEXT_SENSOR
  if (this->cloud_status_ != nullptr)
    this->cloud_status_->publish_state(st);
#endif
}

void HameAtComponent::set_cloud_enabled(bool enabled) {
  if (enabled == this->cloud_enabled_)
    return;
  this->cloud_enabled_ = enabled;
  ESP_LOGI(TAG, "Cloud access %s", enabled ? "enabled" : "disabled (everything answered locally)");
  if (this->worker_started_)
    this->worker_.set_cloud_enabled(this->mqtt_forwarding_());
}

void HameAtComponent::set_http_mode(HameEndpoint endpoint, CloudMode mode) {
  switch (endpoint) {
    case HameEndpoint::TIME:
      this->time_mode_ = mode;
      break;
    case HameEndpoint::DEVICE_CONFIG:
      this->config_mode_ = mode;
      break;
    case HameEndpoint::REPORTS:
      this->reports_mode_ = mode;
      break;
    case HameEndpoint::FIRMWARE:
      break;  // allow_mcu_ota
  }
}

CloudMode HameAtComponent::http_mode_(HameEndpoint endpoint) const {
  if (!this->cloud_enabled_)
    return CloudMode::LOCAL;
  switch (endpoint) {
    case HameEndpoint::TIME:
      return this->time_mode_;
    case HameEndpoint::DEVICE_CONFIG:
      return this->config_mode_;
    case HameEndpoint::REPORTS:
      return this->reports_mode_;
    case HameEndpoint::FIRMWARE:
      return CloudMode::FORWARD;
  }
  return CloudMode::LOCAL;
}

static const char *mode_name(CloudMode m) {
  return m == CloudMode::FORWARD ? "forward" : m == CloudMode::LOCAL ? "local" : "auto";
}

void HameAtComponent::send_to_mcu(const std::string &payload, const std::string &topic) {
  std::string t = topic;
  if (t.empty()) {
    const auto &topics = this->emulator_.mqtt_topics();
    if (topics.empty()) {
      ESP_LOGW(TAG, "MCU has no MQTT subscription yet; message dropped");
      return;
    }
    t = topics.front();
  }
  this->emulator_.mqtt_receive(t, payload);
}

void HameAtComponent::ble_write_to_mcu(const std::vector<uint8_t> &data, uint16_t uuid) {
  this->emulator_.ble_write(uuid, data);
}

// ---------------------------------------------------------------------------
// Fc41dBackend
// ---------------------------------------------------------------------------

WifiStatus HameAtComponent::wifi_status() {
  WifiStatus st;
  st.connected = wifi::global_wifi_component != nullptr && wifi::global_wifi_component->is_connected();
  if (!st.connected)
    return st;
  st.ssid = WiFi.SSID().c_str();
  st.bssid = WiFi.BSSIDstr().c_str();
  st.rssi = WiFi.RSSI();
  st.channel = WiFi.channel();
  st.ip = ip_str(WiFi.localIP());
  st.gateway = ip_str(WiFi.gatewayIP());
  st.netmask = ip_str(WiFi.subnetMask());
  st.dns = ip_str(WiFi.dnsIP(0));
  uint8_t mac[6];
  get_mac_address_raw(mac);
  char buf[18];
  snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  st.mac = buf;
  return st;
}

void HameAtComponent::wifi_credentials(const std::string &ssid, const std::string & /*password*/) {
  // ESPHome owns the station; app provisioning does not change it.
  ESP_LOGI(TAG, "MCU stored Wi-Fi credentials for '%s' (ignored; ESPHome manages Wi-Fi)", ssid.c_str());
}

void HameAtComponent::ble_advertise(const std::string &name, uint16_t service,
                                    const std::vector<uint16_t> & /*chars*/) {
#ifdef USE_BK72XX_BLE_SERVER
  if (this->ble_ != nullptr) {
    ESP_LOGI(TAG, "MCU set up BLE: '%s', service 0x%04X", name.c_str(), service);
    this->ble_->start_advertising(name);
    return;
  }
#endif
  ESP_LOGD(TAG, "MCU asked for BLE advertising as '%s' (no BLE server configured)", name.c_str());
}

void HameAtComponent::ble_stop_advertising() {
#ifdef USE_BK72XX_BLE_SERVER
  if (this->ble_ != nullptr)
    this->ble_->stop_advertising();
#endif
}

void HameAtComponent::ble_notify(uint16_t uuid, const std::vector<uint8_t> &data) {
#ifdef USE_BK72XX_BLE_SERVER
  if (this->ble_ != nullptr) {
    auto *chr = this->ble_->get_characteristic(uuid);
    if (chr != nullptr)
      chr->notify(data);
  }
#endif
  this->notify_callbacks_.call(uuid, data);
  this->on_ble_notify_.trigger(data);
}

void HameAtComponent::mqtt_connected(const MqttSession &session) {
  ESP_LOGI(TAG, "MCU MQTT session: %s:%d client '%s'%s", session.host.c_str(), session.port,
           session.client_id.c_str(), session.ssl ? " (TLS)" : "");
  if (this->worker_started_)
    this->worker_.set_session(session);
}

void HameAtComponent::mqtt_closed() {
  if (this->worker_started_)
    this->worker_.clear_session();
}

void HameAtComponent::mqtt_subscribed(const std::string &topic) {
  ESP_LOGD(TAG, "MCU subscribed to %s", topic.c_str());
  this->mcu_subscribed_ms_ = millis();
  if (this->worker_started_)
    this->worker_.subscribe(topic);
}

void HameAtComponent::mqtt_publish(const std::string &topic, const std::string &payload, bool retain) {
  ESP_LOGD(TAG, "MCU -> %s: %s", topic.c_str(), payload.c_str());
  this->on_mcu_publish_.trigger(topic, payload);
  this->publish_callbacks_.call(topic, payload);
  if (this->worker_started_ && this->mqtt_forwarding_())
    this->worker_.publish(topic, payload, retain);
}

bool HameAtComponent::mqtt_broker_echoes() {
  // A connected cloud session delivers the MCU's publishes back to it;
  // otherwise the emulator echoes them itself.
  return this->worker_started_ && this->mqtt_forwarding_() && this->worker_.cloud_state() == "connected";
}

bool HameAtComponent::local_time_(LocalTime &out) {
#ifdef USE_TIME
  if (this->time_ == nullptr)
    return false;
  ESPTime t = this->time_->now();
  if (!t.is_valid())
    return false;
  out = {t.year, t.month, t.day_of_month, t.hour, t.minute, t.second};
  return true;
#else
  return false;
#endif
}

void HameAtComponent::answer_locally_(const HttpRequest &request, std::function<void(HttpResult)> done) {
  // Answer as Marsrelay does: the battery needs the clock reply and its
  // output limits (getDeviceFire); reports are acknowledged.
  LocalTime t;
  bool have_time = this->local_time_(t);
  HttpResult r;
  std::string body(request.body.begin(), request.body.end());
  if (!local_hame_answer(request.method, request.url, body, have_time ? &t : nullptr, r.status, r.body)) {
    ESP_LOGW(TAG, "No valid time for the battery's clock request yet");
    r.status = 0;
    r.error = 702;
  } else {
    ESP_LOGD(TAG, "HTTP %s %s answered locally (%d)", request.method.c_str(), request.url.c_str(), r.status);
  }
  done(r);
}

void HameAtComponent::forward_http_(const HttpRequest &request, std::function<void(HttpResult)> done) {
  if (!this->worker_started_) {
    HttpResult r;
    r.error = 701;
    done(r);
    return;
  }
  HttpJob job;
  job.id = this->next_http_id_++;
  job.method = request.method;
  job.url = request.url;
  job.content_type = request.content_type;
  job.headers = request.headers;
  job.body = request.body;
  this->http_pending_[job.id] = std::move(done);
  this->worker_.http(std::move(job));
}

void HameAtComponent::http_request(const HttpRequest &request, std::function<void(HttpResult)> done) {
  // LAN requests (CT, P1 and Shelly meters) always go out.
  if (!is_hame_cloud_url(request.url)) {
    this->forward_http_(request, std::move(done));
    return;
  }
  const HameEndpoint kind = classify_hame_url(request.url);
  // MCU firmware images: the update is driven by the cloud/app and fetched by
  // the MCU itself. Only let it through when explicitly allowed.
  if (kind == HameEndpoint::FIRMWARE && !(this->allow_mcu_ota_ && this->cloud_enabled_)) {
    ESP_LOGW(TAG, "Blocked an MCU firmware download (%s); set allow_mcu_ota to permit", request.url.c_str());
    HttpResult r;
    r.status = 404;
    done(r);
    return;
  }
  const CloudMode mode = this->http_mode_(kind);
  ESP_LOGV(TAG, "HTTP %s (%s): %s", request.url.c_str(), hame_endpoint_name(kind), mode_name(mode));
  if (mode == CloudMode::LOCAL) {
    this->answer_locally_(request, std::move(done));
    return;
  }
  if (mode == CloudMode::FORWARD) {
    this->forward_http_(request, std::move(done));
    return;
  }
  // AUTO: the real cloud, and the local answer when that fails or is slow.
  // Slow happens: the net worker may be in a TLS handshake (~12 s with a
  // client key), and the MCU ignores commands while it waits for HTTP.
  HttpRequest copy = request;
  auto answered = std::make_shared<bool>(false);  // main loop only
  this->set_timeout(AUTO_HTTP_WAIT_MS, [this, copy, done, answered]() {
    if (*answered)
      return;
    *answered = true;
    ESP_LOGI(TAG, "Cloud slow for %s; answering locally", hame_endpoint_name(classify_hame_url(copy.url)));
    this->answer_locally_(copy, done);
  });
  this->forward_http_(request, [this, copy, done, answered](HttpResult r) {
    if (*answered)
      return;  // answered locally meanwhile
    *answered = true;
    if (r.error == 0 && r.status >= 200 && r.status < 500) {
      done(std::move(r));
      return;
    }
    ESP_LOGI(TAG, "Cloud unavailable for %s (%s); answering locally", hame_endpoint_name(classify_hame_url(copy.url)),
             r.error ? ("error " + std::to_string(r.error)).c_str() : ("HTTP " + std::to_string(r.status)).c_str());
    this->answer_locally_(copy, done);
  });
}

// ---------------------------------------------------------------------------
// Sockets: non-blocking lwIP, serviced from loop(). The MCU reaches its CT
// meter over a UDP service and Shelly meters over TCP.
// ---------------------------------------------------------------------------

void HameAtComponent::socket_open(const SocketSpec &spec, std::function<void(int)> done) {
  this->socket_close(spec.id);
  const bool udp = spec.kind.rfind("UDP", 0) == 0;
  int fd = lwip_socket(AF_INET, udp ? SOCK_DGRAM : SOCK_STREAM, 0);
  if (fd < 0) {
    done(SOCK_ERR_CREATE);
    return;
  }
  int one = 1;
  lwip_fcntl(fd, F_SETFL, lwip_fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
  LiveSocket ls;
  ls.spec = spec;
  ls.fd = fd;
  if (spec.kind == "UDP SERVICE") {
    lwip_setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
    lwip_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in local {};
    local.sin_family = AF_INET;
    local.sin_port = htons(spec.local_port);
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    if (lwip_bind(fd, reinterpret_cast<struct sockaddr *>(&local), sizeof(local)) != 0) {
      lwip_close(fd);
      done(SOCK_ERR_BIND);
      return;
    }
    ESP_LOGD(TAG, "Socket %d: UDP service on port %d", spec.id, spec.local_port);
    this->sockets_[spec.id] = ls;
    done(0);
    return;
  }
  struct sockaddr_in remote {};
  remote.sin_family = AF_INET;
  remote.sin_port = htons(spec.port);
  if (!resolve(spec.host, remote.sin_addr)) {
    lwip_close(fd);
    done(SOCK_ERR_DNS);
    return;
  }
  int r = lwip_connect(fd, reinterpret_cast<struct sockaddr *>(&remote), sizeof(remote));
  if (udp || r == 0) {
    ESP_LOGD(TAG, "Socket %d: %s to %s:%d", spec.id, spec.kind.c_str(), spec.host.c_str(), spec.port);
    this->sockets_[spec.id] = ls;
    done(0);
    return;
  }
  if (errno != EINPROGRESS) {
    lwip_close(fd);
    done(SOCK_ERR_CONNECT);
    return;
  }
  ls.connecting = true;
  ls.started = millis();
  ls.open_done = std::move(done);
  this->sockets_[spec.id] = std::move(ls);
}

void HameAtComponent::socket_send(int id, const std::string &data, const std::string &ip, int port) {
  auto it = this->sockets_.find(id);
  if (it == this->sockets_.end() || it->second.connecting)
    return;
  LiveSocket &ls = it->second;
  std::string host = ip.empty() ? ls.spec.host : ip;
  int to_port = ip.empty() ? ls.spec.port : port;
  if (ls.spec.kind == "UDP SERVICE") {
    struct sockaddr_in to {};
    to.sin_family = AF_INET;
    to.sin_port = htons(to_port);
    if (host.empty() || to_port == 0 || !resolve(host, to.sin_addr)) {
      ESP_LOGW(TAG, "Socket %d: no target for %u bytes", id, (unsigned) data.size());
      return;
    }
    lwip_sendto(ls.fd, data.data(), data.size(), 0, reinterpret_cast<struct sockaddr *>(&to), sizeof(to));
    return;
  }
  lwip_send(ls.fd, data.data(), data.size(), 0);
}

void HameAtComponent::socket_close(int id) {
  auto it = this->sockets_.find(id);
  if (it == this->sockets_.end())
    return;
  lwip_close(it->second.fd);
  this->sockets_.erase(it);
}

void HameAtComponent::service_sockets_() {
  char buf[1460];
  for (auto it = this->sockets_.begin(); it != this->sockets_.end();) {
    int id = it->first;
    LiveSocket &ls = it->second;
    if (ls.connecting) {
      int err = 0;
      socklen_t len = sizeof(err);
      fd_set wfds;
      FD_ZERO(&wfds);
      FD_SET(ls.fd, &wfds);
      struct timeval tv {};
      int ready = lwip_select(ls.fd + 1, nullptr, &wfds, nullptr, &tv);
      if (ready > 0) {
        lwip_getsockopt(ls.fd, SOL_SOCKET, SO_ERROR, &err, &len);
      } else if (millis() - ls.started < SOCK_CONNECT_TIMEOUT_MS) {
        ++it;
        continue;
      } else {
        err = ETIMEDOUT;
      }
      auto done = std::move(ls.open_done);
      if (err != 0) {
        lwip_close(ls.fd);
        it = this->sockets_.erase(it);
        if (done)
          done(SOCK_ERR_CONNECT);
        continue;
      }
      ls.connecting = false;
      if (done)
        done(0);
      ++it;
      continue;
    }
    bool closed = false;
    for (int guard = 0; guard < 8; guard++) {
      struct sockaddr_in from {};
      socklen_t from_len = sizeof(from);
      int n = lwip_recvfrom(ls.fd, buf, sizeof(buf), 0, reinterpret_cast<struct sockaddr *>(&from), &from_len);
      if (n > 0) {
        char ip[16] = "";
        if (from_len >= sizeof(from) && from.sin_family == AF_INET)
          lwip_inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));
        this->emulator_.socket_received(id, std::string(buf, n), ip, ntohs(from.sin_port));
        continue;
      }
      if (n == 0 && ls.spec.kind.rfind("TCP", 0) == 0)
        closed = true;
      else if (n < 0 && errno != EWOULDBLOCK && errno != EAGAIN)
        closed = ls.spec.kind.rfind("TCP", 0) == 0;
      break;
    }
    if (closed) {
      lwip_close(ls.fd);
      it = this->sockets_.erase(it);
      this->emulator_.socket_closed(id);
      continue;
    }
    ++it;
  }
}

void HameAtComponent::ssl_cert(const std::string &kind, const std::vector<uint8_t> &data) {
  ESP_LOGD(TAG, "MCU uploaded %s (%u bytes)", kind.c_str(), (unsigned) data.size());
  if (kind != "CA" && kind != "User Cert" && kind != "User Key") {
    ESP_LOGW(TAG, "Unknown certificate kind '%s'", kind.c_str());
    return;
  }
  // Only the net worker keeps them (for the cloud link): RAM is short, and
  // the TLS handshake with a client certificate needs every KB.
  if (this->worker_started_)
    this->worker_.set_credential(kind, std::string(data.begin(), data.end()));
}

void HameAtComponent::module_restart() {
  ESP_LOGI(TAG, "MCU restarted the module");
  if (this->worker_started_)
    this->worker_.clear_session();
}

void HameAtComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Hame AT emulator:\n"
                "  BLE MAC: %s\n"
                "  Cloud access: %s\n"
                "    MQTT: %s, time: %s, device config: %s, reports: %s\n"
                "  MCU OTA: %s",
                this->ble_mac_.c_str(), this->cloud_enabled_ ? "on" : "off (all local)", mode_name(this->mqtt_mode_),
                mode_name(this->time_mode_), mode_name(this->config_mode_), mode_name(this->reports_mode_),
                this->allow_mcu_ota_ ? "allowed" : "blocked");
}

}  // namespace esphome::hame_at
