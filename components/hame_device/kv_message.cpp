#include "kv_message.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace esphome::hame_device {

static std::string trim(const std::string &s) {
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n'))
    a++;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n'))
    b--;
  return s.substr(a, b - a);
}

bool parse_kv(const std::string &payload, KvMap &out) {
  bool any = false;
  size_t pos = 0;
  while (pos <= payload.size()) {
    size_t comma = payload.find(',', pos);
    if (comma == std::string::npos)
      comma = payload.size();
    const std::string item = payload.substr(pos, comma - pos);
    const size_t eq = item.find('=');
    if (eq != std::string::npos) {
      std::string key = trim(item.substr(0, eq));
      if (!key.empty()) {
        out[key] = trim(item.substr(eq + 1));
        any = true;
      }
    }
    pos = comma + 1;
  }
  return any;
}

static bool render_into(const std::string &tpl, const std::string &value, const KeyLookup &lookup, std::string &out,
                        std::string *missing, bool allow_sections) {
  for (size_t i = 0; i < tpl.size(); i++) {
    const char c = tpl[i];
    const char next = i + 1 < tpl.size() ? tpl[i + 1] : '\0';
    if ((c == '{' || c == '}' || c == '[' || c == ']') && next == c) {
      out += c;
      i++;
      continue;
    }
    if (c == '[' && allow_sections) {
      const size_t close = tpl.find(']', i + 1);
      if (close == std::string::npos) {
        out += tpl.substr(i);
        break;
      }
      std::string part;
      if (render_into(tpl.substr(i + 1, close - i - 1), value, lookup, part, nullptr, false))
        out += part;
      i = close;
      continue;
    }
    if (c != '{') {
      out += c;
      continue;
    }
    const size_t close = tpl.find('}', i + 1);
    if (close == std::string::npos) {  // unbalanced: copy verbatim
      out += tpl.substr(i);
      break;
    }
    std::string name = tpl.substr(i + 1, close - i - 1);
    std::string fallback;
    bool has_fallback = false;
    const size_t bar = name.find('|');
    if (bar != std::string::npos) {
      fallback = name.substr(bar + 1);
      name = name.substr(0, bar);
      has_fallback = true;
    }
    if (name == "value") {
      out += value;
    } else {
      const std::string *v = lookup ? lookup(name) : nullptr;
      if (v != nullptr) {
        out += *v;
      } else if (has_fallback) {
        out += fallback;
      } else {
        if (missing != nullptr)
          *missing = name;
        return false;
      }
    }
    i = close;
  }
  return true;
}

bool render_template(const std::string &tpl, const std::string &value, const KeyLookup &lookup, std::string &out,
                     std::string *missing) {
  out.clear();
  return render_into(tpl, value, lookup, out, missing, true);
}

bool MessageRule::matches(const KvMap &msg) const {
  if (this->key_count != 0 && msg.size() != this->key_count)
    return false;
  for (const auto &k : this->required) {
    if (!msg.count(k))
      return false;
  }
  for (const auto &k : this->excluded) {
    if (msg.count(k))
      return false;
  }
  return true;
}

static bool split_time(const std::string &in, int &h, int &m) {
  const size_t colon = in.find(':');
  if (colon == std::string::npos || colon == 0 || colon + 1 >= in.size() || in.find(':', colon + 1) != std::string::npos)
    return false;
  double hv, mv;
  if (!parse_number(in.substr(0, colon), hv) || !parse_number(in.substr(colon + 1), mv))
    return false;
  h = static_cast<int>(hv);
  m = static_cast<int>(mv);
  return hv == h && mv == m && h >= 0 && m >= 0;
}

bool pad_time(const std::string &in, std::string &out) {
  int h, m;
  if (!split_time(in, h, m) || h > 99 || m > 99)
    return false;
  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
  out = buf;
  return true;
}

bool unpad_time(const std::string &in, std::string &out) {
  int h, m;
  if (!split_time(in, h, m) || h > 23 || m > 59)
    return false;
  out = std::to_string(h) + ":" + std::to_string(m);
  return true;
}

bool parse_number(const std::string &s, double &out) {
  if (s.empty())
    return false;
  char *end = nullptr;
  const double v = strtod(s.c_str(), &end);
  if (end == s.c_str() || *end != '\0' || std::isnan(v))
    return false;
  out = v;
  return true;
}

bool NumberField::extract(const KvMap &msg, double &out) const {
  if (this->keys.empty())
    return false;
  double sum = 0, lo = 0, hi = 0;
  size_t n = 0;
  for (const auto &key : this->keys) {
    auto it = msg.find(key);
    double v;
    if (it == msg.end() || !parse_number(it->second, v)) {
      if (this->aggregate == SUM)
        return false;
      continue;
    }
    if (this->bit >= 0) {
      const long long raw = std::llround(v);
      v = static_cast<double>((raw >> this->bit) & ((1LL << this->bits) - 1));
    }
    const bool out_of_range = (this->use_min && v < this->min) || (this->use_max && v > this->max);
    if (out_of_range || (this->ignore_zero && v == 0)) {
      if (this->aggregate == SUM)
        return false;
      continue;
    }
    lo = n == 0 ? v : std::min(lo, v);
    hi = n == 0 ? v : std::max(hi, v);
    sum += v;
    n++;
  }
  if (n == 0)
    return false;
  double r = sum;
  switch (this->aggregate) {
    case SUM:
      break;
    case MIN:
      r = lo;
      break;
    case MAX:
      r = hi;
      break;
    case AVERAGE:
      r = sum / static_cast<double>(n);
      break;
    case SPREAD:
      r = hi - lo;
      break;
  }
  out = r * this->multiply;
  return true;
}

bool NumberField::touches(const KvMap &msg) const {
  for (const auto &key : this->keys) {
    if (msg.count(key))
      return true;
  }
  return false;
}

std::string format_number(double v) {
  char buf[32];
  if (std::fabs(v - std::round(v)) < 1e-9 && std::fabs(v) < 1e15) {
    snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(std::llround(v)));
  } else {
    snprintf(buf, sizeof(buf), "%g", v);
  }
  return buf;
}

}  // namespace esphome::hame_device
