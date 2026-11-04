#include "tls_session.h"

#ifdef USE_LIBRETINY

#include <Arduino.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>

#include <lwip/sockets.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/pk.h>
#include <mbedtls/platform.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_internal.h>
#include <mbedtls/x509_crt.h>

namespace esphome::hame_at {

struct TlsSession::Impl {
  int fd{-1};
  int sock_errno{0};  ///< errno of the last failed send/recv
  bool started{false};
  bool have_cert{false};
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config conf;
  mbedtls_x509_crt ca;
  mbedtls_x509_crt cert;
  mbedtls_pk_context key;
};

// Randomness. Not lt_rand_bytes(): it is rand() (predictable keys), and it
// stores 32-bit words through an int* cast, which on this ARMv5TE core lands
// on the aligned-down address when `out` is unaligned. mbedTLS asks for the
// RSA premaster's 46 random bytes at an unaligned address right after the 2
// version bytes, so those got overwritten; the server then rejected our
// Finished with bad_record_mac whenever it chose RSA key exchange.
// Instead: mbedTLS's CTR-DRBG, seeded from the BK7231N's hardware TRNG
// (registers of the SDK's irda driver; not on the original BK7231).
static constexpr uintptr_t TRNG_CTRL = 0x00802480;
static constexpr uintptr_t TRNG_DATA = 0x00802484;
static constexpr uint32_t TRNG_EN = 1;

static uint32_t trng_word() {
  volatile uint32_t *ctrl = reinterpret_cast<volatile uint32_t *>(TRNG_CTRL);
  volatile uint32_t *data = reinterpret_cast<volatile uint32_t *>(TRNG_DATA);
  *ctrl = *ctrl | TRNG_EN;
  const uint32_t v = *data;
  *ctrl = *ctrl & ~TRNG_EN;
  return v;
}

static int trng_entropy(void *, unsigned char *out, size_t len) {
  while (len > 0) {
    const uint32_t w = trng_word();
    const size_t n = len < sizeof(w) ? len : sizeof(w);
    memcpy(out, &w, n);  // byte-wise: `out` may be unaligned
    out += n;
    len -= n;
  }
  return 0;
}

static mbedtls_ctr_drbg_context g_drbg;
static bool g_drbg_ready = false;

static bool drbg_seed() {
  if (g_drbg_ready)
    return true;
  mbedtls_ctr_drbg_init(&g_drbg);
  static const unsigned char PERS[] = "hame_at tls";
  if (mbedtls_ctr_drbg_seed(&g_drbg, trng_entropy, nullptr, PERS, sizeof(PERS) - 1) != 0) {
    mbedtls_ctr_drbg_free(&g_drbg);
    return false;
  }
  g_drbg_ready = true;
  return true;
}

// Only the net worker task runs TLS sessions, so the DRBG needs no lock.
static int tls_random(void *, unsigned char *out, size_t len) { return mbedtls_ctr_drbg_random(&g_drbg, out, len); }

static constexpr int ERR_NET_SEND_FAILED = -0x004E;
static constexpr int ERR_NET_RECV_FAILED = -0x004C;

static int sock_send(void *ctx, const unsigned char *buf, size_t len) {
  auto *s = static_cast<TlsSession::Impl *>(ctx);
  const int r = lwip_send(s->fd, buf, len, MSG_DONTWAIT);
  if (r >= 0)
    return r;
  const int e = errno;
  if (e == EWOULDBLOCK || e == EAGAIN || e == EINTR)
    return MBEDTLS_ERR_SSL_WANT_WRITE;
  s->sock_errno = e;
  return ERR_NET_SEND_FAILED;
}

static int sock_recv(void *ctx, unsigned char *buf, size_t len) {
  auto *s = static_cast<TlsSession::Impl *>(ctx);
  const int r = lwip_recv(s->fd, buf, len, MSG_DONTWAIT);
  if (r > 0)
    return r;
  if (r == 0)
    return MBEDTLS_ERR_SSL_CONN_EOF;
  const int e = errno;
  if (e == EWOULDBLOCK || e == EAGAIN || e == EINTR)
    return MBEDTLS_ERR_SSL_WANT_READ;
  s->sock_errno = e;
  return ERR_NET_RECV_FAILED;
}

// The BDK's mbedTLS sizes its record buffers on demand (Beken's
// MBEDTLS_SSL_DYNAMIC_CONTENT_LEN) in steps of 512/4096/8192/... and never
// shrinks them. Reads always ask for 4096 (it adds 1 KB of slack), but writes
// of at most SEND_CHUNK keep the send buffer at 512.
static constexpr size_t IN_BUF_KEEP = 4096;
static constexpr size_t OUT_BUF_KEEP = 512;
static constexpr size_t SEND_CHUNK = 480;  // + 5 header bytes must fit OUT_BUF_KEEP

// Pass-through to the SDK allocator that notes how low the heap gets while
// mbedTLS allocates (set only during start(); the net worker is the only
// mbedTLS user, so plain counters will do).
extern "C" void *tls_mbedtls_mem_calloc(size_t n, size_t size);
extern "C" void tls_mbedtls_mem_free(void *ptr);
static TlsSession::HandshakeStats *g_stats = nullptr;

static void *counting_calloc(size_t n, size_t size) {
  void *p = tls_mbedtls_mem_calloc(n, size);
  if (g_stats != nullptr && p != nullptr) {
    const uint32_t free_now = lt_heap_get_free();
    if (free_now < g_stats->lowest_free)
      g_stats->lowest_free = free_now;
    if (n * size > g_stats->largest_alloc)
      g_stats->largest_alloc = n * size;
    g_stats->allocs++;
  }
  return p;
}

namespace {
struct CountAllocs {
  explicit CountAllocs(TlsSession::HandshakeStats *s) {
    *s = TlsSession::HandshakeStats{};
    s->lowest_free = lt_heap_get_free();
    g_stats = s;
    mbedtls_platform_set_calloc_free(counting_calloc, tls_mbedtls_mem_free);
  }
  ~CountAllocs() {
    mbedtls_platform_set_calloc_free(tls_mbedtls_mem_calloc, tls_mbedtls_mem_free);
    g_stats = nullptr;
  }
};
}  // namespace

// Offered cipher suites: ECDHE first, AES-128 before AES-256. Short, so the
// ClientHello stays far below 512 bytes (see the wrapper below); suites this
// SDK build lacks are skipped by mbedTLS.
static const int CIPHERSUITES[] = {
    MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256, MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
    MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,    MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
    MBEDTLS_TLS_RSA_WITH_AES_128_GCM_SHA256,       MBEDTLS_TLS_RSA_WITH_AES_128_CBC_SHA256,
    MBEDTLS_TLS_RSA_WITH_AES_128_CBC_SHA,          MBEDTLS_TLS_RSA_WITH_AES_256_CBC_SHA256,
    MBEDTLS_TLS_RSA_WITH_AES_256_CBC_SHA,          0,
};

#if defined(HAME_AT_WRAP_CONFIRM_CONTENT_LEN)
#if !defined(MBEDTLS_SSL_DYNAMIC_CONTENT_LEN)
#error "HAME_AT_WRAP_CONFIRM_CONTENT_LEN needs the BDK's MBEDTLS_SSL_DYNAMIC_CONTENT_LEN"
#endif
// ssl_write_client_hello() first makes room for 512 + 1024 bytes, which the
// SDK rounds up to a 4 KB send buffer, kept through the whole handshake (the
// peak) for a message of ~150 bytes plus the host name. The SNI and session
// ticket extensions confirm their real size themselves, and with the short
// suite list above the rest fits 512 bytes. Only for our sessions (our RNG);
// other mbedTLS users (LibreTiny's MbedTLSClient) are left alone.
extern "C" int __real_mbedtls_ssl_confirm_content_len(mbedtls_ssl_context *ssl, int in_record, size_t content_len);
extern "C" int __wrap_mbedtls_ssl_confirm_content_len(mbedtls_ssl_context *ssl, int in_record, size_t content_len) {
  if (!in_record && ssl->state == MBEDTLS_SSL_CLIENT_HELLO && content_len == MBEDTLS_SSL_BUFFER_MIN + 1024 &&
      ssl->conf != nullptr && ssl->conf->f_rng == tls_random && ssl->conf->ciphersuite_list[0] == CIPHERSUITES)
    content_len = MBEDTLS_SSL_BUFFER_MIN;
  return __real_mbedtls_ssl_confirm_content_len(ssl, in_record, content_len);
}
#endif

static std::string code(const char *where, int ret, int sock_errno = 0) {
  char buf[80];
  if (ret == MBEDTLS_ERR_SSL_ALLOC_FAILED) {
    snprintf(buf, sizeof(buf), "%s: out of memory (-0x%04X)", where, static_cast<unsigned>(-ret));
  } else if ((ret == ERR_NET_SEND_FAILED || ret == ERR_NET_RECV_FAILED) && sock_errno != 0) {
    snprintf(buf, sizeof(buf), "%s: socket %s failed, errno %d%s", where, ret == ERR_NET_SEND_FAILED ? "send" : "recv",
             sock_errno, sock_errno == ENOMEM ? " (out of memory)" : "");
  } else {
    snprintf(buf, sizeof(buf), "%s: -0x%04X", where, static_cast<unsigned>(-ret));
  }
  return buf;
}

TlsSession::TlsSession() : impl_(new Impl()) {}

TlsSession::~TlsSession() {
  this->finish();
  delete this->impl_;
}

bool TlsSession::start(int fd, const std::string &host, const std::string &ca, const std::string &cert,
                       const std::string &key, uint32_t timeout_ms, std::string &err) {
  this->finish();
  const uint32_t t0 = millis();
  CountAllocs counting(&this->stats_);
  struct Elapsed {
    TlsSession::HandshakeStats &st;
    uint32_t t0;
    ~Elapsed() { st.ms = millis() - t0; }
  } elapsed{this->stats_, t0};
  Impl &s = *this->impl_;
  s.fd = fd;
  s.sock_errno = 0;
  mbedtls_ssl_init(&s.ssl);
  mbedtls_ssl_config_init(&s.conf);
  mbedtls_x509_crt_init(&s.ca);
  mbedtls_x509_crt_init(&s.cert);
  mbedtls_pk_init(&s.key);
  s.started = true;

  if (!drbg_seed()) {
    err = "random number generator: seeding failed";
    return false;
  }
  int ret = mbedtls_ssl_config_defaults(&s.conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                        MBEDTLS_SSL_PRESET_DEFAULT);
  if (ret != 0) {
    err = code("config", ret);
    return false;
  }
  ret = mbedtls_x509_crt_parse(&s.ca, reinterpret_cast<const unsigned char *>(ca.c_str()), ca.size() + 1);
  if (ret < 0) {
    err = code("CA certificate", ret);
    return false;
  }
  mbedtls_ssl_conf_ca_chain(&s.conf, &s.ca, nullptr);
  mbedtls_ssl_conf_authmode(&s.conf, MBEDTLS_SSL_VERIFY_REQUIRED);
  if (!cert.empty() && !key.empty()) {
    ret = mbedtls_x509_crt_parse(&s.cert, reinterpret_cast<const unsigned char *>(cert.c_str()), cert.size() + 1);
    if (ret < 0) {
      err = code("client certificate", ret);
      return false;
    }
    ret = mbedtls_pk_parse_key(&s.key, reinterpret_cast<const unsigned char *>(key.c_str()), key.size() + 1,
                               nullptr, 0);
    if (ret < 0) {
      err = code("client key", ret);
      return false;
    }
    ret = mbedtls_ssl_conf_own_cert(&s.conf, &s.cert, &s.key);
    if (ret != 0) {
      err = code("client certificate", ret);
      return false;
    }
    s.have_cert = true;
  }
  mbedtls_ssl_conf_rng(&s.conf, tls_random, nullptr);
  // After a handshake the server aborted, try the SDK's default suite list
  // (and its usual ClientHello) once, to tell the two apart in the log.
  this->stats_.default_suites = this->fallback_next_;
  if (!this->fallback_next_)
    mbedtls_ssl_conf_ciphersuites(&s.conf, CIPHERSUITES);
  this->fallback_next_ = false;
#if defined(MBEDTLS_ECP_C)
  // Offer only P-256 for ECDHE: servers would otherwise pick P-384, which
  // takes longer and more memory on this chip.
  static const mbedtls_ecp_group_id CURVES[] = {MBEDTLS_ECP_DP_SECP256R1, MBEDTLS_ECP_DP_NONE};
  mbedtls_ssl_conf_curves(&s.conf, CURVES);
#endif
  ret = mbedtls_ssl_setup(&s.ssl, &s.conf);
  if (ret != 0) {
    err = code("setup", ret);
    return false;
  }
  ret = mbedtls_ssl_set_hostname(&s.ssl, host.c_str());
  if (ret != 0) {
    err = code("hostname", ret);
    return false;
  }
  int flags = lwip_fcntl(fd, F_GETFL, 0);
  lwip_fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  mbedtls_ssl_set_bio(&s.ssl, &s, sock_send, sock_recv, nullptr);

  const uint32_t start = millis();
  while ((ret = mbedtls_ssl_handshake(&s.ssl)) != 0) {
    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
      const uint32_t flags_v = mbedtls_ssl_get_verify_result(&s.ssl);
      err = code("handshake", ret, s.sock_errno);
      if (ret == MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE && s.ssl.in_msg != nullptr) {
        char alert[32];
        snprintf(alert, sizeof(alert), " (server sent alert %u)", static_cast<unsigned>(s.ssl.in_msg[1]));
        err += alert;
        this->fallback_next_ = !this->stats_.default_suites;
      }
      char where[96];
      snprintf(where, sizeof(where), " at step %d, %s, %s suites", s.ssl.state, this->suite_name_(),
               this->stats_.default_suites ? "SDK default" : "our");
      err += where;
      if (flags_v != 0 && flags_v != static_cast<uint32_t>(-1)) {
        char extra[32];
        snprintf(extra, sizeof(extra), ", verify flags 0x%X", static_cast<unsigned>(flags_v));
        err += extra;
      }
      return false;
    }
    if (millis() - start > timeout_ms) {
      err = "handshake: timeout";
      return false;
    }
    delay(5);
  }
  this->stats_.suite = this->suite_name_();
  this->trim_after_handshake_();
  this->error_.clear();
  return true;
}

const char *TlsSession::suite_name_() const {
  const mbedtls_ssl_context &ssl = this->impl_->ssl;
  int id = 0;
  if (ssl.session_negotiate != nullptr)
    id = ssl.session_negotiate->ciphersuite;
  else if (ssl.session != nullptr)
    id = ssl.session->ciphersuite;
  const char *name = id != 0 ? mbedtls_ssl_get_ciphersuite_name(id) : nullptr;
  return name != nullptr ? name : "no suite yet";
}

void TlsSession::trim_after_handshake_() {
  Impl &s = *this->impl_;
  // Only the handshake needs the certificates; no renegotiation follows.
#if defined(MBEDTLS_X509_CRT_PARSE_C)
  if (s.ssl.session != nullptr && s.ssl.session->peer_cert != nullptr) {
    mbedtls_x509_crt_free(s.ssl.session->peer_cert);
    mbedtls_free(s.ssl.session->peer_cert);
    s.ssl.session->peer_cert = nullptr;
  }
#endif
  mbedtls_ssl_conf_ca_chain(&s.conf, nullptr, nullptr);
  mbedtls_x509_crt_free(&s.ca);
  this->shrink_buffers_();
}

void TlsSession::shrink_buffers_() {
#if defined(MBEDTLS_SSL_DYNAMIC_CONTENT_LEN)
  Impl &s = *this->impl_;
  // Smaller copies of idle buffers (the copy keeps the first bytes and the
  // record pointers). On failure the old buffer stays.
  if (s.ssl.in_max_content_len > IN_BUF_KEEP && s.ssl.in_left == 0 && mbedtls_ssl_get_bytes_avail(&s.ssl) == 0)
    mbedtls_ssl_alloc_record_buf(&s.ssl, 1, IN_BUF_KEEP);
  if (s.ssl.out_max_content_len > OUT_BUF_KEEP && s.ssl.out_left == 0)
    mbedtls_ssl_alloc_record_buf(&s.ssl, 0, OUT_BUF_KEEP);
#endif
}

int TlsSession::recv_some(uint8_t *buf, size_t len) {
  Impl &s = *this->impl_;
  if (!s.started)
    return -1;
  const int ret = mbedtls_ssl_read(&s.ssl, buf, len);
  if (ret > 0) {
    this->shrink_buffers_();  // after a large record from the server
    return ret;
  }
  if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
    return 0;
  if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
    this->error_ = "server ended TLS (close_notify)";
  } else if (ret == 0 || ret == MBEDTLS_ERR_SSL_CONN_EOF) {
    this->error_ = "server closed TCP without close_notify";
  } else if (ret == MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE && s.ssl.in_msg != nullptr) {
    // e.g. 20 bad_record_mac (it could not decrypt us), 40 handshake_failure
    char buf[48];
    snprintf(buf, sizeof(buf), "server sent TLS alert %u", static_cast<unsigned>(s.ssl.in_msg[1]));
    this->error_ = buf;
  } else {
    this->error_ = code("read", ret, s.sock_errno);
  }
  return -1;
}

bool TlsSession::send_all(const uint8_t *data, size_t len, uint32_t timeout_ms) {
  Impl &s = *this->impl_;
  if (!s.started)
    return false;
  const uint32_t start = millis();
  size_t done = 0;
  while (done < len) {
    const int ret = mbedtls_ssl_write(&s.ssl, data + done, std::min(len - done, SEND_CHUNK));
    if (ret > 0) {
      done += static_cast<size_t>(ret);
      continue;
    }
    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
      this->error_ = code("write", ret, s.sock_errno);
      return false;
    }
    if (millis() - start > timeout_ms) {
      this->error_ = "write: timeout";
      return false;
    }
    delay(2);
  }
  return true;
}

void TlsSession::finish() {
  Impl &s = *this->impl_;
  if (!s.started)
    return;
  if (s.ssl.state == MBEDTLS_SSL_HANDSHAKE_OVER)
    mbedtls_ssl_close_notify(&s.ssl);
  mbedtls_ssl_free(&s.ssl);
  mbedtls_ssl_config_free(&s.conf);
  mbedtls_x509_crt_free(&s.ca);
  mbedtls_x509_crt_free(&s.cert);
  mbedtls_pk_free(&s.key);
  s.started = false;
  s.have_cert = false;
  s.fd = -1;
}

}  // namespace esphome::hame_at

#endif  // USE_LIBRETINY
