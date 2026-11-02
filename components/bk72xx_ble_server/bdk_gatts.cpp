// Every Beken BDK call bk72xx_ble_server makes. Written against beken-bdk
// 3.0.78 (pinned by esphome's bk72xx_ble) and its BLE 5.1 stack (BK7231N),
// modelled on the SDK's own AT server (func/at_server/atsvr_cmd/atsvr_ble.c)
// and the "comm" profile behind bk_ble_create_db()
// (driver/ble/ble_5_1/ble_pub/profiles/comm).

#include "bdk_gatts.h"

#ifdef USE_BK72XX_BLE_SERVER

#if !defined(CLANG_TIDY) && __has_include("ble_api.h") && __has_include("app_ble.h")

#include <cstring>

extern "C" {
#include "app_ble.h"  // app_ble_env_state_get, app_ble_actv_state_get, actv_state_t,
                      // bk_ble_* (via ble_api_5_x.h)
}

#include "esphome/core/log.h"

namespace esphome::bk72xx_ble_server {

static const char *const TAG = "bk72xx_ble_server.bdk";

// BLE 5.1 attribute layout: {uuid[16], perm, ext_perm, max_size}. The AT
// server's own table uses the 5.2 field names, so pin the 5.1 one here.
static_assert(sizeof(bk_attm_desc_t) == 22, "beken-bdk BLE 5.1 attribute descriptor layout changed");

uint16_t bdk_perm_read() { return RD_MASK; }
uint16_t bdk_perm_write_req() { return WRITE_REQ_MASK; }
uint16_t bdk_perm_write_cmd() { return WRITE_COMMAND_MASK; }
uint16_t bdk_perm_notify() { return NTF_MASK; }
uint16_t bdk_perm_indicate() { return IND_MASK; }
// RI: the value is not stored in the stack's DB; reads raise GATTC_READ_REQ_IND,
// which the comm profile turns into a BLE_5_READ_EVENT notice.
uint16_t bdk_ext_perm_read_ind() { return RI_MASK; }

static BdkNoticeHandler s_handler = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static uint8_t s_adv_idx = UNKNOW_ACT_IDX;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Storage for the DB: bk_ble_create_db() copies the cfg struct but keeps the
// att_db pointer, so the descriptors must outlive the call.
static constexpr uint8_t MAX_ATTRS = 32;
static bk_attm_desc_t s_db[MAX_ATTRS];  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Runs in the BDK BLE task.
static void notice_cb(ble_notice_t notice, void *param) {
  if (s_handler == nullptr)
    return;
  switch (notice) {
    case BLE_5_WRITE_EVENT: {
      auto *w = static_cast<write_req_t *>(param);
      s_handler(BdkNotice::WRITE, w->conn_idx, w->att_idx, w->value, w->len, w->prf_id, nullptr, 0);
      break;
    }
    case BLE_5_READ_EVENT: {
      auto *r = static_cast<read_req_t *>(param);
      // The comm profile allocates r->value (r->size bytes) and copies
      // r->length bytes back to the peer after we return.
      r->length = s_handler(BdkNotice::READ, r->conn_idx, r->att_idx, nullptr, 0, r->prf_id, r->value, r->size);
      break;
    }
    case BLE_5_MTU_CHANGE: {
      auto *m = static_cast<mtu_change_t *>(param);
      s_handler(BdkNotice::MTU_CHANGE, m->conn_idx, 0, nullptr, 0, m->mtu_size, nullptr, 0);
      break;
    }
    case BLE_5_CONNECT_EVENT: {
      auto *c = static_cast<conn_ind_t *>(param);
      s_handler(BdkNotice::CONNECT, c->conn_idx, 0, c->peer_addr, GAP_BD_ADDR_LEN, c->peer_addr_type, nullptr, 0);
      break;
    }
    case BLE_5_DISCONNECT_EVENT: {
      auto *d = static_cast<discon_ind_t *>(param);
      s_handler(BdkNotice::DISCONNECT, d->conn_idx, 0, nullptr, 0, d->reason, nullptr, 0);
      break;
    }
    case BLE_5_CREATE_DB: {
      auto *cd = static_cast<create_db_t *>(param);
      s_handler(BdkNotice::CREATE_DB, 0, 0, nullptr, 0, static_cast<uint16_t>(cd->prf_id << 8 | cd->status), nullptr,
                0);
      break;
    }
    case BLE_5_ATT_INFO_REQ: {
      // Asked before a long/prepared write; allow up to the value buffer size.
      auto *a = static_cast<att_info_req_t *>(param);
      a->length = BLE_CHAR_DATA_LEN;
      a->status = ERR_SUCCESS;
      break;
    }
    default:
      break;
  }
}

void bdk_install_notice_handler(BdkNoticeHandler handler) {
  s_handler = handler;
  ble_set_notice_cb(notice_cb);
}

bool bdk_ready() { return app_ble_env_state_get() == APP_BLE_READY; }

bool bdk_create_db(uint8_t prf_id, uint16_t service_uuid16, const BdkAttr *attrs, uint8_t count) {
  if (count > MAX_ATTRS) {
    ESP_LOGE(TAG, "Too many attributes (%u > %u)", count, MAX_ATTRS);
    return false;
  }
  memset(s_db, 0, sizeof(s_db));
  for (uint8_t i = 0; i < count; i++) {
    s_db[i].uuid[0] = attrs[i].uuid16 & 0xFF;
    s_db[i].uuid[1] = attrs[i].uuid16 >> 8;
    s_db[i].perm = attrs[i].perm;
    s_db[i].ext_perm = attrs[i].ext_perm;
    s_db[i].max_size = attrs[i].max_size;
  }
  struct bk_ble_db_cfg cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.prf_task_id = prf_id;
  cfg.uuid[0] = service_uuid16 & 0xFF;
  cfg.uuid[1] = service_uuid16 >> 8;
  cfg.att_db_nb = count;
  cfg.start_hdl = 0;  // allocate dynamically
  cfg.att_db = s_db;
  cfg.svc_perm = BK_PERM_SET(SVC_UUID_LEN, UUID_16);
  ble_err_t ret = bk_ble_create_db(&cfg);
  if (ret != ERR_SUCCESS) {
    ESP_LOGV(TAG, "create_db rejected (err %d)", static_cast<int>(ret));
    return false;
  }
  return true;
}

bool bdk_adv_start(const uint8_t *adv, uint8_t adv_len, const uint8_t *rsp, uint8_t rsp_len, uint16_t interval) {
  if (!bdk_ready())
    return false;
  // An activity that was created before survives a connection: restart it.
  if (s_adv_idx != UNKNOW_ACT_IDX) {
    switch (app_ble_actv_state_get(s_adv_idx)) {
      case ACTV_ADV_STARTED:
        return true;
      case ACTV_ADV_CREATED:
        return bk_ble_start_advertising(s_adv_idx, 0, nullptr) == ERR_SUCCESS;
      default:
        break;
    }
  }
  struct adv_param p;
  memset(&p, 0, sizeof(p));
  if (adv_len > MAX_ADV_DATA_LEN)
    adv_len = MAX_ADV_DATA_LEN;
  if (rsp_len > MAX_ADV_DATA_LEN)
    rsp_len = MAX_ADV_DATA_LEN;
  memcpy(p.advData, adv, adv_len);
  p.advDataLen = adv_len;
  memcpy(p.respData, rsp, rsp_len);
  p.respDataLen = rsp_len;
  p.channel_map = 0x07;
  p.interval_min = interval;
  p.interval_max = interval;
  p.duration = 0;
  p.prop = (1 << ADV_PROP_CONNECTABLE_POS) | (1 << ADV_PROP_SCANNABLE_POS);
  uint8_t idx = app_ble_get_idle_actv_idx_handle(ADV_ACTV);
  if (idx == UNKNOW_ACT_IDX) {
    ESP_LOGW(TAG, "No idle advertising activity");
    return false;
  }
  ble_err_t ret = bk_ble_adv_start(idx, &p, nullptr);
  if (ret != ERR_SUCCESS) {
    ESP_LOGV(TAG, "adv_start rejected (err %d)", static_cast<int>(ret));
    return false;
  }
  s_adv_idx = idx;
  return true;
}

void bdk_adv_stop() {
  if (s_adv_idx != UNKNOW_ACT_IDX && app_ble_actv_state_get(s_adv_idx) == ACTV_ADV_STARTED)
    bk_ble_stop_advertising(s_adv_idx, nullptr);
}

void bdk_adv_delete() {
  if (s_adv_idx == UNKNOW_ACT_IDX)
    return;
  switch (app_ble_actv_state_get(s_adv_idx)) {
    case ACTV_ADV_STARTED:
      bk_ble_adv_stop(s_adv_idx, nullptr);  // stop + delete
      break;
    case ACTV_ADV_CREATED:
      bk_ble_delete_advertising(s_adv_idx, nullptr);
      break;
    default:
      break;
  }
  s_adv_idx = UNKNOW_ACT_IDX;
}

bool bdk_notify(uint8_t conn_idx, uint8_t prf_id, uint16_t att_idx, const uint8_t *data, uint16_t len) {
  ble_err_t ret = bk_ble_conidx_send_ntf(conn_idx, len, const_cast<uint8_t *>(data), prf_id, att_idx);
  return ret == ERR_SUCCESS;
}

void bdk_disconnect(uint8_t conn_idx) { bk_ble_disconnect(conn_idx); }

void bdk_local_addr(uint8_t out[6]) { bk_ble_local_pub_addr(out); }

}  // namespace esphome::bk72xx_ble_server

#else  // no BLE 5.x SDK (clang-tidy, or a non-5.x chip)

#error "bk72xx_ble_server requires the Beken BLE 5.x SDK (BK7231N)"

#endif
#endif  // USE_BK72XX_BLE_SERVER
