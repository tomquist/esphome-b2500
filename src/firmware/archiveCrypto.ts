/**
 * Decrypts the firmware archive the build published.
 *
 * The build encrypts the whole ZIP with AES-256-GCM under sha256 of the build
 * password (see `scripts/encryptFirmware.js`), rather than relying on the ZIP
 * format's own encryption, which known plaintext breaks. The framing is the
 * same one `src/crypto.ts` uses for the config in the other direction:
 *
 *     iv (12 bytes) || auth tag (16 bytes) || ciphertext
 *
 * WebCrypto wants the tag appended to the ciphertext instead, so it is moved
 * before decrypting.
 */

const IV_BYTES = 12;
const TAG_BYTES = 16;

export class WrongPasswordError extends Error {
  constructor() {
    super(
      'The firmware archive could not be decrypted with the build password.'
    );
    this.name = 'WrongPasswordError';
  }
}

const archiveKey = async (password: string): Promise<CryptoKey> => {
  const digest = await crypto.subtle.digest(
    'SHA-256',
    new TextEncoder().encode(password)
  );
  return crypto.subtle.importKey('raw', digest, { name: 'AES-GCM' }, false, [
    'decrypt',
  ]);
};

/** Returns the plain ZIP, or throws {@link WrongPasswordError}. */
export const decryptFirmwareArchive = async (
  encrypted: Blob,
  password: string
): Promise<Blob> => {
  const bytes = new Uint8Array(await encrypted.arrayBuffer());
  if (bytes.length <= IV_BYTES + TAG_BYTES) {
    throw new WrongPasswordError();
  }

  const iv = bytes.subarray(0, IV_BYTES);
  const tag = bytes.subarray(IV_BYTES, IV_BYTES + TAG_BYTES);
  const ciphertext = bytes.subarray(IV_BYTES + TAG_BYTES);
  const payload = new Uint8Array(ciphertext.length + tag.length);
  payload.set(ciphertext);
  payload.set(tag, ciphertext.length);

  // Only the decrypt call is caught: a failure to derive the key is a bug or a
  // missing WebCrypto, not an archive we should report as undecryptable.
  const key = await archiveKey(password);
  let plain: ArrayBuffer;
  try {
    plain = await crypto.subtle.decrypt({ name: 'AES-GCM', iv }, key, payload);
  } catch (error) {
    // GCM fails closed, and it cannot tell a wrong key from a modified
    // archive. Either way the only thing to do is not flash it.
    throw new WrongPasswordError();
  }
  return new Blob([plain]);
};
