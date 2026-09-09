/**
 * Decrypts the firmware archive the build published.
 *
 * The build seals the archive to this page's ephemeral public key under a
 * throwaway key pair of its own, and prefixes that pair's public half, which is
 * also bound into the key derivation (`scripts/buildCrypto.js`, `sealToClient`):
 *
 *     ephemeral public key (65) || iv (12) || auth tag (16) || ciphertext
 *
 * Only this page can open it, and only while it still holds the private half of
 * the pair it generated for this build - the runner cannot decrypt what it
 * published once the job is over.
 */

import { deriveSharedKey, encodeInfo, importRemotePublicKey } from '../crypto';

const PUBLIC_KEY_BYTES = 65;
const IV_BYTES = 12;
const TAG_BYTES = 16;
const HEADER_BYTES = PUBLIC_KEY_BYTES + IV_BYTES + TAG_BYTES;

const FIRMWARE_INFO = 'esphome-b2500 firmware v1';

export class UndecryptableArchiveError extends Error {
  constructor() {
    super(
      'The firmware archive does not belong to this build, or was modified.'
    );
    this.name = 'UndecryptableArchiveError';
  }
}

/** Returns the plain ZIP, or throws {@link UndecryptableArchiveError}. */
export const decryptFirmwareArchive = async (
  encrypted: Blob,
  privateKey: CryptoKey
): Promise<Blob> => {
  const bytes = new Uint8Array(await encrypted.arrayBuffer());
  if (bytes.length <= HEADER_BYTES) {
    throw new UndecryptableArchiveError();
  }

  const senderKey = bytes.subarray(0, PUBLIC_KEY_BYTES);
  const iv = bytes.subarray(PUBLIC_KEY_BYTES, PUBLIC_KEY_BYTES + IV_BYTES);
  const tag = bytes.subarray(PUBLIC_KEY_BYTES + IV_BYTES, HEADER_BYTES);
  const ciphertext = bytes.subarray(HEADER_BYTES);

  // WebCrypto wants the tag after the ciphertext.
  const payload = new Uint8Array(ciphertext.length + tag.length);
  payload.set(ciphertext);
  payload.set(tag, ciphertext.length);

  // A header that is not a point on the curve is an unusable archive rather
  // than a bug, so this is inside the same guard as the decryption itself.
  let plain: ArrayBuffer;
  try {
    const key = await deriveSharedKey(
      privateKey,
      await importRemotePublicKey(senderKey),
      // The sender's public key is bound into the derivation, so a swapped
      // header cannot be paired with a ciphertext it did not travel with.
      encodeInfo(FIRMWARE_INFO, senderKey),
      'decrypt'
    );
    plain = await crypto.subtle.decrypt({ name: 'AES-GCM', iv }, key, payload);
  } catch (error) {
    // GCM fails closed, and it cannot tell an archive from another build from
    // one that was modified. Either way the only thing to do is not flash it.
    throw new UndecryptableArchiveError();
  }
  return new Blob([plain]);
};
