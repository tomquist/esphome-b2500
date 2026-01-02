import { templates } from './templates';

export type TemplateVersion = keyof typeof templates;

export interface MQTTSettings {
  enabled: boolean;
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
  id?: string;
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

export interface NativeApiSettings {
  enabled: boolean;
}

export const validEspTemperatureVariants = ['internal', 'ntc'] as const;
export const espTemperatureVariantLabels: Record<
  (typeof validEspTemperatureVariants)[number],
  string
> = {
  internal: 'ESP32 Internal Sensor',
  ntc: 'NTC Thermistor',
};

export interface EspTemperatureSettings {
  variant: (typeof validEspTemperatureVariants)[number];
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

export const validFlashSized = ['2MB', '4MB', '8MB', '16MB', '32MB'] as const;
export type FlashSize = (typeof validFlashSized)[number];

export const validLogLevels = ['DEBUG', 'INFO'] as const;
export type LogLevel = (typeof validLogLevels)[number];

export interface FormValues {
  template_version: TemplateVersion;
  name: string;
  log_level: LogLevel;
  friendly_name: string;
  poll_interval_seconds: number;
  mqtt: MQTTSettings;
  wifi: WifiSettings;
  board: string;
  variant: PlatformVariant;
  idf_platform_version: string;
  flash_size: FlashSize;
  enable_auto_restart: boolean;
  auto_restart: AutoRestartSettings;
  enable_powerzero: boolean;
  powerzero: PowerzeroSettings;
  enable_enforce_dod: boolean;
  enable_cellquery: boolean;
  enable_timer_query: boolean;
  enable_experimental_commands: boolean;
  enable_cmd30: boolean;
  enable_esp_temperature: boolean;
  esp_temperature: EspTemperatureSettings;
  enable_set_mqtt: boolean;
  enable_reset_mqtt: boolean;
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

  native_api: NativeApiSettings;

  /**
   * Needed for some ESP32-S3 boards to enable DIO flash mode.
   */
  enable_dio_flash_mode: boolean;

  /**
   * Use legacy entity names with device prefix (e.g., "B2500 - 1 - Storage: Battery Level")
   * instead of simplified names (e.g., "Battery Level").
   * Defaults to true for backward compatibility.
   */
  use_legacy_entity_names: boolean;

  storages: Storage[];
}
