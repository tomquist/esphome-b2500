#!/usr/bin/env node
'use strict';

/**
 * Generates the repo's long-lived build key pair.
 *
 * The private half decrypts every configuration ever submitted through the web
 * builder, so rotating it is the one maintenance job this scheme has. Nothing
 * else needs to change to rotate: the public half is deployment configuration,
 * not code.
 *
 *   node scripts/generateBuildKeypair.js
 *
 * Then, in the repository settings:
 *   - Secrets   > BUILD_PRIVATE_KEY   = the PEM below
 *   - Variables > BUILD_PUBLIC_KEY    = the base64 line below
 *
 * Builds started from a page that was deployed with the previous public key
 * fail to decrypt after a rotation, so publish the new page first, or expect
 * the handful of in-flight builds to fail.
 */

const crypto = require('crypto');
const { rawFromPublicKey } = require('./buildCrypto.js');

const { privateKey, publicKey } = crypto.generateKeyPairSync('ec', {
  namedCurve: 'prime256v1',
});

process.stdout.write(
  [
    'BUILD_PRIVATE_KEY (repository secret, never commit this):',
    '',
    privateKey.export({ type: 'pkcs8', format: 'pem' }).trim(),
    '',
    'BUILD_PUBLIC_KEY (repository variable, safe to publish):',
    '',
    rawFromPublicKey(publicKey).toString('base64'),
    '',
  ].join('\n')
);
