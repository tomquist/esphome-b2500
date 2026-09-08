import test from 'node:test';
import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import fs from 'node:fs';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const {
  FIRMWARE_INFO,
  InvalidPublicKeyError,
  PUBLIC_KEY_BYTES,
  openConfig,
  parsePublicKey,
  publicKeyFromRaw,
  rawFromPublicKey,
  sealToClient,
} = require('./buildCrypto.js');

const keyPair = () => crypto.generateKeyPairSync('ec', { namedCurve: 'prime256v1' });

const pem = (key) => key.export({ type: 'pkcs8', format: 'pem' });

// What the browser does to produce client_payload.config.
const sealConfig = (plaintext, clientPrivate, repoPublic, info) => {
  const shared = crypto.diffieHellman({
    privateKey: clientPrivate,
    publicKey: repoPublic,
  });
  const key = Buffer.from(
    crypto.hkdfSync('sha256', shared, Buffer.alloc(0), Buffer.from(info), 32)
  );
  const iv = crypto.randomBytes(12);
  const cipher = crypto.createCipheriv('aes-256-gcm', key, iv);
  const ciphertext = Buffer.concat([cipher.update(plaintext), cipher.final()]);
  return Buffer.concat([iv, cipher.getAuthTag(), ciphertext]).toString(
    'base64'
  );
};

test('a raw public key survives a round trip', () => {
  const { publicKey } = keyPair();
  const raw = rawFromPublicKey(publicKey);

  assert.equal(raw.length, PUBLIC_KEY_BYTES);
  assert.equal(raw[0], 0x04);
  assert.deepEqual(rawFromPublicKey(publicKeyFromRaw(raw)), raw);
});

test('openConfig reads what the client sealed', () => {
  const repo = keyPair();
  const client = keyPair();
  const config = JSON.stringify({ config: { template_version: 'v2' } });

  const payload = sealConfig(
    Buffer.from(config),
    client.privateKey,
    repo.publicKey,
    'esphome-b2500 config v1'
  );

  assert.equal(
    openConfig(
      payload,
      pem(repo.privateKey),
      parsePublicKey(rawFromPublicKey(client.publicKey).toString('base64'))
    ).toString(),
    config
  );
});

test('openConfig rejects a payload sealed to a different repo key', () => {
  const repo = keyPair();
  const client = keyPair();

  const payload = sealConfig(
    Buffer.from('{}'),
    client.privateKey,
    keyPair().publicKey,
    'esphome-b2500 config v1'
  );

  assert.throws(() =>
    openConfig(
      payload,
      pem(repo.privateKey),
      parsePublicKey(rawFromPublicKey(client.publicKey).toString('base64'))
    )
  );
});

test('the two directions derive different keys', () => {
  const repo = keyPair();
  const client = keyPair();

  // The same ECDH secret with the firmware info must not open the config.
  const payload = sealConfig(
    Buffer.from('{}'),
    client.privateKey,
    repo.publicKey,
    FIRMWARE_INFO
  );

  assert.throws(() =>
    openConfig(
      payload,
      pem(repo.privateKey),
      parsePublicKey(rawFromPublicKey(client.publicKey).toString('base64'))
    )
  );
});

test('sealToClient prefixes a fresh public key every time', () => {
  const client = keyPair();
  const clientPublic = parsePublicKey(
    rawFromPublicKey(client.publicKey).toString('base64')
  );

  const first = sealToClient(Buffer.from('firmware'), clientPublic);
  const second = sealToClient(Buffer.from('firmware'), clientPublic);

  assert.equal(first[0], 0x04);
  assert.notDeepEqual(
    first.subarray(0, PUBLIC_KEY_BYTES),
    second.subarray(0, PUBLIC_KEY_BYTES)
  );
});

const badKeys = {
  'an empty value': '',
  'not base64 of 65 bytes': Buffer.alloc(10).toString('base64'),
  'a compressed point': Buffer.concat([
    Buffer.from([0x02]),
    Buffer.alloc(64),
  ]).toString('base64'),
  'a point that is not on the curve': Buffer.concat([
    Buffer.from([0x04]),
    Buffer.alloc(64, 0xaa),
  ]).toString('base64'),
};

for (const [name, value] of Object.entries(badKeys)) {
  test(`parsePublicKey rejects ${name}`, () => {
    assert.throws(() => parsePublicKey(value), InvalidPublicKeyError);
  });
}

// The workflow rejects a malformed payload before it spends five minutes on a
// compile, which only helps if its pattern actually matches a real key. Reading
// it back out of the workflow is the only way that stays true: a base64 length
// is easy to get wrong by one character, and nothing else would notice.
test('the workflow pattern accepts the keys the browser produces', () => {
  const workflow = fs.readFileSync(
    new URL('../.github/workflows/build-esphome.yml', import.meta.url),
    'utf-8'
  );
  const declared = workflow.match(
    /CLIENT_PUBLIC_KEY" =~ \^(?<pattern>\S+)\s*\]\]/
  );
  assert.ok(declared, 'no CLIENT_PUBLIC_KEY check found in the workflow');
  const pattern = new RegExp(`^${declared.groups.pattern}`);

  for (let index = 0; index < 20; index += 1) {
    const { publicKey } = keyPair();
    assert.match(rawFromPublicKey(publicKey).toString('base64'), pattern);
  }
});
