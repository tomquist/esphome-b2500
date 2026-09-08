'use strict';

/**
 * The crypto the build shares with the browser.
 *
 * Every build is keyed by an ephemeral P-256 key pair the browser generates and
 * never transmits. What the dispatch payload carries is only its public half,
 * so the payload holds no secret at all and the build never has to move one
 * through a shell variable.
 *
 * Two directions, each with its own derived key:
 *
 *   config    browser ephemeral private <-> repo static public
 *             The runner has to read the config to render the YAML, so this one
 *             is addressed to the repo's long-lived key. `BUILD_PRIVATE_KEY` is
 *             the only secret in the build, and only the render step needs it.
 *
 *   firmware  runner ephemeral private <-> browser ephemeral public
 *             Nothing but the browser can read this, and the step that writes
 *             it holds no secret: it makes a throwaway key pair and ships the
 *             public half in the object header. The runner cannot decrypt what
 *             it published once the job is over.
 *
 * Both derive AES-256-GCM keys through HKDF-SHA256 over the ECDH shared secret,
 * with a per-direction `info` string so the two can never collide.
 * `src/crypto.ts` and `src/firmware/archiveCrypto.ts` are the browser halves;
 * `src/firmware/buildCrypto.test.ts` runs them against this file.
 */

const crypto = require('crypto');

const CURVE_WEBCRYPTO = 'P-256';
const CURVE_NODE = 'prime256v1';

// 0x04 || X (32) || Y (32) - what WebCrypto's "raw" export produces.
const PUBLIC_KEY_BYTES = 65;
const IV_BYTES = 12;
const TAG_BYTES = 16;
const KEY_BYTES = 32;

const CONFIG_INFO = 'esphome-b2500 config v1';
const FIRMWARE_INFO = 'esphome-b2500 firmware v1';

class InvalidPublicKeyError extends Error {
  constructor(message) {
    super(message);
    this.name = 'InvalidPublicKeyError';
  }
}

/** Reads an uncompressed P-256 point, as JWK so no DER prefix has to be faked. */
const publicKeyFromRaw = (raw) => {
  if (!Buffer.isBuffer(raw) || raw.length !== PUBLIC_KEY_BYTES) {
    throw new InvalidPublicKeyError(
      `A public key must be ${PUBLIC_KEY_BYTES} bytes, got ${
        Buffer.isBuffer(raw) ? raw.length : typeof raw
      }`
    );
  }
  if (raw[0] !== 0x04) {
    throw new InvalidPublicKeyError('Expected an uncompressed P-256 point');
  }
  try {
    return crypto.createPublicKey({
      key: {
        kty: 'EC',
        crv: CURVE_WEBCRYPTO,
        x: raw.subarray(1, 33).toString('base64url'),
        y: raw.subarray(33, 65).toString('base64url'),
      },
      format: 'jwk',
    });
  } catch (error) {
    // Node rejects a point that is not on the curve, which is the check that
    // matters here: an invalid-curve point could otherwise leak key material.
    throw new InvalidPublicKeyError('Not a valid P-256 public key');
  }
};

const rawFromPublicKey = (publicKey) => {
  const { x, y } = publicKey.export({ format: 'jwk' });
  return Buffer.concat([
    Buffer.from([0x04]),
    Buffer.from(x, 'base64url'),
    Buffer.from(y, 'base64url'),
  ]);
};

/** Parses the base64 public key as it arrives in the dispatch payload. */
const parsePublicKey = (base64) => {
  if (typeof base64 !== 'string' || base64 === '') {
    throw new InvalidPublicKeyError('No public key was supplied');
  }
  return publicKeyFromRaw(Buffer.from(base64, 'base64'));
};

const deriveKey = (privateKey, publicKey, info) => {
  const shared = crypto.diffieHellman({ privateKey, publicKey });
  return Buffer.from(
    crypto.hkdfSync(
      'sha256',
      shared,
      Buffer.alloc(0),
      Buffer.from(info),
      KEY_BYTES
    )
  );
};

const encrypt = (key, plaintext) => {
  const iv = crypto.randomBytes(IV_BYTES);
  const cipher = crypto.createCipheriv('aes-256-gcm', key, iv);
  const ciphertext = Buffer.concat([cipher.update(plaintext), cipher.final()]);
  return Buffer.concat([iv, cipher.getAuthTag(), ciphertext]);
};

const decrypt = (key, blob) => {
  if (blob.length <= IV_BYTES + TAG_BYTES) {
    throw new Error('The encrypted payload is too short');
  }
  const decipher = crypto.createDecipheriv(
    'aes-256-gcm',
    key,
    blob.subarray(0, IV_BYTES)
  );
  decipher.setAuthTag(blob.subarray(IV_BYTES, IV_BYTES + TAG_BYTES));
  return Buffer.concat([
    decipher.update(blob.subarray(IV_BYTES + TAG_BYTES)),
    decipher.final(),
  ]);
};

/**
 * Decrypts the config the browser addressed to the repo's static key. Throws if
 * the payload was not produced by the holder of `clientPublicKey`.
 */
const openConfig = (encryptedConfig, privateKeyPem, clientPublicKey) => {
  const key = deriveKey(
    crypto.createPrivateKey(privateKeyPem),
    clientPublicKey,
    CONFIG_INFO
  );
  return decrypt(key, Buffer.from(encryptedConfig, 'base64'));
};

/**
 * Encrypts to the browser's ephemeral public key under a throwaway key pair,
 * and prefixes its public half:
 *
 *     ephemeral public key (65) || iv (12) || auth tag (16) || ciphertext
 */
const sealToClient = (plaintext, clientPublicKey) => {
  const ephemeral = crypto.generateKeyPairSync('ec', {
    namedCurve: CURVE_NODE,
  });
  const key = deriveKey(ephemeral.privateKey, clientPublicKey, FIRMWARE_INFO);
  return Buffer.concat([
    rawFromPublicKey(ephemeral.publicKey),
    encrypt(key, plaintext),
  ]);
};

module.exports = {
  CONFIG_INFO,
  FIRMWARE_INFO,
  IV_BYTES,
  InvalidPublicKeyError,
  KEY_BYTES,
  PUBLIC_KEY_BYTES,
  TAG_BYTES,
  openConfig,
  parsePublicKey,
  publicKeyFromRaw,
  rawFromPublicKey,
  sealToClient,
};
