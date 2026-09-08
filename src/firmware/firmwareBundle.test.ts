import fs from 'fs';
import path from 'path';
import {
  createFileIndex,
  extractFirmwareBundle,
  WrongPasswordError,
} from './firmwareBundle';

// A ZIP created the same way the build workflow does it:
//   zip -P test-password firmware.zip b2500-esp32s3/*
const archiveBytes = fs.readFileSync(
  path.join(__dirname, '__fixtures__', 'firmware.zip')
);
const password = 'test-password';
const metadata = { name: 'B2500', version: '2026.8.1' };

const blobs = new Map<string, Blob>();

// Not jest.fn(): react-scripts enables `resetMocks`, which would strip the
// implementation before each test.
let counter = 0;
URL.createObjectURL = (blob: Blob) => {
  const url = `blob:test/${(counter += 1)}`;
  blobs.set(url, blob);
  return url;
};
URL.revokeObjectURL = (url: string) => {
  blobs.delete(url);
};

beforeEach(() => {
  blobs.clear();
});

const archive = () => new Blob([archiveBytes]);

const textOf = async (url: string) => {
  const blob = blobs.get(url);
  if (!blob) {
    throw new Error(`No blob registered for ${url}`);
  }
  return Buffer.from(await blob.arrayBuffer()).toString();
};

describe('extractFirmwareBundle', () => {
  it('decrypts the archive and rewrites the manifest', async () => {
    const bundle = await extractFirmwareBundle(archive(), password, metadata);

    expect(bundle.chipFamily).toBe('ESP32-S3');
    expect(bundle.manifest.name).toBe('B2500');
    expect(bundle.manifest.version).toBe('2026.8.1');

    const manifest = JSON.parse(await textOf(bundle.manifestUrl));
    const part = manifest.builds[0].parts[0];
    expect(part.offset).toBe(0);
    expect(part.path).toMatch(/^blob:/);
    expect(await textOf(part.path)).toBe('FACTORY-BIN-CONTENT-0123456789');

    bundle.release();
    expect(blobs.size).toBe(0);
  });

  it('reports a wrong password', async () => {
    await expect(
      extractFirmwareBundle(archive(), 'wrong-password', metadata)
    ).rejects.toBeInstanceOf(WrongPasswordError);
    expect(blobs.size).toBe(0);
  });
});

describe('createFileIndex', () => {
  it('resolves manifest paths relative to the manifest directory', () => {
    const findFile = createFileIndex([
      'b2500-esp32s3/manifest.json',
      'b2500-esp32s3/b2500-esp32s3.factory.bin',
    ]);

    expect(findFile('b2500-esp32s3.factory.bin', 'b2500-esp32s3')).toBe(
      'b2500-esp32s3/b2500-esp32s3.factory.bin'
    );
    expect(findFile('manifest.json')).toBe('b2500-esp32s3/manifest.json');
  });

  it('keeps same-named files in different directories apart', () => {
    const findFile = createFileIndex([
      'esp32/firmware.bin',
      'esp32s3/firmware.bin',
    ]);

    expect(findFile('firmware.bin', 'esp32')).toBe('esp32/firmware.bin');
    expect(findFile('firmware.bin', 'esp32s3')).toBe('esp32s3/firmware.bin');
    // Ambiguous without a directory: better to fail than to flash the wrong one.
    expect(findFile('firmware.bin')).toBeUndefined();
  });

  it('falls back to an unambiguous file name', () => {
    const findFile = createFileIndex(['firmware/b2500.factory.bin']);

    expect(findFile('b2500.factory.bin', 'somewhere-else')).toBe(
      'firmware/b2500.factory.bin'
    );
  });
});
