#include "b2500_base.h"
#include "b2500_v1.h"
#include "b2500_v2.h"
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
  TEMPLATABLE_VALUE(int, out)
  TEMPLATABLE_VALUE(bool, active)

 public:
  void play(Ts... x) override { this->parent_->set_out_active(this->out_.value(x...), this->active_.value(x...)); }
};

template<typename... Ts> class SetDischargeThresholdAction : public Action<Ts...>, public Parented<B2500ComponentV1> {
  TEMPLATABLE_VALUE(int, threshold)

 public:
  void play(Ts... x) override { this->parent_->set_discharge_threshold(this->threshold_.value(x...)); }
};

}  // namespace b2500
}  // namespace esphome
