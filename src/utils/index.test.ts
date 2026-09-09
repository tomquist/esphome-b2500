import fs from 'fs';
import path from 'path';
import { generateRandomIdentifier, isPlatformVersionValid } from './index';

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
