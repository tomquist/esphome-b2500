import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { createVerify, generateKeyPairSync } from 'node:crypto';

import {
  IDENTIFIER_PATTERN,
  PUBLIC_KEY_PATTERN,
  MAX_BODY_LENGTH,
  RESPONSE_MARGIN_MS,
  MAX_CONFIG_LENGTH,
  createAppJwt,
  createHandler,
  createTokenSource,
  parseBuildRequest,
} from './index.mjs';

const PUBLIC_KEY = `B${'A'.repeat(86)}=`;
const CONFIG = Buffer.from('sealed configuration bytes!').toString('base64');
const VALID = {
  identifier: 'happy-fluffy-otter-0123456789abcdef01234567',
  config: CONFIG,
  public_key: PUBLIC_KEY,
};

const post = (body, extra = {}) => ({
  requestContext: { http: { method: 'POST' } },
  body: typeof body === 'string' ? body : JSON.stringify(body),
  isBase64Encoded: false,
  ...extra,
});

const silent = { log() {}, error() {} };

const jsonResponse = (status, body) =>
  new Response(body === undefined ? null : JSON.stringify(body), { status });

// A fetch that records what it was asked and answers from a list of responses.
const recordingFetch = (...responses) => {
  const calls = [];
  const fetchImpl = async (url, init = {}) => {
    calls.push({ url, ...init });
    const next = responses.shift();
    if (next instanceof Error) throw next;
    return next;
  };
  return { calls, fetchImpl };
};

const makeHandler = (fetchImpl, getToken = async () => 'installation-token') =>
  createHandler({
    owner: 'tomquist',
    repo: 'esphome-b2500',
    workflow: 'build-esphome.yml',
    ref: 'main',
    getToken,
    fetchImpl,
    log: silent,
  });

test('a valid request becomes a workflow_dispatch of the build workflow', async () => {
  const { calls, fetchImpl } = recordingFetch(jsonResponse(204));
  const response = await makeHandler(fetchImpl)(post(VALID));

  assert.equal(response.statusCode, 202);
  assert.deepEqual(JSON.parse(response.body), { identifier: VALID.identifier });
  assert.equal(calls.length, 1);
  assert.equal(
    calls[0].url,
    'https://api.github.com/repos/tomquist/esphome-b2500/actions/workflows/build-esphome.yml/dispatches'
  );
  assert.equal(calls[0].method, 'POST');
  assert.equal(calls[0].headers.Authorization, 'Bearer installation-token');
  assert.deepEqual(JSON.parse(calls[0].body), { ref: 'main', inputs: VALID });
});

test('GitHub answering 200 with run details also counts as dispatched', async () => {
  const { fetchImpl } = recordingFetch(
    jsonResponse(200, { workflow_run_id: 1 })
  );
  const response = await makeHandler(fetchImpl)(post(VALID));
  assert.equal(response.statusCode, 202);
});

test('a base64-encoded body is decoded before it is checked', async () => {
  const { calls, fetchImpl } = recordingFetch(jsonResponse(204));
  const event = post(Buffer.from(JSON.stringify(VALID)).toString('base64'), {
    isBase64Encoded: true,
  });
  const response = await makeHandler(fetchImpl)(event);
  assert.equal(response.statusCode, 202);
  assert.deepEqual(JSON.parse(calls[0].body).inputs, VALID);
});

test('anything but POST is refused without calling GitHub', async () => {
  const { calls, fetchImpl } = recordingFetch();
  for (const method of ['GET', 'PUT', 'DELETE']) {
    const response = await makeHandler(fetchImpl)({
      requestContext: { http: { method } },
    });
    assert.equal(response.statusCode, 405);
  }
  assert.equal(calls.length, 0);
});

test('an oversized body is refused before it is parsed', async () => {
  const { calls, fetchImpl } = recordingFetch();
  const response = await makeHandler(fetchImpl)(
    post('x'.repeat(MAX_BODY_LENGTH + 1))
  );
  assert.equal(response.statusCode, 413);
  assert.equal(calls.length, 0);
});

test('malformed requests are refused without calling GitHub', async () => {
  const cases = [
    ['not json', 'not json'],
    ['null', 'null'],
    ['an array', '[]'],
    ['a missing identifier', { ...VALID, identifier: undefined }],
    ['an identifier with a slash', { ...VALID, identifier: '../x' }],
    ['an identifier with upper case', { ...VALID, identifier: 'Happy' }],
    ['an over-long identifier', { ...VALID, identifier: 'a'.repeat(65) }],
    ['an identifier that is not a string', { ...VALID, identifier: 42 }],
    ['a short public key', { ...VALID, public_key: 'AAAA' }],
    [
      'a public key with a newline',
      { ...VALID, public_key: `${PUBLIC_KEY}\n` },
    ],
    ['an empty config', { ...VALID, config: '' }],
    ['a config that is not base64', { ...VALID, config: 'not base64!' }],
    ['a config of the wrong length', { ...VALID, config: 'QUJDQ' }],
    [
      'an over-long config',
      { ...VALID, config: 'A'.repeat(MAX_CONFIG_LENGTH + 4) },
    ],
    [
      'the old dispatch envelope',
      { event_type: 'build_esphome', client_payload: VALID },
    ],
    ['an extra field', { ...VALID, ref: 'refs/heads/evil' }],
  ];
  for (const [name, body] of cases) {
    const { calls, fetchImpl } = recordingFetch();
    const response = await makeHandler(fetchImpl)(post(body));
    assert.equal(response.statusCode, 400, name);
    assert.ok(JSON.parse(response.body).error, name);
    assert.equal(calls.length, 0, name);
  }
});

test('a dispatch GitHub refuses is reported as a gateway error', async () => {
  const { fetchImpl } = recordingFetch(
    jsonResponse(422, { message: 'Unexpected inputs provided' })
  );
  const response = await makeHandler(fetchImpl)(post(VALID));
  assert.equal(response.statusCode, 502);
});

test('a dispatch that never reaches GitHub is reported as a gateway error', async () => {
  const { fetchImpl } = recordingFetch(new TypeError('fetch failed'));
  const response = await makeHandler(fetchImpl)(post(VALID));
  assert.equal(response.statusCode, 502);
});

test('failing to get a token is reported without dispatching', async () => {
  const { calls, fetchImpl } = recordingFetch();
  const response = await makeHandler(fetchImpl, async () => {
    throw new Error('no key');
  })(post(VALID));
  assert.equal(response.statusCode, 502);
  assert.equal(calls.length, 0);
});

// The workflow checks the same two fields again. If the dispatcher's copy of a
// pattern drifted from the workflow's, it would either pass requests the
// workflow fails a minute later or refuse ones the workflow would build.
test('the identifier and key patterns are the ones the workflow enforces', () => {
  const workflow = fs.readFileSync(
    new URL('../../.github/workflows/build-esphome.yml', import.meta.url),
    'utf-8'
  );
  for (const [variable, pattern] of [
    ['IDENTIFIER', IDENTIFIER_PATTERN],
    ['CLIENT_PUBLIC_KEY', PUBLIC_KEY_PATTERN],
  ]) {
    const declared = workflow.match(
      new RegExp(`"\\$${variable}" =~ (?<pattern>\\^\\S+)\\s*\\]\\]`)
    );
    assert.ok(declared, `no ${variable} check found in the workflow`);
    assert.equal(pattern.source, declared.groups.pattern, variable);
  }
});

test('a key the browser produces is accepted', () => {
  const { publicKey: point } = generateKeyPairSync('ec', {
    namedCurve: 'P-256',
    publicKeyEncoding: { type: 'spki', format: 'der' },
  });
  // The last 65 bytes of a P-256 SPKI are the uncompressed point, which is
  // what WebCrypto's 'raw' export gives the page.
  const raw = point.subarray(point.length - 65).toString('base64');
  const { inputs } = parseBuildRequest(
    JSON.stringify({ ...VALID, public_key: raw })
  );
  assert.equal(inputs.public_key, raw);
});

const lambdaContext = (remainingMs) => ({
  getRemainingTimeInMillis: () => remainingMs,
});

// Never answers, as a GitHub that hangs would not; gives up only when aborted.
const hangingFetch =
  (calls) =>
  async (url, init = {}) => {
    calls.push({ url, ...init });
    return new Promise((resolve, reject) => {
      init.signal?.addEventListener('abort', () => reject(init.signal.reason));
    });
  };

test('every GitHub call shares one deadline taken from the invocation', async () => {
  const { calls, fetchImpl } = recordingFetch(jsonResponse(204));
  const tokenSignals = [];
  const handler = makeHandler(fetchImpl, async ({ signal }) => {
    tokenSignals.push(signal);
    return 'installation-token';
  });
  const response = await handler(post(VALID), lambdaContext(15000));

  assert.equal(response.statusCode, 202);
  assert.ok(tokenSignals[0] instanceof AbortSignal);
  assert.equal(calls[0].signal, tokenSignals[0]);
});

test('a dispatch GitHub never answers is given up before the invocation ends', async () => {
  const calls = [];
  const handler = makeHandler(hangingFetch(calls));
  const started = Date.now();
  const response = await handler(
    post(VALID),
    lambdaContext(RESPONSE_MARGIN_MS + 50)
  );

  assert.equal(response.statusCode, 502);
  assert.equal(calls.length, 1);
  assert.ok(Date.now() - started < 1000, 'gave up at the deadline');
});

test('a token GitHub never issues is given up before the invocation ends', async () => {
  const calls = [];
  const fetchImpl = hangingFetch(calls);
  const getToken = createTokenSource({
    appId: 1,
    owner: 'tomquist',
    repo: 'esphome-b2500',
    loadPrivateKey: async () => privateKey,
    fetchImpl,
  });
  const response = await makeHandler(fetchImpl, getToken)(
    post(VALID),
    lambdaContext(RESPONSE_MARGIN_MS + 50)
  );

  assert.equal(response.statusCode, 502);
  // Stuck on the installation lookup, so the dispatch was never attempted.
  assert.equal(calls.length, 1);
  assert.match(calls[0].url, /\/installation$/);
});

test('parseBuildRequest returns exactly the three workflow inputs', () => {
  assert.deepEqual(parseBuildRequest(JSON.stringify(VALID)), { inputs: VALID });
});

const { privateKey, publicKey } = generateKeyPairSync('rsa', {
  modulusLength: 2048,
  privateKeyEncoding: { type: 'pkcs1', format: 'pem' },
  publicKeyEncoding: { type: 'spki', format: 'pem' },
});

const decodeSegment = (segment) =>
  JSON.parse(Buffer.from(segment, 'base64url').toString('utf8'));

test('the app JWT is RS256-signed, issued by the app and short-lived', () => {
  const nowMs = 1_800_000_000_000;
  const jwt = createAppJwt(12345, privateKey, nowMs);
  const [header, payload, signature] = jwt.split('.');

  assert.deepEqual(decodeSegment(header), { alg: 'RS256', typ: 'JWT' });
  const claims = decodeSegment(payload);
  assert.equal(claims.iss, '12345');
  assert.equal(claims.iat, nowMs / 1000 - 60);
  assert.ok(
    claims.exp - claims.iat <= 600,
    'GitHub caps app JWTs at ten minutes'
  );

  const verifier = createVerify('RSA-SHA256');
  verifier.update(`${header}.${payload}`);
  assert.ok(verifier.verify(publicKey, Buffer.from(signature, 'base64url')));
});

test('installation tokens are narrowed to actions: write on the repository and reused', async () => {
  let nowMs = Date.parse('2026-11-01T12:00:00Z');
  const issued = (token) =>
    jsonResponse(201, {
      token,
      expires_at: new Date(nowMs + 60 * 60 * 1000).toISOString(),
    });
  const { calls, fetchImpl } = recordingFetch(
    jsonResponse(200, { id: 777 }),
    issued('first'),
    issued('second')
  );
  let keyLoads = 0;
  const getToken = createTokenSource({
    appId: 12345,
    owner: 'tomquist',
    repo: 'esphome-b2500',
    loadPrivateKey: async () => {
      keyLoads += 1;
      return privateKey;
    },
    fetchImpl,
    now: () => nowMs,
  });

  assert.equal(await getToken(), 'first');
  assert.equal(
    calls[0].url,
    'https://api.github.com/repos/tomquist/esphome-b2500/installation'
  );
  assert.match(
    calls[0].headers.Authorization,
    /^Bearer [\w-]+\.[\w-]+\.[\w-]+$/
  );
  assert.equal(
    calls[1].url,
    'https://api.github.com/app/installations/777/access_tokens'
  );
  assert.deepEqual(JSON.parse(calls[1].body), {
    repositories: ['esphome-b2500'],
    permissions: { actions: 'write' },
  });

  // Half an hour on, the token is still good.
  nowMs += 30 * 60 * 1000;
  assert.equal(await getToken(), 'first');
  assert.equal(calls.length, 2);

  // Inside the renewal margin a new one is minted, without looking up the
  // installation or loading the key again.
  nowMs += 27 * 60 * 1000;
  assert.equal(await getToken(), 'second');
  assert.equal(calls.length, 3);
  assert.match(calls[2].url, /\/app\/installations\/777\/access_tokens$/);
  assert.equal(keyLoads, 1);
});

test('an installation lookup GitHub refuses surfaces as an error', async () => {
  const { fetchImpl } = recordingFetch(
    jsonResponse(404, { message: 'Not Found' })
  );
  const getToken = createTokenSource({
    appId: 1,
    owner: 'tomquist',
    repo: 'esphome-b2500',
    loadPrivateKey: async () => privateKey,
    fetchImpl,
  });
  await assert.rejects(getToken(), /installation failed: HTTP 404/);
});
