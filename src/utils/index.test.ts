import fs from 'fs';
import path from 'path';
import { generateRandomIdentifier } from './index';

// Read out of the workflow rather than copied: the identifier is used unquoted
// in an S3 object key, and a copy here would stay green while the workflow
// rejected every build.
const workflow = fs.readFileSync(
  path.join(__dirname, '..', '..', '.github', 'workflows', 'build-esphome.yml'),
  'utf-8'
);
const declared = workflow.match(/"\$IDENTIFIER" =~ \^(?<pattern>\S+) \]\]/);
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
