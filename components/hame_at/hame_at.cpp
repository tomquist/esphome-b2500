#include "hame_at.h"

#include <WiFi.h>

#include <cstdio>

#include "esphome/components/wifi/wifi_component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::hame_at {

static const char *const TAG = "hame_at";

static std::string host_of(const std::string &url) {
  size_t start = url.find("://");
  start = start == std::string::npos ? 0 : start + 3;
  size_t end = url.find_first_of(":/?", start);
  return url.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

static std::string ip_str(const IPAddress &ip) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  return buf;
}

static bool ends_with(const std::string &s, const std::string &suffix) {
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

void HameAtComponent::setup() {
  this->emulator_.engine().set_output([this](const uint8_t *data, size_t len) { this->write_array(data, len); });
  this->emulator_.engine().set_line_logger([](bool from_mcu, const std::string &line) {
    // Payloads can carry credentials (QMTCONN) and keys (QSSLCERT data is
    // never a line); keep the transcript at VERBOSE.
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
    // Phone writes go to the MCU; connection state is reported as +QBLESTAT.
    for (uint16_t uuid : {0xFF01, 0xFF06}) {
      auto *chr = this->ble_->get_characteristic(uuid);
      if (chr != nullptr)
        chr->add_on_write_callback([this, uuid](const std::vector<uint8_t> &v) { this->emulator_.ble_write(uuid, v); });
    }
    this->ble_->add_on_connection_callback([this](bool connected) { this->emulator_.ble_connected(connected); });
  }
#endif

  this->worker_started_ = this->worker_.start(this->cloud_enabled_);
  if (!this->worker_started_)
    ESP_LOGE(TAG, "Could not start the network task; cloud forwarding and HTTP are unavailable");

  // The MCU may have been waiting for the module since power-up.
  this->emulator_.announce_ready();
}

void HameAtComponent::loop() {
  const uint32_t now = millis();

  // UART from the MCU.
  int avail = this->available();
  while (avail > 0) {
    size_t n = std::min<size_t>(avail, sizeof(this->rx_buf_));
    if (!this->read_array(this->rx_buf_, n))
      break;
    this->emulator_.feed(this->rx_buf_, n, now);
    avail = this->available();
  }
  this->emulator_.loop(now);

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
  ESP_LOGI(TAG, "Cloud forwarding %s", enabled ? "enabled" : "disabled");
  if (this->worker_started_)
    this->worker_.set_cloud_enabled(enabled);
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
  if (this->worker_started_) {
    this->worker_.set_credentials(this->creds_);
    this->worker_.set_session(session);
  }
}

void HameAtComponent::mqtt_closed() {
  if (this->worker_started_)
    this->worker_.clear_session();
}

void HameAtComponent::mqtt_subscribed(const std::string &topic) {
  ESP_LOGD(TAG, "MCU subscribed to %s", topic.c_str());
  if (this->worker_started_)
    this->worker_.subscribe(topic);
}

void HameAtComponent::mqtt_publish(const std::string &topic, const std::string &payload) {
  ESP_LOGD(TAG, "MCU -> %s: %s", topic.c_str(), payload.c_str());
  this->on_mcu_publish_.trigger(topic, payload);
  if (this->worker_started_)
    this->worker_.publish(topic, payload);  // dropped by the worker when the cloud is off
}

bool HameAtComponent::local_date_info_(std::string &body) {
#ifdef USE_TIME
  if (this->time_ == nullptr)
    return false;
  ESPTime t = this->time_->now();
  if (!t.is_valid())
    return false;
  // "_YYYY_MM_DD_hh_mm_ss_" in local time, then four fields a CT ignores
  // (AstraMeter docs/marstek-mqtt-http.md §6.3). unverified for the B2500.
  char buf[48];
  snprintf(buf, sizeof(buf), "_%04d_%02d_%02d_%02d_%02d_%02d_04_0_0_0", t.year, t.month, t.day_of_month, t.hour,
           t.minute, t.second);
  body = buf;
  return true;
#else
  return false;
#endif
}

void HameAtComponent::http_request(const std::string &method, const std::string &url,
                                   const std::string &content_type, const std::vector<uint8_t> &body,
                                   std::function<void(HttpResult)> done) {
  const std::string host = host_of(url);
  const bool hame = ends_with(host, "hamedata.com");

  // MCU firmware images: the update is driven by the cloud/app and fetched by
  // the MCU itself. Only let it through when explicitly allowed.
  if (hame && !this->allow_mcu_ota_ && (url.find(".bin") != std::string::npos)) {
    ESP_LOGW(TAG, "Blocked an MCU firmware download (%s); set allow_mcu_ota to permit", url.c_str());
    HttpResult r;
    r.status = 404;
    done(r);
    return;
  }

  if (hame && !this->cloud_enabled_) {
    HttpResult r;
    r.status = 200;
    if (url.find("getDateInfo") != std::string::npos) {
      if (!this->local_date_info_(r.body)) {
        ESP_LOGW(TAG, "No valid time for the battery's clock request yet");
        r.status = 0;
        r.error = 702;
      }
    }
    // Stats uploads (put_tenmins_v2, puterrinfo, setB2500Report...): accept and drop.
    ESP_LOGD(TAG, "HTTP %s %s answered locally", method.c_str(), url.c_str());
    done(r);
    return;
  }

  if (!this->worker_started_) {
    HttpResult r;
    r.error = 701;
    done(r);
    return;
  }
  HttpJob job;
  job.id = this->next_http_id_++;
  job.method = method;
  job.url = url;
  job.content_type = content_type;
  job.body = body;
  this->http_pending_[job.id] = std::move(done);
  this->worker_.http(std::move(job));
}

void HameAtComponent::ssl_cert(const std::string &kind, const std::vector<uint8_t> &data) {
  std::string pem(data.begin(), data.end());
  if (kind == "CA")
    this->creds_.ca = pem;
  else if (kind == "User Cert")
    this->creds_.client_cert = pem;
  else if (kind == "User Key")
    this->creds_.client_key = pem;
  else
    ESP_LOGW(TAG, "Unknown certificate kind '%s'", kind.c_str());
  ESP_LOGD(TAG, "MCU uploaded %s (%u bytes)", kind.c_str(), (unsigned) data.size());
  if (this->worker_started_)
    this->worker_.set_credentials(this->creds_);
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
                "  Cloud forwarding: %s\n"
                "  MCU OTA: %s",
                this->ble_mac_.c_str(), YESNO(this->cloud_enabled_), this->allow_mcu_ota_ ? "allowed" : "blocked");
}

}  // namespace esphome::hame_at
