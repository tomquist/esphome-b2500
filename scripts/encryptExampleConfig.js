#!/usr/bin/env node
'use strict';

/**
 * Builds a throwaway dispatch payload from an example config, so the PR job can
 * exercise the real render and packaging path without a repository secret.
 *
 * Both key pairs are generated here and thrown away with the runner: this
 * stands in for the browser and for the repository at once.
 *
 *   node scripts/encryptExampleConfig.js <config.json> <output directory>
 *
 * Writes `private_key.pem`, `client_public_key.txt` and `encrypted_config.txt`.
 */

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const { CONFIG_INFO, rawFromPublicKey } = require('./buildCrypto');

const IV_BYTES = 12;

const [configPath, outputDirectory] = process.argv.slice(2);
if (!configPath || !outputDirectory) {
  console.error(
    'usage: encryptExampleConfig.js <config.json> <output directory>'
  );
  process.exit(2);
}

const repo = crypto.generateKeyPairSync('ec', { namedCurve: 'prime256v1' });
const client = crypto.generateKeyPairSync('ec', { namedCurve: 'prime256v1' });

const shared = crypto.diffieHellman({
  privateKey: client.privateKey,
  publicKey: repo.publicKey,
});
const key = Buffer.from(
  crypto.hkdfSync(
    'sha256',
    shared,
    Buffer.alloc(0),
    Buffer.from(CONFIG_INFO),
    32
  )
);

const payload = {
  secrets: [],
  config: JSON.parse(fs.readFileSync(configPath, 'utf-8')),
};
const iv = crypto.randomBytes(IV_BYTES);
const cipher = crypto.createCipheriv('aes-256-gcm', key, iv);
const ciphertext = Buffer.concat([
  cipher.update(Buffer.from(JSON.stringify(payload))),
  cipher.final(),
]);

fs.mkdirSync(outputDirectory, { recursive: true });
fs.writeFileSync(
  path.join(outputDirectory, 'private_key.pem'),
  repo.privateKey.export({ type: 'pkcs8', format: 'pem' })
);
fs.writeFileSync(
  path.join(outputDirectory, 'client_public_key.txt'),
  rawFromPublicKey(client.publicKey).toString('base64')
);
fs.writeFileSync(
  path.join(outputDirectory, 'encrypted_config.txt'),
  Buffer.concat([iv, cipher.getAuthTag(), ciphertext]).toString('base64')
);
