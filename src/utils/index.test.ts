import { generateRandomIdentifier } from './index';

// The build workflow rejects anything that does not match this, and the
// identifier is used unquoted in an S3 object key.
const IDENTIFIER = /^[a-z0-9-]{1,64}$/;

describe('generateRandomIdentifier', () => {
  it('matches what the build workflow accepts', () => {
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
