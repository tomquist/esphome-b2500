#include "hame_relay.h"
#include <set>
#include "esphome/components/network/util.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/mqtt/mqtt_client.h"
#include "lwip/dns.h"
#include "lwip/err.h"

namespace esphome {
namespace hame_relay {

HameRelayComponent::HameRelayComponent() {
    credentials_.client_id = App.get_name() + "-" + get_mac_address();
}

void HameRelayComponent::add_device(const std::string &device_id, const std::string &mac) {
    device_ids_.push_back(device_id);
    device_macs_.push_back(mac);
    id_to_mac_[device_id] = mac;
    mac_to_id_[mac] = device_id;
    this->resubscribe_subscriptions_();
}

void HameRelayComponent::set_keep_alive(uint16_t keep_alive_s) { this->mqtt_backend_.set_keep_alive(keep_alive_s); }

void HameRelayComponent::remove_device_by_id(const std::string &device_id) {
    auto it = std::find(device_ids_.begin(), device_ids_.end(), device_id);
    if (it != device_ids_.end()) {
        std::string mac = id_to_mac_[device_id];
        
        // Remove from data structures
        device_ids_.erase(it);
        device_macs_.erase(device_macs_.begin() + std::distance(device_ids_.begin(), it));
        id_to_mac_.erase(device_id);
        mac_to_id_.erase(mac);
        
        // Let resubscribe_subscriptions_ handle the MQTT unsubscriptions
        this->resubscribe_subscriptions_();
    }
}

void HameRelayComponent::remove_device_by_mac(const std::string &mac) {
    auto it = std::find(device_macs_.begin(), device_macs_.end(), mac);
    if (it != device_macs_.end()) {
        std::string device_id = mac_to_id_[mac];
        
        // Remove from data structures
        device_macs_.erase(it);
        device_ids_.erase(device_ids_.begin() + std::distance(device_macs_.begin(), it));
        mac_to_id_.erase(mac);
        id_to_mac_.erase(device_id);
        
        // Let resubscribe_subscriptions_ handle the MQTT unsubscriptions
        this->resubscribe_subscriptions_();
    }
}

void HameRelayComponent::setup() {
    // Clear any existing subscriptions on setup
    subscribed_topics_.clear();
    external_subscriptions_.clear();
    
    mqtt_backend_.set_on_message([this](const char *topic, const char *payload, size_t len, size_t index, size_t total) {
        if (index == 0) {
            std::string full_payload(payload, len);
            std::string topic_str(topic);
            ESP_LOGD(TAG, "Received message on internal MQTT: %s: %s", topic, full_payload.c_str());

            // Find device ID in topic and replace with MAC
            for (const auto &device_id : device_ids_) {
                if (topic_str.find("/App/" + device_id) != std::string::npos) {
                    // Replace device ID with MAC in topic
                    std::string mac = id_to_mac_[device_id];
                    std::string new_topic = topic_str;
                    size_t pos = new_topic.find(device_id);
                    new_topic.replace(pos, device_id.length(), mac);

                    if (mqtt::global_mqtt_client->publish(new_topic, full_payload)) {
                        ESP_LOGI(TAG, "Forwarded message to external MQTT on topic: %s", new_topic.c_str());
                    } else {
                        ESP_LOGW(TAG, "Failed to forward message to external MQTT on topic: %s", new_topic.c_str());
                    }
                    break;
                }
            }
        }
    });

    mqtt_backend_.set_on_disconnect([this](mqtt::MQTTClientDisconnectReason reason) {
        if (state_ != mqtt::MQTT_CLIENT_DISABLED) {
            state_ = mqtt::MQTT_CLIENT_DISCONNECTED;
            ESP_LOGW(TAG, "MQTT client disconnected with reason: %d", static_cast<int>(reason));
        }
    });

    state_ = mqtt::MQTT_CLIENT_DISCONNECTED;
    ESP_LOGI(TAG, "MQTT Client setup complete");
}

bool HameRelayComponent::can_proceed() {
  return network::is_disabled() || this->state_ == mqtt::MQTT_CLIENT_DISABLED || this->is_connected();
}

void HameRelayComponent::start_dnslookup_() {
    status_set_warning();
    dns_resolve_error_ = false;
    dns_resolved_ = false;

    ip_addr_t addr;
#if USE_NETWORK_IPV6
    err_t err = dns_gethostbyname_addrtype(credentials_.address.c_str(), &addr,
                                          dns_found_callback, this, LWIP_DNS_ADDRTYPE_IPV6_IPV4);
#else
    err_t err = dns_gethostbyname_addrtype(credentials_.address.c_str(), &addr,
                                          dns_found_callback, this, LWIP_DNS_ADDRTYPE_IPV4);
#endif

    switch (err) {
        case ERR_OK:
            dns_resolved_ = true;
            ip_ = network::IPAddress(&addr);
            start_connect_();
            return;
        case ERR_INPROGRESS:
            ESP_LOGD(TAG, "Resolving MQTT broker IP address...");
            break;
        default:
            ESP_LOGW(TAG, "Error resolving MQTT broker IP address: %d", err);
            break;
    }

    state_ = mqtt::MQTT_CLIENT_RESOLVING_ADDRESS;
    connect_begin_ = millis();
}

void HameRelayComponent::check_dnslookup_() {
    if (!dns_resolved_ && millis() - connect_begin_ > 20000) {
        dns_resolve_error_ = true;
    }

    if (dns_resolve_error_) {
        ESP_LOGW(TAG, "Couldn't resolve IP address for '%s'!", credentials_.address.c_str());
        state_ = mqtt::MQTT_CLIENT_DISCONNECTED;
        return;
    }

    if (!dns_resolved_) {
        return;
    }

    ESP_LOGD(TAG, "Resolved broker IP address to %s", ip_.str().c_str());
    start_connect_();
}

void HameRelayComponent::dns_found_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    auto *instance = static_cast<HameRelayComponent *>(callback_arg);
    if (ipaddr == nullptr) {
        instance->dns_resolve_error_ = true;
    } else {
        instance->ip_ = network::IPAddress(ipaddr);
        instance->dns_resolved_ = true;
    }
}

void HameRelayComponent::start_connect_() {
    if (!network::is_connected()) {
        return;
    }

    ESP_LOGI(TAG, "Connecting to MQTT...");
    mqtt_backend_.disconnect();

    mqtt_backend_.set_client_id(credentials_.client_id.c_str());
    mqtt_backend_.set_clean_session(credentials_.clean_session);

    const char *username = credentials_.username.empty() ? nullptr : credentials_.username.c_str();
    const char *password = credentials_.password.empty() ? nullptr : credentials_.password.c_str();
    mqtt_backend_.set_credentials(username, password);

    mqtt_backend_.set_server(credentials_.address.c_str(), credentials_.port);
    mqtt_backend_.connect();
    
    state_ = mqtt::MQTT_CLIENT_CONNECTING;
    connect_begin_ = millis();
}

void HameRelayComponent::check_connected_() {
    if (!mqtt_backend_.connected()) {
        if (millis() - connect_begin_ > 60000) {
            state_ = mqtt::MQTT_CLIENT_DISCONNECTED;
            start_dnslookup_();
        }
        return;
    }

    if (state_ != mqtt::MQTT_CLIENT_CONNECTED) {
        state_ = mqtt::MQTT_CLIENT_CONNECTED;
        status_clear_warning();
        ESP_LOGI(TAG, "MQTT Connected!");
        
        // MQTT Client needs some time to be fully set up
        delay(100);  // NOLINT

        // Clear internal subscriptions
        subscribed_topics_.clear();
        
        // Resubscribe to internal topics
        resubscribe_subscriptions_();
    }
}

bool HameRelayComponent::is_connected() {
  return this->state_ == mqtt::MQTT_CLIENT_CONNECTED && this->mqtt_backend_.connected();
}

void HameRelayComponent::loop() {
    mqtt_backend_.loop();

    const uint32_t now = millis();

    switch (state_) {
        case mqtt::MQTT_CLIENT_DISABLED:
            return;
        case mqtt::MQTT_CLIENT_DISCONNECTED:
            if (now - connect_begin_ > 5000) {
                this->start_dnslookup_();
            }
            break;
        case mqtt::MQTT_CLIENT_RESOLVING_ADDRESS:
            this->check_dnslookup_();
            break;
        case mqtt::MQTT_CLIENT_CONNECTING:
            this->check_connected_();
            break;
        case mqtt::MQTT_CLIENT_CONNECTED:
            if (!this->mqtt_backend_.connected()) {
                state_ = mqtt::MQTT_CLIENT_DISCONNECTED;
                ESP_LOGW(TAG, "Lost MQTT Client connection!");
                // Clear subscription status on disconnect
                for (auto &topic : subscribed_topics_) {
                    topic.second = false;
                }
                start_dnslookup_();
            } else {
                this->last_connected_ = now;
                this->resubscribe_subscriptions_();
            }
            break;
    }

    if (millis() - last_connected_ > 60000) {
        ESP_LOGE(TAG, "Can't connect to MQTT... Restarting...");
        App.reboot();
    }
}

void HameRelayComponent::resubscribe_subscriptions_() {
    if (state_ != mqtt::MQTT_CLIENT_CONNECTED) {
        return;
    }

    // Create sets of current expected topics for both internal and external MQTT
    std::set<std::string> desired_internal_topics;
    std::set<std::string> desired_external_topics;
    
    for (const auto &device_id : device_ids_) {
        desired_internal_topics.insert("hame_energy/+/App/" + device_id + "/ctrl");
    }
    
    for (const auto &mac : device_macs_) {
        desired_external_topics.insert("hame_energy/+/device/" + mac + "/ctrl");
    }

    // Handle internal MQTT topics
    // Unsubscribe from old topics
    for (auto it = subscribed_topics_.begin(); it != subscribed_topics_.end();) {
        if (desired_internal_topics.find(it->first) == desired_internal_topics.end()) {
            mqtt_backend_.unsubscribe(it->first.c_str());
            ESP_LOGI(TAG, "Unsubscribed from internal topic: %s", it->first.c_str());
            it = subscribed_topics_.erase(it);
        } else {
            ++it;
        }
    }

    // Subscribe to new internal topics
    for (const auto &topic : desired_internal_topics) {
        if (subscribed_topics_.find(topic) == subscribed_topics_.end()) {
            if (mqtt_backend_.subscribe(topic.c_str(), 0)) {
                ESP_LOGI(TAG, "Subscribed to internal topic: %s", topic.c_str());
                subscribed_topics_[topic] = true;
            } else {
                ESP_LOGE(TAG, "Failed to subscribe to internal topic: %s", topic.c_str());
                status_set_warning();
            }
        }
    }

    // Handle external MQTT topics if global client exists
    if (mqtt::global_mqtt_client != nullptr) {
        // Unsubscribe from removed topics
        for (auto it = external_subscriptions_.begin(); it != external_subscriptions_.end();) {
            if (desired_external_topics.find(it->first) == desired_external_topics.end()) {
                mqtt::global_mqtt_client->unsubscribe(it->first);
                ESP_LOGI(TAG, "Unsubscribed from external topic: %s", it->first.c_str());
                it = external_subscriptions_.erase(it);
            } else {
                ++it;
            }
        }

        // Subscribe to new external topics
        for (const auto &topic : desired_external_topics) {
            if (external_subscriptions_.find(topic) == external_subscriptions_.end()) {
                mqtt::global_mqtt_client->subscribe(topic, [this](const std::string &topic, const std::string &payload) {
                    ESP_LOGD(TAG, "Global MQTT received message on %s: %s", topic.c_str(), payload.c_str());
                    
                    // Find MAC in topic and replace with device ID
                    for (const auto &mac : device_macs_) {
                        if (topic.find("/device/" + mac) != std::string::npos) {
                            std::string device_id = mac_to_id_[mac];
                            std::string new_topic = topic;
                            size_t pos = new_topic.find(mac);
                            new_topic.replace(pos, mac.length(), device_id);

                            if (mqtt_backend_.publish(new_topic.c_str(), payload.c_str(), 
                                                   payload.length(), 0, false)) {
                                ESP_LOGI(TAG, "Forwarded message to internal MQTT on topic: %s", 
                                       new_topic.c_str());
                            } else {
                                ESP_LOGW(TAG, "Failed to forward message to internal MQTT on topic: %s", 
                                       new_topic.c_str());
                            }
                            break;
                        }
                    }
                });
                external_subscriptions_[topic] = true;
                ESP_LOGI(TAG, "Subscribed to external topic: %s", topic.c_str());
            }
        }
    }
}

void HameRelayComponent::enable() {
  if (this->state_ != mqtt::MQTT_CLIENT_DISABLED)
    return;
  ESP_LOGD(TAG, "Enabling MQTT...");
  this->state_ = mqtt::MQTT_CLIENT_DISCONNECTED;
  this->last_connected_ = millis();
  this->start_dnslookup_();
}

void HameRelayComponent::disable() {
  if (this->state_ == mqtt::MQTT_CLIENT_DISABLED)
    return;
  ESP_LOGD(TAG, "Disabling MQTT...");
  this->state_ = mqtt::MQTT_CLIENT_DISABLED;
  this->on_shutdown();
}

} // namespace hame_relay
} // namespace esphome