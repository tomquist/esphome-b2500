#pragma once

// TLS over an already connected lwIP socket, straight on the SDK's mbedTLS
// (LibreTiny's MbedTLSClient has a stub lastError(), a connected() that
// reads a stale errno, and on the eval kit the broker never got the first
// record written after the handshake). Method names avoid read/write/close:
// lwip/sockets.h defines those as macros.

#ifdef USE_LIBRETINY

#include <cstddef>
#include <cstdint>
#include <string>

namespace esphome::hame_at {

class TlsSession {
 public:
  TlsSession();
  ~TlsSession();
  TlsSession(const TlsSession &) = delete;
  TlsSession &operator=(const TlsSession &) = delete;

  /// Handshake on `fd` (a connected TCP socket, made non-blocking here).
  /// `ca` is required (PEM); `cert`/`key` may be empty. On failure `err`
  /// says where it failed, with the mbedTLS code.
  bool start(int fd, const std::string &host, const std::string &ca, const std::string &cert,
             const std::string &key, uint32_t timeout_ms, std::string &err);
  /// >0 bytes read, 0 nothing yet, -1 closed or failed (see last_error()).
  int recv_some(uint8_t *buf, size_t len);
  /// Writes everything (waits while the socket is full, up to timeout_ms).
  bool send_all(const uint8_t *data, size_t len, uint32_t timeout_ms);
  void finish();
  const std::string &last_error() const { return this->error_; }

  /// Memory seen during the last start(): free heap after each mbedTLS
  /// allocation, lowest value, and the largest single allocation.
  struct HandshakeStats {
    uint32_t ms{0};
    uint32_t lowest_free{0};
    uint32_t largest_alloc{0};
    uint32_t allocs{0};
  };
  const HandshakeStats &handshake_stats() const { return this->stats_; }

 protected:
  void trim_after_handshake_();
  void shrink_buffers_();
  struct Impl;
  Impl *impl_;
  std::string error_;
  HandshakeStats stats_;
};

}  // namespace esphome::hame_at

#endif  // USE_LIBRETINY
