// Template rendering, for the two properties the YAML depends on that nothing
// else covers: that requester strings are escaped where they land in a quoted
// scalar, and that the `!secret` fallback for a missing MAC emits a real YAML
// tag rather than a quoted literal.
//
// The PR job builds every example, but all of them supply a MAC, so the
// fallback branch has never been exercised anywhere.
//
//   node --test scripts/renderTemplates.test.mjs

import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const nunjucks = require('nunjucks');
// The real one, not a copy: a render.js that spread the config last would be
// the git_sha vulnerability again, and a private copy here would stay green.
const { buildRenderContext } = require('./render.js');

const templatePath = (name) =>
  new URL(`../src/${name}`, import.meta.url).pathname;

const TEMPLATES = {
  v1: templatePath('template.jinja2'),
  v2: templatePath('template_v2.jinja2'),
  'v2-minimal': templatePath('template_v2_minimal.jinja2'),
};

const baseConfig = () => ({
  name: 'b2500',
  friendly_name: 'B2500',
  wifi: { ssid: 'net', password: 'secret' },
  mqtt: { enabled: false, topic: 'b2500', broker: 'mqtt.local', port: 1883 },
  storages: [{ name: 'Battery', version: 2, mac_address: '00:11:22:33:44:55' }],
});

const render = (version, config) =>
  nunjucks.configure({ autoescape: false }).renderString(
    fs.readFileSync(TEMPLATES[version], 'utf-8'),
    buildRenderContext(config, {
      GITHUB_SHA: 'trusted-sha',
      AUTOMATED_BUILD: 'true',
    })
  );

for (const version of Object.keys(TEMPLATES)) {
  test(`${version}: a MAC is emitted as a quoted scalar`, () => {
    const yaml = render(version, baseConfig());
    assert.match(yaml, /^ {2}- mac_address: "00:11:22:33:44:55"$/m);
  });

  test(`${version}: a missing MAC falls back to a real !secret tag`, () => {
    const config = baseConfig();
    delete config.storages[0].mac_address;
    const yaml = render(version, config);

    // Unquoted, or YAML reads it as the literal string "!secret …" - which is
    // what the old `| default(...)` produced, so the fallback never worked.
    assert.match(yaml, /^ {2}- mac_address: !secret hm2500_1_mac$/m);
    assert.doesNotMatch(yaml, /mac_address: "!secret/);
  });

  test(`${version}: quotes and backslashes in a MAC are escaped`, () => {
    const config = baseConfig();
    config.storages[0].mac_address = 'AA"BB\\CC';
    const yaml = render(version, config);

    assert.match(yaml, /^ {2}- mac_address: "AA\\"BB\\\\CC"$/m);
  });
}

// v1 has no external_components block, so no ref to poison.
for (const version of ['v2', 'v2-minimal']) {
  test(`${version}: the trusted git ref wins over one from the config`, () => {
    // Through render.js's own context builder, so this pins the layer that
    // holds even if validateConfig's reserved-key check were removed.
    const config = baseConfig();
    config.git_sha = 'attacker-branch';
    const yaml = render(version, config);

    assert.match(yaml, /^ {6}ref: "trusted-sha"$/m);
    assert.doesNotMatch(yaml, /attacker-branch/);
  });
}
