#pragma once

// Minimal MQTT 3.1 / 3.1.1 client packet codec: what cloud forwarding needs to
// act like the Quectel module's own MQTT client (CONNECT with credentials,
// SUBSCRIBE, PUBLISH QoS 0/1, PUBACK, PINGREQ, DISCONNECT). Platform
// independent and host-tested.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace esphome::hame_at::mqtt {

enum PacketType : uint8_t {
  CONNECT = 1,
  CONNACK = 2,
  PUBLISH = 3,
  PUBACK = 4,
  SUBSCRIBE = 8,
  SUBACK = 9,
  PINGREQ = 12,
  PINGRESP = 13,
  DISCONNECT = 14,
};

/// `level` 4 = MQTT 3.1.1 ("MQTT"), 3 = MQTT 3.1 ("MQIsdp").
std::vector<uint8_t> encode_connect(const std::string &client_id, const std::string &username,
                                    const std::string &password, uint16_t keepalive_s, uint8_t level = 4,
                                    bool clean_session = true);
std::vector<uint8_t> encode_subscribe(uint16_t packet_id, const std::string &topic, uint8_t qos);
std::vector<uint8_t> encode_publish(const std::string &topic, const std::string &payload, uint8_t qos = 0,
                                    bool retain = false, uint16_t packet_id = 0);
std::vector<uint8_t> encode_puback(uint16_t packet_id);
std::vector<uint8_t> encode_pingreq();
std::vector<uint8_t> encode_disconnect();

struct Packet {
  uint8_t type{0};
  uint8_t flags{0};
  std::vector<uint8_t> body;

  // PUBLISH
  std::string topic;
  std::string payload;
  uint8_t qos{0};
  uint16_t packet_id{0};
  // CONNACK: return code; SUBACK: granted QoS (0x80 = failure)
  uint8_t code{0};
};

/// Incremental decoder for packets from the broker.
class Decoder {
 public:
  static constexpr size_t MAX_PACKET = 8 * 1024;

  /// Append received bytes.
  void feed(const uint8_t *data, size_t len);
  /// Next complete packet, if any. Returns false when more bytes are needed.
  bool next(Packet &out);
  /// True after a malformed or oversized packet; the stream must be dropped.
  bool error() const { return this->error_; }
  void reset() {
    this->buf_.clear();
    this->error_ = false;
  }

 protected:
  std::vector<uint8_t> buf_;
  bool error_{false};
};

}  // namespace esphome::hame_at::mqtt
