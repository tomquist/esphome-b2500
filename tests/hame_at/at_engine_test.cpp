// AtEngine: line parsing, argument splitting, resync, DATA and TRANSPARENT modes.

#include "at_engine.h"
#include "test_util.h"

using namespace esphome::hame_at;

struct Harness {
  AtEngine engine;
  std::string out;
  std::vector<std::string> seen;
  uint32_t now{1000};

  Harness() {
    engine.set_output([this](const uint8_t *d, size_t n) { out.append(reinterpret_cast<const char *>(d), n); });
    engine.on("", [](AtEngine &e, const AtCommand &) { e.ok(); });
    engine.set_default_handler([this](AtEngine &e, const AtCommand &c) {
      seen.push_back(c.name);
      e.ok();
    });
  }
  void send(const std::string &s) { engine.feed(reinterpret_cast<const uint8_t *>(s.data()), s.size(), now); }
  std::string take() {
    std::string o = out;
    out.clear();
    return o;
  }
};

static void test_parse() {
  AtCommand c;
  EXPECT(parse_at_line("AT", c));
  EXPECT_STR(c.name, "");
  EXPECT(parse_at_line("AT+QVERSION", c));
  EXPECT_STR(c.name, "QVERSION");
  EXPECT(c.kind == AtCommand::Kind::EXEC);
  EXPECT(parse_at_line("AT+QBLEADDR?", c));
  EXPECT(c.kind == AtCommand::Kind::QUERY);
  EXPECT(parse_at_line("at+qmtcfg=?", c));
  EXPECT_STR(c.name, "QMTCFG");
  EXPECT(c.kind == AtCommand::Kind::TEST);
  EXPECT(parse_at_line("ATE0", c));
  EXPECT_STR(c.name, "E0");

  EXPECT(parse_at_line("AT+QMTOPEN=0,\"eu.hamedata.com\",1883", c));
  EXPECT(c.kind == AtCommand::Kind::SET);
  EXPECT_EQ(c.args.size(), 3u);
  EXPECT_STR(c.arg(1), "eu.hamedata.com");
  EXPECT_EQ(c.arg_int(2), 1883L);
  EXPECT_EQ(c.arg_int(5, -7), -7L);

  // Commas inside quotes are not separators.
  EXPECT(parse_at_line("AT+QMTPUB=0,0,0,0,\"hame_energy/HMA-1/device/abc/ctrl\",9,\"p1=1,p2=0\"", c));
  EXPECT_EQ(c.args.size(), 7u);
  EXPECT_STR(c.arg(6), "p1=1,p2=0");
  EXPECT_STR(raw_arg_tail(c.raw_args, 6), "\"p1=1,p2=0\"");

  // Empty arguments are kept.
  EXPECT(parse_at_line("AT+QSTAAPINFODEF=,", c));
  EXPECT_EQ(c.args.size(), 2u);
  EXPECT(!parse_at_line("hello", c));
  EXPECT(!parse_at_line("AT+QFOO#", c));
}

static void test_lines_and_resync() {
  Harness h;
  h.send("AT\r\n");
  EXPECT_STR(h.take(), "\r\nOK\r\n");
  // CR only, LF only, and several lines in one burst.
  h.send("AT+QA\rAT+QB\nAT+QC\r\n");
  EXPECT_EQ(h.seen.size(), 3u);
  EXPECT_STR(h.take(), "\r\nOK\r\n\r\nOK\r\n\r\nOK\r\n");
  // Split across feeds.
  h.seen.clear();
  h.send("AT+QMT");
  h.send("OPEN=0,\"x\",1\r\n");
  EXPECT_EQ(h.seen.size(), 1u);
  EXPECT_STR(h.seen[0], "QMTOPEN");
  // Leading bytes lost (BK UART FIFO): resync on "AT+".
  h.seen.clear();
  h.take();
  h.send("\x01\x02xyzAT+QSTAST\r\n");
  EXPECT_EQ(h.seen.size(), 1u);
  EXPECT_STR(h.seen[0], "QSTAST");
  EXPECT_EQ(h.engine.get_garbage_count(), 1u);
  // Pure noise is dropped without a reply.
  h.take();
  h.send("garbage\r\n");
  EXPECT_STR(h.take(), "");
  EXPECT_EQ(h.engine.get_garbage_count(), 2u);
  EXPECT_EQ(h.engine.get_unknown_count(), 5u);
}

static void test_echo() {
  Harness h;
  h.engine.set_echo(true);
  h.send("AT\r\n");
  EXPECT_STR(h.take(), "AT\r\n\r\nOK\r\n");
}

static void test_data_mode() {
  Harness h;
  std::vector<uint8_t> got;
  h.engine.on("QSSLCERT", [&](AtEngine &e, const AtCommand &c) {
    e.send_line("CONNECT");
    e.expect_data(static_cast<size_t>(c.arg_int(2)), [&](AtEngine &e, std::vector<uint8_t> d) {
      got = d;
      e.ok();
    });
  });
  // Data containing CR/LF and "AT" must not be parsed as commands.
  h.send("AT+QSSLCERT=\"CA\",2,10\r\n");
  EXPECT_STR(h.take(), "\r\nCONNECT\r\n");
  EXPECT(h.engine.get_mode() == AtEngine::Mode::DATA);
  h.send("AT\r\n12");
  h.send("3456AT\r\n");  // 4 + 6 = 10 bytes data, then "AT\r\n" is a command again
  EXPECT_EQ(got.size(), 10u);
  EXPECT_STR(std::string(got.begin(), got.end()), "AT\r\n123456");
  EXPECT_STR(h.take(), "\r\nOK\r\n\r\nOK\r\n");
  EXPECT(h.engine.get_mode() == AtEngine::Mode::LINE);

  // Short data times out and returns to command mode.
  got.clear();
  h.send("AT+QSSLCERT=\"CA\",2,100\r\n");
  h.send("abc");
  h.now += AtEngine::DATA_TIMEOUT_MS + 1;
  h.engine.poll(h.now);
  EXPECT_EQ(got.size(), 3u);
  EXPECT(h.engine.get_mode() == AtEngine::Mode::LINE);
}

static void test_transparent() {
  Harness h;
  std::string sunk;
  bool exited = false;
  h.engine.enter_transparent([&](const uint8_t *d, size_t n) { sunk.append(reinterpret_cast<const char *>(d), n); },
                             [&]() { exited = true; });
  h.send("AT+QVERSION\r\n");  // raw data, not a command
  EXPECT_STR(sunk, "AT+QVERSION\r\n");
  EXPECT(h.seen.empty());
  // '+' inside data passes through.
  h.send("a+b");
  EXPECT_STR(sunk, "AT+QVERSION\r\na+b");
  // "+++" as its own burst, then the guard time: exit with OK.
  h.take();
  h.send("+++");
  h.now += 100;
  h.engine.poll(h.now);
  EXPECT(!exited);
  h.now += AtEngine::PLUS_GUARD_MS;
  h.engine.poll(h.now);
  EXPECT(exited);
  EXPECT_STR(h.take(), "\r\nOK\r\n");
  EXPECT(h.engine.get_mode() == AtEngine::Mode::LINE);
  // A lone "+" burst is data.
  sunk.clear();
  h.engine.enter_transparent([&](const uint8_t *d, size_t n) { sunk.append(reinterpret_cast<const char *>(d), n); });
  h.send("+");
  h.now += AtEngine::PLUS_GUARD_MS + 1;
  h.engine.poll(h.now);
  EXPECT_STR(sunk, "+");
  EXPECT(h.engine.get_mode() == AtEngine::Mode::TRANSPARENT);
}

int main() {
  test_parse();
  test_lines_and_resync();
  test_echo();
  test_data_mode();
  test_transparent();
  return finish();
}
