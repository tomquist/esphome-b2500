// Starts a firmware build for the web builder.
//
// The page cannot call GitHub itself: starting a workflow needs a token, and a
// static page has nowhere to keep one. This function holds the credentials of
// a GitHub App installed on this repository, checks that a request is shaped
// like a build request, and passes it on as a `workflow_dispatch` of
// .github/workflows/build-esphome.yml. It runs behind a Lambda function URL;
// CORS is configured on the URL (see template.yaml), not here.
//
// The endpoint is public by design - anyone can start a build, exactly as
// anyone can open the page. What it must not be is a general way to reach the
// GitHub API: the installation token it mints is narrowed to `actions: write`
// on this one repository, it only ever dispatches the one workflow on the one
// ref, and the only inputs that get through are the three the workflow takes,
// each checked against the shape the workflow checks again on its side.
//
// No dependencies beyond what the Lambda Node.js runtime ships: `fetch`,
// `node:crypto` for the app JWT, and the AWS SDK for the private key.

import { createSign } from 'node:crypto';

// Same patterns as "Validate the dispatch payload" in build-esphome.yml, which
// index.test.mjs holds them to.
export const IDENTIFIER_PATTERN = /^[a-z0-9-]{1,64}$/;
// 65 bytes of uncompressed P-256 point: 87 base64 characters and one "=".
export const PUBLIC_KEY_PATTERN = /^[A-Za-z0-9+/]{87}=$/;
const BASE64_PATTERN = /^[A-Za-z0-9+/]+={0,2}$/;

// A real configuration seals to a few kilobytes. workflow_dispatch caps all
// inputs together at 65,535 characters; staying well below that means an
// accepted request is never one GitHub turns away.
export const MAX_CONFIG_LENGTH = 32768;
export const MAX_BODY_LENGTH = 40000;

const GITHUB_API = 'https://api.github.com';
const API_HEADERS = {
  Accept: 'application/vnd.github+json',
  'X-GitHub-Api-Version': '2022-11-28',
  'User-Agent': 'esphome-b2500-dispatcher',
};

// Installation tokens live an hour. Renewed this long before they expire, so
// a token is never used in its last minutes.
const TOKEN_RENEW_MARGIN_MS = 5 * 60 * 1000;

const respond = (statusCode, body) => ({
  statusCode,
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify(body),
});

const reject = (statusCode, error) => respond(statusCode, { error });

const base64url = (value) =>
  Buffer.from(value)
    .toString('base64')
    .replace(/=+$/, '')
    .replace(/\+/g, '-')
    .replace(/\//g, '_');

/**
 * The JWT a GitHub App authenticates as itself with. Backdated a minute
 * against clock drift and valid for nine, inside GitHub's ten-minute cap.
 */
export const createAppJwt = (appId, privateKeyPem, nowMs) => {
  const now = Math.floor(nowMs / 1000);
  const header = base64url(JSON.stringify({ alg: 'RS256', typ: 'JWT' }));
  const payload = base64url(
    JSON.stringify({ iat: now - 60, exp: now + 540, iss: String(appId) })
  );
  const signer = createSign('RSA-SHA256');
  signer.update(`${header}.${payload}`);
  return `${header}.${payload}.${base64url(signer.sign(privateKeyPem))}`;
};

/**
 * Checks a request body and returns the workflow inputs it carries, or a
 * message saying what is wrong with it. Unknown fields are refused rather
 * than dropped, so a client that sends something new finds out here.
 */
export const parseBuildRequest = (body) => {
  let request;
  try {
    request = JSON.parse(body);
  } catch {
    return { error: 'Request body is not JSON' };
  }
  if (
    request === null ||
    typeof request !== 'object' ||
    Array.isArray(request)
  ) {
    return { error: 'Request body must be a JSON object' };
  }
  const allowed = new Set(['identifier', 'config', 'public_key']);
  const unknown = Object.keys(request).filter((key) => !allowed.has(key));
  if (unknown.length > 0) {
    return { error: `Unexpected field: ${unknown.sort().join(', ')}` };
  }
  const { identifier, config, public_key: publicKey } = request;
  if (typeof identifier !== 'string' || !IDENTIFIER_PATTERN.test(identifier)) {
    return { error: 'Invalid build identifier' };
  }
  if (typeof publicKey !== 'string' || !PUBLIC_KEY_PATTERN.test(publicKey)) {
    return { error: 'Invalid build public key' };
  }
  if (
    typeof config !== 'string' ||
    config.length === 0 ||
    config.length > MAX_CONFIG_LENGTH ||
    config.length % 4 !== 0 ||
    !BASE64_PATTERN.test(config)
  ) {
    return { error: 'Invalid encrypted configuration' };
  }
  return { inputs: { identifier, config, public_key: publicKey } };
};

/**
 * Mints installation tokens for the app, narrowed to `actions: write` on the
 * one repository, and keeps one for as long as it is good.
 */
export const createTokenSource = ({
  appId,
  owner,
  repo,
  loadPrivateKey,
  fetchImpl = fetch,
  now = Date.now,
}) => {
  let privateKey;
  let installationId;
  let cached;

  const call = async (path, init, what) => {
    const response = await fetchImpl(`${GITHUB_API}${path}`, init);
    if (!response.ok) {
      const detail = await response.text().catch(() => '');
      throw new Error(`${what} failed: HTTP ${response.status} ${detail}`);
    }
    return response.json();
  };

  return async () => {
    if (cached && cached.expiresAt - TOKEN_RENEW_MARGIN_MS > now()) {
      return cached.token;
    }
    privateKey ??= await loadPrivateKey();
    const appHeaders = {
      ...API_HEADERS,
      Authorization: `Bearer ${createAppJwt(appId, privateKey, now())}`,
    };
    installationId ??= (
      await call(
        `/repos/${owner}/${repo}/installation`,
        { headers: appHeaders },
        'Looking up the app installation'
      )
    ).id;
    const issued = await call(
      `/app/installations/${installationId}/access_tokens`,
      {
        method: 'POST',
        headers: { ...appHeaders, 'Content-Type': 'application/json' },
        body: JSON.stringify({
          repositories: [repo],
          permissions: { actions: 'write' },
        }),
      },
      'Creating an installation token'
    );
    cached = { token: issued.token, expiresAt: Date.parse(issued.expires_at) };
    return cached.token;
  };
};

/**
 * The function URL handler (payload format 2.0), with what it talks to passed
 * in so the tests can stand in for GitHub.
 */
export const createHandler = ({
  owner,
  repo,
  workflow,
  ref,
  getToken,
  fetchImpl = fetch,
  log = console,
}) => {
  return async (event) => {
    const method = event?.requestContext?.http?.method;
    if (method !== 'POST') {
      return reject(405, 'Only POST is supported');
    }
    const raw = event.body ?? '';
    // A loose bound on the encoded form first, so an oversized body is never
    // decoded at all.
    if (raw.length > MAX_BODY_LENGTH * 2) {
      return reject(413, 'Request body too large');
    }
    const body = event.isBase64Encoded
      ? Buffer.from(raw, 'base64').toString('utf8')
      : raw;
    if (body.length > MAX_BODY_LENGTH) {
      return reject(413, 'Request body too large');
    }

    const { inputs, error } = parseBuildRequest(body);
    if (error) {
      return reject(400, error);
    }

    let token;
    try {
      token = await getToken();
    } catch (err) {
      log.error(`Could not authenticate with GitHub: ${err.message}`);
      return reject(
        502,
        'The build service could not authenticate with GitHub'
      );
    }

    let response;
    try {
      response = await fetchImpl(
        `${GITHUB_API}/repos/${owner}/${repo}/actions/workflows/${workflow}/dispatches`,
        {
          method: 'POST',
          headers: {
            ...API_HEADERS,
            Authorization: `Bearer ${token}`,
            'Content-Type': 'application/json',
          },
          body: JSON.stringify({ ref, inputs }),
        }
      );
    } catch (err) {
      log.error(
        `Dispatch for ${inputs.identifier} did not reach GitHub: ${err.message}`
      );
      return reject(502, 'The build service could not reach GitHub');
    }

    if (!response.ok) {
      const detail = await response.text().catch(() => '');
      log.error(
        `Dispatch for ${inputs.identifier} rejected: HTTP ${response.status} ${detail}`
      );
      return reject(502, 'GitHub did not accept the build');
    }

    log.log(`Dispatched build ${inputs.identifier}`);
    return respond(202, { identifier: inputs.identifier });
  };
};

const loadPrivateKeyFromSsm = async () => {
  // Shipped with the Lambda Node.js runtime, so it is not a dependency here,
  // and imported lazily so the tests never need it.
  const { SSMClient, GetParameterCommand } = await import(
    '@aws-sdk/client-ssm'
  );
  const { Parameter } = await new SSMClient({}).send(
    new GetParameterCommand({
      Name: process.env.GITHUB_APP_PRIVATE_KEY_PARAMETER,
      WithDecryption: true,
    })
  );
  return Parameter.Value;
};

let defaultHandler;

export const handler = (event) => {
  if (!defaultHandler) {
    const owner = process.env.GITHUB_OWNER;
    const repo = process.env.GITHUB_REPO;
    defaultHandler = createHandler({
      owner,
      repo,
      workflow: process.env.GITHUB_WORKFLOW_FILE,
      ref: process.env.GITHUB_WORKFLOW_REF,
      getToken: createTokenSource({
        appId: process.env.GITHUB_APP_ID,
        owner,
        repo,
        loadPrivateKey: loadPrivateKeyFromSsm,
      }),
    });
  }
  return defaultHandler(event);
};
