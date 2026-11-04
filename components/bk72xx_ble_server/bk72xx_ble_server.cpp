#include "bk72xx_ble_server.h"

#ifdef USE_BK72XX_BLE_SERVER

#include <algorithm>
#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::bk72xx_ble_server {

static const char *const TAG = "bk72xx_ble_server";

static constexpr uint32_t RETRY_MS = 250;
static constexpr uint32_t DB_TIMEOUT_MS = 5000;
static constexpr uint16_t VALUE_MAX_SIZE = 512;

// The BDK notice callback has no user argument.
static BK72xxBLEServer *s_server = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static uint16_t notice_trampoline(BdkNotice notice, uint8_t conn_idx, uint16_t att_idx, const uint8_t *data,
                                  uint16_t len, uint16_t value16, uint8_t *read_buf, uint16_t read_cap) {
  if (s_server == nullptr)
    return 0;
  return s_server->on_notice_(notice, conn_idx, att_idx, data, len, value16, read_buf, read_cap);
}

// ---------------------------------------------------------------------------
// BLECharacteristic
// ---------------------------------------------------------------------------

bool BLECharacteristic::notify(const std::vector<uint8_t> &value) {
  if (this->parent_ == nullptr)
    return false;
  return this->parent_->notify_(this, value);
}

void BLECharacteristic::set_value(const std::vector<uint8_t> &value) {
  if (this->parent_ != nullptr)
    this->parent_->set_read_value_(this, value);
}

// ---------------------------------------------------------------------------
// BLE task side: copy out and return. Never touches triggers.
// ---------------------------------------------------------------------------

uint16_t BK72xxBLEServer::on_notice_(BdkNotice notice, uint8_t conn_idx, uint16_t att_idx, const uint8_t *data,
                                     uint16_t len, uint16_t value16, uint8_t *read_buf, uint16_t read_cap) {
  if (notice == BdkNotice::READ) {
    // Answered synchronously from the cached value.
    LockGuard guard(this->read_lock_);
    for (auto &rv : this->read_values_) {
      if (rv.att_idx == att_idx) {
        uint16_t n = std::min<uint16_t>(rv.len, read_cap);
        memcpy(read_buf, rv.data, n);
        return n;
      }
    }
    // CCC descriptor reads.
    for (auto *chr : this->characteristics_) {
      if (chr->ccc_idx_ != 0 && chr->ccc_idx_ == att_idx && read_cap >= 2) {
        read_buf[0] = chr->ccc_ & 0xFF;
        read_buf[1] = chr->ccc_ >> 8;
        return 2;
      }
    }
    return 0;
  }

  if (notice == BdkNotice::WRITE) {
    // CCC writes are tracked right here so a notify issued on the main task
    // straight after sees the subscription.
    for (auto *chr : this->characteristics_) {
      if (chr->ccc_idx_ != 0 && chr->ccc_idx_ == att_idx && len >= 2 && data != nullptr) {
        chr->ccc_ = data[0] | (data[1] << 8);
      }
    }
  }

  GattsEvent *ev = this->pool_.allocate();
  if (ev == nullptr) {
    this->queue_.increment_dropped_count();
    return 0;
  }
  ev->notice = notice;
  ev->conn_idx = conn_idx;
  ev->att_idx = att_idx;
  ev->value16 = value16;
  ev->len = std::min<uint16_t>(len, sizeof(ev->data));
  if (data != nullptr && ev->len > 0)
    memcpy(ev->data, data, ev->len);
  this->queue_.push(ev);
  return 0;
}

// ---------------------------------------------------------------------------
// Main task
// ---------------------------------------------------------------------------

void BK72xxBLEServer::setup() {
  s_server = this;
  this->build_db_();
  this->build_adv_();
}

void BK72xxBLEServer::build_db_() {
  // Attribute 0: primary service declaration. The comm profile derives
  // att_idx = handle - start handle, so these indices are what the SDK reports.
  this->db_.clear();
  this->db_.push_back({UUID_PRIMARY_SERVICE, bdk_perm_read(), 0, 0});
  for (auto *chr : this->characteristics_) {
    // Characteristic declaration.
    this->db_.push_back({UUID_CHARACTERISTIC, bdk_perm_read(), 0, 0});
    uint16_t perm = 0;
    if (chr->read_)
      perm |= bdk_perm_read();
    if (chr->write_)
      perm |= bdk_perm_write_req();
    if (chr->write_no_response_)
      perm |= bdk_perm_write_cmd();
    if (chr->notify_)
      perm |= bdk_perm_notify();
    chr->value_idx_ = this->db_.size();
    this->db_.push_back({chr->uuid_, perm, bdk_ext_perm_read_ind(), VALUE_MAX_SIZE});
    if (chr->notify_) {
      chr->ccc_idx_ = this->db_.size();
      this->db_.push_back({UUID_CCCD, static_cast<uint16_t>(bdk_perm_read() | bdk_perm_write_req()), 0, 0});
    }
    if (chr->read_) {
      ReadValue rv{};
      rv.att_idx = chr->value_idx_;
      this->read_values_.push_back(rv);
    }
  }
}

void BK72xxBLEServer::build_adv_() {
  // Advertising: flags + complete local name (truncated to fit 31 bytes).
  this->adv_data_ = {0x02, 0x01, 0x06};
  std::string name = this->name_.substr(0, 31 - 3 - 2);
  this->adv_data_.push_back(name.size() + 1);
  this->adv_data_.push_back(0x09);
  this->adv_data_.insert(this->adv_data_.end(), name.begin(), name.end());
  // Scan response: the 16-bit service UUID.
  this->rsp_data_ = {0x03, 0x03, static_cast<uint8_t>(this->service_uuid_ & 0xFF),
                     static_cast<uint8_t>(this->service_uuid_ >> 8)};
}

void BK72xxBLEServer::loop() {
  // Drain SDK events first.
  GattsEvent *ev;
  while ((ev = this->queue_.pop()) != nullptr) {
    this->handle_event_(*ev);
    this->pool_.release(ev);
  }
  uint16_t dropped = this->queue_.get_and_reset_dropped_count();
  if (dropped > 0)
    ESP_LOGW(TAG, "Dropped %u BLE events (queue full)", dropped);

  const uint32_t now = millis();
  switch (this->state_) {
    case State::WAIT_STACK:
      if (!this->ble_->is_active()) {
        this->ble_->enable();
        return;
      }
      // bk72xx_ble registered its scan-report callback in enable(); this
      // component owns the notice callback from here on (no scanning).
      bdk_install_notice_handler(notice_trampoline);
      this->state_ = State::CREATE_DB;
      this->last_attempt_ms_ = 0;
      return;

    case State::CREATE_DB:
      if (now - this->last_attempt_ms_ < RETRY_MS)
        return;
      this->last_attempt_ms_ = now;
      if (bdk_create_db(this->prf_id_, this->service_uuid_, this->db_.data(), this->db_.size())) {
        ESP_LOGD(TAG, "Service 0x%04X: creating %u attributes", this->service_uuid_, (unsigned) this->db_.size());
        this->state_ = State::WAIT_DB;
      }
      return;

    case State::WAIT_DB:
      if (now - this->last_attempt_ms_ > DB_TIMEOUT_MS) {
        ESP_LOGE(TAG, "Service database creation timed out");
        this->state_ = State::FAILED;
        this->mark_failed();
      }
      return;

    case State::ADVERTISE:
      if (!this->adv_wanted_ || this->connected_) {
        this->state_ = State::RUNNING;
        return;
      }
      if (now - this->last_attempt_ms_ < RETRY_MS)
        return;
      this->last_attempt_ms_ = now;
      if (bdk_adv_start(this->adv_data_.data(), this->adv_data_.size(), this->rsp_data_.data(),
                        this->rsp_data_.size(), this->adv_interval_)) {
        ESP_LOGI(TAG, "Advertising as '%s'", this->name_.c_str());
        this->state_ = State::RUNNING;
      }
      return;

    case State::RUNNING:
    case State::FAILED:
      return;
  }
}

void BK72xxBLEServer::handle_event_(const GattsEvent &ev) {
  switch (ev.notice) {
    case BdkNotice::CREATE_DB: {
      uint8_t status = ev.value16 & 0xFF;
      if (this->state_ != State::WAIT_DB)
        return;
      if (status != 0) {
        ESP_LOGE(TAG, "Service database creation failed (status %u)", status);
        this->state_ = State::FAILED;
        this->mark_failed();
        return;
      }
      ESP_LOGD(TAG, "Service database created");
      this->state_ = State::ADVERTISE;
      this->last_attempt_ms_ = 0;
      return;
    }
    case BdkNotice::CONNECT: {
      this->connected_ = true;
      this->conn_idx_ = ev.conn_idx;
      this->mtu_ = 23;
      char addr[18];
      // Peer address arrives LSB first.
      snprintf(addr, sizeof(addr), "%02X:%02X:%02X:%02X:%02X:%02X", ev.data[5], ev.data[4], ev.data[3], ev.data[2],
               ev.data[1], ev.data[0]);
      ESP_LOGI(TAG, "Central %s connected (conn %u)", addr, ev.conn_idx);
      this->connection_callbacks_.call(true);
      this->on_connect_.trigger();
      return;
    }
    case BdkNotice::DISCONNECT: {
      ESP_LOGI(TAG, "Central disconnected (reason 0x%02X)", ev.value16);
      this->connected_ = false;
      for (auto *chr : this->characteristics_)
        chr->ccc_ = 0;
      this->connection_callbacks_.call(false);
      this->on_disconnect_.trigger();
      if (this->state_ == State::RUNNING) {
        this->state_ = State::ADVERTISE;
        this->last_attempt_ms_ = 0;
      }
      return;
    }
    case BdkNotice::MTU_CHANGE:
      this->mtu_ = ev.value16;
      ESP_LOGD(TAG, "MTU %u", this->mtu_);
      this->mtu_callbacks_.call(this->mtu_);
      return;
    case BdkNotice::WRITE: {
      for (auto *chr : this->characteristics_) {
        if (chr->ccc_idx_ != 0 && chr->ccc_idx_ == ev.att_idx) {
          ESP_LOGD(TAG, "0x%04X notifications %s", chr->uuid_, chr->ccc_ ? "enabled" : "disabled");
          return;
        }
        if (chr->value_idx_ == ev.att_idx) {
          std::vector<uint8_t> value(ev.data, ev.data + ev.len);
          char hex[format_hex_pretty_size(16)];
          ESP_LOGV(TAG, "Write 0x%04X (%u bytes): %s", chr->uuid_, ev.len,
                   format_hex_pretty_to(hex, ev.data, std::min<uint16_t>(ev.len, 16)));
          chr->write_callbacks_.call(value);
          chr->on_write_.trigger(value);
          return;
        }
      }
      ESP_LOGW(TAG, "Write to unknown attribute %u", ev.att_idx);
      return;
    }
    default:
      return;
  }
}

bool BK72xxBLEServer::notify_(BLECharacteristic *chr, const std::vector<uint8_t> &value) {
  this->set_read_value_(chr, value);
  if (!this->connected_ || !chr->notify_)
    return false;
  uint16_t max_len = this->mtu_ > 3 ? this->mtu_ - 3 : 20;
  if (value.size() > max_len) {
    ESP_LOGW(TAG, "Notification of %u bytes exceeds MTU payload %u; truncated", (unsigned) value.size(), max_len);
  }
  uint16_t len = std::min<size_t>(value.size(), max_len);
  if (!bdk_notify(this->conn_idx_, this->prf_id_, chr->value_idx_, value.data(), len)) {
    ESP_LOGW(TAG, "Notify on 0x%04X failed", chr->uuid_);
    return false;
  }
  return true;
}

void BK72xxBLEServer::set_read_value_(BLECharacteristic *chr, const std::vector<uint8_t> &value) {
  if (!chr->read_)
    return;
  LockGuard guard(this->read_lock_);
  for (auto &rv : this->read_values_) {
    if (rv.att_idx == chr->value_idx_) {
      rv.len = std::min<size_t>(value.size(), MAX_READ_VALUE);
      memcpy(rv.data, value.data(), rv.len);
      return;
    }
  }
}

void BK72xxBLEServer::start_advertising(const std::string &name) {
  if (!name.empty() && name != this->name_) {
    this->name_ = name;
    this->build_adv_();
    // A restarted activity keeps its old payload: replace it.
    if (this->state_ == State::RUNNING || this->state_ == State::ADVERTISE)
      bdk_adv_delete();
  }
  this->adv_wanted_ = true;
  if (this->state_ == State::RUNNING && !this->connected_) {
    this->state_ = State::ADVERTISE;
    this->last_attempt_ms_ = 0;
  }
}

void BK72xxBLEServer::stop_advertising() {
  this->adv_wanted_ = false;
  if (!this->connected_)
    bdk_adv_stop();
}

BLECharacteristic *BK72xxBLEServer::get_characteristic(uint16_t uuid) {
  for (auto *chr : this->characteristics_) {
    if (chr->uuid_ == uuid)
      return chr;
  }
  return nullptr;
}

void BK72xxBLEServer::disconnect() {
  if (this->connected_)
    bdk_disconnect(this->conn_idx_);
}

void BK72xxBLEServer::dump_config() {
  ESP_LOGCONFIG(TAG,
                "BK72xx BLE server:\n"
                "  Name: %s\n"
                "  Service: 0x%04X",
                this->name_.c_str(), this->service_uuid_);
  for (auto *chr : this->characteristics_) {
    ESP_LOGCONFIG(TAG, "  Characteristic 0x%04X (att %u):%s%s%s", chr->uuid_, chr->value_idx_,
                  chr->read_ ? " read" : "", chr->can_write() ? " write" : "", chr->notify_ ? " notify" : "");
  }
}

}  // namespace esphome::bk72xx_ble_server

#endif  // USE_BK72XX_BLE_SERVER
