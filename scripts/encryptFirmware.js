'use strict';

/**
 * Encrypts the firmware archive for the public bucket.
 *
 * The archive used to be protected by the ZIP format's own encryption
 * (`zip -P`), which is PKWARE's stream cipher: a dozen bytes of known plaintext
 * recover the internal keys and unlock the whole archive. A firmware archive is
 * close to worst case for that - anyone can run a build of their own at the
 * pinned ESPHome version and get byte-identical bootloader and partition
 * images to use as that plaintext.
 *
 * So the ZIP is written plain and sealed as a whole to the ephemeral public key
 * the browser sent with the build request. This step holds no secret: it makes
 * a throwaway key pair of its own and ships the public half in the header, so
 * once the job ends nothing on the runner can read what it published.
 *
 * See `scripts/buildCrypto.js` for the framing and the derivation, and
 * `src/firmware/archiveCrypto.ts` for the browser side.
 */

const fs = require('fs');
const { parsePublicKey, sealToClient } = require('./buildCrypto');

if (require.main === module) {
  const [input, output] = process.argv.slice(2);
  if (!input || !output) {
    console.error('usage: encryptFirmware.js <input> <output>');
    process.exit(2);
  }
  const clientPublicKey = parsePublicKey(process.env.CLIENT_PUBLIC_KEY);
  fs.writeFileSync(
    output,
    sealToClient(fs.readFileSync(input), clientPublicKey)
  );
}
