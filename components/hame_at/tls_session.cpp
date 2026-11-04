#include "tls_session.h"

#ifdef USE_LIBRETINY

#include <Arduino.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <lwip/sockets.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

namespace esphome::hame_at {

struct TlsSession::Impl {
  int fd{-1};
  bool started{false};
  bool have_cert{false};
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config conf;
  mbedtls_x509_crt ca;
  mbedtls_x509_crt cert;
  mbedtls_pk_context key;
};

static int tls_random(void *, unsigned char *out, size_t len) {
  lt_rand_bytes(out, len);
  return 0;
}

static int sock_send(void *ctx, const unsigned char *buf, size_t len) {
  const int fd = *static_cast<int *>(ctx);
  const int r = lwip_send(fd, buf, len, MSG_DONTWAIT);
  if (r >= 0)
    return r;
  return (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR) ? MBEDTLS_ERR_SSL_WANT_WRITE : -0x004E;
}

static int sock_recv(void *ctx, unsigned char *buf, size_t len) {
  const int fd = *static_cast<int *>(ctx);
  const int r = lwip_recv(fd, buf, len, MSG_DONTWAIT);
  if (r > 0)
    return r;
  if (r == 0)
    return MBEDTLS_ERR_SSL_CONN_EOF;
  return (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR) ? MBEDTLS_ERR_SSL_WANT_READ : -0x004C;
}

static std::string code(const char *where, int ret) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%s: -0x%04X", where, static_cast<unsigned>(-ret));
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
  Impl &s = *this->impl_;
  s.fd = fd;
  mbedtls_ssl_init(&s.ssl);
  mbedtls_ssl_config_init(&s.conf);
  mbedtls_x509_crt_init(&s.ca);
  mbedtls_x509_crt_init(&s.cert);
  mbedtls_pk_init(&s.key);
  s.started = true;

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
  mbedtls_ssl_set_bio(&s.ssl, &s.fd, sock_send, sock_recv, nullptr);

  const uint32_t start = millis();
  while ((ret = mbedtls_ssl_handshake(&s.ssl)) != 0) {
    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
      const uint32_t flags_v = mbedtls_ssl_get_verify_result(&s.ssl);
      err = code("handshake", ret);
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
  this->error_.clear();
  return true;
}

int TlsSession::recv_some(uint8_t *buf, size_t len) {
  Impl &s = *this->impl_;
  if (!s.started)
    return -1;
  const int ret = mbedtls_ssl_read(&s.ssl, buf, len);
  if (ret > 0)
    return ret;
  if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
    return 0;
  if (ret == 0 || ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || ret == MBEDTLS_ERR_SSL_CONN_EOF) {
    this->error_ = "closed by the server";
  } else {
    this->error_ = code("read", ret);
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
    const int ret = mbedtls_ssl_write(&s.ssl, data + done, len - done);
    if (ret > 0) {
      done += static_cast<size_t>(ret);
      continue;
    }
    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
      this->error_ = code("write", ret);
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
