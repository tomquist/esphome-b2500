// MQTT codec and CloudLink against a fake transport / broker.

#include "cloud_link.h"
#include "mqtt_codec.h"
#include "test_util.h"

using namespace esphome::hame_at;
namespace mq = esphome::hame_at::mqtt;

static std::string hex(const std::vector<uint8_t> &v) {
  static const char *d = "0123456789abcdef";
  std::string s;
  for (uint8_t b : v) {
    s += d[b >> 4];
    s += d[b & 15];
  }
  return s;
}

static void test_codec() {
  // CONNECT, MQTT 3.1.1, clean session, no credentials, keepalive 60, id "a".
  EXPECT_STR(hex(mq::encode_connect("a", "", "", 60)), "100d00044d5154540402003c000161");
  // With user and password.
  auto c = mq::encode_connect("id", "u", "p", 30);
  EXPECT_EQ(c[9], 0xC2);  // flags: user, password, clean
  // MQTT 3.1 protocol name.
  auto c3 = mq::encode_connect("id", "", "", 30, 3);
  EXPECT_STR(std::string(c3.begin() + 4, c3.begin() + 10), "MQIsdp");
  EXPECT_EQ(c3[10], 3);
  EXPECT_STR(hex(mq::encode_subscribe(1, "t", 0)), "820600010001" "7400");
  EXPECT_STR(hex(mq::encode_publish("t", "hi", 0)), "30050001746869");
  EXPECT_STR(hex(mq::encode_pingreq()), "c000");

  // Remaining length > 127 uses two bytes.
  std::string big(200, 'x');
  auto p = mq::encode_publish("t", big, 0);
  EXPECT_EQ(p[1], (203 % 128) | 0x80);
  EXPECT_EQ(p[2], 1);

  // Decoder: split feeds, several packets, PUBLISH QoS 1.
  mq::Decoder d;
  std::vector<uint8_t> stream = {0x20, 0x02, 0x00, 0x00};               // CONNACK ok
  auto pub = mq::encode_publish("hame_energy/x/App/y/ctrl", "cd=1", 1, false, 7);
  stream.insert(stream.end(), pub.begin(), pub.end());
  stream.push_back(0xD0);
  stream.push_back(0x00);  // PINGRESP
  d.feed(stream.data(), 3);
  mq::Packet pk;
  EXPECT(!d.next(pk));
  d.feed(stream.data() + 3, stream.size() - 3);
  EXPECT(d.next(pk));
  EXPECT_EQ(pk.type, mq::CONNACK);
  EXPECT_EQ(pk.code, 0);
  EXPECT(d.next(pk));
  EXPECT_EQ(pk.type, mq::PUBLISH);
  EXPECT_STR(pk.topic, "hame_energy/x/App/y/ctrl");
  EXPECT_STR(pk.payload, "cd=1");
  EXPECT_EQ(pk.qos, 1);
  EXPECT_EQ(pk.packet_id, 7);
  EXPECT(d.next(pk));
  EXPECT_EQ(pk.type, mq::PINGRESP);
  EXPECT(!d.next(pk));
  EXPECT(!d.error());

  // Oversized packets are rejected.
  mq::Decoder bad;
  std::vector<uint8_t> huge = {0x30, 0xFF, 0xFF, 0x7F};
  bad.feed(huge.data(), huge.size());
  EXPECT(!bad.next(pk));
  EXPECT(bad.error());
}

// A broker on the other end of the fake transport.
struct FakeTransport : Transport {
  bool accept{true};
  bool is_connected{false};
  int connects{0};
  std::string host;
  uint16_t port{0};
  bool tls{false};
  std::string client_cert;
  std::vector<uint8_t> to_client;
  mq::Decoder from_client;
  std::vector<mq::Packet> received;

  bool connect(const std::string &h, uint16_t p, const TlsCredentials *t) override {
    connects++;
    host = h;
    port = p;
    tls = t != nullptr;
    client_cert = t ? t->client_cert : "";
    from_client.reset();
    is_connected = accept;
    return accept;
  }
  bool connected() override { return is_connected; }
  int read(uint8_t *buf, size_t len) override {
    if (!is_connected)
      return -1;
    size_t n = std::min(len, to_client.size());
    std::copy(to_client.begin(), to_client.begin() + n, buf);
    to_client.erase(to_client.begin(), to_client.begin() + n);
    return static_cast<int>(n);
  }
  bool write(const uint8_t *data, size_t len) override {
    if (!is_connected)
      return false;
    from_client.feed(data, len);
    mq::Packet p;
    while (from_client.next(p))
      received.push_back(p);
    return true;
  }
  void close() override { is_connected = false; }
  void push(const std::vector<uint8_t> &bytes) { to_client.insert(to_client.end(), bytes.begin(), bytes.end()); }
  int count(uint8_t type) const {
    int n = 0;
    for (auto &p : received)
      n += p.type == type;
    return n;
  }
};

static MqttSession tls_session() {
  MqttSession s;
  s.host = "broker.example";
  s.port = 8883;
  s.client_id = "mst_aabbccddeeff";
  s.ssl = true;
  s.keepalive = 60;
  return s;
}

static void test_link_flow() {
  FakeTransport t;
  CloudLink link(&t);
  std::vector<std::pair<std::string, std::string>> delivered;
  link.set_on_message([&](const std::string &tp, const std::string &pl) { delivered.push_back({tp, pl}); });
  uint32_t now = 1000;

  link.set_network(true, now);
  link.service(now);
  EXPECT(link.state() == CloudLink::State::NO_SESSION);

  // TLS session without certificates yet: keep waiting.
  link.set_session(tls_session(), now);
  link.subscribe("hame_energy/HMA-1/App/aabbccddeeff/ctrl");
  link.publish("hame_energy/HMA-1/device/aabbccddeeff/ctrl", "early");
  link.service(now);
  EXPECT_EQ(t.connects, 0);
  EXPECT(link.state() == CloudLink::State::NO_SESSION);

  // Uploaded one at a time, as the MCU does. A client certificate without
  // its key (upload in progress): still waiting.
  link.set_credential("CA", std::string("CA"), now);
  link.set_credential("User Cert", std::string("CERT"), now);
  link.service(now);
  EXPECT_EQ(t.connects, 0);

  link.set_credential("User Key", std::string("KEY"), now);
  link.publish("hame_energy/HMA-1/device/aabbccddeeff/ctrl", "queued");
  link.service(now);
  EXPECT_EQ(t.connects, 1);
  EXPECT_STR(t.host, "broker.example");
  EXPECT_EQ(t.port, 8883);
  EXPECT(t.tls);
  EXPECT_STR(t.client_cert, "CERT");
  EXPECT(link.state() == CloudLink::State::WAIT_CONNACK);
  EXPECT_EQ(t.count(mq::CONNECT), 1);

  // CONNACK: subscriptions go out, then the queued publishes.
  t.push({0x20, 0x02, 0x00, 0x00});
  link.service(now += 10);
  EXPECT(link.state() == CloudLink::State::CONNECTED);
  EXPECT_EQ(t.count(mq::SUBSCRIBE), 1);
  // Publishes made while waiting (for certificates, here) follow in order.
  EXPECT_EQ(t.count(mq::PUBLISH), 2);
  EXPECT_STR(t.received[t.received.size() - 2].payload, "early");
  EXPECT_STR(t.received.back().payload, "queued");

  // Live publish and a new subscription while connected.
  link.publish("hame_energy/HMA-1/device/aabbccddeeff/ctrl", "p1=1,p2=0");
  EXPECT_EQ(t.count(mq::PUBLISH), 3);
  EXPECT_EQ(t.received.back().flags & 0x01, 0);
  link.publish("hame_energy/HMN-1/device/x/ctrl", "a=1", true);
  EXPECT_EQ(t.received.back().flags & 0x01, 1);  // retain
  link.subscribe("other/topic");
  EXPECT_EQ(t.count(mq::SUBSCRIBE), 2);

  // Broker message, QoS 1: delivered and acknowledged.
  t.push(mq::encode_publish("hame_energy/HMA-1/App/aabbccddeeff/ctrl", "cd=1", 1, false, 42));
  link.service(now += 10);
  EXPECT_EQ(delivered.size(), 1u);
  EXPECT_STR(delivered[0].second, "cd=1");
  EXPECT_EQ(t.count(mq::PUBACK), 1);

  // Keepalive: ping after half the interval of silence.
  link.service(now += 30000);
  EXPECT_EQ(t.count(mq::PINGREQ), 1);
  t.push({0xD0, 0x00});
  link.service(now += 10);
  EXPECT(link.state() == CloudLink::State::CONNECTED);

  // Silence for 1.5 intervals: reconnect with backoff.
  link.service(now += 91000);
  EXPECT(link.state() == CloudLink::State::BACKOFF);
  link.service(now += 1000);
  EXPECT_EQ(t.connects, 1);  // still backing off
  link.service(now += CloudLink::BACKOFF_MIN_MS);
  EXPECT_EQ(t.connects, 2);
}

static void test_link_failures_and_switch() {
  FakeTransport t;
  CloudLink link(&t);
  uint32_t now = 1000;
  link.set_network(true, now);
  MqttSession plain;
  plain.host = "broker.local";
  plain.port = 1883;
  plain.client_id = "hm_x";
  plain.keepalive = 60;
  link.set_session(plain, now);  // no TLS: no certificates needed

  // Refused connections back off exponentially.
  t.accept = false;
  link.service(now);
  EXPECT_EQ(t.connects, 1);
  EXPECT(!t.tls);
  link.service(now += CloudLink::BACKOFF_MIN_MS);
  EXPECT_EQ(t.connects, 2);
  link.service(now += CloudLink::BACKOFF_MIN_MS);
  EXPECT_EQ(t.connects, 2);  // now waiting 10 s
  link.service(now += CloudLink::BACKOFF_MIN_MS);
  EXPECT_EQ(t.connects, 3);

  // A broker that refuses CONNECT.
  t.accept = true;
  link.service(now += 20000);
  EXPECT_EQ(t.connects, 4);
  t.push({0x20, 0x02, 0x00, 0x05});
  link.service(now += 10);
  EXPECT(link.state() == CloudLink::State::BACKOFF);

  // No CONNACK at all.
  link.service(now += 40000);
  EXPECT(link.state() == CloudLink::State::WAIT_CONNACK);
  link.service(now += CloudLink::CONNACK_TIMEOUT_MS + 1);
  EXPECT(link.state() == CloudLink::State::BACKOFF);

  // Switch off: disconnect, nothing goes out; switch on: reconnect at once.
  link.service(now += 80000);
  t.push({0x20, 0x02, 0x00, 0x00});
  link.service(now += 10);
  EXPECT(link.state() == CloudLink::State::CONNECTED);
  link.set_enabled(false, now);
  EXPECT(link.state() == CloudLink::State::DISABLED);
  EXPECT(!t.is_connected);
  EXPECT_EQ(t.count(mq::DISCONNECT), 1);
  int pubs = t.count(mq::PUBLISH);
  link.publish("t", "x");
  link.service(now += 100000);
  EXPECT_EQ(t.count(mq::PUBLISH), pubs);
  int before = t.connects;
  link.set_enabled(true, now);
  link.service(now);
  EXPECT_EQ(t.connects, before + 1);

  // Wi-Fi down: drop the link, don't retry until it is back.
  t.push({0x20, 0x02, 0x00, 0x00});
  link.service(now += 10);
  link.set_network(false, now);
  EXPECT(link.state() == CloudLink::State::BACKOFF);
  before = t.connects;
  link.service(now += 600000);
  EXPECT_EQ(t.connects, before);
  link.set_network(true, now);
  link.service(now);
  EXPECT_EQ(t.connects, before + 1);

  // The MCU closes its session: ours goes too.
  link.clear_session();
  EXPECT(link.state() == CloudLink::State::NO_SESSION);
  EXPECT(!t.is_connected);
}

int main() {
  test_codec();
  test_link_flow();
  test_link_failures_and_switch();
  return finish();
}
