#pragma once

// Thin wrapper around the Beken BDK (BLE 5.1, BK7231N) GATT-server surface.
// Every SDK call bk72xx_ble_server makes lives in bdk_gatts.cpp, so the
// component itself stays SDK-free (same split as esphome's bk72xx_ble).

#include "esphome/core/defines.h"

#ifdef USE_BK72XX_BLE_SERVER

#include <cstddef>
#include <cstdint>

namespace esphome::bk72xx_ble_server {

/// Raw notice ids the component cares about (mirrors ble_notice_t).
enum class BdkNotice : uint8_t {
  OTHER,
  WRITE,
  READ,
  MTU_CHANGE,
  CONNECT,
  DISCONNECT,
  CREATE_DB,
  ATT_INFO_REQ,
};

/// One attribute of the service database in SDK-independent form.
struct BdkAttr {
  uint16_t uuid16;
  uint16_t perm;      // BDK attm perm bit field (see bdk_gatts.cpp helpers)
  uint16_t ext_perm;  // BDK attm extended perm bit field
  uint16_t max_size;
};

/// Notice callback signature delivered from the BDK BLE task.
/// For READ, the handler must fill `read_buf` (capacity `read_cap`) and
/// return the number of bytes written. For every other notice the return
/// value is ignored.
using BdkNoticeHandler = uint16_t (*)(BdkNotice notice, uint8_t conn_idx, uint16_t att_idx, const uint8_t *data,
                                      uint16_t len, uint16_t value16, uint8_t *read_buf, uint16_t read_cap);

// Attribute permission builders (BLE 5.1 attm layout).
uint16_t bdk_perm_read();
uint16_t bdk_perm_write_req();
uint16_t bdk_perm_write_cmd();
uint16_t bdk_perm_notify();
uint16_t bdk_perm_indicate();
/// Extended perm for a value attribute whose reads are answered by the app
/// (read-indication) rather than from the stack's own DB copy.
uint16_t bdk_ext_perm_read_ind();

static constexpr uint16_t UUID_PRIMARY_SERVICE = 0x2800;
static constexpr uint16_t UUID_CHARACTERISTIC = 0x2803;
static constexpr uint16_t UUID_CCCD = 0x2902;

/// Install the notice callback (replaces whatever bk72xx_ble registered).
void bdk_install_notice_handler(BdkNoticeHandler handler);
/// True once the BDK app task accepts commands (APP_BLE_READY).
bool bdk_ready();
/// Queue creation of the service database. `attrs` must outlive the call
/// (the SDK keeps the pointer until BLE_5_CREATE_DB). Returns false if the
/// stack rejected it (retry later).
bool bdk_create_db(uint8_t prf_id, uint16_t service_uuid16, const BdkAttr *attrs, uint8_t count);
/// Start connectable+scannable legacy advertising with the given payloads.
/// Returns false if the stack is busy (retry later).
bool bdk_adv_start(const uint8_t *adv, uint8_t adv_len, const uint8_t *rsp, uint8_t rsp_len, uint16_t interval);
/// Stop advertising, keeping the activity for a later bdk_adv_start().
void bdk_adv_stop();
/// Drop the advertising activity so the next bdk_adv_start() creates a new one
/// with fresh payloads (a restarted activity keeps its old data).
void bdk_adv_delete();
/// Send a notification on the value attribute `att_idx` of profile `prf_id`.
bool bdk_notify(uint8_t conn_idx, uint8_t prf_id, uint16_t att_idx, const uint8_t *data, uint16_t len);
/// Disconnect a central.
void bdk_disconnect(uint8_t conn_idx);
/// Controller public address, LSB first.
void bdk_local_addr(uint8_t out[6]);

}  // namespace esphome::bk72xx_ble_server

#endif  // USE_BK72XX_BLE_SERVER
