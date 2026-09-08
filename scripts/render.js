const fs = require('fs');
const nunjucks = require('nunjucks');
const { openConfig, parsePublicKey } = require('./buildCrypto');
const { validateConfig } = require('./validateConfig');

// The config is addressed to the repo's static key, so this step needs the
// private half - and it is the only step that does. Deriving the key here
// rather than in the workflow keeps it out of the shell and out of GITHUB_ENV.
const configJSON = openConfig(
  process.env.ENCRYPTED_CONFIG,
  process.env.BUILD_PRIVATE_KEY,
  parsePublicKey(process.env.CLIENT_PUBLIC_KEY)
).toString('utf-8');

nunjucks
  .configure({ autoescape: false })
  .addGlobal('git_sha', process.env.GITHUB_SHA)
  .addGlobal('automated_build', process.env.AUTOMATED_BUILD === 'true');
const { config, secrets } = JSON.parse(configJSON);

// Reject anything in the config that could break out of the YAML the templates
// generate before it is rendered. The config comes from a public, unauthenticated
// dispatch proxy, so it is fully attacker controlled.
validateConfig(config);

// Mask all secrets
for (const secret of secrets || []) {
  if (typeof secret === 'string' && secret.trim() !== '') {
    console.log(`::add-mask::${secret}`);
  }
}

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
const renderedConfig = nunjucks.renderString(template, config);
fs.writeFileSync('device.yaml', renderedConfig);
