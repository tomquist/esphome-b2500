import fs from 'fs';
import path from 'path';
import { decryptFirmwareArchive, WrongPasswordError } from './archiveCrypto';
import { extractFirmwareBundle } from './firmwareBundle';

// The other half of the pair: what the build runs to produce the object the
// browser downloads. Importing it here is the point of the test - the two sides
// have to agree on the key derivation and on the iv || tag || ciphertext
// framing, and nothing else checks that.
// eslint-disable-next-line @typescript-eslint/no-var-requires
const { encryptFirmware } = require('../../scripts/encryptFirmware.js');

const plainArchive = fs.readFileSync(
  path.join(__dirname, '__fixtures__', 'firmware.zip')
);
const password = 'test-password';

const encrypted = (
  archive: Buffer = plainArchive,
  buildPassword: string = password
) => new Blob([encryptFirmware(archive, buildPassword) as Buffer]);

describe('decryptFirmwareArchive', () => {
  it('reads back what the build wrote', async () => {
    const archive = await decryptFirmwareArchive(encrypted(), password);

    expect(Buffer.from(await archive.arrayBuffer())).toEqual(plainArchive);
  });

  it('rejects a wrong password', async () => {
    await expect(
      decryptFirmwareArchive(encrypted(), 'not-the-password')
    ).rejects.toBeInstanceOf(WrongPasswordError);
  });

  it('rejects an archive that was modified in the bucket', async () => {
    const bytes = new Uint8Array(await encrypted().arrayBuffer());
    // A byte of ciphertext, past the iv and the tag.
    bytes[bytes.length - 1] ^= 0xff;

    await expect(
      decryptFirmwareArchive(new Blob([bytes]), password)
    ).rejects.toBeInstanceOf(WrongPasswordError);
  });

  it('rejects an object too short to hold an iv and a tag', async () => {
    await expect(
      decryptFirmwareArchive(new Blob([new Uint8Array(20)]), password)
    ).rejects.toBeInstanceOf(WrongPasswordError);
  });

  it('produces something extractFirmwareBundle can read', async () => {
    // Not jest.fn(): react-scripts enables resetMocks.
    const blobs = new Map<string, Blob>();
    let counter = 0;
    URL.createObjectURL = (blob: Blob) => {
      const url = `blob:crypto/${(counter += 1)}`;
      blobs.set(url, blob);
      return url;
    };
    URL.revokeObjectURL = (url: string) => {
      blobs.delete(url);
    };

    const archive = await decryptFirmwareArchive(encrypted(), password);
    const bundle = await extractFirmwareBundle(archive, {
      name: 'B2500',
      version: '2026.8.1',
    });

    expect(bundle.chipFamily).toBe('ESP32-S3');
    bundle.release();
  });
});
