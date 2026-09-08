'use strict';

/**
 * Encrypts the firmware archive before it goes to the public bucket.
 *
 * The archive used to be protected by the ZIP format's own encryption
 * (`zip -P`), which is PKWARE's stream cipher: a dozen bytes of known plaintext
 * recover the internal keys and unlock the whole archive. A firmware archive is
 * close to worst case for that - anyone can run a build of their own at the
 * pinned ESPHome version and get byte-identical bootloader and partition
 * images, and a manifest.json that differs only in a name - so the archive
 * password was never the barrier it looked like.
 *
 * So the ZIP is now written plain and encrypted as a whole with AES-256-GCM,
 * under the same key the browser already uses for the config: sha256 of the
 * build password. `src/crypto.ts` writes the config the same way and this is
 * its mirror image, so the framing below has to stay in step with both it and
 * `src/firmware/archiveCrypto.ts`:
 *
 *     iv (12 bytes) || auth tag (16 bytes) || ciphertext
 *
 * GCM also authenticates, which the ZIP cipher does not: the browser now
 * rejects an archive that was modified in the bucket instead of flashing it.
 */

const crypto = require('crypto');
const fs = require('fs');

const IV_BYTES = 12;

/** Returns `iv || tag || ciphertext` for the given plaintext. */
const encryptFirmware = (plaintext, password) => {
  if (typeof password !== 'string' || password === '') {
    throw new Error('A build password is required');
  }
  const key = crypto.createHash('sha256').update(password).digest();
  const iv = crypto.randomBytes(IV_BYTES);
  const cipher = crypto.createCipheriv('aes-256-gcm', key, iv);
  const ciphertext = Buffer.concat([cipher.update(plaintext), cipher.final()]);
  return Buffer.concat([iv, cipher.getAuthTag(), ciphertext]);
};

module.exports = { encryptFirmware, IV_BYTES };

if (require.main === module) {
  const [input, output] = process.argv.slice(2);
  if (!input || !output) {
    console.error('usage: encryptFirmware.js <input> <output>');
    process.exit(2);
  }
  // From the environment rather than argv: a command line is visible to every
  // other process on the runner.
  const password = process.env.PASSWORD;
  if (!password) {
    console.error('PASSWORD is not set');
    process.exit(2);
  }
  fs.writeFileSync(output, encryptFirmware(fs.readFileSync(input), password));
}
