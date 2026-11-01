#include "at_engine.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace esphome::hame_at {

const std::string &AtCommand::empty_() {
  static const std::string EMPTY;
  return EMPTY;
}

long AtCommand::arg_int(size_t i, long def) const {
  if (i >= this->args.size() || this->args[i].empty())
    return def;
  const char *s = this->args[i].c_str();
  char *end = nullptr;
  long v = strtol(s, &end, 10);
  if (end == s || *end != '\0')
    return def;
  return v;
}

std::vector<std::string> split_at_args(const std::string &raw) {
  std::vector<std::string> out;
  std::string cur;
  bool quoted = false;
  bool was_quoted = false;
  for (char c : raw) {
    if (c == '"') {
      quoted = !quoted;
      was_quoted = true;
      continue;
    }
    if (c == ',' && !quoted) {
      out.push_back(cur);
      cur.clear();
      was_quoted = false;
      continue;
    }
    cur.push_back(c);
  }
  if (!cur.empty() || was_quoted || !out.empty() || !raw.empty())
    out.push_back(cur);
  return out;
}

std::string raw_arg_tail(const std::string &raw, size_t index) {
  if (index == 0)
    return raw;
  bool quoted = false;
  size_t seen = 0;
  for (size_t i = 0; i < raw.size(); i++) {
    if (raw[i] == '"')
      quoted = !quoted;
    else if (raw[i] == ',' && !quoted && ++seen == index)
      return raw.substr(i + 1);
  }
  return "";
}

bool parse_at_line(const std::string &line, AtCommand &out) {
  if (line.size() < 2)
    return false;
  if (toupper(static_cast<unsigned char>(line[0])) != 'A' || toupper(static_cast<unsigned char>(line[1])) != 'T')
    return false;
  out = AtCommand{};
  out.line = line;
  std::string rest = line.substr(2);
  if (rest.empty()) {
    out.name = "";
    out.kind = AtCommand::Kind::EXEC;
    return true;
  }
  // Basic commands without '+' (ATE0, ATE1, ATO, ...).
  if (rest[0] != '+') {
    out.name.clear();
    for (char c : rest)
      out.name.push_back(static_cast<char>(toupper(static_cast<unsigned char>(c))));
    out.kind = AtCommand::Kind::EXEC;
    return true;
  }
  rest = rest.substr(1);
  size_t i = 0;
  while (i < rest.size() && (isalnum(static_cast<unsigned char>(rest[i])) || rest[i] == '_'))
    i++;
  out.name.clear();
  for (size_t j = 0; j < i; j++)
    out.name.push_back(static_cast<char>(toupper(static_cast<unsigned char>(rest[j]))));
  std::string tail = rest.substr(i);
  if (tail.empty()) {
    out.kind = AtCommand::Kind::EXEC;
  } else if (tail == "?") {
    out.kind = AtCommand::Kind::QUERY;
  } else if (tail == "=?") {
    out.kind = AtCommand::Kind::TEST;
  } else if (tail[0] == '=') {
    out.kind = AtCommand::Kind::SET;
    out.raw_args = tail.substr(1);
    out.args = split_at_args(out.raw_args);
  } else {
    return false;
  }
  return !out.name.empty();
}

void AtEngine::send_raw(const uint8_t *data, size_t len) {
  if (this->output_ && len > 0)
    this->output_(data, len);
}

void AtEngine::send_line(const std::string &text) {
  if (this->logger_)
    this->logger_(false, text);
  std::string framed = "\r\n" + text + "\r\n";
  this->send_raw(framed);
}

void AtEngine::expect_data(size_t len, DataCallback callback) {
  this->data_cb_ = std::move(callback);
  this->data_buf_.clear();
  this->data_buf_.reserve(len);
  this->data_remaining_ = len;
  this->data_deadline_ = this->last_rx_ms_ + DATA_TIMEOUT_MS;
  if (len == 0) {
    auto cb = std::move(this->data_cb_);
    this->data_cb_ = nullptr;
    if (cb)
      cb(*this, {});
    return;
  }
  this->mode_ = Mode::DATA;
}

void AtEngine::enter_transparent(Sink sink, std::function<void()> on_exit) {
  this->sink_ = std::move(sink);
  this->on_transparent_exit_ = std::move(on_exit);
  this->plus_buf_.clear();
  this->mode_ = Mode::TRANSPARENT;
  this->line_.clear();
}

void AtEngine::exit_transparent() {
  if (this->mode_ != Mode::TRANSPARENT)
    return;
  this->flush_plus_();
  this->mode_ = Mode::LINE;
  this->sink_ = nullptr;
  auto cb = std::move(this->on_transparent_exit_);
  this->on_transparent_exit_ = nullptr;
  if (cb)
    cb();
}

void AtEngine::flush_plus_() {
  if (!this->plus_buf_.empty() && this->sink_)
    this->sink_(reinterpret_cast<const uint8_t *>(this->plus_buf_.data()), this->plus_buf_.size());
  this->plus_buf_.clear();
}

void AtEngine::poll(uint32_t now_ms) {
  if (this->mode_ == Mode::DATA && static_cast<int32_t>(now_ms - this->data_deadline_) > 0) {
    // The MCU sent fewer bytes than announced. Hand over what arrived and go
    // back to command mode rather than swallowing the next commands.
    auto cb = std::move(this->data_cb_);
    auto buf = std::move(this->data_buf_);
    this->data_cb_ = nullptr;
    this->data_buf_.clear();
    this->data_remaining_ = 0;
    this->mode_ = Mode::LINE;
    if (cb)
      cb(*this, std::move(buf));
  }
  if (this->mode_ == Mode::TRANSPARENT && !this->plus_buf_.empty() && now_ms - this->plus_since_ >= PLUS_GUARD_MS) {
    if (this->plus_buf_ == "+++") {
      this->plus_buf_.clear();
      this->exit_transparent();
      this->ok();
    } else {
      this->flush_plus_();  // a lone "+" or "++" was data after all
    }
  }
}

void AtEngine::feed(const uint8_t *data, size_t len, uint32_t now_ms) {
  this->last_rx_ms_ = now_ms;
  size_t i = 0;
  while (i < len) {
    // A command line ended by CR may be followed by its LF. Drop it even if
    // the command switched to DATA or TRANSPARENT mode in between.
    if (this->skip_lf_) {
      this->skip_lf_ = false;
      if (data[i] == '\n') {
        i++;
        continue;
      }
    }
    switch (this->mode_) {
      case Mode::DATA: {
        size_t n = std::min(this->data_remaining_, len - i);
        this->data_buf_.insert(this->data_buf_.end(), data + i, data + i + n);
        this->data_remaining_ -= n;
        i += n;
        if (this->data_remaining_ == 0) {
          this->mode_ = Mode::LINE;
          auto cb = std::move(this->data_cb_);
          auto buf = std::move(this->data_buf_);
          this->data_cb_ = nullptr;
          this->data_buf_.clear();
          if (cb)
            cb(*this, std::move(buf));
        }
        break;
      }
      case Mode::TRANSPARENT: {
        // "+++" must arrive on its own, so hold back a run of '+' that starts a
        // burst until the guard time has passed; anything else flushes it.
        uint8_t c = data[i++];
        if (c == '+' && this->plus_buf_.size() < 3 && (this->plus_buf_.size() > 0 || i == 1)) {
          if (this->plus_buf_.empty())
            this->plus_since_ = now_ms;
          this->plus_buf_.push_back('+');
          break;
        }
        this->flush_plus_();
        if (this->sink_)
          this->sink_(&c, 1);
        break;
      }
      case Mode::LINE: {
        uint8_t c = data[i++];
        if (c == '\r' || c == '\n') {
          this->skip_lf_ = c == '\r';
          if (!this->line_.empty() && !this->line_overflow_)
            this->handle_line_(this->line_);
          this->line_.clear();
          this->line_overflow_ = false;
          break;
        }
        if (this->line_.size() >= MAX_LINE) {
          this->line_overflow_ = true;
          break;
        }
        this->line_.push_back(static_cast<char>(c));
        break;
      }
    }
  }
}

void AtEngine::handle_line_(const std::string &line) {
  // Resync: the BK UART can drop the first bytes of a long line, and a module
  // reset can leave noise. Start at the last "AT" that begins a command.
  std::string l = line;
  if (l.size() < 2 || toupper(static_cast<unsigned char>(l[0])) != 'A' ||
      toupper(static_cast<unsigned char>(l[1])) != 'T') {
    size_t pos = l.find("AT+");
    if (pos == std::string::npos) {
      this->garbage_count_++;
      if (this->logger_)
        this->logger_(true, "<garbage> " + l);
      return;
    }
    this->garbage_count_++;
    l = l.substr(pos);
  }
  if (this->logger_)
    this->logger_(true, l);
  AtCommand cmd;
  if (!parse_at_line(l, cmd)) {
    this->garbage_count_++;
    this->error();
    return;
  }
  if (this->echo_)
    this->send_raw(l + "\r\n");
  auto it = this->handlers_.find(cmd.name);
  if (it != this->handlers_.end()) {
    it->second(*this, cmd);
    return;
  }
  this->unknown_count_++;
  if (this->unknown_commands_.size() < 32 &&
      std::find(this->unknown_commands_.begin(), this->unknown_commands_.end(), cmd.name) ==
          this->unknown_commands_.end())
    this->unknown_commands_.push_back(cmd.name);
  if (this->default_handler_) {
    this->default_handler_(*this, cmd);
  } else {
    this->ok();
  }
}

}  // namespace esphome::hame_at
