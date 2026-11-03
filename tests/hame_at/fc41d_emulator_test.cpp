// Fc41dEmulator driven through a B2500-style session. Command lines are the
// format strings from the HMA-1 V255 / HMB-1 V144 / HMJ-2 V118 MCU images with
// values filled in. Formats and timing follow the hm2500 firmware emulator
// (branch claude/clever-fermi-m8nfb5), which runs those images against a
// module model; nothing here is confirmed by a capture of a stock module yet.

#include "fc41d_emulator.h"
#include "test_util.h"

#include <functional>
#include <map>

using namespace esphome::hame_at;

struct FakeBackend : Fc41dBackend {
  WifiStatus wifi;
  std::vector<std::pair<uint16_t, std::vector<uint8_t>>> notifications;
  std::vector<std::pair<std::string, std::string>> published;
  std::vector<bool> retained;
  bool broker_echoes{false};
  std::vector<std::pair<std::string, std::string>> last_headers;
  std::vector<std::string> subscribed;
  std::vector<std::string> requests;
  std::vector<std::pair<std::string, size_t>> certs;
  std::function<void(HttpResult)> pending_http;
  bool answer_http_now{false};
  HttpResult http_answer;
  std::string adv_name;
  uint16_t adv_service{0};
  std::vector<uint16_t> adv_chars;
  int restarts{0};
  std::string creds;
  MqttSession session;
  int sessions{0};
  int closed{0};
  std::vector<SocketSpec> opened;
  std::vector<std::string> sent;
  std::vector<int> socket_closes;

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
  void mqtt_publish(const std::string &topic, const std::string &payload, bool retain) override {
    published.push_back({topic, payload});
    retained.push_back(retain);
  }
  bool mqtt_broker_echoes() override { return broker_echoes; }
  void http_request(const HttpRequest &req, std::function<void(HttpResult)> done) override {
    last_headers = req.headers;
    requests.push_back(req.method + " " + req.url +
                       (req.body.empty() ? "" : " " + std::string(req.body.begin(), req.body.end())));
    if (answer_http_now)
      done(http_answer);
    else
      pending_http = done;
  }
  void socket_open(const SocketSpec &spec, std::function<void(int)> done) override {
    opened.push_back(spec);
    done(0);
  }
  void socket_send(int id, const std::string &data, const std::string &ip, int port) override {
    sent.push_back(std::to_string(id) + ":" + data + "@" + ip + ":" + std::to_string(port));
  }
  void socket_close(int id) override { socket_closes.push_back(id); }
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

  explicit Harness(bool boot = true) {
    emu.engine().set_output([this](const uint8_t *d, size_t n) { out.append(reinterpret_cast<const char *>(d), n); });
    emu.set_ble_mac("AA:BB:CC:DD:EE:FF");
    backend.wifi.connected = true;
    backend.wifi.ssid = "home";
    backend.wifi.bssid = "11:22:33:44:55:66";
    backend.wifi.rssi = -61;
    backend.wifi.channel = 6;
    backend.wifi.ip = "192.168.1.50";
    backend.wifi.gateway = "192.168.1.1";
    backend.wifi.netmask = "255.255.255.0";
    backend.wifi.dns = "192.168.1.1";
    backend.wifi.mac = "c8:47:8c:00:00:01";
    if (boot) {
      emu.power_on(now);
      tick(Fc41dEmulator::BOOT_MS);
      tick(Fc41dEmulator::WIFI_IDLE_MS);
      tick(Fc41dEmulator::DHCP_MS);
      out.clear();
    }
  }
  void send(const std::string &s) { emu.feed(reinterpret_cast<const uint8_t *>(s.data()), s.size(), now); }
  // Send a command, then let 100 ms pass so delayed URCs arrive.
  std::string cmd(const std::string &line, uint32_t wait = 100) {
    out.clear();
    send(line + "\r\n");
    tick(wait);
    std::string o = out;
    out.clear();
    return o;
  }
  void tick(uint32_t ms) {
    // advance in 10 ms steps, like a main loop
    for (uint32_t t = 0; t < ms; t += 10) {
      now += 10;
      emu.loop(now);
    }
  }
};

static void test_boot_and_wifi() {
  Harness h(false);
  h.emu.power_on(h.now);
  h.tick(200);
  EXPECT_STR(h.out, "");
  h.tick(200);
  EXPECT_STR(h.out, "\r\nready\r\n");
  h.out.clear();
  // The MCU keeps talking: no Wi-Fi URC while it is busy.
  for (int i = 0; i < 5; i++) {
    h.cmd("AT", 1000);
  }
  EXPECT(h.out.find("QSTASTAT") == std::string::npos);
  EXPECT(!h.emu.wifi_announced());
  // Quiet for 2.5 s: WLAN_CONNECTED, then GOT_IP 300 ms later.
  h.out.clear();
  uint32_t quiet_since = h.now - 1000;  // the last AT was 1000 ms ago
  while (h.out.empty() && h.now - quiet_since < 5000)
    h.tick(10);
  EXPECT(h.now - quiet_since >= Fc41dEmulator::WIFI_IDLE_MS);
  EXPECT_STR(h.out, "\r\n+QSTASTAT:WLAN_CONNECTED\r\n");
  h.out.clear();
  h.tick(290);
  EXPECT_STR(h.out, "");
  h.tick(20);
  EXPECT_STR(h.out, "\r\n+QSTASTAT:GOT_IP\r\n");
  EXPECT(h.emu.wifi_announced());

  EXPECT_STR(h.cmd("AT+QSTAST"), "\r\n+QSTAST:STATION_UP\r\n\r\nOK\r\n");
  EXPECT_STR(h.cmd("AT+QGETWIFISTATE"),
             "\r\n+QGETWIFISTATE: ssid=home,bssid=11:22:33:44:55:66,rssi=-61,channel=6\r\n\r\nOK\r\n");
  EXPECT_STR(h.cmd("AT+QGETIP=station"),
             "\r\n+QGETIP:ip:192.168.1.50,gate:192.168.1.1,mask:255.255.255.0,dns:192.168.1.1\r\n\r\nOK\r\n");
  EXPECT_STR(h.cmd("AT+QWLMAC"), "\r\n+QWLMAC: c8:47:8c:00:00:01\r\n\r\nOK\r\n");

  // Station lost and back.
  h.out.clear();
  h.backend.wifi.connected = false;
  h.tick(10);
  EXPECT_STR(h.out, "\r\n+QSTASTAT:WLAN_DISCONNECTED\r\n");
  EXPECT_STR(h.cmd("AT+QSTAST"), "\r\n+QSTAST:STATION_DOWN\r\n\r\nOK\r\n");
  EXPECT_STR(h.cmd("AT+QGETWIFISTATE"), "\r\n+QGETWIFISTATE: 0\r\n\r\nOK\r\n");
  EXPECT_STR(h.cmd("AT+QGETIP=station"), "\r\nERROR\r\n");
  h.backend.wifi.connected = true;
  h.out.clear();
  h.tick(3000);
  EXPECT(h.out.find("+QSTASTAT:WLAN_CONNECTED") != std::string::npos);
  EXPECT(h.out.find("+QSTASTAT:GOT_IP") != std::string::npos);
}

static void test_wifi_asked_and_reannounce() {
  // Asking for the station state announces it without waiting for quiet.
  Harness h(false);
  h.emu.power_on(h.now);
  h.tick(300);
  h.out.clear();
  EXPECT_STR(h.cmd("AT+QSTAST", 0), "\r\n+QSTAST:STATION_DOWN\r\n\r\nOK\r\n");
  h.tick(10);
  EXPECT(h.out.find("+QSTASTAT:WLAN_CONNECTED") != std::string::npos);
  h.tick(300);
  EXPECT(h.emu.wifi_announced());

  // The MCU never uses the network: announced again after 8 s, at most 3 times.
  h.out.clear();
  h.tick(Fc41dEmulator::REANNOUNCE_MS + 400);
  EXPECT(h.out.find("+QSTASTAT:WLAN_CONNECTED") != std::string::npos);
  int count = 1;
  for (int i = 0; i < 6; i++) {
    h.out.clear();
    h.tick(Fc41dEmulator::REANNOUNCE_MS + 400);
    count += h.out.find("+QSTASTAT:WLAN_CONNECTED") != std::string::npos;
  }
  EXPECT_EQ(count, 3);

  // A firmware that uses the network is left alone.
  Harness g;
  g.cmd("AT+QMTOPEN=0,\"eu.hamedata.com\",8883");
  g.out.clear();
  g.tick(Fc41dEmulator::REANNOUNCE_MS * 2);
  EXPECT(g.out.find("QSTASTAT") == std::string::npos);

  // Credentials from the app: OK, then a "rejoin" of ESPHome's station.
  EXPECT_STR(g.cmd("AT+QSTAAPINFODEF=myssid,secret"), "\r\nOK\r\n");
  EXPECT_STR(g.backend.creds, "myssid/secret");
  g.tick(Fc41dEmulator::WIFI_JOIN_MS + Fc41dEmulator::DHCP_MS);
  EXPECT(g.out.find("+QSTASTAT:WLAN_CONNECTED") != std::string::npos);
  EXPECT(g.out.find("+QSTASTAT:GOT_IP") != std::string::npos);
}

static void test_system() {
  Harness h;
  EXPECT_STR(h.cmd("AT"), "\r\nOK\r\n");
  // The MCU reports the part after "_" to the cloud (fcv=202409090159).
  EXPECT_STR(h.cmd("AT+QVERSION"), "\r\n+QVERSION: FC41DAAR18A04M02_HM.bin_202409090159\r\n\r\nOK\r\n");
  EXPECT_STR(h.cmd("AT+QWLANOTA=http://x/fc41d.rbl"), "\r\nERROR\r\n");
  // AT+QRST: OK, pending output dropped, "ready" ~350 ms later, Wi-Fi again.
  h.send("AT+QMTOPEN=0,\"eu.hamedata.com\",8883\r\nAT+QRST\r\n");
  EXPECT_EQ(h.backend.restarts, 1);
  h.out.clear();
  h.tick(200);
  EXPECT(h.out.find("+QMTOPEN") == std::string::npos);
  EXPECT(h.emu.booting());
  h.tick(200);
  EXPECT_STR(h.out, "\r\nready\r\n");
  EXPECT(!h.emu.wifi_announced());
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
  EXPECT_STR(h.cmd("AT+QBLESTAT"), "\r\n+QBLESTAT:ADVERTISING\r\n\r\nOK\r\n");
  EXPECT_STR(h.cmd("AT+QBLETRANMODE=ff02"), "\r\nOK\r\n");

  // A write without a session opens one first.
  h.out.clear();
  h.emu.ble_write(0xFF01, {0x73, 0x06, 0x23, 0x03, 0x01, 0x54});
  EXPECT_STR(h.out, std::string("\r\n+QBLESTAT:CONNECTED\r\n\x73\x06\x23\x03\x01\x54", 29));
  EXPECT(h.emu.ble_session());

  // Hex payload.
  EXPECT_STR(h.cmd("AT+QBLEGATTSNTFY=ff02,6,730623030154"), "\r\nOK\r\n");
  EXPECT_EQ(h.backend.notifications.back().second.size(), 6u);
  EXPECT_EQ(h.backend.notifications.back().second[0], 0x73);
  // Raw binary payload with CR and LF inside: taken by length.
  std::string raw("\x73\x08\x0d\x0a\x41\x54\x0d\x0a", 8);
  h.out.clear();
  h.send("AT+QBLEGATTSNTFY=ff02,8," + raw + "\r\n");
  h.tick(10);
  EXPECT_STR(h.out, "\r\nOK\r\n");
  EXPECT_EQ(h.backend.notifications.back().second.size(), 8u);
  EXPECT_EQ(h.backend.notifications.back().second[3], 0x0a);
  // Hex arriving in pieces is not mistaken for raw bytes.
  h.out.clear();
  h.send("AT+QBLEGATTSNTFY=ff02,4,7306");
  h.tick(10);
  EXPECT_STR(h.out, "");
  h.send("2303\r\n");
  h.tick(10);
  EXPECT_STR(h.out, "\r\nOK\r\n");
  EXPECT_EQ(h.backend.notifications.back().second.size(), 4u);
  EXPECT_EQ(h.backend.notifications.back().second[2], 0x23);
  // Cell info as text.
  EXPECT_STR(h.cmd("AT+QBLEGATTSNTFY=ff02,10_24_25_3162_3160_3161"), "\r\nOK\r\n");
  auto &cells = h.backend.notifications.back().second;
  EXPECT_STR(std::string(cells.begin(), cells.end()), "10_24_25_3162_3160_3161");

  h.out.clear();
  h.emu.ble_connected(false);
  EXPECT_STR(h.out, "\r\n+QBLESTAT:DISCONNECTED\r\n");
}

static void test_mqtt() {
  Harness h;
  EXPECT_STR(h.cmd("AT+QMTPUB=0,0,0,0,\"t\",1,\"x\""), "\r\nOK\r\n\r\n+QMTPUB: 0,0,2\r\n");
  h.cmd("AT+QMTCFG=\"version\",0,4");
  h.cmd("AT+QMTCFG=\"keepalive\",0,60");
  h.cmd("AT+QMTCFG=\"ssl\",0,1,2");
  EXPECT_STR(h.cmd("AT+QMTOPEN=0,\"eu.hamedata.com\",8883"), "\r\nOK\r\n\r\n+QMTOPEN: 0,0\r\n");
  EXPECT_STR(h.cmd("AT+QMTCONN=0,\"mst_aabbccddeeff\",\"user\",\"pw,1\""), "\r\nOK\r\n\r\n+QMTCONN: 0,0,0\r\n");
  EXPECT(h.emu.mqtt_connected());
  EXPECT_EQ(h.backend.sessions, 1);
  EXPECT_STR(h.backend.session.host, "eu.hamedata.com");
  EXPECT_EQ(h.backend.session.port, 8883);
  EXPECT_STR(h.backend.session.client_id, "mst_aabbccddeeff");
  EXPECT_STR(h.backend.session.username, "user");
  EXPECT_STR(h.backend.session.password, "pw,1");
  EXPECT(h.backend.session.ssl);
  EXPECT_EQ(h.backend.session.keepalive, 60);
  EXPECT_STR(h.cmd("AT+QMTSUB=0,1,\"hame_energy/HMA-1/App/aabbccddeeff/ctrl\",0"),
             "\r\nOK\r\n\r\n+QMTSUB: 0,1,0,0\r\n");

  // Payload with commas, '=' and a quote: taken by length.
  std::string payload = "p1=1,p2=1,w1=24,fc=\"x\"";
  EXPECT_STR(h.cmd("AT+QMTPUB=0,0,0,0,\"hame_energy/HMA-1/device/aabbccddeeff/ctrl\"," +
                   std::to_string(payload.size()) + ",\"" + payload + "\""),
             "\r\nOK\r\n\r\n+QMTPUB: 0,0,0\r\n");
  EXPECT_STR(h.backend.published.back().second, payload);
  // Prompt form: "> ", payload, Ctrl-Z.
  EXPECT_STR(h.cmd("AT+QMTPUB=0,0,0,0,\"other/topic\""), "\r\n>");
  h.out.clear();
  h.send(std::string("cd=1,x\x1a"));
  h.tick(50);
  EXPECT_STR(h.out, "\r\nOK\r\n\r\n+QMTPUB: 0,0,0\r\n");
  EXPECT_STR(h.backend.published.back().first, "other/topic");
  EXPECT_STR(h.backend.published.back().second, "cd=1,x");

  // Broker messages: message ids count up; filters with wildcards match.
  h.out.clear();
  EXPECT(h.emu.mqtt_receive("hame_energy/HMA-1/App/aabbccddeeff/ctrl", "cd=01"));
  EXPECT(h.emu.mqtt_receive("hame_energy/HMA-1/App/aabbccddeeff/ctrl", "cd=01"));
  EXPECT_STR(h.out, "\r\n+QMTRECV: 0,1,\"hame_energy/HMA-1/App/aabbccddeeff/ctrl\",5,\"cd=01\"\r\n"
                    "\r\n+QMTRECV: 0,2,\"hame_energy/HMA-1/App/aabbccddeeff/ctrl\",5,\"cd=01\"\r\n");
  EXPECT(!h.emu.mqtt_receive("other/topic", "cd=01"));
  h.cmd("AT+QMTSUB=0,2,\"marstek_energy/+/App/#\",0");
  EXPECT(h.emu.mqtt_receive("marstek_energy/HMA-1/App/x/ctrl", "cd=1"));

  // Retain flag (Jupiter publishes with retain=1).
  h.cmd("AT+QMTPUB=0,0,0,1,\"hame_energy/HMN-1/device/x/ctrl\",3,\"a=1\"");
  EXPECT(h.backend.retained.back());

  // Self-echo: Venus publishes to its own server topic and expects it back.
  h.cmd("AT+QMTSUB=0,3,\"marstek/VNSE3-0/server/x/ctrl\",0");
  std::string echo = h.cmd("AT+QMTPUB=0,0,0,0,\"marstek/VNSE3-0/server/x/ctrl\",4,\"ping\"");
  EXPECT(echo.find("+QMTRECV: 0,") != std::string::npos);
  EXPECT(echo.find("\"marstek/VNSE3-0/server/x/ctrl\",4,\"ping\"") != std::string::npos);
  // ...but not when a real broker (cloud forwarding) already echoes it.
  h.backend.broker_echoes = true;
  echo = h.cmd("AT+QMTPUB=0,0,0,0,\"marstek/VNSE3-0/server/x/ctrl\",4,\"ping\"");
  EXPECT(echo.find("+QMTRECV") == std::string::npos);
  h.backend.broker_echoes = false;
  // A B2500 publishes to its device topic and subscribes to App: no echo.
  echo = h.cmd("AT+QMTPUB=0,0,0,0,\"hame_energy/HMA-1/device/aabbccddeeff/ctrl\",3,\"x=1\"");
  EXPECT(echo.find("+QMTRECV") == std::string::npos);

  // Losing Wi-Fi drops the session with a URC.
  h.out.clear();
  h.backend.wifi.connected = false;
  h.tick(10);
  EXPECT(h.out.find("+QMTSTAT: 0,1") != std::string::npos);
  EXPECT(!h.emu.mqtt_connected());
  EXPECT_EQ(h.backend.closed, 1);
  // Without an IP an open fails.
  EXPECT_STR(h.cmd("AT+QMTOPEN=0,\"eu.hamedata.com\",8883", 200), "\r\nOK\r\n\r\n+QMTOPEN: 0,-1\r\n");
}

static void test_ssl() {
  Harness h;
  // Nothing stored: every kind reported missing, so the MCU uploads. One
  // block, no checksums (module build 202409090159).
  EXPECT_STR(h.cmd("AT+QSSLCERT?"), "\r\n+QSSLCERT: \"CA\",0\r\n+QSSLCERT: \"User Cert\",0\r\n"
                                    "+QSSLCERT: \"User Key\",0\r\n\r\nOK\r\n");
  std::string pem = "-----BEGIN CERTIFICATE-----\r\nMIIB\r\n-----END CERTIFICATE-----\r\n";
  EXPECT_STR(h.cmd("AT+QSSLCERT=\"User Key\",2," + std::to_string(pem.size())), "\r\nCONNECT\r\n");
  h.out.clear();
  h.send(pem);
  EXPECT_STR(h.out, "\r\nOK\r\n");
  EXPECT_EQ(h.backend.certs.size(), 1u);
  EXPECT_STR(h.backend.certs[0].first, "User Key");
  // Stored: '"User Key",1', which is what the MCU looks for.
  EXPECT_STR(h.cmd("AT+QSSLCERT?"), "\r\n+QSSLCERT: \"CA\",0\r\n+QSSLCERT: \"User Cert\",0\r\n"
                                    "+QSSLCERT: \"User Key\",1\r\n\r\nOK\r\n");
  // Later module builds append a checksum.
  h.emu.set_module_version("FC41DAAR18A04M02_HM.bin_202503010000");
  char ck[8];
  snprintf(ck, sizeof(ck), "%x", Fc41dEmulator::cert_checksum(pem));
  EXPECT_STR(h.cmd("AT+QSSLCERT?"), std::string("\r\n+QSSLCERT: \"CA\",0,0\r\n+QSSLCERT: \"User Cert\",0,0\r\n") +
                                        "+QSSLCERT: \"User Key\",1," + ck + "\r\n\r\nOK\r\n");
  // Read back, delete.
  EXPECT_STR(h.cmd("AT+QSSLCERT=\"User Key\",1"), "\r\nCONNECT\r\n" + pem + "\r\nOK\r\n");
  EXPECT_STR(h.cmd("AT+QSSLCERT=\"User Key\",0"), "\r\nOK\r\n");
  EXPECT_STR(h.cmd("AT+QSSLCERT=\"User Key\",1"), "\r\nERROR\r\n");
  // CRC-16/CCITT-FALSE check value.
  EXPECT_EQ(Fc41dEmulator::cert_checksum("123456789"), 0x29B1);
}

static void test_http() {
  Harness h;
  std::string url = "http://eu.hamedata.com/app/neng/getDateInfoeu.php?uid=aabbccddeeff&fcv=202409090159&aid=HMA-1";
  EXPECT_STR(h.cmd("AT+QHTTPCFG=\"url\",\"" + url + "\""), "\r\nOK\r\n");
  // A backend that answers at once: the result still comes >= 300 ms after OK.
  h.backend.answer_http_now = true;
  h.backend.http_answer.status = 200;
  h.backend.http_answer.body = "_2026_11_01_21_30_00_04_0_0_0";
  EXPECT_STR(h.cmd("AT+QHTTPGET=60", 250), "\r\nOK\r\n");
  EXPECT_STR(h.backend.requests[0], "GET " + url);
  EXPECT(h.backend.last_headers.empty());
  EXPECT_STR(h.cmd("AT+QHTTPGET=60", 10), "\r\nERROR\r\n");  // busy until the result
  h.out.clear();
  h.tick(60);
  EXPECT_STR(h.out, "\r\n+QHTTPGET: 0,200,29\r\n");
  EXPECT_STR(h.cmd("AT+QHTTPREAD=60"), "\r\nCONNECT\r\n_2026_11_01_21_30_00_04_0_0_0\r\nOK\r\n\r\n+QHTTPREAD: 0\r\n");

  // Request headers (Venus E 3.0/D/A): set, replace, remove.
  h.cmd("AT+QHTTPCFG=\"header\",\"Authorization\",\"Bearer a,b\"");
  h.cmd("AT+QHTTPCFG=\"header\",\"X-Dev\",\"1\"");
  h.cmd("AT+QHTTPCFG=\"header\",\"X-Dev\",\"2\"");
  h.cmd("AT+QHTTPGET=60", 400);
  EXPECT_EQ(h.backend.last_headers.size(), 2u);
  EXPECT_STR(h.backend.last_headers[0].second, "Bearer a,b");
  EXPECT_STR(h.backend.last_headers[1].second, "2");
  h.cmd("AT+QHTTPCFG=\"header\",\"Authorization\"");
  h.cmd("AT+QHTTPGET=60", 400);
  EXPECT_EQ(h.backend.last_headers.size(), 1u);

  // A slow backend: the result follows as soon as it arrives.
  h.backend.answer_http_now = false;
  h.cmd("AT+QHTTPCFG=\"url\",\"http://eu.hamedata.com/app/Solar/puterrinfo.php\"");
  EXPECT_STR(h.cmd("AT+QHTTPPOST=7,120,120"), "\r\nCONNECT\r\n");
  h.out.clear();
  h.send("err=1,2");
  h.tick(10);
  EXPECT_STR(h.out, "\r\nOK\r\n");
  EXPECT_STR(h.backend.requests.back(), "POST http://eu.hamedata.com/app/Solar/puterrinfo.php err=1,2");
  h.tick(1000);
  h.out.clear();
  HttpResult fail;
  fail.error = 702;
  h.backend.pending_http(fail);
  h.tick(10);
  EXPECT_STR(h.out, "\r\n+QHTTPPOST: 702\r\n");
}

static void test_sockets() {
  Harness h;
  // B2500 v2: UDP service towards the CT meter, buffered (mode 0).
  EXPECT_STR(h.cmd("AT+QIOPEN=1,\"UDP SERVICE\",\"192.168.1.255\",12345,12345,0"), "\r\nOK\r\n\r\n+QIOPEN: 1,0\r\n");
  EXPECT_EQ(h.backend.opened.size(), 1u);
  EXPECT_STR(h.backend.opened[0].kind, "UDP SERVICE");
  EXPECT_EQ(h.backend.opened[0].local_port, 12345);
  // Inline send with a target, as the images do.
  EXPECT_STR(h.cmd("AT+QISEND=1,5,\"hello\",\"192.168.1.20\",12345"), "\r\n+QISEND: 5\r\n\r\nOK\r\n");
  EXPECT_STR(h.backend.sent.back(), "1:hello@192.168.1.20:12345");
  // Prompt form.
  EXPECT_STR(h.cmd("AT+QISEND=1,3"), "\r\n> ");
  h.out.clear();
  h.send("a\r\n");
  h.tick(10);
  EXPECT_STR(h.out, "\r\n+QISEND: 3\r\n\r\nOK\r\n");
  EXPECT_STR(h.cmd("AT+QISEND=1,0"), "\r\n+QISEND: 8,8,0\r\n\r\nOK\r\n");
  // Data in: URC, then AT+QIRD.
  h.out.clear();
  h.emu.socket_received(1, "HME-4|1|2|3", "192.168.1.20", 12345);
  EXPECT_STR(h.out, "\r\n+QIURC: \"recv\",1\r\n");
  EXPECT_STR(h.cmd("AT+QIRD=1,0"), "\r\n+QIRD: 11,0,11\r\n\r\nOK\r\n");
  EXPECT_STR(h.cmd("AT+QIRD=1,1500"), "\r\n+QIRD: 11\r\nHME-4|1|2|3\r\nOK\r\n");
  // Push mode (mode 1) delivers the data with the URC.
  h.cmd("AT+QIOPEN=2,\"UDP SERVICE\",\"127.0.0.1\",0,22222,1");
  h.out.clear();
  h.emu.socket_received(2, "abc", "192.168.1.30", 22222);
  EXPECT_STR(h.out, "\r\n+QIURC: \"recv\",2,3,\"192.168.1.30\",22222\r\nabc\r\n");
  h.out.clear();
  h.emu.socket_closed(2);
  EXPECT_STR(h.out, "\r\n+QIURC: \"closed\",2\r\n");
  EXPECT_STR(h.cmd("AT+QICLOSE=2"), "\r\nOK\r\n");
  EXPECT_EQ(h.backend.socket_closes.back(), 2);
  EXPECT_EQ(h.emu.socket_count(), 1u);
}

int main() {
  test_boot_and_wifi();
  test_wifi_asked_and_reannounce();
  test_system();
  test_ble();
  test_mqtt();
  test_ssl();
  test_http();
  test_sockets();
  return finish();
}
