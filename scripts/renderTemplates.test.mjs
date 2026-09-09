// render.js, for the parts nothing else covers:
//
//   - requester strings are escaped where they land in a quoted YAML scalar,
//   - the `!secret` fallback for a missing MAC emits a real YAML tag rather
//     than a quoted literal (no example supplies a config without a MAC, so
//     the PR job never exercises that branch),
//   - the trusted git ref wins over one supplied in the config,
//   - and secrets are registered for masking without letting the payload end
//     the workflow command.
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
const { buildRenderContext, MAX_SECRETS, maskSecrets } = require('./render.js');

// The URL itself, not its pathname: readFileSync takes a file: URL, and
// .pathname would yield /C:/... on Windows. Matches the sibling tests.
const templatePath = (name) => new URL(`../src/${name}`, import.meta.url);

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

// maskSecrets guards a public log against the requester's own payload, and
// until now it ran only inside render.js's main() where nothing could reach it.
const masked = (secrets) => {
  const lines = [];
  maskSecrets(secrets, (line) => lines.push(line));
  return lines;
};

test('escapes the characters the runner unescapes', () => {
  // A raw % would register a mask for a different string than the payload
  // holds, and a secret containing %25 would then not be redacted at all.
  assert.deepEqual(masked(['100%25pure', 'a%b']), [
    '::add-mask::100%2525pure',
    '::add-mask::a%25b',
  ]);
});

test('masks every secret it is given, however short', () => {
  // The old code masked anything non-empty; a length floor here would publish
  // short passwords.
  assert.deepEqual(masked(['ab', 'a longer one']), [
    '::add-mask::ab',
    '::add-mask::a longer one',
  ]);
});

test('skips blank and non-string entries', () => {
  assert.deepEqual(masked(['', '   ', 42, null, undefined, {}]), []);
});

test('tolerates a missing list', () => {
  assert.deepEqual(masked(undefined), []);
  assert.deepEqual(masked(null), []);
});

test('rejects a list that is not one', () => {
  // A string would iterate its characters and mask single letters everywhere.
  assert.throws(() => masked('abcd'), /secrets must be an array/);
});

test('caps how many it registers', () => {
  const many = Array.from(
    { length: MAX_SECRETS + 10 },
    (_, i) => `secret-${i}`
  );
  assert.equal(masked(many).length, MAX_SECRETS);
});

test('cannot be made to emit a second workflow command', () => {
  // A newline would end the ::add-mask:: and run the rest as its own command;
  // ::stop-commands:: would then silence every mask after it.
  const lines = masked(['aaa\n::stop-commands::x\nbbb']);

  assert.equal(lines.length, 1);
  assert.doesNotMatch(lines[0], /\n/);
  // Escaped the way the runner unescapes it, so the mask registers the real
  // value rather than a mangled one.
  assert.equal(lines[0], '::add-mask::aaa%0A::stop-commands::x%0Abbb');
});
