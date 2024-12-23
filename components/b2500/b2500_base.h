#pragma once

#ifdef USE_ESP32

#include <esp_gattc_api.h>
#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/time/real_time_clock.h"

#include "b2500_state.h"

namespace esphome {
namespace b2500 {
namespace espbt = esphome::esp32_ble_tracker;

static const esp32_ble_tracker::ESPBTUUID B2500_SERVICE_UUID =
    esp32_ble_tracker::ESPBTUUID::from_raw("0000ff00-0000-1000-8000-00805f9b34fb");
static const esp32_ble_tracker::ESPBTUUID B2500_STATUS_UUID =
    esp32_ble_tracker::ESPBTUUID::from_raw("0000ff02-0000-1000-8000-00805f9b34fb");
static const esp32_ble_tracker::ESPBTUUID B2500_COMMAND_UUID =
    esp32_ble_tracker::ESPBTUUID::from_raw("0000ff01-0000-1000-8000-00805f9b34fb");

class B2500ComponentBase : public PollingComponent, public ble_client::BLEClientNode {
 public:
  B2500ComponentBase(int generation);

  B2500State *get_state() { return this->state_; }

  /** @return `true` if the `BLEClient::node_state` is `ClientState::ESTABLISHED`. */
  bool is_connected() { return this->node_state == esp32_ble_tracker::ClientState::ESTABLISHED; }

  void dump_config() override;
  void update() override;
  void setup() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  virtual std::vector<std::string> get_valid_charge_modes() { return {}; }
  virtual std::string get_charge_mode() = 0;

  void set_time(time::RealTimeClock *time) { this->time_ = time; }

  // Actions
  virtual bool set_charge_mode(const std::string &charge_mode) = 0;
  bool reboot();
  bool factory_reset();
  bool set_dod(float dod);
  bool set_wifi(const std::string &ssid, const std::string &password);
  bool set_mqtt(bool ssl, const std::string &host, uint16_t port, const std::string &username,
                const std::string &password);
  bool reset_mqtt();
  bool set_datetime(ESPTime datetime);

 protected:
  void send_command(std::vector<uint8_t> payload);

  void process_message_(uint8_t *data, uint16_t data_len);
  void read_sensors_(uint8_t *value, uint16_t value_len);
  void request_read_values_();

  uint16_t read_handle_;
  uint16_t desc_handle;
  uint16_t write_handle_;

  // Queue of all outstanding requests
  std::queue<std::vector<uint8_t>> requests_;

  B2500State *state_;
  int generation_;

  void poll_once();
  virtual void poll_runtime_info_();
  void enqueue_simple_command(B2500Command cmd);
  void enqueue_command(std::vector<uint8_t> data);

  virtual void interpret_message(B2500Message message);
  void interpret_runtime_info();
  void interpret_cell_info();

  bool process_requests_();

  time::RealTimeClock *time_;
};

}  // namespace b2500
}  // namespace esphome

#endif  // USE_ESP32
