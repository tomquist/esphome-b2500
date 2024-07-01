import crypto from 'crypto';
import { FormValues } from '../types';

export const defaultFormValues: FormValues = {
  name: 'b2500',
  friendly_name: 'B2500',
  poll_interval_seconds: 5,
  mqtt: {
    topic: 'b2500',
    broker: 'mqttbroker.local',
    port: 1883,
    username: '',
    password: '',
    discovery: false,
  },
  wifi: {
    ssid: 'MyWifi',
    password: 'MyPassword',
  },
  board: 'esp32dev',
  variant: 'auto',
  idf_platform_version: '',
  enable_auto_restart: true,
  auto_restart: {
    restart_after_error_count: 8,
  },
  enable_cellquery: false,
  enable_timer_query: true,
  enable_cmd30: false,
  enable_esp_temperature: false,
  enable_powermeter: false,
  enable_experimental_commands: false,
  enable_hexdump: false,
  enable_set_wifi: false,
  set_wifi: {
    ssid: 'MyWifi',
    password: 'MyPassword',
  },
  enable_set_mqtt: false,
  powermeter: {
    tx_pin: 'GPIO6',
    rx_pin: 'GPIO7',
    baud_rate: 9600,
    stop_bits: 1,
  },
  enable_enforce_dod: false,
  enable_powerzero: false,
  powerzero: {
    grid_power_topic: 'tibber-esp/sensor/power/state',
    limit_cmd_topic: 'openDTU/XXXXXXXXXXXX/cmd/limit_persistent_relative',
    limit_state_topic: 'openDTU/XXXXXXXXXXXX/state/limit_relative',
  },
  enable_manual_ip: false,
  manual_ip: {
    ip: '192.168.1.100',
    gateway: '192.168.1.1',
    subnet: '255.255.255.0',
    dns: '192.168.1.1',
  },
  enable_web_server: true,
  web_server: {
    port: 80,
    js_include: './v2/www.js',
  },
  enable_ota: true,
  ota: {
    password: 'my_ota_password',
    enable_unprotected_writes: false,
  },
  enable_fallback_hotspot: true,
  fallback_hotspot: {
    ssid: 'ESPHome-b2500',
    enable_captive_portal: true,
  },
  storages: [{ name: 'B2500', version: 1, mac_address: '00:00:00:00:00:00' }],
};

export const mergeDeep = (target: any, source: any) => {
  for (const key of Object.keys(source)) {
    if (source[key] instanceof Object) {
      if (!target[key]) {
        Object.assign(target, { [key]: {} });
      }
      mergeDeep(target[key], source[key]);
    } else {
      if (target[key] === undefined) {
        Object.assign(target, { [key]: source[key] });
      }
    }
  }
  return target;
};

export const getAllSecrets = (debouncedFormValues: FormValues) => {
  const secrets = [];
  if (debouncedFormValues.mqtt.password) {
    secrets.push(debouncedFormValues.mqtt.password);
  }
  if (debouncedFormValues.wifi.password) {
    secrets.push(debouncedFormValues.wifi.password);
  }
  if (debouncedFormValues.set_wifi.password) {
    secrets.push(debouncedFormValues.set_wifi.password);
  }
  if (debouncedFormValues.ota.password) {
    secrets.push(debouncedFormValues.ota.password);
  }
  for (const storage of debouncedFormValues.storages) {
    if (storage.mac_address) {
      secrets.push(storage.mac_address);
    }
  }
  return secrets;
};

export const redactSecrets = (config: FormValues) => {
  const redactedConfig = JSON.parse(JSON.stringify(config));
  redactedConfig.mqtt.password = '***';
  redactedConfig.wifi.password = '***';
  redactedConfig.set_wifi.password = '***';
  redactedConfig.ota.password = '***';
  for (const storage of redactedConfig.storages) {
    storage.mac_address = '***';
  }
  return redactedConfig;
};

export const validateConfig = (config: FormValues) => {
  const errors = [];
  if (config.storages.length === 0) {
    errors.push('At least one storage is required');
  }
  for (const storage of config.storages) {
    if (storage.name.trim() === '') {
      errors.push('Storage name is required');
    }
    if (storage.version < 1 || storage.version > 2) {
      errors.push('Storage version is invalid');
    }
    if (storage.mac_address.trim() === '') {
      errors.push('Storage MAC address is required');
    }
    if (
      !/^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$/.test(storage.mac_address)
    ) {
      errors.push('Storage MAC address is invalid');
    }
  }
  if (config.wifi.ssid.trim() === '') {
    errors.push('WiFi SSID is required');
  }
  if (config.wifi.password.trim() === '') {
    errors.push('WiFi password is required');
  }
  if (config.mqtt.topic.trim() === '') {
    errors.push('MQTT topic is required');
  }
  if (config.mqtt.broker.trim() === '') {
    errors.push('MQTT broker is required');
  }
  if (config.mqtt.port <= 0 || config.mqtt.port > 65535) {
    errors.push('MQTT port is invalid');
  }
  if (config.auto_restart.restart_after_error_count < 0) {
    errors.push('Auto restart count is invalid');
  }
  if (config.enable_powermeter) {
    if (config.powermeter.baud_rate < 0) {
      errors.push('Powermeter baud rate is invalid');
    }
    if (
      config.powermeter.stop_bits !== 1 &&
      config.powermeter.stop_bits !== 2
    ) {
      errors.push('Powermeter stop bits is invalid');
    }
    if (config.powermeter.tx_pin.trim() === '') {
      errors.push('Powermeter TX pin is required');
    }
    if (config.powermeter.rx_pin.trim() === '') {
      errors.push('Powermeter RX pin is required');
    }
  }
  if (config.enable_manual_ip) {
    if (config.manual_ip.ip.trim() === '') {
      errors.push('Manual IP address is required');
    }
    if (config.manual_ip.gateway.trim() === '') {
      errors.push('Manual gateway is required');
    }
    if (config.manual_ip.subnet.trim() === '') {
      errors.push('Manual subnet is required');
    }
    if (config.manual_ip.dns.trim() === '') {
      errors.push('Manual DNS is required');
    }
  }
  if (config.enable_web_server) {
    if (config.web_server.port <= 0 || config.web_server.port > 65535) {
      errors.push('Web server port is invalid');
    }
  }
  if (config.enable_ota) {
    if (config.ota.password.trim() === '') {
      errors.push('OTA password is required');
    }
    if (config.ota.enable_unprotected_writes) {
      errors.push(
        'Unprotected writes is not supported by this tool. See https://github.com/esphome/esphome/pull/5535 for more information and build the image manually.'
      );
    }
  }
  if (config.enable_fallback_hotspot) {
    if (config.fallback_hotspot.ssid.trim() === '') {
      errors.push('Fallback hotspot SSID is required');
    }
  }
  if (config.enable_set_wifi) {
    if (config.set_wifi.ssid.trim() === '') {
      errors.push('Set WiFi SSID is required');
    }
    if (config.set_wifi.password.trim() === '') {
      errors.push('Set WiFi password is required');
    }
  }
  if (config.enable_powerzero) {
    if (config.powerzero.grid_power_topic.trim() === '') {
      errors.push('Powerzero grid power topic is required');
    }
    if (config.powerzero.limit_cmd_topic.trim() === '') {
      errors.push('Powerzero limit command topic is required');
    }
    if (config.powerzero.limit_state_topic.trim() === '') {
      errors.push('Powerzero limit state topic is required');
    }
  }
  return errors;
};

export const newIssueLink = ({
  config,
  build,
}: {
  config: FormValues;
  build?: string;
}) => {
  const redactedConfig = redactSecrets(config);
  const body = `<!-- Please describe the issue you are experiencing and provide as much context as possible. -->\n
**Describe the issue**\n
<!-- Please describe the issue you are experiencing and provide as much context as possible. -->\n
**Steps to reproduce**\n
<!-- If applicable, provide steps to reproduce the issue. -->\n
**Expected behavior**\n
<!-- What should happen? -->\n
**Actual behavior**\n
<!-- What happens instead? -->\n
**Additional context**\n
<!-- Add any other context about the problem here. -->\n
**Screenshots**\n
<!-- If applicable, add screenshots to help explain your problem. -->\n
**Device information**\n
- ESP Board: [e.g. ESP32-C3]\n
- B2500 Brand: [e.g. Marstek]\n
- B2500 Version: [e.g. v1, v2, v3]\n
${build ? `- Build: [${build}]\n` : ''}
**Configuration**\n
\`\`\`json\n${JSON.stringify(redactedConfig, null, 2)}\n\`\`\`\n
`;
  return `https://github.com/tomquist/esphome-b2500/issues/new?body=${encodeURIComponent(body)}`;
};

export const generatePassword = () => {
  return crypto.randomBytes(16).toString('base64').slice(0, 16);
};

export const generateRandomIdentifier = () => {
  const adjectives = [
    'tiny',
    'fluffy',
    'happy',
    'sad',
    'angry',
    'brave',
    'quick',
    'lazy',
    'bright',
    'dark',
    'funky',
    'wobbly',
    'jumpy',
    'sparkly',
    'grumpy',
    'cheerful',
    'sneaky',
    'silly',
    'noisy',
    'sleepy',
    'bouncy',
    'zany',
    'quirky',
    'clumsy',
    'jazzy',
    'wiggly',
    'perky',
    'giggly',
    'bizarre',
    'nutty',
  ];

  const animals = [
    'cat',
    'dog',
    'rabbit',
    'lion',
    'tiger',
    'bear',
    'fox',
    'wolf',
    'mouse',
    'horse',
    'unicorn',
    'dragon',
    'penguin',
    'flamingo',
    'octopus',
    'giraffe',
    'koala',
    'panda',
    'squirrel',
    'hedgehog',
    'dolphin',
    'kangaroo',
    'lemur',
    'meerkat',
    'narwhal',
    'platypus',
    'quokka',
    'sloth',
    'toucan',
  ];

  const randomAdjective1 =
    adjectives[Math.floor(Math.random() * adjectives.length)];
  const randomAdjective2 =
    adjectives[Math.floor(Math.random() * adjectives.length)];
  const randomAnimal = animals[Math.floor(Math.random() * animals.length)];
  const randomNumber = Math.floor(Math.random() * 1000); // Adding a random number between 0 and 999

  return `${randomAdjective1}-${randomAdjective2}-${randomAnimal}-${randomNumber}`;
};
