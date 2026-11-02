#include "mqtt_codec.h"

namespace esphome::hame_at::mqtt {

static void put_u16(std::vector<uint8_t> &v, uint16_t x) {
  v.push_back(x >> 8);
  v.push_back(x & 0xFF);
}

static void put_str(std::vector<uint8_t> &v, const std::string &s) {
  put_u16(v, static_cast<uint16_t>(s.size()));
  v.insert(v.end(), s.begin(), s.end());
}

static std::vector<uint8_t> frame(uint8_t first, const std::vector<uint8_t> &body) {
  std::vector<uint8_t> out;
  out.reserve(body.size() + 5);
  out.push_back(first);
  size_t len = body.size();
  do {
    uint8_t b = len % 128;
    len /= 128;
    if (len > 0)
      b |= 0x80;
    out.push_back(b);
  } while (len > 0);
  out.insert(out.end(), body.begin(), body.end());
  return out;
}

std::vector<uint8_t> encode_connect(const std::string &client_id, const std::string &username,
                                    const std::string &password, uint16_t keepalive_s, uint8_t level,
                                    bool clean_session) {
  std::vector<uint8_t> body;
  put_str(body, level == 3 ? "MQIsdp" : "MQTT");
  body.push_back(level == 3 ? 3 : 4);
  uint8_t flags = clean_session ? 0x02 : 0x00;
  if (!username.empty())
    flags |= 0x80;
  if (!username.empty() && !password.empty())
    flags |= 0x40;
  body.push_back(flags);
  put_u16(body, keepalive_s);
  put_str(body, client_id);
  if (flags & 0x80)
    put_str(body, username);
  if (flags & 0x40)
    put_str(body, password);
  return frame(CONNECT << 4, body);
}

std::vector<uint8_t> encode_subscribe(uint16_t packet_id, const std::string &topic, uint8_t qos) {
  std::vector<uint8_t> body;
  put_u16(body, packet_id);
  put_str(body, topic);
  body.push_back(qos & 0x03);
  return frame(SUBSCRIBE << 4 | 0x02, body);
}

std::vector<uint8_t> encode_publish(const std::string &topic, const std::string &payload, uint8_t qos, bool retain,
                                    uint16_t packet_id) {
  std::vector<uint8_t> body;
  put_str(body, topic);
  if (qos > 0)
    put_u16(body, packet_id);
  body.insert(body.end(), payload.begin(), payload.end());
  return frame(PUBLISH << 4 | (qos & 0x03) << 1 | (retain ? 1 : 0), body);
}

std::vector<uint8_t> encode_puback(uint16_t packet_id) {
  std::vector<uint8_t> body;
  put_u16(body, packet_id);
  return frame(PUBACK << 4, body);
}

std::vector<uint8_t> encode_pingreq() { return {PINGREQ << 4, 0}; }
std::vector<uint8_t> encode_disconnect() { return {DISCONNECT << 4, 0}; }

void Decoder::feed(const uint8_t *data, size_t len) {
  if (this->error_)
    return;
  this->buf_.insert(this->buf_.end(), data, data + len);
}

bool Decoder::next(Packet &out) {
  if (this->error_ || this->buf_.size() < 2)
    return false;
  // Remaining length: up to 4 bytes of 7-bit groups.
  size_t len = 0;
  size_t mult = 1;
  size_t pos = 1;
  for (;;) {
    if (pos >= this->buf_.size())
      return false;
    uint8_t b = this->buf_[pos++];
    len += (b & 0x7F) * mult;
    if ((b & 0x80) == 0)
      break;
    mult *= 128;
    if (pos > 4) {
      this->error_ = true;
      return false;
    }
  }
  if (len > MAX_PACKET) {
    this->error_ = true;
    return false;
  }
  if (this->buf_.size() < pos + len)
    return false;

  out = Packet{};
  out.type = this->buf_[0] >> 4;
  out.flags = this->buf_[0] & 0x0F;
  out.body.assign(this->buf_.begin() + pos, this->buf_.begin() + pos + len);
  this->buf_.erase(this->buf_.begin(), this->buf_.begin() + pos + len);

  const auto &b = out.body;
  switch (out.type) {
    case CONNACK:
      if (b.size() < 2) {
        this->error_ = true;
        return false;
      }
      out.code = b[1];
      break;
    case SUBACK:
      if (b.size() < 3) {
        this->error_ = true;
        return false;
      }
      out.packet_id = b[0] << 8 | b[1];
      out.code = b[2];
      break;
    case PUBACK:
      if (b.size() >= 2)
        out.packet_id = b[0] << 8 | b[1];
      break;
    case PUBLISH: {
      out.qos = (out.flags >> 1) & 0x03;
      if (b.size() < 2) {
        this->error_ = true;
        return false;
      }
      size_t tlen = b[0] << 8 | b[1];
      size_t p = 2 + tlen;
      if (p > b.size() || (out.qos > 0 && p + 2 > b.size())) {
        this->error_ = true;
        return false;
      }
      out.topic.assign(b.begin() + 2, b.begin() + p);
      if (out.qos > 0) {
        out.packet_id = b[p] << 8 | b[p + 1];
        p += 2;
      }
      out.payload.assign(b.begin() + p, b.end());
      break;
    }
    default:
      break;
  }
  return true;
}

}  // namespace esphome::hame_at::mqtt
