#pragma once

// AT command engine for emulating a Quectel AT-firmware module (FC41D) towards
// a host MCU. Platform independent: no ESPHome or SDK includes, so it is
// compiled and tested on the host (tests/hame_at).
//
// Bytes from the MCU go into feed(); everything the engine wants to send to
// the MCU goes through the output callback. Three input modes:
//   - LINE: AT command lines terminated by CR and/or LF;
//   - DATA: a handler asked for exactly N raw bytes (AT+QSSLCERT, AT+QHTTPPOST,
//     AT+QISEND), delivered to its callback;
//   - TRANSPARENT: everything is forwarded raw to a sink until "+++" arrives
//     as a burst of its own followed by a guard time (AT+QBLETRANMODE).

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace esphome::hame_at {

struct AtCommand {
  enum class Kind : uint8_t { EXEC, QUERY, SET, TEST };
  std::string name;               // upper-cased, without "AT+", e.g. "QMTPUB"; "" for plain "AT"
  Kind kind{Kind::EXEC};
  std::vector<std::string> args;  // SET only; surrounding quotes removed
  std::string raw_args;           // SET only; everything after '=' verbatim
  std::string line;               // the full line as received (resynced)

  /// Argument i, or `def` if missing.
  const std::string &arg(size_t i, const std::string &def = empty_()) const {
    return i < args.size() ? args[i] : def;
  }
  /// Argument i parsed as an integer, or `def` if missing or not a number.
  long arg_int(size_t i, long def = 0) const;

 private:
  static const std::string &empty_();
};

/// Split an argument list on commas outside double quotes, removing the quotes.
std::vector<std::string> split_at_args(const std::string &raw);
/// The raw argument text starting at argument `index` (top-level commas
/// outside quotes count as separators), quotes left in place.
std::string raw_arg_tail(const std::string &raw, size_t index);
/// Parse one command line. Returns false for lines that are not AT commands.
bool parse_at_line(const std::string &line, AtCommand &out);

class AtEngine {
 public:
  using Output = std::function<void(const uint8_t *data, size_t len)>;
  using Handler = std::function<void(AtEngine &engine, const AtCommand &cmd)>;
  using DataCallback = std::function<void(AtEngine &engine, std::vector<uint8_t> data)>;
  using Sink = std::function<void(const uint8_t *data, size_t len)>;
  using LineLogger = std::function<void(bool from_mcu, const std::string &line)>;

  enum class Mode : uint8_t { LINE, DATA, TRANSPARENT };

  static constexpr size_t MAX_LINE = 2048;
  static constexpr uint32_t DATA_TIMEOUT_MS = 5000;
  static constexpr uint32_t PLUS_GUARD_MS = 500;

  void set_output(Output output) { this->output_ = std::move(output); }
  void set_line_logger(LineLogger logger) { this->logger_ = std::move(logger); }
  /// Echo received command lines back (ATE1). Off by default.
  void set_echo(bool echo) { this->echo_ = echo; }
  bool get_echo() const { return this->echo_; }

  /// Register a handler for a command name ("QMTPUB", or "" for plain "AT").
  void on(const std::string &name, Handler handler) { this->handlers_[name] = std::move(handler); }
  /// Handler for commands nobody registered. Default: reply OK and count it.
  void set_default_handler(Handler handler) { this->default_handler_ = std::move(handler); }

  /// Feed bytes received from the MCU. `now_ms` drives the timeouts.
  void feed(const uint8_t *data, size_t len, uint32_t now_ms);
  /// Advance timeouts without input (call regularly).
  void poll(uint32_t now_ms);

  // --- Output helpers. Responses are framed "\r\n<text>\r\n". ---
  void send_line(const std::string &text);
  void ok() { this->send_line("OK"); }
  void error() { this->send_line("ERROR"); }
  /// Unframed bytes, e.g. a "> " prompt or a raw read body.
  void send_raw(const uint8_t *data, size_t len);
  void send_raw(const std::string &text) { this->send_raw(reinterpret_cast<const uint8_t *>(text.data()), text.size()); }

  /// Switch to DATA mode: the next `len` bytes go to `callback`. Typically
  /// called by a handler right after sending its prompt ("CONNECT" or "> ").
  void expect_data(size_t len, DataCallback callback);
  /// Switch to TRANSPARENT mode until "+++" (or exit_transparent()).
  void enter_transparent(Sink sink, std::function<void()> on_exit = nullptr);
  void exit_transparent();
  Mode get_mode() const { return this->mode_; }

  uint32_t get_unknown_count() const { return this->unknown_count_; }
  uint32_t get_garbage_count() const { return this->garbage_count_; }
  const std::vector<std::string> &get_unknown_commands() const { return this->unknown_commands_; }

 protected:
  void handle_line_(const std::string &line);
  void flush_plus_();

  Output output_;
  LineLogger logger_;
  std::map<std::string, Handler> handlers_;
  Handler default_handler_;

  Mode mode_{Mode::LINE};
  std::string line_;
  bool line_overflow_{false};
  bool skip_lf_{false};

  // DATA mode
  size_t data_remaining_{0};
  std::vector<uint8_t> data_buf_;
  DataCallback data_cb_;
  uint32_t data_deadline_{0};

  // TRANSPARENT mode
  Sink sink_;
  std::function<void()> on_transparent_exit_;
  std::string plus_buf_;
  uint32_t plus_since_{0};
  uint32_t last_rx_ms_{0};

  bool echo_{false};
  uint32_t unknown_count_{0};
  uint32_t garbage_count_{0};
  std::vector<std::string> unknown_commands_;  // distinct names, capped
};

}  // namespace esphome::hame_at
