import test from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const { validateConfig, InvalidConfigError } = require('./validateConfig.js');

const baseConfig = () => ({
  template_version: 'v2',
  name: 'b2500',
  friendly_name: 'B2500',
  poll_interval_seconds: 5,
  log_level: 'INFO',
  flash_size: '4MB',
  mqtt: { enabled: true, topic: 'b2500', broker: 'mqtt.local', port: 1883 },
  wifi: { ssid: 'net', password: 'secret' },
  fallback_hotspot: { ssid: 'Fallback' },
  powermeter: { tx_pin: 'GPIO6', rx_pin: 'GPIO7', baud_rate: 9600, stop_bits: 1 },
  auto_restart: { restart_after_error_count: 8 },
  web_server: { port: 80 },
  storages: [
    { name: 'Battery', version: 2, mac_address: '00:11:22:33:44:55' },
  ],
});

test('accepts a representative config', () => {
  assert.doesNotThrow(() => validateConfig(baseConfig()));
});

test('accepts values that contain quotes and backslashes', () => {
  const config = baseConfig();
  config.wifi.password = 'p"a\\ss';
  config.mqtt.topic = 'home/b2500';
  config.fallback_hotspot.ssid = 'Guest "Net"';
  assert.doesNotThrow(() => validateConfig(config));
});

test('accepts numeric fields given as strings', () => {
  const config = baseConfig();
  config.mqtt.port = '8883';
  config.poll_interval_seconds = '10';
  config.storages[0].version = '1';
  assert.doesNotThrow(() => validateConfig(config));
});

test('accepts a config that omits optional scalar fields', () => {
  const config = baseConfig();
  delete config.flash_size;
  delete config.log_level;
  delete config.powermeter;
  delete config.storages[0].mac_address;
  config.web_server.port = '';
  assert.doesNotThrow(() => validateConfig(config));
});

test('accepts a plain platform version', () => {
  const config = baseConfig();
  config.idf_platform_version = '55.3.37';
  assert.doesNotThrow(() => validateConfig(config));
  config.idf_platform_version = '55.3.37-1';
  assert.doesNotThrow(() => validateConfig(config));
});

const injections = {
  'newline in log_level': (c) => {
    c.log_level = 'INFO\n\npackages:\n  evil: github://a/b@main';
  },
  'newline in flash_size': (c) => {
    c.flash_size = '4MB\nexternal_components: []';
  },
  'newline in a nested string': (c) => {
    c.wifi.ssid = 'net\nexternal_components: []';
  },
  'carriage return in a string': (c) => {
    c.mqtt.topic = 'a\rb';
  },
  'newline deep in an array element': (c) => {
    c.storages[0].name = 'x\npackages: {}';
  },
  'newline in a numeric string': (c) => {
    c.mqtt.port = '1883\nfoo: bar';
  },
};

for (const [name, mutate] of Object.entries(injections)) {
  test(`rejects ${name}`, () => {
    const config = baseConfig();
    mutate(config);
    assert.throws(() => validateConfig(config), InvalidConfigError);
  });
}

const badShapes = {
  'flash_size with a YAML tag': (c) => {
    c.flash_size = '!include /etc/passwd';
  },
  'flash_size that is not a flash size': (c) => {
    c.flash_size = '4 MB; rm -rf';
  },
  'log_level outside the allow-list': (c) => {
    c.log_level = 'TRACE';
  },
  'port out of range': (c) => {
    c.mqtt.port = 99999;
  },
  'port that is not an integer': (c) => {
    c.web_server.port = '80abc';
  },
  'pin with punctuation': (c) => {
    c.powermeter.tx_pin = 'GPIO6:evil';
  },
  'non-integer storage version': (c) => {
    c.storages[0].version = '2.5';
  },
  'storages that is not an array': (c) => {
    c.storages = { length: 1 };
  },
  'a MAC address that is not one': (c) => {
    c.storages[0].mac_address = '00:11:22:33:44:55 evil';
  },
  'a MAC address carrying a YAML tag': (c) => {
    c.storages[0].mac_address = '!include /etc/passwd';
  },
  'a MAC address closing the quoted scalar': (c) => {
    c.storages[0].mac_address = 'AA", id: x, foo: "bar';
  },
  'a platform_version pointing at a URL': (c) => {
    c.idf_platform_version =
      'https://attacker.example/platform-espressif32.zip';
  },
  'a platform_version pointing at a git repository': (c) => {
    c.idf_platform_version =
      'https://github.com/attacker/platform-espressif32.git#main';
  },
  'a platform_version pointing at a local path': (c) => {
    c.idf_platform_version = '/github/workspace/evil';
  },
};

for (const [name, mutate] of Object.entries(badShapes)) {
  test(`rejects ${name}`, () => {
    const config = baseConfig();
    mutate(config);
    assert.throws(() => validateConfig(config), InvalidConfigError);
  });
}

test('rejects a non-object config', () => {
  assert.throws(() => validateConfig(null), InvalidConfigError);
  assert.throws(() => validateConfig([]), InvalidConfigError);
});
