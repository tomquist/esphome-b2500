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

// What survives verbatim: fields that describe the build rather than the person
// asking for it, and that are the first things you want when diagnosing a
// failure - which board, which variant, which flash size, which template.
//
// Two conditions, both required. A field must carry nothing about the user or
// their network, which is why a MAC address is absent though it is as tightly
// constrained as anything here. And it must be pinned to a known shape by
// validateConfig, so what reaches a public log is a token from a set we chose
// rather than whatever the requester sent. The test enforces the second
// condition; the first is a judgement made per field, here.
const KEEP = new Set([
  'template_version',
  'log_level',
  'flash_size',
  'board',
  'variant',
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
