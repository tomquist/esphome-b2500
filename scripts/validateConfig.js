'use strict';

/**
 * Validates the decrypted build configuration before it is rendered into the
 * ESPHome YAML.
 *
 * The config arrives from the browser via a public, unauthenticated dispatch
 * proxy, so every value is attacker controlled. The Jinja templates interpolate
 * most values inside double-quoted YAML strings through the `yaml_string`
 * escaper, but a handful of fields (versions, ports, pins, log level, flash
 * size, ...) are emitted as bare YAML scalars. Without validation an attacker
 * could smuggle a newline into one of those and inject arbitrary top-level YAML
 * such as `packages:` or `external_components:`, which ESPHome would fetch and
 * execute as Python at compile time on the build runner.
 *
 * Two layers guard against that:
 *   1. No string anywhere in the config may contain a control character
 *      (newline, carriage return, ...). A YAML-structure injection needs a line
 *      break to open a new key, so this alone stops the class of attack.
 *   2. The fields that are rendered as bare scalars are additionally held to a
 *      strict shape, which also rejects same-line YAML tricks (`!include`,
 *      `&anchor`, `*alias`, ...) that do not need a newline.
 *
 * Escaping is not the whole story, though. A value can be perfectly quoted and
 * still be dangerous because of what ESPHome does with it: `platform_version`
 * is handed to PlatformIO as its `platform` spec, and PlatformIO accepts a URL
 * or a git repository there and runs the Python inside the package it fetches.
 * Fields like that need an allow-list of their own, whatever the escaping.
 */

class InvalidConfigError extends Error {
  constructor(message) {
    super(message);
    this.name = 'InvalidConfigError';
  }
}

// Matches any control character except tab (0x09): 0x00-0x08, 0x0a-0x1f
// and DEL (0x7f). Notably matches newline and carriage return, which is what
// a YAML-structure injection needs to open a new key.
// eslint-disable-next-line no-control-regex
const CONTROL_CHARS = /[\x00-\x08\x0a-\x1f\x7f]/;

// Names the templates expect to come from the build, not from the requester.
// `git_sha` reaches the `ref` that ESPHome fetches the b2500 component from,
// which is a fetcher rather than a plain YAML scalar, so it gets a second
// layer: render.js overrides it in the context. `ref` itself is overridden by
// the templates' own `{% set ref = ... %}` rather than by render.js, and is
// listed here so a template that stopped doing that would not silently expose
// it.
const RESERVED_KEYS = ['git_sha', 'automated_build', 'ref'];

// Matches getMaxBleDevices() in src/utils/index.ts, which is 9 because that is
// what esp32_ble_tracker allows. Kept in step by src/utils/index.test.ts.
const MAX_STORAGES = 9;

const LOG_LEVELS = new Set([
  'NONE',
  'ERROR',
  'WARN',
  'INFO',
  'DEBUG',
  'VERBOSE',
  'VERY_VERBOSE',
]);

const rejectControlChars = (value, path) => {
  if (typeof value === 'string') {
    if (CONTROL_CHARS.test(value)) {
      throw new InvalidConfigError(
        `Value at ${path} contains a control character`
      );
    }
    return;
  }
  if (Array.isArray(value)) {
    value.forEach((item, index) =>
      rejectControlChars(item, `${path}[${index}]`)
    );
    return;
  }
  if (value && typeof value === 'object') {
    for (const [key, item] of Object.entries(value)) {
      // Keys are template-selected, but guard them too for good measure.
      if (CONTROL_CHARS.test(key)) {
        throw new InvalidConfigError(
          `Object key at ${path} contains a control character`
        );
      }
      rejectControlChars(item, `${path}.${key}`);
    }
  }
};

// Accepts a value that is either absent/empty (template default kicks in) or a
// non-negative integer, whether it arrived as a number or a numeric string.
const isBlank = (value) =>
  value === undefined || value === null || value === '';

const asInteger = (value) => {
  if (typeof value === 'number') {
    return Number.isInteger(value) ? value : undefined;
  }
  if (typeof value === 'string' && /^[0-9]+$/.test(value)) {
    return Number.parseInt(value, 10);
  }
  return undefined;
};

const requireInteger = (value, path, { min, max }) => {
  if (isBlank(value)) {
    return;
  }
  const parsed = asInteger(value);
  if (parsed === undefined || parsed < min || parsed > max) {
    throw new InvalidConfigError(
      `${path} must be an integer between ${min} and ${max}`
    );
  }
};

const requirePattern = (value, path, pattern, description) => {
  if (isBlank(value)) {
    return;
  }
  if (typeof value !== 'string' || !pattern.test(value)) {
    throw new InvalidConfigError(`${path} must be ${description}`);
  }
};

/**
 * Throws {@link InvalidConfigError} if the config could break out of the YAML
 * the templates generate. Returns nothing; the caller renders `config` as-is.
 */
const validateConfig = (config) => {
  if (!config || typeof config !== 'object' || Array.isArray(config)) {
    throw new InvalidConfigError('Configuration must be an object');
  }

  rejectControlChars(config, 'config');

  for (const key of RESERVED_KEYS) {
    if (Object.prototype.hasOwnProperty.call(config, key)) {
      throw new InvalidConfigError(
        `${key} is set by the build, not the config`
      );
    }
  }

  // Bare-scalar fields: hold them to a strict shape so nothing but the expected
  // token can reach the YAML unquoted.
  requirePattern(
    config.flash_size,
    'flash_size',
    /^[0-9]{1,3}MB$/,
    'a flash size such as 4MB'
  );

  // Reaches ESPHome as `platform_version`, which it forwards to PlatformIO as
  // the `platform` spec. PlatformIO resolves a URL, a git repository or a local
  // path there and executes the platform package's build scripts, so anything
  // but a plain version number is remote code execution on the build runner.
  requirePattern(
    config.idf_platform_version,
    'idf_platform_version',
    /^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}(-[0-9A-Za-z]{1,16})?$/,
    'a version number such as 55.3.37'
  );

  if (!isBlank(config.log_level) && !LOG_LEVELS.has(String(config.log_level))) {
    throw new InvalidConfigError(
      `log_level must be one of ${[...LOG_LEVELS].join(', ')}`
    );
  }

  requireInteger(config.poll_interval_seconds, 'poll_interval_seconds', {
    min: 1,
    max: 86400,
  });

  const webServer = config.web_server;
  if (webServer && typeof webServer === 'object') {
    requireInteger(webServer.port, 'web_server.port', { min: 1, max: 65535 });
  }

  const mqtt = config.mqtt;
  if (mqtt && typeof mqtt === 'object') {
    requireInteger(mqtt.port, 'mqtt.port', { min: 1, max: 65535 });
  }

  const powermeter = config.powermeter;
  if (powermeter && typeof powermeter === 'object') {
    requirePattern(
      powermeter.tx_pin,
      'powermeter.tx_pin',
      /^[A-Za-z0-9_]+$/,
      'a pin name such as GPIO6'
    );
    requirePattern(
      powermeter.rx_pin,
      'powermeter.rx_pin',
      /^[A-Za-z0-9_]+$/,
      'a pin name such as GPIO7'
    );
    requireInteger(powermeter.baud_rate, 'powermeter.baud_rate', {
      min: 1,
      max: 1000000,
    });
    requireInteger(powermeter.stop_bits, 'powermeter.stop_bits', {
      min: 1,
      max: 2,
    });
  }

  const autoRestart = config.auto_restart;
  if (autoRestart && typeof autoRestart === 'object') {
    requireInteger(
      autoRestart.restart_after_error_count,
      'auto_restart.restart_after_error_count',
      { min: 0, max: 1000000 }
    );
  }

  if (config.storages !== undefined) {
    if (!Array.isArray(config.storages)) {
      throw new InvalidConfigError('storages must be an array');
    }
    // Each storage expands to roughly a hundred YAML entities, and the build is
    // unauthenticated, so the list needs an upper bound - but the bound is the
    // one the UI offers, not a smaller number, or a supported configuration
    // stops building.
    if (config.storages.length > MAX_STORAGES) {
      throw new InvalidConfigError(
        `storages must hold at most ${MAX_STORAGES} entries`
      );
    }
    config.storages.forEach((storage, index) => {
      if (storage && typeof storage === 'object') {
        requireInteger(storage.version, `storages[${index}].version`, {
          min: 0,
          max: 99,
        });
        requirePattern(
          storage.mac_address,
          `storages[${index}].mac_address`,
          // Colons only: ESPHome's cv.mac_address splits on ":" and requires
          // six parts, so a dash-separated address is invalid there too.
          // normalizeImportedConfig() in src/utils converts the one input that
          // can carry dashes (an imported JSON file), so reaching this means a
          // hand-made payload, and failing here gives a better message than
          // failing in ESPHome.
          /^[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}$/,
          'a MAC address such as 00:11:22:33:44:55'
        );
      }
    });
  }
};

module.exports = { validateConfig, InvalidConfigError };
