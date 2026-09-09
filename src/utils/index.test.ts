import fs from 'fs';
import path from 'path';
import {
  FormValues,
  validEspTemperatureVariants,
  validPlatformVariants,
} from '../types';
import {
  BOARD,
  defaultFormValues,
  generateRandomIdentifier,
  getMaxBleDevices,
  isPlatformVersionValid,
  MAC_ADDRESS,
  normalizeImportedConfig,
} from './index';

// Read out of the workflow rather than copied: the identifier is used unquoted
// in an S3 object key, and a copy here would stay green while the workflow
// rejected every build.
const workflow = fs.readFileSync(
  path.join(__dirname, '..', '..', '.github', 'workflows', 'build-esphome.yml'),
  'utf-8'
);
const declared = workflow.match(/"\$IDENTIFIER" =~ \^(?<pattern>\S+)\s*\]\]/);
if (!declared) {
  throw new Error('no IDENTIFIER check found in the build workflow');
}
const IDENTIFIER = new RegExp(`^${declared.groups!.pattern}`);

describe('generateRandomIdentifier', () => {
  it('matches the pattern the build workflow enforces', () => {
    for (let i = 0; i < 200; i += 1) {
      const identifier = generateRandomIdentifier();
      expect(identifier).toMatch(IDENTIFIER);
      expect(identifier.length).toBeLessThanOrEqual(64);
    }
  });

  it('picks both words from the full lists', () => {
    const words = new Set<string>();
    for (let i = 0; i < 500; i += 1) {
      generateRandomIdentifier()
        .split('-')
        .slice(0, 3)
        .forEach((word) => words.add(word));
    }
    // 30 adjectives + 29 animals; a stuck picker would show up as a handful.
    expect(words.size).toBeGreaterThan(50);
  });

  it('ends in 96 bits of randomness', () => {
    const suffixes = new Set<string>();
    for (let i = 0; i < 500; i += 1) {
      const suffix = generateRandomIdentifier().split('-').pop();
      expect(suffix).toMatch(/^[0-9a-f]{24}$/);
      suffixes.add(suffix as string);
    }
    expect(suffixes.size).toBe(500);
  });
});

describe('isPlatformVersionValid', () => {
  // Read out of validateConfig rather than restated: a form that accepts more
  // than the build does means a five-minute build and an opaque failure.
  const source = fs.readFileSync(
    path.join(__dirname, '..', '..', 'scripts', 'validateConfig.js'),
    'utf-8'
  );
  const declared = source.match(
    /idf_platform_version',\s*\/(?<pattern>\^[^/]+\$)\//
  );

  it('mirrors the pattern the build enforces', () => {
    expect(declared).not.toBeNull();
    const build = new RegExp(declared!.groups!.pattern);

    for (const value of [
      '55.3.37',
      '55.3.37-1',
      '6.0.0-rc1',
      'https://attacker.example/platform.zip',
      'https://github.com/a/b.git#main',
      '/github/workspace/evil',
      'recommended',
      '55.3',
    ]) {
      expect(isPlatformVersionValid(value)).toBe(build.test(value));
    }
  });

  it('accepts an empty value, which the build treats as absent', () => {
    expect(isPlatformVersionValid('')).toBe(true);
    expect(isPlatformVersionValid(undefined)).toBe(true);
  });
});

describe('MAC_ADDRESS', () => {
  // The client is the strict side: formatMacAddress guarantees colons for typed
  // input and both boundaries normalise, so anything the form accepts must be
  // something the build accepts too.
  it('is the pattern the build enforces', () => {
    const source = fs.readFileSync(
      path.join(__dirname, '..', '..', 'scripts', 'validateConfig.js'),
      'utf-8'
    );
    const declared = source.match(
      /mac_address`,\s*(?:\/\/[^\n]*\n\s*)*\/(?<pattern>\^[^/]+\$)\//
    );
    expect(declared).not.toBeNull();
    const build = new RegExp(declared!.groups!.pattern);

    for (const value of [
      'AA:BB:CC:DD:EE:FF',
      '00:11:22:33:44:55',
      'AA-BB-CC-DD-EE-FF',
      'AA-BB:CC-DD:EE-FF',
      'AABBCCDDEEFF',
      'AA:BB:CC:DD:EE',
      'GG:BB:CC:DD:EE:FF',
      'AA:BB:CC:DD:EE:FF ',
    ]) {
      expect(MAC_ADDRESS.test(value)).toBe(build.test(value));
    }
  });
});

describe('the storage cap', () => {
  // The server rejects a config with more storages than it allows; the form
  // offers up to getMaxBleDevices(). A cap below that means a configuration the
  // UI builds for you is refused after the dispatch.
  it('is the number the form lets you add', () => {
    const source = fs.readFileSync(
      path.join(__dirname, '..', '..', 'scripts', 'validateConfig.js'),
      'utf-8'
    );
    const declared = source.match(/const MAX_STORAGES = (?<max>\d+);/);

    expect(declared).not.toBeNull();
    expect(Number(declared!.groups!.max)).toBe(getMaxBleDevices());
  });
});

describe('normalizeImportedConfig', () => {
  const withMac = (mac: string) =>
    ({
      ...defaultFormValues,
      storages: [{ name: 'a', version: 2, mac_address: mac }],
    }) as FormValues;

  it('rewrites a dash-separated MAC to colons', () => {
    // ESPHome splits on ":", so a dash address is invalid all the way down.
    expect(
      normalizeImportedConfig(withMac('AA-BB-CC-DD-EE-FF')).storages[0]
        .mac_address
    ).toBe('AA:BB:CC:DD:EE:FF');
  });

  it('leaves a colon-separated MAC alone', () => {
    expect(
      normalizeImportedConfig(withMac('AA:BB:CC:DD:EE:FF')).storages[0]
        .mac_address
    ).toBe('AA:BB:CC:DD:EE:FF');
  });

  it('drops storages that are not objects, and a list that is not one', () => {
    // A localStorage blob written by an older build can be any shape; degrade
    // to "no storages" rather than throwing somewhere further up.
    const shapeless = { ...defaultFormValues, storages: {} } as never;
    expect(normalizeImportedConfig(shapeless).storages).toEqual([]);
    expect(
      normalizeImportedConfig({
        ...defaultFormValues,
        storages: [null, 'x', { name: 'a', version: 2, mac_address: '' }],
      } as never).storages
    ).toHaveLength(1);
  });

  it('produces something the build accepts', () => {
    // The server pattern, read out of validateConfig rather than restated.
    const source = fs.readFileSync(
      path.join(__dirname, '..', '..', 'scripts', 'validateConfig.js'),
      'utf-8'
    );
    const declared = source.match(
      /mac_address`,\s*(?:\/\/[^\n]*\n\s*)*\/(?<pattern>\^[^/]+\$)\//
    );
    expect(declared).not.toBeNull();

    const normalized = normalizeImportedConfig(withMac('AA-BB-CC-DD-EE-FF'))
      .storages[0].mac_address;
    expect(normalized).toMatch(new RegExp(declared!.groups!.pattern));
  });
});

describe('BOARD', () => {
  // The build prints the board verbatim in the redacted failure log, so it has
  // to be a token the build recognises. A client that accepts more than the
  // build does turns a typo into a five-minute build and an opaque failure.
  it('is the pattern the build enforces', () => {
    const source = fs.readFileSync(
      path.join(__dirname, '..', '..', 'scripts', 'validateConfig.js'),
      'utf-8'
    );
    const declared = source.match(
      /'board',\s*(?:\/\/[^\n]*\n\s*)*\/(?<pattern>\^[^/]+\$)\//
    );
    expect(declared).not.toBeNull();
    const build = new RegExp(declared!.groups!.pattern);

    for (const value of [
      'esp32dev',
      'esp32-s3-devkitc-1',
      'az-delivery-devkit-v4',
      'um_tinys3',
      'esp32 dev',
      '../../etc/passwd',
      '-leading-dash',
      '',
    ]) {
      expect(BOARD.test(value)).toBe(build.test(value));
    }
  });
});

describe('the variant allow-lists', () => {
  const source = () =>
    fs.readFileSync(
      path.join(__dirname, '..', '..', 'scripts', 'validateConfig.js'),
      'utf-8'
    );

  const declaredSet = (name: string) => {
    const block = source().match(
      new RegExp(`const ${name} = new Set\\(\\[?(?<body>[^)]*)\\]?\\)`)
    );
    expect(block).not.toBeNull();
    return [...block!.groups!.body.matchAll(/'([^']+)'/g)].map((m) => m[1]);
  };

  it('match the values the form offers', () => {
    expect(declaredSet('PLATFORM_VARIANTS').sort()).toEqual(
      [...validPlatformVariants].sort()
    );
    expect(declaredSet('ESP_TEMPERATURE_VARIANTS').sort()).toEqual(
      [...validEspTemperatureVariants].sort()
    );
  });
});
