// src/types.ts
export interface MQTTSettings {
  topic: string;
  broker: string;
  port: number;
  username: string;
  password: string;
  discovery: boolean;
}

export interface WifiSettings {
  ssid: string;
  password: string;
}

export interface SetWifiSettings {
  ssid: string;
  password: string;
}

export interface Storage {
  name: string;
  version: number;
  mac_address: string;
}

export interface ManualIP {
  ip: string;
  gateway: string;
  subnet: string;
  dns: string;
}

export interface PowerzeroSettings {
  grid_power_topic: string;
  limit_cmd_topic: string;
  limit_state_topic: string;
}

export interface PowermeterSettings {
  tx_pin: string;
  rx_pin: string;
  baud_rate: number;
  stop_bits: 1 | 2;
}

export interface WebServerSettings {
  port: number;
  js_include: string;
}

export interface FallbackHotspotSettings {
  ssid: string;
  enable_captive_portal: boolean;
}

export interface OtaSettings {
  password: string;
  enable_unprotected_writes: boolean;
}

export interface AutoRestartSettings {
  restart_after_error_count: number;
}

export const validPlatformVariants = [
  'auto',
  'esp32',
  'esp32s2',
  'esp32s3',
  'esp32c3',
  'esp32h2',
] as const;
export type PlatformVariant = (typeof validPlatformVariants)[number];

export interface FormValues {
  name: string;
  friendly_name: string;
  poll_interval_seconds: number;
  mqtt: MQTTSettings;
  wifi: WifiSettings;
  board: string;
  variant: PlatformVariant;
  idf_platform_version: string;
  enable_auto_restart: boolean;
  auto_restart: AutoRestartSettings;
  enable_powerzero: boolean;
  powerzero: PowerzeroSettings;
  enable_enforce_dod: boolean;
  enable_cellquery: boolean;
  enable_experimental_commands: boolean;
  enable_cmd30: boolean;
  enable_esp_temperature: boolean;
  enable_set_mqtt: boolean;
  enable_hexdump: boolean;

  enable_powermeter: boolean;
  powermeter: PowermeterSettings;

  enable_set_wifi: boolean;
  set_wifi: SetWifiSettings;

  enable_manual_ip: boolean;
  manual_ip: ManualIP;

  enable_web_server: boolean;
  web_server: WebServerSettings;

  enable_ota: boolean;
  ota: OtaSettings;

  enable_fallback_hotspot: boolean;
  fallback_hotspot: FallbackHotspotSettings;

  storages: Storage[];
}
