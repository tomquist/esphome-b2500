import {
  InvalidManifestError,
  manifestFileNames,
  normalizeManifest,
  withResolvedPaths,
} from './manifest';

const metadata = { name: 'B2500', version: '2026.8.1' };

// The manifest the ESPHome build action writes without `--complete-manifest`.
const partialManifest = {
  chipFamily: 'ESP32-S3',
  ota: { path: 'b2500-esp32s3.ota.bin', md5: 'abc' },
  parts: [{ path: 'b2500-esp32s3.factory.bin', offset: 0 }],
};

describe('normalizeManifest', () => {
  it('wraps a partial manifest into a complete one', () => {
    const manifest = normalizeManifest(partialManifest, metadata);

    expect(manifest.name).toBe('B2500');
    expect(manifest.version).toBe('2026.8.1');
    expect(manifest.builds).toHaveLength(1);
    expect(manifest.builds[0].chipFamily).toBe('ESP32-S3');
    expect(manifest.builds[0].parts).toEqual([
      { path: 'b2500-esp32s3.factory.bin', offset: 0 },
    ]);
  });

  it('keeps the values of a complete manifest', () => {
    const manifest = normalizeManifest(
      {
        name: 'My Device',
        version: '2026.1.0',
        new_install_prompt_erase: true,
        builds: [partialManifest],
      },
      metadata
    );

    expect(manifest.name).toBe('My Device');
    expect(manifest.version).toBe('2026.1.0');
    expect(manifest.new_install_prompt_erase).toBe(true);
    expect(manifest.builds).toHaveLength(1);
  });

  it('defaults a missing part offset to zero', () => {
    const manifest = normalizeManifest(
      { chipFamily: 'ESP32', parts: [{ path: 'firmware.bin' }] },
      metadata
    );

    expect(manifest.builds[0].parts[0].offset).toBe(0);
  });

  it('rejects manifests without a flashable build', () => {
    expect(() => normalizeManifest({ chipFamily: 'RP2040' }, metadata)).toThrow(
      InvalidManifestError
    );
    expect(() => normalizeManifest({ builds: [] }, metadata)).toThrow(
      InvalidManifestError
    );
    expect(() => normalizeManifest('nope', metadata)).toThrow(
      InvalidManifestError
    );
  });
});

describe('manifestFileNames', () => {
  it('returns unique file names without directories', () => {
    const manifest = normalizeManifest(
      {
        builds: [
          {
            chipFamily: 'ESP32',
            parts: [
              { path: 'b2500-esp32/bootloader.bin', offset: 0 },
              { path: 'b2500-esp32/firmware.bin', offset: 65536 },
              { path: 'firmware.bin', offset: 65536 },
            ],
          },
        ],
      },
      metadata
    );

    expect(manifestFileNames(manifest)).toEqual([
      'bootloader.bin',
      'firmware.bin',
    ]);
  });
});

describe('withResolvedPaths', () => {
  it('replaces part paths with the resolved URLs', () => {
    const manifest = normalizeManifest(partialManifest, metadata);
    const resolved = withResolvedPaths(manifest, () => 'blob:firmware');

    expect(resolved.builds[0].parts[0].path).toBe('blob:firmware');
    expect(resolved.builds[0].parts[0].offset).toBe(0);
    // The input manifest is not modified.
    expect(manifest.builds[0].parts[0].path).toBe('b2500-esp32s3.factory.bin');
  });

  it('fails when a firmware file is missing from the archive', () => {
    const manifest = normalizeManifest(partialManifest, metadata);

    expect(() => withResolvedPaths(manifest, () => undefined)).toThrow(
      /b2500-esp32s3.factory.bin/
    );
  });
});
