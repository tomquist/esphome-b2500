#pragma once

// Network worker for hame_at on LibreTiny: one FreeRTOS task that owns every
// blocking socket operation, so TLS handshakes and HTTP requests never stall
// the ESPHome main loop or the MCU's UART.
//
//   main task  --commands-->  worker task (CloudLink + HTTP client)
//   main task  <--results---  worker task
//
// Both directions are mutex-protected queues; nothing else is shared.

#include "esphome/core/defines.h"

#ifdef USE_LIBRETINY

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "esphome/core/helpers.h"

#include "cloud_link.h"
#include "fc41d_emulator.h"

namespace esphome::hame_at {

struct HttpJob {
  uint32_t id{0};
  std::string method;
  std::string url;
  std::string content_type;
  std::vector<uint8_t> body;
};

class NetWorker {
 public:
  /// Start the task. `cloud_enabled` is the initial forwarding state.
  bool start(bool cloud_enabled);

  // --- Main task -> worker (thread-safe) ---
  void set_cloud_enabled(bool enabled);
  void set_network(bool up);
  void set_session(const MqttSession &session);
  void set_credentials(const TlsCredentials &creds);
  void clear_session();
  void subscribe(const std::string &topic);
  void publish(const std::string &topic, const std::string &payload);
  void http(HttpJob job);

  // --- Worker -> main task (thread-safe) ---
  bool pop_message(std::string &topic, std::string &payload);
  bool pop_http_result(uint32_t &id, HttpResult &result);
  /// Drain log lines produced in the worker (logged from the main task).
  bool pop_log(std::string &line);
  std::string cloud_state();
  uint32_t free_stack_words();

 protected:
  enum class CmdType : uint8_t { ENABLE, NETWORK, SESSION, CREDENTIALS, CLEAR, SUBSCRIBE, PUBLISH };
  struct Cmd {
    CmdType type;
    bool flag{false};
    MqttSession session;
    TlsCredentials creds;
    std::string a;
    std::string b;
  };

  static void task_entry_(void *arg);
  void run_();
  void push_cmd_(Cmd &&cmd);
  HttpResult do_http_(const HttpJob &job);
  void log_(const std::string &line);

  Mutex lock_;
  std::deque<Cmd> cmds_;
  std::deque<HttpJob> http_jobs_;
  std::deque<std::pair<std::string, std::string>> messages_;
  std::deque<std::pair<uint32_t, HttpResult>> http_results_;
  std::deque<std::string> logs_;
  std::string state_str_{"starting"};
  void *task_{nullptr};
  bool initial_enabled_{true};
};

}  // namespace esphome::hame_at

#endif  // USE_LIBRETINY
