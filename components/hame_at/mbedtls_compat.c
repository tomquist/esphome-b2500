// LibreTiny's MbedTLSClient calls mbedtls_net_set_nonblock(), but nothing in
// the Beken build defines it: the BDK's mbedTLS port has it commented out
// (func/mbedtls/mbedtls-port/src/tls_net.c, in 3.0.33 and 3.0.78 alike) and
// MBEDTLS_NET_C is off, so WiFiClientSecure fails to link on BK72xx. Realtek
// AmebaZ ships it as a fixup (cores/realtek-ambz/base/fixups/net_sockets.c);
// Beken does not (LibreTiny v1.14.0). Provide it, mirroring the port's own
// mbedtls_net_set_block(). Weak, so an upstream fix takes precedence.

#include <lwip/sockets.h>
#include <mbedtls/net_sockets.h>

__attribute__((weak)) int mbedtls_net_set_nonblock(mbedtls_net_context *ctx) {
  return lwip_fcntl(ctx->fd, F_SETFL, lwip_fcntl(ctx->fd, F_GETFL, 0) | O_NONBLOCK);
}
