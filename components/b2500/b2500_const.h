#pragma once
#include "esphome/core/helpers.h"

static const char *const TAG = "b2500";

enum B2500Command : uint8_t {
  CMD_SET_REGION = 0x02,
  CMD_RUNTIME_INFO = 0x03,
  CMD_DEVICE_INFO = 0x04,
  CMD_SET_WIFI = 0x05,
  CMD_WIFI_STATE = 0x08,
  // Firmware Version > 133
  CMD_WIFI_INFO = 0x09,
  CMD_SET_DOD = 0x0B,
  CMD_SET_DISCHARGE_THRESHOLD = 0x0C,
  CMD_LOAD_FIRST_ENABLED = 0x0D,
  CMD_POWER_OUT = 0x0E,
  CMD_CELL_INFO = 0x0F,
  CMD_SET_MQTT = 0x20,
  // Firmware Version > 133
  CMD_FC41D_INFO = 0x23,
  CMD_REBOOT = 0x25,
  CMD_FACTORY_RESET = 0x26,
  CMD_ENABLE_ADAPTIVE_MODE = 0x11,
  CMD_SET_TIMERS = 0x12,
  CMD_GET_TIMERS = 0x13,
  CMD_SET_DATETIME = 0x14,

  CMD_UNKNOWN = 0xFF,
};

enum DeviceScene : uint8_t { SCENE_DAY = 0x00, SCENE_NIGHT = 0x01, SCENE_DUSK_DAWN = 0x02, SCENE_UNKNOWN = 0xFF };

enum DeviceRegion : uint8_t {
  REGION_EU = 0x00,
  REGION_CHINA = 0x01,
  REGION_NON_EU = 0x02,
  REGION_NOT_SET = 0xFF,
  REGION_UNKNOWN = 0xFE
};
