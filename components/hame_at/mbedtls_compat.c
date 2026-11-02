// LibreTiny's MbedTLSClient calls mbedtls_net_set_nonblock(), but beken-bdk
// 3.0.78 (the release esphome's bk72xx_ble pins) has it commented out in its
// mbedTLS port (func/mbedtls/mbedtls-port/src/tls_net.c), so TLS clients fail
// to link. Provide it, mirroring the port's own mbedtls_net_set_block(). Weak,
// so an SDK that ships it again takes precedence.

#include <lwip/sockets.h>
#include <mbedtls/net_sockets.h>

__attribute__((weak)) int mbedtls_net_set_nonblock(mbedtls_net_context *ctx) {
  return lwip_fcntl(ctx->fd, F_SETFL, lwip_fcntl(ctx->fd, F_GETFL, 0) | O_NONBLOCK);
}
