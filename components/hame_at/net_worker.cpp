#include "net_worker.h"
#include "tls_session.h"

#ifdef USE_LIBRETINY

#include <Arduino.h>
#include <FreeRTOS.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <task.h>

#include <cstdio>
#include <cstring>

namespace esphome::hame_at {

// TLS handshakes with an RSA client certificate need a deep stack.
static constexpr uint32_t TASK_STACK_BYTES = 10 * 1024;
static constexpr uint32_t CONNECT_TIMEOUT_MS = 10000;
static constexpr uint32_t HTTP_TIMEOUT_MS = 10000;
static constexpr size_t HTTP_MAX_BODY = 8 * 1024;
static constexpr size_t MAX_MESSAGES = 16;
static constexpr size_t MAX_LOGS = 16;

// Quectel HTTP error codes (EC2x family; unverified for the FC41D build).
static constexpr int HTTP_ERR_UNKNOWN = 701;
static constexpr int HTTP_ERR_TIMEOUT = 702;
static constexpr int HTTP_ERR_CONNECT = 713;

// ---------------------------------------------------------------------------
// Transport over LibreTiny's LwIP / mbedTLS clients
// ---------------------------------------------------------------------------

// 1 alive, 0 closed by the peer, -errno on a socket error. Defined at the end
// of the file: lwip/sockets.h defines connect/read/... as macros.
static int socket_alive(int fd);

class LtTransport : public Transport {
 public:
  bool connect(const std::string &host, uint16_t port, const TlsCredentials *tls) override {
    this->close();
    this->tcp_ = new WiFiClient();  // NOLINT: owned here, freed in close()
    if (this->tcp_->connect(host.c_str(), port, CONNECT_TIMEOUT_MS) != 1) {
      this->last_error_ = "TCP connect failed";
      this->close();
      return false;
    }
    if (tls != nullptr) {
      // TLS on the same socket, straight on mbedTLS (see tls_session.h).
      const uint32_t heap_before = lt_heap_get_free();
      std::string err;
      if (!this->tls_.start(this->tcp_->fd(), host, tls->ca, tls->client_cert, tls->client_key, CONNECT_TIMEOUT_MS,
                            err)) {
        char heap[80];
        snprintf(heap, sizeof(heap), "; heap %u B free before, %u B at the lowest", static_cast<unsigned>(heap_before),
                 static_cast<unsigned>(this->tls_.handshake_stats().lowest_free));
        this->last_error_ = "TLS " + err + heap;
        this->close();
        return false;
      }
      this->secure_ = true;
      const auto &st = this->tls_.handshake_stats();
      char info[128];
      snprintf(info, sizeof(info),
               "TLS handshake %u ms; heap %u B free before, %u B at the lowest (largest block %u B), %u B after",
               static_cast<unsigned>(st.ms), static_cast<unsigned>(heap_before), static_cast<unsigned>(st.lowest_free),
               static_cast<unsigned>(st.largest_alloc), static_cast<unsigned>(lt_heap_get_free()));
      this->info_ = info;
    } else {
      this->info_.clear();
    }
    this->last_error_.clear();
    return true;
  }
  bool connected() override {
    if (this->tcp_ == nullptr)
      return false;
    // Not LwIPClient::connected(): it does a zero-length recv, which sets no
    // errno, and then decides on whatever errno some other socket call left.
    // Peek one byte instead: 0 = closed by the peer, EWOULDBLOCK = alive.
    const int fd = this->tcp_->fd();
    if (fd < 0) {
      this->last_error_ = "no socket";
      return false;
    }
    const int r = socket_alive(fd);
    if (r > 0)
      return true;
    this->last_error_ = r == 0 ? "server closed TCP" : "socket error " + std::to_string(-r);
    return false;
  }
  int read(uint8_t *buf, size_t len) override {
    if (this->tcp_ == nullptr)
      return -1;
    if (this->secure_) {
      const int n = this->tls_.recv_some(buf, len);
      if (n < 0)
        this->last_error_ = this->tls_.last_error();
      return n;
    }
    int avail = this->tcp_->available();
    if (avail <= 0)
      return this->connected() ? 0 : -1;
    int n = this->tcp_->read(buf, std::min<size_t>(len, avail));
    return n < 0 ? 0 : n;
  }
  bool write(const uint8_t *data, size_t len) override {
    if (this->tcp_ == nullptr)
      return false;
    if (this->secure_) {
      if (!this->tls_.send_all(data, len, CONNECT_TIMEOUT_MS)) {
        this->last_error_ = this->tls_.last_error();
        return false;
      }
      return true;
    }
    return this->tcp_->write(data, len) == len;
  }
  void close() override {
    if (this->secure_)
      this->tls_.finish();
    this->secure_ = false;
    if (this->tcp_ != nullptr)
      this->tcp_->stop();
    delete this->tcp_;  // NOLINT
    this->tcp_ = nullptr;
  }
  std::string last_error() const override { return this->last_error_; }
  std::string connect_info() const override { return this->info_; }

 protected:
  WiFiClient *tcp_{nullptr};
  TlsSession tls_;
  bool secure_{false};
  std::string last_error_;
  std::string info_;
};

// ---------------------------------------------------------------------------
// Main-task API
// ---------------------------------------------------------------------------

bool NetWorker::start(bool cloud_enabled) {
  this->initial_enabled_ = cloud_enabled;
  TaskHandle_t handle = nullptr;
  BaseType_t ok = xTaskCreate(task_entry_, "hame_net", TASK_STACK_BYTES / sizeof(StackType_t), this,
                              tskIDLE_PRIORITY + 1, &handle);
  this->task_ = handle;
  return ok == pdPASS;
}

void NetWorker::push_cmd_(Cmd &&cmd) {
  LockGuard guard(this->lock_);
  this->cmds_.push_back(std::move(cmd));
}

void NetWorker::set_cloud_enabled(bool enabled) {
  Cmd c{CmdType::ENABLE};
  c.flag = enabled;
  this->push_cmd_(std::move(c));
}
void NetWorker::set_network(bool up) {
  Cmd c{CmdType::NETWORK};
  c.flag = up;
  this->push_cmd_(std::move(c));
}
void NetWorker::set_session(const MqttSession &session) {
  Cmd c{CmdType::SESSION};
  c.session = session;
  this->push_cmd_(std::move(c));
}
void NetWorker::set_credentials(const TlsCredentials &creds) {
  Cmd c{CmdType::CREDENTIALS};
  c.creds = creds;
  this->push_cmd_(std::move(c));
}
void NetWorker::clear_session() { this->push_cmd_(Cmd{CmdType::CLEAR}); }
void NetWorker::subscribe(const std::string &topic) {
  Cmd c{CmdType::SUBSCRIBE};
  c.a = topic;
  this->push_cmd_(std::move(c));
}
void NetWorker::publish(const std::string &topic, const std::string &payload, bool retain) {
  Cmd c{CmdType::PUBLISH};
  c.a = topic;
  c.b = payload;
  c.flag = retain;
  this->push_cmd_(std::move(c));
}
void NetWorker::http(HttpJob job) {
  LockGuard guard(this->lock_);
  this->http_jobs_.push_back(std::move(job));
}

bool NetWorker::pop_message(std::string &topic, std::string &payload) {
  LockGuard guard(this->lock_);
  if (this->messages_.empty())
    return false;
  topic = std::move(this->messages_.front().first);
  payload = std::move(this->messages_.front().second);
  this->messages_.pop_front();
  return true;
}

bool NetWorker::pop_http_result(uint32_t &id, HttpResult &result) {
  LockGuard guard(this->lock_);
  if (this->http_results_.empty())
    return false;
  id = this->http_results_.front().first;
  result = std::move(this->http_results_.front().second);
  this->http_results_.pop_front();
  return true;
}

bool NetWorker::pop_log(std::string &line) {
  LockGuard guard(this->lock_);
  if (this->logs_.empty())
    return false;
  line = std::move(this->logs_.front());
  this->logs_.pop_front();
  return true;
}

std::string NetWorker::cloud_state() {
  LockGuard guard(this->lock_);
  return this->state_str_;
}

uint32_t NetWorker::free_stack_words() {
  if (this->task_ == nullptr)
    return 0;
  return uxTaskGetStackHighWaterMark(static_cast<TaskHandle_t>(this->task_));
}

void NetWorker::log_(const std::string &line) {
  LockGuard guard(this->lock_);
  if (this->logs_.size() >= MAX_LOGS)
    this->logs_.pop_front();
  this->logs_.push_back(line);
}

// ---------------------------------------------------------------------------
// Worker task
// ---------------------------------------------------------------------------

void NetWorker::task_entry_(void *arg) {
  static_cast<NetWorker *>(arg)->run_();
  vTaskDelete(nullptr);
}

void NetWorker::run_() {
  LtTransport transport;
  CloudLink link(&transport);
  link.set_logger([this](const std::string &m) { this->log_(m); });
  link.set_on_message([this](const std::string &topic, const std::string &payload) {
    LockGuard guard(this->lock_);
    if (this->messages_.size() >= MAX_MESSAGES)
      this->messages_.pop_front();
    this->messages_.emplace_back(topic, payload);
  });
  link.set_enabled(this->initial_enabled_, millis());

  for (;;) {
    // Apply commands from the main task.
    std::deque<Cmd> cmds;
    HttpJob job;
    bool have_job = false;
    {
      LockGuard guard(this->lock_);
      cmds.swap(this->cmds_);
      if (!this->http_jobs_.empty()) {
        job = std::move(this->http_jobs_.front());
        this->http_jobs_.pop_front();
        have_job = true;
      }
    }
    uint32_t now = millis();
    for (auto &c : cmds) {
      switch (c.type) {
        case CmdType::ENABLE:
          link.set_enabled(c.flag, now);
          break;
        case CmdType::NETWORK:
          link.set_network(c.flag, now);
          break;
        case CmdType::SESSION:
          link.set_session(c.session, now);
          break;
        case CmdType::CREDENTIALS:
          link.set_credentials(c.creds, now);
          break;
        case CmdType::CLEAR:
          link.clear_session();
          break;
        case CmdType::SUBSCRIBE:
          link.subscribe(c.a);
          break;
        case CmdType::PUBLISH:
          link.publish(c.a, c.b, c.flag);
          break;
      }
    }

    link.service(millis());
    {
      LockGuard guard(this->lock_);
      this->state_str_ = link.state_str();
    }

    if (have_job) {
      HttpResult r = this->do_http_(job);
      LockGuard guard(this->lock_);
      this->http_results_.emplace_back(job.id, std::move(r));
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// Minimal HTTP/1.0 client: one request per connection, body read to EOF.
HttpResult NetWorker::do_http_(const HttpJob &job) {
  HttpResult res;
  std::string url = job.url;
  bool https = false;
  if (url.rfind("http://", 0) == 0) {
    url = url.substr(7);
  } else if (url.rfind("https://", 0) == 0) {
    url = url.substr(8);
    https = true;
  }
  size_t slash = url.find('/');
  std::string hostport = slash == std::string::npos ? url : url.substr(0, slash);
  std::string path = slash == std::string::npos ? "/" : url.substr(slash);
  std::string host = hostport;
  uint16_t port = https ? 443 : 80;
  size_t colon = hostport.find(':');
  if (colon != std::string::npos) {
    host = hostport.substr(0, colon);
    port = static_cast<uint16_t>(atoi(hostport.c_str() + colon + 1));
  }

  WiFiClient *client;
  MbedTLSClient tls;
  WiFiClient tcp;
  if (https) {
    tls.setInsecure();  // the module's own HTTPS client does not verify either (QSSLCFG "verify")
    tls.setHandshakeTimeout(CONNECT_TIMEOUT_MS);
    client = &tls;
  } else {
    client = &tcp;
  }
  if (client->connect(host.c_str(), port, CONNECT_TIMEOUT_MS) != 1) {
    res.error = HTTP_ERR_CONNECT;
    return res;
  }

  std::string req = job.method + " " + path + " HTTP/1.0\r\nHost: " + hostport + "\r\nConnection: close\r\n";
  for (const auto &h : job.headers)
    req += h.first + ": " + h.second + "\r\n";
  if (job.method == "POST") {
    req += "Content-Type: " + (job.content_type.empty() ? std::string("application/x-www-form-urlencoded")
                                                         : job.content_type) +
           "\r\nContent-Length: " + std::to_string(job.body.size()) + "\r\n";
  }
  req += "\r\n";
  client->write(reinterpret_cast<const uint8_t *>(req.data()), req.size());
  if (!job.body.empty())
    client->write(job.body.data(), job.body.size());

  std::string raw;
  uint32_t start = millis();
  uint8_t buf[256];
  while (millis() - start < HTTP_TIMEOUT_MS) {
    int avail = client->available();
    if (avail > 0) {
      int n = client->read(buf, std::min<size_t>(sizeof(buf), avail));
      if (n > 0 && raw.size() < HTTP_MAX_BODY + 1024)
        raw.append(reinterpret_cast<char *>(buf), n);
      continue;
    }
    if (!client->connected())
      break;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  bool timed_out = client->connected();
  client->stop();

  size_t hdr_end = raw.find("\r\n\r\n");
  if (raw.rfind("HTTP/", 0) != 0 || hdr_end == std::string::npos) {
    res.error = timed_out ? HTTP_ERR_TIMEOUT : HTTP_ERR_UNKNOWN;
    return res;
  }
  size_t sp = raw.find(' ');
  res.status = atoi(raw.c_str() + sp + 1);
  std::string headers = raw.substr(0, hdr_end);
  std::string body = raw.substr(hdr_end + 4);
  for (auto &c : headers)
    c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  if (headers.find("transfer-encoding: chunked") != std::string::npos) {
    std::string out;
    size_t pos = 0;
    while (pos < body.size()) {
      size_t eol = body.find("\r\n", pos);
      if (eol == std::string::npos)
        break;
      size_t len = strtoul(body.substr(pos, eol - pos).c_str(), nullptr, 16);
      if (len == 0)
        break;
      out.append(body, eol + 2, len);
      pos = eol + 2 + len + 2;
    }
    body = out;
  }
  if (body.size() > HTTP_MAX_BODY)
    body.resize(HTTP_MAX_BODY);
  res.body = std::move(body);
  return res;
}

}  // namespace esphome::hame_at

#include <cerrno>

#include <lwip/sockets.h>

namespace esphome::hame_at {

static int socket_alive(int fd) {
  uint8_t b;
  errno = 0;
  const int r = lwip_recv(fd, &b, 1, MSG_PEEK | MSG_DONTWAIT);
  if (r > 0)
    return 1;
  if (r == 0)
    return 0;
  const int e = errno;
  return (e == EWOULDBLOCK || e == EAGAIN || e == EINTR || e == 0) ? 1 : -e;
}

}  // namespace esphome::hame_at

#endif  // USE_LIBRETINY
