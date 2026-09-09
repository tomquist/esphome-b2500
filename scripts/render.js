const fs = require('fs');
const nunjucks = require('nunjucks');
const { openConfig, parsePublicKey } = require('./buildCrypto');
const { validateConfig } = require('./validateConfig');

/**
 * The render context. The config IS the context, and in nunjucks a context
 * value shadows a global - so `addGlobal` would let the requester set `git_sha`
 * and with it the `ref` the templates fetch the b2500 component from. The
 * trusted values go in last and win.
 *
 * Exported so the tests pin this rather than a copy of it: a version that
 * spread the config last would be the vulnerability again, and a test with its
 * own correct copy would stay green.
 */
const buildRenderContext = (config, env) => ({
  ...config,
  git_sha: env.GITHUB_SHA,
  automated_build: env.AUTOMATED_BUILD === 'true',
});

const MAX_SECRETS = 64;

/**
 * Registers the values the browser marked as secrets so they are redacted from
 * the log.
 *
 * The list comes from the payload like everything else, and unlike `config` it
 * does not go through validateConfig: a string here would iterate its
 * characters and mask single letters across the whole log, an enormous array
 * would drown it, and a newline would end the `::add-mask::` command and let
 * the rest of the line run as a workflow command of its own - `::stop-commands::`
 * there would silence every mask that follows.
 *
 * Short values are masked too. Masking a two-character password makes the log
 * noisy, but not masking it publishes the password.
 *
 * Values are escaped the way @actions/core escapes workflow-command data,
 * because the runner unescapes it symmetrically: emitting a raw `%` would
 * register a mask for a different string than the one in the payload, and a
 * secret containing `%25`, `%0D` or `%0A` would then go unredacted. Escaping
 * also removes the line terminators that would otherwise end the command and
 * let the rest of the value run as a command of its own.
 */
const maskSecrets = (secrets, log = console.log) => {
  if (secrets != null && !Array.isArray(secrets)) {
    throw new Error('secrets must be an array');
  }
  for (const secret of (secrets || []).slice(0, MAX_SECRETS)) {
    if (typeof secret === 'string' && secret.trim() !== '') {
      log(
        `::add-mask::${secret
          .replace(/%/g, '%25')
          .replace(/\r/g, '%0D')
          .replace(/\n/g, '%0A')}`
      );
    }
  }
};

module.exports = { buildRenderContext, MAX_SECRETS, maskSecrets };

const main = () => {
  // The config is addressed to the repo's static key, so this step needs the
  // private half - and it is the only step that does. Deriving the key here
  // rather than in the workflow keeps it out of the shell and out of GITHUB_ENV.
  const configJSON = openConfig(
    process.env.ENCRYPTED_CONFIG,
    process.env.BUILD_PRIVATE_KEY,
    parsePublicKey(process.env.CLIENT_PUBLIC_KEY)
  ).toString('utf-8');

  nunjucks.configure({ autoescape: false });
  const { config, secrets } = JSON.parse(configJSON);

  // Reject anything in the config that could break out of the YAML the templates
  // generate before it is rendered. The config comes from a public, unauthenticated
  // dispatch proxy, so it is fully attacker controlled.
  validateConfig(config);

  maskSecrets(secrets);

  // Write the config to a file for debugging
  fs.writeFileSync('config.json', JSON.stringify(config, null, 2));

  const templateV1 = fs.readFileSync('./src/template.jinja2', 'utf-8');
  const templateV2 = fs.readFileSync('./src/template_v2.jinja2', 'utf-8');
  const templateV2Minimal = fs.readFileSync(
    './src/template_v2_minimal.jinja2',
    'utf-8'
  );

  let template;
  switch (config.template_version) {
    case 'v1':
      template = templateV1;
      break;
    case 'v2':
      template = templateV2;
      break;
    case 'v2-minimal':
      template = templateV2Minimal;
      break;
    default:
      throw new Error(`Unknown template version: ${config.template_version}`);
  }
  const renderedConfig = nunjucks.renderString(
    template,
    buildRenderContext(config, process.env)
  );
  fs.writeFileSync('device.yaml', renderedConfig);
};

if (require.main === module) {
  main();
}
