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

void AtEngine::send_bare_line(const std::string &text) {
  if (this->logger_)
    this->logger_(false, text);
  this->send_raw(text + "\r\n");
}

static bool starts_with_ci(const std::string &s, const std::string &prefix) {
  if (s.size() < prefix.size())
    return false;
  for (size_t i = 0; i < prefix.size(); i++) {
    if (toupper(static_cast<unsigned char>(s[i])) != toupper(static_cast<unsigned char>(prefix[i])))
      return false;
  }
  return true;
}

// Start of the next command: "AT" followed by '+', 'E', a line end, or the
// end of what has arrived so far.
static size_t find_at_ci(const std::string &s) {
  for (size_t i = 0; i + 1 < s.size(); i++) {
    if (toupper(static_cast<unsigned char>(s[i])) != 'A' || toupper(static_cast<unsigned char>(s[i + 1])) != 'T')
      continue;
    if (i + 2 >= s.size())
      return i;
    char n = s[i + 2];
    if (n == '+' || n == 'E' || n == 'e' || n == '\r' || n == '\n')
      return i;
  }
  return std::string::npos;
}

static std::string unquote(const std::string &s) {
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
    return s.substr(1, s.size() - 2);
  return s;
}

void AtEngine::on_inline(const std::string &name, size_t fields, bool hex, Handler handler) {
  this->inline_.push_back({"AT+" + name + "=", fields, hex});
  this->handlers_[name] = std::move(handler);
}

void AtEngine::expect_data(size_t len, DataCallback callback) {
  this->skip_repeats_ = false;
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

void AtEngine::expect_until(uint8_t terminator, DataCallback callback) {
  this->data_cb_ = std::move(callback);
  this->until_ = terminator;
  this->data_deadline_ = this->last_rx_ms_ + DATA_TIMEOUT_MS;
  this->mode_ = Mode::UNTIL;
}

void AtEngine::enter_transparent(Sink sink, std::function<void()> on_exit) {
  this->sink_ = std::move(sink);
  this->on_transparent_exit_ = std::move(on_exit);
  this->plus_buf_.clear();
  this->mode_ = Mode::TRANSPARENT;
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

void AtEngine::finish_data_() {
  this->mode_ = Mode::LINE;
  auto cb = std::move(this->data_cb_);
  auto buf = std::move(this->data_buf_);
  this->data_cb_ = nullptr;
  this->data_buf_.clear();
  this->data_remaining_ = 0;
  if (cb)
    cb(*this, std::move(buf));
}

void AtEngine::poll(uint32_t now_ms) {
  if ((this->mode_ == Mode::DATA || this->mode_ == Mode::UNTIL) &&
      static_cast<int32_t>(now_ms - this->data_deadline_) > 0) {
    // The MCU sent less than announced. Hand over what arrived and go back to
    // command mode rather than swallowing the next commands.
    if (this->mode_ == Mode::UNTIL) {
      this->data_buf_.assign(this->rx_.begin(), this->rx_.end());
      this->rx_.clear();
    }
    this->finish_data_();
  }
  if (this->mode_ == Mode::LINE && !this->rx_.empty() && now_ms - this->last_rx_ms_ > DATA_TIMEOUT_MS) {
    // An inline command whose payload never completed: drop it.
    this->garbage_count_++;
    if (this->logger_)
      this->logger_(true, "<incomplete> " + this->rx_.substr(0, 64));
    this->rx_.clear();
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
  this->rx_.append(reinterpret_cast<const char *>(data), len);
  this->burst_start_ = true;
  this->process_();
}

void AtEngine::process_() {
  while (!this->rx_.empty()) {
    // A command line ended by CR may be followed by its LF. Drop it even if
    // the command switched to DATA or TRANSPARENT mode in between.
    if (this->skip_lf_) {
      this->skip_lf_ = false;
      if (this->rx_[0] == '\n') {
        this->rx_.erase(0, 1);
        continue;
      }
    }
    switch (this->mode_) {
      case Mode::DATA: {
        if (this->skip_repeats_ && this->data_buf_.empty()) {
          if (this->rx_.size() < 2)
            return;
          if (starts_with_ci(this->rx_, "AT")) {
            size_t end = this->rx_.find_first_of("\r\n");
            if (end == std::string::npos)
              return;  // wait for the whole line
            if (this->logger_)
              this->logger_(true, "<repeated> " + this->rx_.substr(0, end));
            this->skip_lf_ = this->rx_[end] == '\r';
            this->rx_.erase(0, end + 1);
            continue;
          }
          this->skip_repeats_ = false;
        }
        size_t n = std::min(this->data_remaining_, this->rx_.size());
        this->data_buf_.insert(this->data_buf_.end(), this->rx_.begin(), this->rx_.begin() + n);
        this->rx_.erase(0, n);
        this->data_remaining_ -= n;
        if (this->data_remaining_ == 0)
          this->finish_data_();
        break;
      }
      case Mode::UNTIL: {
        size_t pos = this->rx_.find(static_cast<char>(this->until_));
        if (pos == std::string::npos)
          return;
        this->data_buf_.assign(this->rx_.begin(), this->rx_.begin() + pos);
        this->rx_.erase(0, pos + 1);
        this->finish_data_();
        break;
      }
      case Mode::TRANSPARENT: {
        // "+++" must arrive on its own, so hold back a run of '+' that starts a
        // burst until the guard time has passed; anything else flushes it.
        std::string chunk;
        chunk.swap(this->rx_);
        for (char ch : chunk) {
          bool start = this->burst_start_;
          this->burst_start_ = false;
          if (ch == '+' && this->plus_buf_.size() < 3 && (!this->plus_buf_.empty() || start)) {
            if (this->plus_buf_.empty())
              this->plus_since_ = this->last_rx_ms_;
            this->plus_buf_.push_back('+');
            continue;
          }
          this->flush_plus_();
          uint8_t c = static_cast<uint8_t>(ch);
          if (this->sink_)
            this->sink_(&c, 1);
        }
        return;
      }
      case Mode::LINE:
        if (!this->line_step_())
          return;
        break;
    }
  }
}

bool AtEngine::line_step_() {
  std::string &rx = this->rx_;
  size_t k = 0;
  while (k < rx.size() && (rx[k] == '\r' || rx[k] == '\n' || rx[k] == '\0' || rx[k] == ' '))
    k++;
  if (k > 0) {
    rx.erase(0, k);
    return true;
  }
  if (rx.size() < 2)
    return false;
  if (!starts_with_ci(rx, "AT")) {
    // Resync: the BK UART can drop the first bytes of a long line, and a
    // module reset can leave noise. Skip to the next "AT"; keep a trailing
    // 'A' that may be the start of one.
    size_t pos = find_at_ci(rx);
    if (pos == std::string::npos)
      pos = (toupper(static_cast<unsigned char>(rx.back())) == 'A') ? rx.size() - 1 : rx.size();
    if (pos == 0)
      return false;
    std::string junk = rx.substr(0, pos);
    rx.erase(0, pos);
    size_t eol = junk.find_first_of("\r\n");
    if (junk.find_first_not_of("\r\n\0 ", 0, 4) != std::string::npos) {
      this->garbage_count_++;
      if (this->logger_)
        this->logger_(true, "<garbage> " + junk.substr(0, eol == std::string::npos ? junk.size() : eol));
    }
    return true;
  }
  for (const auto &spec : this->inline_) {
    if (!starts_with_ci(rx, spec.prefix))
      continue;
    Step st = this->parse_inline_(spec);
    if (st == Step::NEED_MORE)
      return false;
    if (st == Step::DONE)
      return true;
    break;  // a plain line after all
  }
  size_t end = rx.find_first_of("\r\n");
  if (end == std::string::npos) {
    if (rx.size() > MAX_LINE) {
      this->garbage_count_++;
      rx.clear();
    }
    return false;
  }
  std::string line = rx.substr(0, end);
  this->skip_lf_ = rx[end] == '\r';
  rx.erase(0, end + 1);
  this->handle_line_(line);
  return true;
}

AtEngine::Step AtEngine::parse_inline_(const InlineSpec &spec) {
  const std::string &rx = this->rx_;
  size_t pos = spec.prefix.size();
  size_t start = pos;
  bool quoted = false;
  std::vector<std::string> fields;
  while (fields.size() < spec.fields) {
    if (pos >= rx.size())
      return Step::NEED_MORE;
    char ch = rx[pos];
    if (ch == '"') {
      quoted = !quoted;
    } else if ((ch == '\r' || ch == '\n') && !quoted) {
      return Step::NOT_INLINE;  // e.g. AT+QISEND=1,0 or the text form of QBLEGATTSNTFY
    } else if (ch == ',' && !quoted) {
      fields.push_back(rx.substr(start, pos - start));
      start = pos + 1;
    }
    pos++;
  }
  const std::string &len_field = fields.back();
  if (len_field.empty() || len_field.find_first_not_of("0123456789") != std::string::npos)
    return Step::NOT_INLINE;
  size_t n = strtoul(len_field.c_str(), nullptr, 10);
  if (n > MAX_LINE * 4)
    return Step::NOT_INLINE;
  if (pos >= rx.size())
    return Step::NEED_MORE;

  std::string payload;
  size_t rest;
  bool done = false;
  if (rx[pos] != '"' && spec.hex) {
    // 2n hex digits followed by CR/LF, or n raw bytes. Wait while everything
    // so far could still be hex.
    size_t h = 0;
    while (h < 2 * n && pos + h < rx.size() && isxdigit(static_cast<unsigned char>(rx[pos + h])))
      h++;
    if (h == 2 * n) {
      if (pos + h >= rx.size())
        return Step::NEED_MORE;
      if (rx[pos + h] == '\r' || rx[pos + h] == '\n') {
        for (size_t i = 0; i < n; i++) {
          char b[3] = {rx[pos + 2 * i], rx[pos + 2 * i + 1], 0};
          payload.push_back(static_cast<char>(strtoul(b, nullptr, 16)));
        }
        rest = pos + h;
        done = true;
      }
    } else if (pos + h >= rx.size()) {
      return Step::NEED_MORE;
    }
  }
  if (!done) {
    if (rx[pos] == '"') {
      // Quoted: <n> bytes between quotes; tolerate a wrong length.
      if (rx.size() < pos + 1 + n + 1)
        return Step::NEED_MORE;
      if (rx[pos + 1 + n] == '"') {
        payload = rx.substr(pos + 1, n);
        rest = pos + 1 + n + 1;
      } else {
        size_t q = rx.find("\"\r", pos + 1);
        if (q == std::string::npos)
          return Step::NEED_MORE;
        payload = rx.substr(pos + 1, q - pos - 1);
        rest = q + 1;
      }
    } else {
      if (rx.size() < pos + n)
        return Step::NEED_MORE;
      payload = rx.substr(pos, n);
      rest = pos + n;
    }
  }
  // Trailing arguments after the payload (UDP target of AT+QISEND).
  size_t eol = rx.find_first_of("\r\n", rest);
  if (eol == std::string::npos)
    return Step::NEED_MORE;
  std::string extra = rx.substr(rest, eol - rest);
  if (!extra.empty() && extra[0] == ',')
    extra.erase(0, 1);

  AtCommand cmd;
  cmd.name = spec.prefix.substr(3, spec.prefix.size() - 4);
  cmd.kind = AtCommand::Kind::SET;
  for (auto &f : fields)
    cmd.args.push_back(unquote(f));
  if (!extra.empty()) {
    for (auto &a : split_at_args(extra))
      cmd.args.push_back(a);
  }
  cmd.has_payload = true;
  cmd.payload = std::move(payload);
  std::string shown = rx.substr(0, pos) + "<" + std::to_string(cmd.payload.size()) + " bytes>" +
                      (extra.empty() ? "" : "," + extra);
  cmd.line = shown;
  this->skip_lf_ = rx[eol] == '\r';
  this->rx_.erase(0, eol + 1);
  this->dispatch_(cmd, shown);
  return Step::DONE;
}

void AtEngine::handle_line_(const std::string &line) {
  AtCommand cmd;
  if (!parse_at_line(line, cmd)) {
    if (this->logger_)
      this->logger_(true, line);
    this->garbage_count_++;
    this->error();
    return;
  }
  this->dispatch_(cmd, line);
}

void AtEngine::dispatch_(AtCommand &cmd, const std::string &shown) {
  if (this->logger_)
    this->logger_(true, shown);
  if (this->echo_)
    this->send_raw(shown + "\r\n");
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
