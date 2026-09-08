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
const { rawFromPublicKey, sealToRepo } = require('./buildCrypto');

const [configPath, outputDirectory] = process.argv.slice(2);
if (!configPath || !outputDirectory) {
  console.error(
    'usage: encryptExampleConfig.js <config.json> <output directory>'
  );
  process.exit(2);
}

const repo = crypto.generateKeyPairSync('ec', { namedCurve: 'prime256v1' });
const client = crypto.generateKeyPairSync('ec', { namedCurve: 'prime256v1' });

const payload = {
  secrets: [],
  config: JSON.parse(fs.readFileSync(configPath, 'utf-8')),
};
// Through buildCrypto rather than a second copy of the framing, so a change to
// the derivation or the layout cannot leave this file quietly wrong.
const encryptedConfig = sealToRepo(
  Buffer.from(JSON.stringify(payload)),
  repo.publicKey,
  client.privateKey
);

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
  encryptedConfig.toString('base64')
);
