import crypto from 'crypto';
import { FormValues } from '../types';

export const defaultFormValues: FormValues = {
  name: 'b2500',
  friendly_name: 'B2500',
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
  enable_auto_restart: true,
  auto_restart: {
    restart_after_error_count: 8,
  },
  enable_cellquery: false,
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
    ota: false,
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

export const newIssueLink = (config: FormValues) => {
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
