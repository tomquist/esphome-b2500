import test from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const { redactConfig } = require('./redactConfig.js');

test('redacts free-form strings but keeps their length', () => {
  const redacted = redactConfig({
    wifi: { ssid: 'MyHomeNet', password: 'hunter22' },
    mqtt: { broker: 'mqtt.local', username: 'tom' },
  });
  assert.deepEqual(redacted, {
    wifi: { ssid: '<string:9>', password: '<string:8>' },
    mqtt: { broker: '<string:10>', username: '<string:3>' },
  });
});

test('keeps numbers, booleans and null so the flags stay readable', () => {
  const redacted = redactConfig({
    enable_powermeter: true,
    poll_interval_seconds: 5,
    idf_platform_version: null,
  });
  assert.deepEqual(redacted, {
    enable_powermeter: true,
    poll_interval_seconds: 5,
    idf_platform_version: null,
  });
});

test('keeps the allow-listed template selectors', () => {
  const redacted = redactConfig({
    template_version: 'v2',
    log_level: 'DEBUG',
    flash_size: '4MB',
    board: 'esp32dev',
    variant: 'auto',
    idf_platform_version: '55.3.37',
    powermeter: { tx_pin: 'GPIO6', rx_pin: 'GPIO7' },
  });
  assert.deepEqual(redacted, {
    template_version: 'v2',
    log_level: 'DEBUG',
    flash_size: '4MB',
    board: 'esp32dev',
    variant: 'auto',
    idf_platform_version: '55.3.37',
    powermeter: { tx_pin: 'GPIO6', rx_pin: 'GPIO7' },
  });
});

test('redacts inside arrays and keeps the storage version', () => {
  const redacted = redactConfig({
    storages: [
      { name: 'Battery', version: 2, mac_address: '00:11:22:33:44:55' },
      { name: 'Second', version: '1' },
    ],
  });
  assert.deepEqual(redacted, {
    storages: [
      { name: '<string:7>', version: 2, mac_address: '<string:17>' },
      { name: '<string:6>', version: '1' },
    ],
  });
});

test('leaves no original string anywhere in the output', () => {
  const config = {
    wifi: { ssid: 'SuperSecretSSID' },
    nested: [{ deep: [{ deeper: 'AlsoSecret' }] }],
  };
  const printed = JSON.stringify(redactConfig(config));
  assert.ok(!printed.includes('SuperSecretSSID'));
  assert.ok(!printed.includes('AlsoSecret'));
});
