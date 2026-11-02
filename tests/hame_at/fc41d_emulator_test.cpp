// Fc41dEmulator driven through a B2500-style session. The command lines are
// the format strings from the HMA-1 V255 / HMB-1 V144 MCU images with values
// filled in; the expected responses follow the FC41D AT manual and stay
// unverified until a UART capture from a real module confirms them.

#include "fc41d_emulator.h"
#include "test_util.h"

#include <functional>

using namespace esphome::hame_at;

struct FakeBackend : Fc41dBackend {
  WifiStatus wifi;
  std::vector<std::pair<uint16_t, std::vector<uint8_t>>> notifications;
  std::vector<std::pair<std::string, std::string>> published;
  std::vector<std::string> subscribed;
  std::vector<std::string> requests;
  std::vector<std::pair<std::string, size_t>> certs;
  std::function<void(HttpResult)> pending_http;
  std::string adv_name;
  uint16_t adv_service{0};
  std::vector<uint16_t> adv_chars;
  int restarts{0};
  std::string creds;
  MqttSession session;
  int sessions{0};
  int closed{0};

  WifiStatus wifi_status() override { return wifi; }
  void wifi_credentials(const std::string &ssid, const std::string &pw) override { creds = ssid + "/" + pw; }
  void ble_advertise(const std::string &name, uint16_t svc, const std::vector<uint16_t> &chars) override {
    adv_name = name;
    adv_service = svc;
    adv_chars = chars;
  }
  void ble_notify(uint16_t uuid, const std::vector<uint8_t> &data) override { notifications.push_back({uuid, data}); }
  void mqtt_connected(const MqttSession &s) override {
    session = s;
    sessions++;
  }
  void mqtt_closed() override { closed++; }
  void mqtt_subscribed(const std::string &topic) override { subscribed.push_back(topic); }
  void mqtt_publish(const std::string &topic, const std::string &payload) override {
    published.push_back({topic, payload});
  }
  void http_request(const std::string &method, const std::string &url, const std::string &,
                    const std::vector<uint8_t> &body, std::function<void(HttpResult)> done) override {
    requests.push_back(method + " " + url + (body.empty() ? "" : " " + std::string(body.begin(), body.end())));
    pending_http = done;
  }
  void ssl_cert(const std::string &kind, const std::vector<uint8_t> &data) override {
    certs.push_back({kind, data.size()});
  }
  void module_restart() override { restarts++; }
};

struct Harness {
  FakeBackend backend;
  Fc41dEmulator emu{&backend};
  std::string out;
  uint32_t now{1000};

  Harness() {
    emu.engine().set_output([this](const uint8_t *d, size_t n) { out.append(reinterpret_cast<const char *>(d), n); });
    emu.set_ble_mac("AA:BB:CC:DD:EE:FF");
    emu.set_module_version("FC41DAAR18A04M02_HM");
    backend.wifi.connected = true;
    backend.wifi.ssid = "home";
    backend.wifi.bssid = "11:22:33:44:55:66";
    backend.wifi.rssi = -61;
    backend.wifi.channel = 6;
    backend.wifi.ip = "192.168.1.50";
    backend.wifi.gateway = "192.168.1.1";
    backend.wifi.netmask = "255.255.255.0";
    backend.wifi.dns = "192.168.1.1";
  }
  void send(const std::string &s) { emu.feed(reinterpret_cast<const uint8_t *>(s.data()), s.size(), now); }
  std::string cmd(const std::string &line) {
    out.clear();
    send(line + "\r\n");
    std::string o = out;
    out.clear();
    return o;
  }
  void tick(uint32_t ms) {
    now += ms;
    emu.loop(now);
  }
};

static void test_system() {
  Harness h;
  EXPECT_STR(h.cmd("AT"), "\r\nOK\r\n");
  EXPECT_STR(h.cmd("AT+QVERSION"), "\r\n+QVERSION: FC41DAAR18A04M02_HM\r\n\r\nOK\r\n");
  // Module OTA is refused: the cloud must never replace ESPHome.
  EXPECT_STR(h.cmd("AT+QWLANOTA=http://x/fc41d.rbl"), "\r\nERROR\r\n");
  // AT+QRST: OK now, "ready" after the simulated reboot.
  EXPECT_STR(h.cmd("AT+QRST"), "\r\nOK\r\n");
  EXPECT_EQ(h.backend.restarts, 1);
  EXPECT(h.emu.rebooting());
  h.tick(100);
  EXPECT_STR(h.out, "");
  h.tick(300);
  EXPECT(!h.emu.rebooting());
  EXPECT(h.out.find("\r\nready\r\n") == 0);
}

static void test_wifi() {
  Harness h;
  // First loop reports the station that is already up.
  h.tick(10);
  EXPECT_STR(h.out, "\r\n+QSTASTAT:WLAN_CONNECTED\r\n\r\n+QSTASTAT:GOT_IP\r\n");
  h.out.clear();
  h.tick(10);
  EXPECT_STR(h.out, "");

  EXPECT_STR(h.cmd("AT+QSTAST"), "\r\n+QSTAST: STATION_UP\r\n\r\nOK\r\n");
  EXPECT_STR(h.cmd("AT+QGETWIFISTATE"),
             "\r\n+QGETWIFISTATE: ssid=home,bssid=11:22:33:44:55:66,rssi=-61,channel=6,cipher_type=4\r\n\r\nOK\r\n");
  EXPECT_STR(h.cmd("AT+QGETIP=station"),
             "\r\n+QGETIP: ip:192.168.1.50,gate:192.168.1.1,mask:255.255.255.0,dns:192.168.1.1\r\n\r\nOK\r\n");
  // Credentials from the app go to the backend; the reply is OK plus the
  // join URCs for the station ESPHome already holds.
  EXPECT_STR(h.cmd("AT+QSTAAPINFODEF=myssid,secret"),
             "\r\nOK\r\n\r\n+QSTASTAT:WLAN_CONNECTED\r\n\r\n+QSTASTAT:GOT_IP\r\n");
  EXPECT_STR(h.backend.creds, "myssid/secret");

  // Station lost: URC, and queries reflect it.
  h.out.clear();
  h.backend.wifi.connected = false;
  h.tick(10);
  EXPECT_STR(h.out, "\r\n+QSTASTAT:WLAN_DISCONNECTED\r\n");
  EXPECT_STR(h.cmd("AT+QSTAST"), "\r\n+QSTAST: STATION_DOWN\r\n\r\nOK\r\n");
  EXPECT_STR(h.cmd("AT+QGETWIFISTATE"), "\r\nERROR\r\n");
}

static void test_ble() {
  Harness h;
  EXPECT_STR(h.cmd("AT+QBLEINIT=2"), "\r\nOK\r\n");
  EXPECT_STR(h.cmd("AT+QBLEADDR?"), "\r\n+QBLEADDR:AA:BB:CC:DD:EE:FF\r\n\r\nOK\r\n");
  h.cmd("AT+QBLENAME=HMA-1_EEFF");
  h.cmd("AT+QBLEGATTSSRV=FF00");
  h.cmd("AT+QBLEGATTSCHAR=ff01");
  h.cmd("AT+QBLEGATTSCHAR=ff02");
  h.cmd("AT+QBLEGATTSCHAR=ff06");
  h.cmd("AT+QBLEADVPARAM=150,150");
  EXPECT_STR(h.cmd("AT+QBLEADVSTART"), "\r\nOK\r\n");
  EXPECT_STR(h.backend.adv_name, "HMA-1_EEFF");
  EXPECT_EQ(h.backend.adv_service, 0xFF00);
  EXPECT_EQ(h.backend.adv_chars.size(), 3u);
  EXPECT_EQ(h.backend.adv_chars[2], 0xFF06);
  EXPECT(h.emu.ble_advertising());
  EXPECT_STR(h.cmd("AT+QBLETRANMODE=ff02"), "\r\nOK\r\n");

  // Central connects; a write reaches the MCU as raw bytes.
  h.out.clear();
  h.emu.ble_connected(true);
  EXPECT_STR(h.out, "\r\n+QBLESTAT:CONNECTED\r\n");
  h.out.clear();
  h.emu.ble_write(0xFF01, {0x73, 0x06, 0x23, 0x03, 0x01, 0x54});
  EXPECT_STR(h.out, std::string("\x73\x06\x23\x03\x01\x54", 6));

  // Binary notification, hex encoded with a length.
  EXPECT_STR(h.cmd("AT+QBLEGATTSNTFY=ff02,6,730623030154"), "\r\nOK\r\n");
  EXPECT_EQ(h.backend.notifications.size(), 1u);
  EXPECT_EQ(h.backend.notifications[0].first, 0xFF02);
  EXPECT_EQ(h.backend.notifications[0].second.size(), 6u);
  EXPECT_EQ(h.backend.notifications[0].second[0], 0x73);
  // Cell info, sent as text without a length.
  EXPECT_STR(h.cmd("AT+QBLEGATTSNTFY=ff02,10_24_25_3162_3160_3161"), "\r\nOK\r\n");
  EXPECT_EQ(h.backend.notifications.size(), 2u);
  EXPECT_STR(std::string(h.backend.notifications[1].second.begin(), h.backend.notifications[1].second.end()),
             "10_24_25_3162_3160_3161");
  // Bad hex is rejected.
  EXPECT_STR(h.cmd("AT+QBLEGATTSNTFY=ff02,2,zz00"), "\r\nERROR\r\n");

  h.out.clear();
  h.emu.ble_connected(false);
  EXPECT_STR(h.out, "\r\n+QBLESTAT:DISCONNECTED\r\n");
}

static void test_mqtt() {
  Harness h;
  // Publishing before a session is open fails like a real module would.
  EXPECT_STR(h.cmd("AT+QMTPUB=0,0,0,0,\"t\",1,\"x\""), "\r\nOK\r\n\r\n+QMTPUB: 0,0,2\r\n");
  h.cmd("AT+QMTCFG=\"version\",0,4");
  h.cmd("AT+QMTCFG=\"keepalive\",0,60");
  h.cmd("AT+QMTCFG=\"ssl\",0,1,2");
  EXPECT_STR(h.cmd("AT+QMTOPEN=0,\"eu.hamedata.com\",8883"), "\r\nOK\r\n\r\n+QMTOPEN: 0,0\r\n");
  EXPECT_STR(h.cmd("AT+QMTCONN=0,\"mst_aabbccddeeff\",\"user\",\"pw,1\""), "\r\nOK\r\n\r\n+QMTCONN: 0,0,0\r\n");
  EXPECT(h.emu.mqtt_connected());
  // Everything a real connection needs reaches the backend.
  EXPECT_EQ(h.backend.sessions, 1);
  EXPECT_STR(h.backend.session.host, "eu.hamedata.com");
  EXPECT_EQ(h.backend.session.port, 8883);
  EXPECT_STR(h.backend.session.client_id, "mst_aabbccddeeff");
  EXPECT_STR(h.backend.session.username, "user");
  EXPECT_STR(h.backend.session.password, "pw,1");
  EXPECT(h.backend.session.ssl);
  EXPECT_EQ(h.backend.session.ssl_ctx, 2);
  EXPECT_EQ(h.backend.session.keepalive, 60);
  EXPECT_EQ(h.backend.session.version, 4);
  EXPECT_STR(h.cmd("AT+QMTSUB=0,1,\"hame_energy/HMA-1/App/aabbccddeeff/ctrl\",0"),
             "\r\nOK\r\n\r\n+QMTSUB: 0,1,0,0\r\n");
  EXPECT_EQ(h.backend.subscribed.size(), 1u);

  // The status payload is full of commas and '=' signs.
  std::string payload = "p1=1,p2=1,w1=24,w2=23,pe=45,vv=110,cs=0,cd=0";
  EXPECT_STR(h.cmd("AT+QMTPUB=0,0,0,0,\"hame_energy/HMA-1/device/aabbccddeeff/ctrl\"," +
                   std::to_string(payload.size()) + ",\"" + payload + "\""),
             "\r\nOK\r\n\r\n+QMTPUB: 0,0,0\r\n");
  EXPECT_EQ(h.backend.published.size(), 1u);
  EXPECT_STR(h.backend.published[0].first, "hame_energy/HMA-1/device/aabbccddeeff/ctrl");
  EXPECT_STR(h.backend.published[0].second, payload);

  // Inject a poll on the subscribed topic; other topics are not delivered.
  h.out.clear();
  h.emu.mqtt_receive("hame_energy/HMA-1/App/aabbccddeeff/ctrl", "cd=01");
  EXPECT_STR(h.out, "\r\n+QMTRECV: 0,0,\"hame_energy/HMA-1/App/aabbccddeeff/ctrl\",5,\"cd=01\"\r\n");
  h.out.clear();
  h.emu.mqtt_receive("other/topic", "cd=01");
  EXPECT_STR(h.out, "");

  // Losing Wi-Fi drops the session with a URC.
  h.tick(10);
  h.out.clear();
  h.backend.wifi.connected = false;
  h.tick(10);
  EXPECT(h.out.find("+QMTSTAT: 0,1") != std::string::npos);
  EXPECT(!h.emu.mqtt_connected());
  EXPECT_EQ(h.backend.closed, 1);
  EXPECT(h.emu.mqtt_topics().empty());
}

static void test_ssl_and_http() {
  Harness h;
  // Certificate upload: CONNECT, raw bytes (with CR/LF inside), OK.
  std::string pem = "-----BEGIN CERTIFICATE-----\r\nMIIB\r\n-----END CERTIFICATE-----\r\n";
  EXPECT_STR(h.cmd("AT+QSSLCERT=\"CA\",2," + std::to_string(pem.size())), "\r\nCONNECT\r\n");
  h.out.clear();
  h.send(pem);
  EXPECT_STR(h.out, "\r\nOK\r\n");
  EXPECT_EQ(h.backend.certs.size(), 1u);
  EXPECT_STR(h.backend.certs[0].first, "CA");
  EXPECT_EQ(h.backend.certs[0].second, pem.size());

  // GET: OK now, +QHTTPGET when the backend answers, body via QHTTPREAD.
  std::string url = "http://eu.hamedata.com/app/neng/getDateInfoeu.php?uid=aabbccddeeff&fcv=x&aid=HMA-1&sv=255&mv=1";
  EXPECT_STR(h.cmd("AT+QHTTPCFG=\"url\",\"" + url + "\""), "\r\nOK\r\n");
  EXPECT_STR(h.cmd("AT+QHTTPGET=60"), "\r\nOK\r\n");
  EXPECT_EQ(h.backend.requests.size(), 1u);
  EXPECT_STR(h.backend.requests[0], "GET " + url);
  // A second request while one is in flight is refused.
  EXPECT_STR(h.cmd("AT+QHTTPGET=60"), "\r\nERROR\r\n");
  h.out.clear();
  HttpResult r;
  r.status = 200;
  r.body = "_2026_11_01_21_30_00_04_0_0_0";
  h.backend.pending_http(r);
  EXPECT_STR(h.out, "\r\n+QHTTPGET: 0,200,29\r\n");
  EXPECT_STR(h.cmd("AT+QHTTPREAD=60"),
             "\r\nCONNECT\r\n_2026_11_01_21_30_00_04_0_0_0\r\nOK\r\n\r\n+QHTTPREAD: 0\r\n");

  // POST with a body.
  h.cmd("AT+QHTTPCFG=\"url\",\"http://eu.hamedata.com/app/Solar/puterrinfo.php\"");
  EXPECT_STR(h.cmd("AT+QHTTPPOST=7,120,120"), "\r\nCONNECT\r\n");
  h.out.clear();
  h.send("err=1,2");
  EXPECT_STR(h.out, "\r\nOK\r\n");
  EXPECT_STR(h.backend.requests[1], "POST http://eu.hamedata.com/app/Solar/puterrinfo.php err=1,2");
  h.out.clear();
  HttpResult fail;
  fail.error = 702;
  h.backend.pending_http(fail);
  EXPECT_STR(h.out, "\r\n+QHTTPPOST: 702\r\n");
}

int main() {
  test_system();
  test_wifi();
  test_ble();
  test_mqtt();
  test_ssl_and_http();
  return finish();
}
