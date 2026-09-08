import fs from 'fs';
import path from 'path';
import { extractFirmwareBundle, WrongPasswordError } from './firmwareBundle';

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
