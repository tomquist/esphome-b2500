#pragma once

#include <sstream>
#include <map>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/mqtt/mqtt_backend_esp32.h"
#include "esphome/components/mqtt/mqtt_client.h"

namespace esphome {
namespace hame_relay {

static const char *const TAG = "mqtt_forwarding";

class HameRelayComponent : public Component {
public:
    HameRelayComponent();
    ~HameRelayComponent() = default;

    // Delete copy constructor and assignment operator
    HameRelayComponent(const HameRelayComponent&) = delete;
    HameRelayComponent& operator=(const HameRelayComponent&) = delete;

    void add_device(const std::string &device_id, const std::string &mac);
    void remove_device_by_id(const std::string &device_id);
    void remove_device_by_mac(const std::string &mac);
    void set_broker_address(const std::string &address) { credentials_.address = address; }
    void set_broker_port(uint16_t port) { credentials_.port = port; }
    void set_keep_alive(uint16_t keep_alive_s);
    void set_ca_certificate(const char *cert) { mqtt_backend_.set_ca_certificate(cert); }
    void set_cl_certificate(const char *cert) { mqtt_backend_.set_cl_certificate(cert); }
    void set_cl_key(const char *key) { mqtt_backend_.set_cl_key(key); }

    float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
    bool is_connected();

    void enable();
    void disable();

    void setup() override;
    void loop() override;
    bool can_proceed() override;

protected:
    /// Reconnect to the MQTT broker if not already connected.
    void start_connect_();
    void start_dnslookup_();
    void check_dnslookup_();
    void check_connected_();
    
    bool subscribe_(const std::string &topic, uint8_t qos);
    void resubscribe_subscription_(mqtt::MQTTSubscription *sub);
    void resubscribe_subscriptions_();

    static void dns_found_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg);

    std::map<std::string, bool> subscribed_topics_;      // Internal MQTT subscriptions
    std::map<std::string, bool> external_subscriptions_; // External MQTT subscriptions
    
    std::vector<std::string> device_ids_;
    std::vector<std::string> device_macs_;
    std::map<std::string, std::string> id_to_mac_;    // Maps device IDs to MAC addresses
    std::map<std::string, std::string> mac_to_id_;    // Maps MAC addresses to device IDs
    bool dns_resolved_{false};
    bool dns_resolve_error_{false};
    network::IPAddress ip_;
    mqtt::MQTTCredentials credentials_;
    mqtt::MQTTClientState state_{mqtt::MQTT_CLIENT_DISABLED};

    mqtt::MQTTBackendESP32 mqtt_backend_;

    uint32_t connect_begin_{0};
    uint32_t last_connected_{0};
};

} // namespace hame_relay
} // namespace esphome
