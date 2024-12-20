#include "b2500_base.h"
#include "b2500_v1.h"
#include "b2500_v2.h"
#include "b2500_codec.h"
#include "b2500_state.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace b2500 {

template<typename... Ts> class SetWifiAction : public Action<Ts...>, public Parented<B2500ComponentBase> {
  TEMPLATABLE_VALUE(std::string, ssid);
  TEMPLATABLE_VALUE(std::string, password);

 public:
  void play(Ts... x) override { this->parent_->set_wifi(this->ssid_.value(x...), this->password_.value(x...)); }
};

template<typename... Ts> class SetMqttAction : public Action<Ts...>, public Parented<B2500ComponentBase> {
  TEMPLATABLE_VALUE(bool, ssl);
  TEMPLATABLE_VALUE(std::string, host);
  TEMPLATABLE_VALUE(uint16_t, port);
  TEMPLATABLE_VALUE(std::string, username);
  TEMPLATABLE_VALUE(std::string, password);

 public:
  void play(Ts... x) override {
    this->parent_->set_mqtt(this->ssl_.value(x...), this->host_.value(x...), this->port_.value(x...),
                            this->username_.value(x...), this->password_.value(x...));
  }
};

template<typename... Ts> class ResetMqttAction : public Action<Ts...>, public Parented<B2500ComponentBase> {
  public:
    void play(Ts... x) override { this->parent_->reset_mqtt(); }
};

template<typename... Ts> class SetDatetimeAction : public Action<Ts...>, public Parented<B2500ComponentBase> {
  TEMPLATABLE_VALUE(ESPTime, datetime)

 public:
  void play(Ts... x) override { this->parent_->set_datetime(this->datetime_.value(x...)); }
};

template<typename... Ts> class RebootAction : public Action<Ts...>, public Parented<B2500ComponentBase> {
 public:
  void play(Ts... x) override { this->parent_->reboot(); }
};

template<typename... Ts> class FactoryResetAction : public Action<Ts...>, public Parented<B2500ComponentBase> {
 public:
  void play(Ts... x) override { this->parent_->factory_reset(); }
};

template<typename... Ts> class SetDodAction : public Action<Ts...>, public Parented<B2500ComponentBase> {
  TEMPLATABLE_VALUE(int, dod)

 public:
  void play(Ts... x) override { this->parent_->set_dod(this->dod_.value(x...)); }
};

template<typename... Ts> class SetChargeModeAction : public Action<Ts...>, public Parented<B2500ComponentBase> {
  TEMPLATABLE_VALUE(std::string, charge_mode)

 public:
  void play(Ts... x) override { this->parent_->set_charge_mode(this->charge_mode_.value(x...)); }
};

template<typename... Ts> class SetOutActiveAction : public Action<Ts...>, public Parented<B2500ComponentV1> {
  TEMPLATABLE_VALUE(int, output)
  TEMPLATABLE_VALUE(bool, active)

 public:
  void play(Ts... x) override { this->parent_->set_out_active(this->output_.value(x...), this->active_.value(x...)); }
};

template<typename... Ts> class SetDischargeThresholdAction : public Action<Ts...>, public Parented<B2500ComponentV1> {
  TEMPLATABLE_VALUE(int, threshold)

 public:
  void play(Ts... x) override { this->parent_->set_discharge_threshold(this->threshold_.value(x...)); }
};

template<typename... Ts> class SetTimerAction : public Action<Ts...>, public Parented<B2500ComponentV2> {
  TEMPLATABLE_VALUE(int, timer)
  TEMPLATABLE_VALUE(optional<bool>, enabled)
  TEMPLATABLE_VALUE(optional<int>, output_power)
  TEMPLATABLE_VALUE(optional<int>, start_hour)
  TEMPLATABLE_VALUE(optional<int>, start_minute)
  TEMPLATABLE_VALUE(optional<int>, end_hour)
  TEMPLATABLE_VALUE(optional<int>, end_minute)

 public:
  void play(Ts... x) override {
    auto timer = this->parent_->get_state()->get_timer(this->timer_.value(x...));
    auto enabled = this->enabled_.value_or(x..., timer.enabled).value_or(timer.enabled);
    auto output_power = this->output_power_.value_or(x..., timer.output_power).value_or(timer.output_power);
    auto start_hour = this->start_hour_.value_or(x..., timer.start.hour).value_or(timer.start.hour);
    auto start_minute = this->start_minute_.value_or(x..., timer.start.minute).value_or(timer.start.minute);
    auto end_hour = this->end_hour_.value_or(x..., timer.end.hour).value_or(timer.end.hour);
    auto end_minute = this->end_minute_.value_or(x..., timer.end.minute).value_or(timer.end.minute);
    this->parent_->set_timer(this->timer_.value(x...), enabled, output_power, start_hour, start_minute, end_hour, end_minute);
  }
};

template<typename... Ts> class SetAdaptiveModeEnabledAction : public Action<Ts...>, public Parented<B2500ComponentV2> {
  TEMPLATABLE_VALUE(bool, enabled)

 public:
  void play(Ts... x) override { this->parent_->set_adaptive_mode_enabled(this->enabled_.value(x...)); }
};

class DeviceInfoTrigger : public Trigger<DeviceInfoPacket> {
 public:
  explicit DeviceInfoTrigger(B2500State *state) {
    state->add_on_message_callback([this, state](B2500Message message) { 
      if (message == B2500Message::B2500_MSG_DEVICE_INFO) {
        ESP_LOGD("b2500.automation", "Device info trigger");
        this->trigger(state->get_device_info());
      }
    });
  }
};

class RuntimeInfoTrigger : public Trigger<RuntimeInfoPacket> {
 public:
  explicit RuntimeInfoTrigger(B2500State *state) {
    state->add_on_message_callback([this, state](B2500Message message) { 
      if (message == B2500Message::B2500_MSG_RUNTIME_INFO) {
        ESP_LOGD("b2500.automation", "RuntimeInfoTrigger");
        this->trigger(state->get_runtime_info());
      }
    });
  }
};

class CellInfoTrigger : public Trigger<CellInfoPacket> {
 public:
  explicit CellInfoTrigger(B2500State *state) {
    state->add_on_message_callback([this, state](B2500Message message) { 
      if (message == B2500Message::B2500_MSG_CELL_INFO) {
        ESP_LOGD("b2500.automation", "CellInfoTrigger");
        this->trigger(state->get_cell_info());
      }
    });
  }
};

class WifiInfoTrigger : public Trigger<WifiInfoPacket> {
 public:
  explicit WifiInfoTrigger(B2500State *state) {
    state->add_on_message_callback([this, state](B2500Message message) { 
      if (message == B2500Message::B2500_MSG_WIFI_INFO) {
        ESP_LOGD("b2500.automation", "WifiInfoTrigger");
        this->trigger(state->get_wifi_info());
      }
    });
  }
};

class FC41DInfoTrigger : public Trigger<FC41DInfoPacket> {
 public:
  explicit FC41DInfoTrigger(B2500State *state) {
    state->add_on_message_callback([this, state](B2500Message message) { 
      if (message == B2500Message::B2500_MSG_FC41D_INFO) {
        ESP_LOGD("b2500.automation", "FC41DInfoTrigger");
        this->trigger(state->get_fc41d_info());
      }
    });
  }
};

class TimerInfoTrigger : public Trigger<TimerInfoPacket> {
 public:
  explicit TimerInfoTrigger(B2500State *state) {
    state->add_on_message_callback([this, state](B2500Message message) { 
      if (message == B2500Message::B2500_MSG_TIMER_INFO) {
        ESP_LOGD("b2500.automation", "TimerInfoTrigger");
        this->trigger(state->get_timer_info());
      }
    });
  }
};

}  // namespace b2500
}  // namespace esphome
