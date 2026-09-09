'use strict';

/**
 * Renders the build configuration for the failure log.
 *
 * Workflow logs on a public repository are readable by anyone, and the config
 * describes the requester's home network: SSID, MQTT broker and username,
 * static IP addresses, topic names. `render.js` masks the values the web
 * builder marks as secrets - the passwords and the MAC addresses - but
 * everything else would otherwise be printed verbatim when a build fails.
 *
 * Redaction keeps what a failed build actually needs: the shape of the config,
 * every number and boolean (so the feature flags stay readable), and a short
 * allow-list of fields that pick template branches and are already constrained
 * by `validateConfig`. Every other string becomes a length bucket - an exact
 * length is a real hint about a password.
 */

// A subset of the fields `validateConfig` constrains to a fixed shape, plus the
// template selector `render.js` switches on. Only a subset: a MAC address is
// equally well constrained and is deliberately not here, because it identifies
// a device. `board` and `variant` used to be here and are not constrained at
// all, so they were free-form requester text going verbatim into a public log.
// Nothing may enter this set that validateConfig does not pin down - the test
// enforces that direction.
const KEEP = new Set([
  'template_version',
  'log_level',
  'flash_size',
  'idf_platform_version',
  'tx_pin',
  'rx_pin',
  'version',
]);

// An exact length is a real hint about a password, so report a bucket instead.
const lengthBucket = (length) => {
  if (length === 0) return '0';
  if (length < 8) return '1-7';
  if (length < 16) return '8-15';
  if (length < 32) return '16-31';
  return '32+';
};

const redact = (value, key) => {
  if (typeof value === 'string') {
    return KEEP.has(key) ? value : `<string:${lengthBucket(value.length)}>`;
  }
  if (Array.isArray(value)) {
    // Array elements inherit the key of the array they are in, so
    // `storages[].version` is still recognised further down.
    return value.map((item) => redact(item, key));
  }
  if (value && typeof value === 'object') {
    return Object.fromEntries(
      Object.entries(value).map(([name, item]) => [name, redact(item, name)])
    );
  }
  return value;
};

/** Returns a copy of `config` safe to print to a public log. */
const redactConfig = (config) => redact(config, undefined);

module.exports = { KEEP, redactConfig };

if (require.main === module) {
  const fs = require('fs');
  const path = process.argv[2];
  if (!path) {
    console.error('usage: redactConfig.js <config.json>');
    process.exit(2);
  }
  const config = JSON.parse(fs.readFileSync(path, 'utf-8'));
  console.log(JSON.stringify(redactConfig(config), null, 2));
}
