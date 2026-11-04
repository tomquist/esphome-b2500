// Gives the BLE controller a chosen public address (bk72xx_ble_server
// `mac_address:`).
//
// The BDK's bdaddr_env_init() (ble_5_1/platform/7231n/entry/ble_main.c) sets
// common_default_bdaddr from wifi_get_mac_address(STA) with the last byte
// + 1, right before rwip_init() hands it to the controller. A Quectel FC41D
// has its own BLE address in the config sector, which ESPHome does not read.
// With -Wl,--wrap=wifi_get_mac_address (added by codegen), this wrapper
// answers that one call (recognised by its return address) with the wanted
// address minus 1; every other caller gets the real Wi-Fi MAC.

#include "esphome/core/defines.h"

#if defined(USE_BK72XX_BLE_SERVER) && defined(USE_BK72XX_BLE_SERVER_MAC)

#include <cstdint>
#include <cstring>

extern "C" {

void __real_wifi_get_mac_address(char *mac, uint8_t type);
void bdaddr_env_init(void);

void __wrap_wifi_get_mac_address(char *mac, uint8_t type) {
  __real_wifi_get_mac_address(mac, type);
  // bdaddr_env_init() calls it 10 bytes into its 0x50-byte body.
  const uintptr_t caller = reinterpret_cast<uintptr_t>(__builtin_return_address(0)) & ~static_cast<uintptr_t>(1);
  const uintptr_t fn = reinterpret_cast<uintptr_t>(&bdaddr_env_init) & ~static_cast<uintptr_t>(1);
  if (caller < fn || caller >= fn + 0x50)
    return;
  static const uint8_t wanted[6] = USE_BK72XX_BLE_SERVER_MAC;
  memcpy(mac, wanted, sizeof(wanted));
  mac[5] = static_cast<char>(wanted[5] - 1);  // bdaddr_env_init() adds 1 again
}

}  // extern "C"

#endif
