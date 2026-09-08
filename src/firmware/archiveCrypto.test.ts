import nodeCrypto from 'node:crypto';

// The runner half. Importing it here is the point of this file: the browser and
// the build have to agree on the curve, the HKDF info strings and the framing,
// and nothing else checks that they still do.
/* eslint-disable @typescript-eslint/no-var-requires */
const {
  openConfig,
  parsePublicKey,
  rawFromPublicKey,
  sealToClient,
} = require('../../scripts/buildCrypto.js');

// The repo's static pair, stood up before src/crypto reads the environment.
const repo = nodeCrypto.generateKeyPairSync('ec', {
  namedCurve: 'prime256v1',
});
process.env.REACT_APP_BUILD_PUBLIC_KEY = rawFromPublicKey(
  repo.publicKey
).toString('base64');

const { encryptConfig, generateBuildKeyPair } = require('../crypto');
const {
  decryptFirmwareArchive,
  UndecryptableArchiveError,
} = require('./archiveCrypto');
const { extractFirmwareBundle } = require('./firmwareBundle');
/* eslint-enable @typescript-eslint/no-var-requires */

const repoPrivateKeyPem = repo.privateKey.export({
  type: 'pkcs8',
  format: 'pem',
});

const clientPublicKey = (keyPair: { publicKey: string }) =>
  parsePublicKey(keyPair.publicKey);

describe('the config the browser sends', () => {
  it('is readable by the build and nothing else', async () => {
    const keyPair = await generateBuildKeyPair();
    const data = {
      secrets: ['hunter2'],
      config: { template_version: 'v2', name: 'b2500' },
    };

    const payload = await encryptConfig(data, keyPair);
    const opened = openConfig(
      payload,
      repoPrivateKeyPem,
      clientPublicKey(keyPair)
    );

    expect(JSON.parse(opened.toString())).toEqual(data);
  });

  it('cannot be opened with a different repo key', async () => {
    const keyPair = await generateBuildKeyPair();
    const payload = await encryptConfig(
      { secrets: [], config: { template_version: 'v2' } },
      keyPair
    );
    const otherKey = nodeCrypto
      .generateKeyPairSync('ec', { namedCurve: 'prime256v1' })
      .privateKey.export({ type: 'pkcs8', format: 'pem' });

    expect(() =>
      openConfig(payload, otherKey, clientPublicKey(keyPair))
    ).toThrow();
  });

  it('carries only a public key, never the private half', async () => {
    const keyPair = await generateBuildKeyPair();

    // What goes on the wire is 65 bytes starting with the uncompressed marker.
    const raw = Buffer.from(keyPair.publicKey, 'base64');
    expect(raw.length).toBe(65);
    expect(raw[0]).toBe(0x04);
    // And the private half is a non-extractable CryptoKey.
    expect(keyPair.privateKey.extractable).toBe(false);
  });
});

describe('the firmware the build publishes', () => {
  const archive = Buffer.from('a plain ZIP would go here');

  it('is readable by the browser that asked for the build', async () => {
    const keyPair = await generateBuildKeyPair();
    const sealed = sealToClient(archive, clientPublicKey(keyPair));

    const plain = await decryptFirmwareArchive(
      new Blob([sealed]),
      keyPair.privateKey
    );

    expect(Buffer.from(await plain.arrayBuffer())).toEqual(archive);
  });

  it('is not readable by another build', async () => {
    const keyPair = await generateBuildKeyPair();
    const other = await generateBuildKeyPair();
    const sealed = sealToClient(archive, clientPublicKey(keyPair));

    await expect(
      decryptFirmwareArchive(new Blob([sealed]), other.privateKey)
    ).rejects.toBeInstanceOf(UndecryptableArchiveError);
  });

  it('is refused when it was modified in the bucket', async () => {
    const keyPair = await generateBuildKeyPair();
    const sealed = Buffer.from(sealToClient(archive, clientPublicKey(keyPair)));
    sealed[sealed.length - 1] ^= 0xff;

    await expect(
      decryptFirmwareArchive(new Blob([sealed]), keyPair.privateKey)
    ).rejects.toBeInstanceOf(UndecryptableArchiveError);
  });

  it('is refused when its key header is not a point on the curve', async () => {
    const keyPair = await generateBuildKeyPair();
    const sealed = Buffer.from(sealToClient(archive, clientPublicKey(keyPair)));
    sealed.fill(0xaa, 1, 65);

    await expect(
      decryptFirmwareArchive(new Blob([sealed]), keyPair.privateKey)
    ).rejects.toBeInstanceOf(UndecryptableArchiveError);
  });

  it('is refused when it is too short to hold a header', async () => {
    const keyPair = await generateBuildKeyPair();

    await expect(
      decryptFirmwareArchive(new Blob([new Uint8Array(80)]), keyPair.privateKey)
    ).rejects.toBeInstanceOf(UndecryptableArchiveError);
  });

  it('comes back as something extractFirmwareBundle can read', async () => {
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

    const fs = require('fs');
    const path = require('path');
    const zip = fs.readFileSync(
      path.join(__dirname, '__fixtures__', 'firmware.zip')
    );
    const keyPair = await generateBuildKeyPair();
    const sealed = sealToClient(zip, clientPublicKey(keyPair));

    const plain = await decryptFirmwareArchive(
      new Blob([sealed]),
      keyPair.privateKey
    );
    const bundle = await extractFirmwareBundle(plain, {
      name: 'B2500',
      version: '2026.8.1',
    });

    expect(bundle.chipFamily).toBe('ESP32-S3');
    bundle.release();
  });
});
