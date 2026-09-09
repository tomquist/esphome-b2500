import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const { KEEP, redactConfig } = require('./redactConfig.js');

test('redacts free-form strings down to a length bucket', () => {
  const redacted = redactConfig({
    wifi: { ssid: 'MyHomeNet', password: 'hunter22' },
    mqtt: { broker: 'mqtt.local', username: 'tom' },
  });
  assert.deepEqual(redacted, {
    wifi: { ssid: '<string:8-15>', password: '<string:8-15>' },
    mqtt: { broker: '<string:8-15>', username: '<string:1-7>' },
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
    idf_platform_version: '55.3.37',
    powermeter: { tx_pin: 'GPIO6', rx_pin: 'GPIO7' },
  });
  assert.deepEqual(redacted, {
    template_version: 'v2',
    log_level: 'DEBUG',
    flash_size: '4MB',
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
      { name: '<string:1-7>', version: 2, mac_address: '<string:16-31>' },
      { name: '<string:1-7>', version: '1' },
    ],
  });
});

test('redacts fields validateConfig does not constrain', () => {
  // board and variant are free-form requester text; a public log is not the
  // place for them verbatim.
  const redacted = redactConfig({ board: 'esp32dev', variant: 'anything' });
  assert.deepEqual(redacted, {
    board: '<string:8-15>',
    variant: '<string:8-15>',
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

// The comment on KEEP asks future editors to keep it a subset of the fields
// validateConfig pins down. Nothing enforced that, so a field added here
// without a matching rule there would go verbatim into a public log.
test('every kept field is one validateConfig constrains', () => {
  const source = fs.readFileSync(
    new URL('./validateConfig.js', import.meta.url),
    'utf-8'
  );
  // `requirePattern(config.x, 'x', ...)` / `requireInteger(value, 'a.b', ...)`
  // and the log_level allow-list, reduced to their leaf field names.
  const constrained = new Set(
    [
      ...source.matchAll(
        /require(?:Pattern|Integer)\(\s*[^,]+,\s*[`'"]([^`'"]+)/g
      ),
    ]
      .map((match) => match[1].split(/[.[]/).pop())
      .concat('log_level', 'template_version')
  );

  for (const kept of KEEP) {
    assert.ok(
      constrained.has(kept),
      `${kept} is kept in the log but validateConfig does not constrain it`
    );
  }
});
