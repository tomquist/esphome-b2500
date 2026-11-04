#pragma once

// bk72xx_ble_server: a minimal BLE GATT server (peripheral) for the BK7231N,
// built on esphome's bk72xx_ble controller component.
//
// One primary service with 16-bit UUIDs, the shape the Hame/Marstek battery
// firmware creates through the Quectel FC41D AT firmware (FF00 with
// ff01/ff02/ff06). SDK events arrive on the BDK BLE task and are handed to the
// ESPHome main loop through a lock-free queue, so triggers and actions only
// ever run on the main task.

#include "esphome/core/defines.h"

#ifdef USE_BK72XX_BLE_SERVER

#include <string>
#include <vector>

#include "esphome/components/bk72xx_ble/bk72xx_ble.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/event_pool.h"
#include "esphome/core/helpers.h"
#include "esphome/core/lock_free_queue.h"

#include "bdk_gatts.h"

namespace esphome::bk72xx_ble_server {

class BK72xxBLEServer;

class BLECharacteristic {
 public:
  explicit BLECharacteristic(uint16_t uuid) : uuid_(uuid) {}
  void set_properties(bool read, bool write, bool write_no_response, bool notify) {
    this->read_ = read;
    this->write_ = write;
    this->write_no_response_ = write_no_response;
    this->notify_ = notify;
  }
  void set_parent(BK72xxBLEServer *parent) { this->parent_ = parent; }

  uint16_t get_uuid() const { return this->uuid_; }
  bool can_read() const { return this->read_; }
  bool can_write() const { return this->write_ || this->write_no_response_; }
  bool can_notify() const { return this->notify_; }

  /// Send a notification (no-op without a connected central). Also becomes
  /// the value returned to reads.
  bool notify(const std::vector<uint8_t> &value);
  /// Set the value returned to reads without notifying.
  void set_value(const std::vector<uint8_t> &value);
  bool notifications_enabled() const { return this->ccc_ != 0; }

  Trigger<std::vector<uint8_t>> *get_on_write_trigger() { return &this->on_write_; }
  /// C++ listeners for writes (main task), e.g. the hame_at bridge.
  void add_on_write_callback(std::function<void(const std::vector<uint8_t> &)> &&cb) {
    this->write_callbacks_.add(std::move(cb));
  }

 protected:
  friend class BK72xxBLEServer;

  uint16_t uuid_;
  bool read_{false};
  bool write_{false};
  bool write_no_response_{false};
  bool notify_{false};
  // Attribute indices within the service DB, set when the DB is built.
  uint16_t value_idx_{0};
  uint16_t ccc_idx_{0};  // 0 = no CCC descriptor
  volatile uint16_t ccc_{0};
  BK72xxBLEServer *parent_{nullptr};
  Trigger<std::vector<uint8_t>> on_write_;
  CallbackManager<void(const std::vector<uint8_t> &)> write_callbacks_;
};

/// One SDK event, copied out of the BLE task.
struct GattsEvent {
  BdkNotice notice;
  uint8_t conn_idx;
  uint16_t att_idx;
  uint16_t value16;
  uint16_t len;
  uint8_t data[247];
  void release() {}
};

static constexpr uint8_t GATTS_EVENT_QUEUE_SIZE = 16;
static constexpr uint16_t MAX_READ_VALUE = 128;  // BLE_CHAR_DATA_LEN in the BDK

class BK72xxBLEServer final : public Component {
 public:
  void set_ble(bk72xx_ble::BK72xxBLE *ble) { this->ble_ = ble; }
  void set_name(const std::string &name) { this->name_ = name; }
  void set_service_uuid(uint16_t uuid) { this->service_uuid_ = uuid; }
  void set_advertising_interval(uint16_t interval) { this->adv_interval_ = interval; }
  /// Advertise as soon as the service exists (default). When false, nothing is
  /// advertised until start_advertising() is called.
  void set_auto_advertise(bool auto_advertise) { this->adv_wanted_ = auto_advertise; }
  void add_characteristic(BLECharacteristic *chr) {
    chr->set_parent(this);
    this->characteristics_.push_back(chr);
  }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI - 1.0f; }

  bool is_connected() const { return this->connected_; }
  uint16_t get_mtu() const { return this->mtu_; }
  void disconnect();
  /// Advertise, optionally under a new name (applies to the next advertising start).
  void start_advertising(const std::string &name = "");
  void stop_advertising();
  BLECharacteristic *get_characteristic(uint16_t uuid);
  /// C++ listeners for connect (true) / disconnect (false), main task.
  void add_on_connection_callback(std::function<void(bool)> &&cb) { this->connection_callbacks_.add(std::move(cb)); }
  /// Called with the ATT MTU the central negotiated.
  void add_on_mtu_callback(std::function<void(uint16_t)> &&cb) { this->mtu_callbacks_.add(std::move(cb)); }

  Trigger<> *get_on_connect_trigger() { return &this->on_connect_; }
  Trigger<> *get_on_disconnect_trigger() { return &this->on_disconnect_; }

  // Internal.
  bool notify_(BLECharacteristic *chr, const std::vector<uint8_t> &value);
  void set_read_value_(BLECharacteristic *chr, const std::vector<uint8_t> &value);
  uint16_t on_notice_(BdkNotice notice, uint8_t conn_idx, uint16_t att_idx, const uint8_t *data, uint16_t len,
                      uint16_t value16, uint8_t *read_buf, uint16_t read_cap);

 protected:
  enum class State : uint8_t { WAIT_STACK, CREATE_DB, WAIT_DB, ADVERTISE, RUNNING, FAILED };

  void build_db_();
  void build_adv_();
  void handle_event_(const GattsEvent &ev);

  bk72xx_ble::BK72xxBLE *ble_{nullptr};
  std::string name_;
  uint16_t service_uuid_{0xFF00};
  uint16_t adv_interval_{160};  // 0.625 ms units -> 100 ms
  std::vector<BLECharacteristic *> characteristics_;
  std::vector<BdkAttr> db_;
  std::vector<uint8_t> adv_data_;
  std::vector<uint8_t> rsp_data_;

  // Read values, shared with the BLE task (guarded by read_lock_).
  Mutex read_lock_;
  struct ReadValue {
    uint16_t att_idx;
    uint16_t len;
    uint8_t data[MAX_READ_VALUE];
  };
  std::vector<ReadValue> read_values_;

  esphome::LockFreeQueue<GattsEvent, GATTS_EVENT_QUEUE_SIZE> queue_;
  esphome::EventPool<GattsEvent, GATTS_EVENT_QUEUE_SIZE - 1> pool_;

  Trigger<> on_connect_;
  Trigger<> on_disconnect_;
  CallbackManager<void(bool)> connection_callbacks_;
  CallbackManager<void(uint16_t)> mtu_callbacks_;

  uint32_t last_attempt_ms_{0};
  uint16_t mtu_{23};
  uint8_t prf_id_{0};
  uint8_t conn_idx_{0};
  bool connected_{false};
  bool adv_wanted_{true};
  State state_{State::WAIT_STACK};
};

template<typename... Ts> class NotifyAction : public Action<Ts...> {
 public:
  explicit NotifyAction(BLECharacteristic *chr) : chr_(chr) {}
  TEMPLATABLE_VALUE(std::vector<uint8_t>, data)

  void play(const Ts &...x) override { this->chr_->notify(this->data_.value(x...)); }

 protected:
  BLECharacteristic *chr_;
};

template<typename... Ts> class SetValueAction : public Action<Ts...> {
 public:
  explicit SetValueAction(BLECharacteristic *chr) : chr_(chr) {}
  TEMPLATABLE_VALUE(std::vector<uint8_t>, data)

  void play(const Ts &...x) override { this->chr_->set_value(this->data_.value(x...)); }

 protected:
  BLECharacteristic *chr_;
};

template<typename... Ts> class ConnectedCondition : public Condition<Ts...>, public Parented<BK72xxBLEServer> {
 public:
  bool check(const Ts &...x) override { return this->parent_->is_connected(); }
};

}  // namespace esphome::bk72xx_ble_server

#endif  // USE_BK72XX_BLE_SERVER
