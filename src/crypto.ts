/**
 * The browser half of the build crypto. `scripts/buildCrypto.js` is the other
 * half and the comment at the top of it describes the scheme; the short version
 * is that each build gets an ephemeral P-256 key pair which never leaves this
 * page, and the dispatch payload carries only its public half.
 *
 * So there is no shared secret to transport, and nothing in the payload is
 * worth stealing on its own.
 */

const CURVE = 'P-256';
const IV_BYTES = 12;
const TAG_BYTES = 16;

const CONFIG_INFO = 'esphome-b2500 config v1';

/**
 * The repo's long-lived public key, as base64 of an uncompressed P-256 point.
 *
 * Deployment configuration rather than a constant in the source: rotating the
 * key pair (see scripts/generateBuildKeypair.js) is then a variable change and
 * a secret change, with no code to redeploy. Unset means the page cannot start
 * builds, which `buildKeyIsConfigured` reports rather than failing later with
 * something cryptic.
 */
const publicKeyBase64 = process.env.REACT_APP_BUILD_PUBLIC_KEY ?? '';

export const buildKeyIsConfigured = () => publicKeyBase64 !== '';

export interface ConfigData {
  secrets: string[];
  config: {
    template_version: string;
    [key: string]: any;
  };
}

/** The ephemeral pair for one build. The private half is never extractable. */
export interface BuildKeyPair {
  privateKey: CryptoKey;
  /** Base64 of the uncompressed point, for the dispatch payload. */
  publicKey: string;
}

const decodeBase64 = (value: string): Uint8Array =>
  Uint8Array.from(atob(value), (character) => character.charCodeAt(0));

const encodeBase64 = (bytes: Uint8Array): string => {
  let binary = '';
  bytes.forEach((byte) => {
    binary += String.fromCharCode(byte);
  });
  return btoa(binary);
};

const importPublicKey = (raw: Uint8Array): Promise<CryptoKey> =>
  crypto.subtle.importKey(
    'raw',
    raw as BufferSource,
    { name: 'ECDH', namedCurve: CURVE },
    false,
    []
  );

/**
 * ECDH against `theirPublicKey`, then HKDF-SHA256 with a per-direction `info`
 * so the config key and the firmware key can never be the same.
 */
export const deriveSharedKey = async (
  privateKey: CryptoKey,
  theirPublicKey: CryptoKey,
  info: string,
  usage: KeyUsage
): Promise<CryptoKey> => {
  const shared = await crypto.subtle.deriveBits(
    { name: 'ECDH', public: theirPublicKey },
    privateKey,
    256
  );
  const hkdfKey = await crypto.subtle.importKey('raw', shared, 'HKDF', false, [
    'deriveBits',
  ]);
  const keyBytes = await crypto.subtle.deriveBits(
    {
      name: 'HKDF',
      hash: 'SHA-256',
      salt: new Uint8Array(0),
      info: new TextEncoder().encode(info),
    },
    hkdfKey,
    256
  );
  return crypto.subtle.importKey('raw', keyBytes, { name: 'AES-GCM' }, false, [
    usage,
  ]);
};

/** Reads the `ephemeral public key || iv || tag || ciphertext` framing. */
export const importRemotePublicKey = (raw: Uint8Array): Promise<CryptoKey> =>
  importPublicKey(raw);

export const generateBuildKeyPair = async (): Promise<BuildKeyPair> => {
  // Not extractable: only the public half ever leaves, and WebCrypto exports
  // public keys regardless of this flag.
  const pair = await crypto.subtle.generateKey(
    { name: 'ECDH', namedCurve: CURVE },
    false,
    ['deriveBits']
  );
  const raw = new Uint8Array(
    await crypto.subtle.exportKey('raw', pair.publicKey)
  );
  return { privateKey: pair.privateKey, publicKey: encodeBase64(raw) };
};

/**
 * Encrypts the configuration to the repo's static key, so the build can render
 * the YAML from it. Returns base64 of `iv || tag || ciphertext`.
 */
export const encryptConfig = async (
  data: ConfigData,
  keyPair: BuildKeyPair
): Promise<string> => {
  if (!buildKeyIsConfigured()) {
    throw new Error('REACT_APP_BUILD_PUBLIC_KEY is not set');
  }
  const key = await deriveSharedKey(
    keyPair.privateKey,
    await importPublicKey(decodeBase64(publicKeyBase64)),
    CONFIG_INFO,
    'encrypt'
  );

  const iv = crypto.getRandomValues(new Uint8Array(IV_BYTES));
  const sealed = new Uint8Array(
    await crypto.subtle.encrypt(
      { name: 'AES-GCM', iv },
      key,
      new TextEncoder().encode(JSON.stringify(data))
    )
  );

  // WebCrypto appends the tag; the build expects it after the iv.
  const ciphertext = sealed.subarray(0, sealed.length - TAG_BYTES);
  const tag = sealed.subarray(sealed.length - TAG_BYTES);
  const payload = new Uint8Array(IV_BYTES + TAG_BYTES + ciphertext.length);
  payload.set(iv);
  payload.set(tag, IV_BYTES);
  payload.set(ciphertext, IV_BYTES + TAG_BYTES);
  return encodeBase64(payload);
};
