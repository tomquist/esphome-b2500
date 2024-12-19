const crypto = require('crypto');
const fs = require('fs');
const nunjucks = require('nunjucks');

const password = process.env.PASSWORD;
const encryptedConfigJson = Buffer.from(process.env.ENCRYPTED_CONFIG, 'base64');
// AES-256-GCM with a 12-byte IV and 16-byte tag
const key = crypto.createHash('sha256').update(password).digest();
const decipher = crypto.createDecipheriv(
  'aes-256-gcm',
  key,
  encryptedConfigJson.subarray(0, 12)
);
decipher.setAuthTag(encryptedConfigJson.subarray(12, 12 + 16));
const configJSON = Buffer.concat([
  decipher.update(encryptedConfigJson.subarray(12 + 16)),
  decipher.final(),
]).toString('utf-8');

nunjucks
  .configure({ autoescape: false })
  .addGlobal('git_sha', process.env.GITHUB_SHA);
const { config, secrets } = JSON.parse(configJSON);

// Mask all secrets
for (const secret of secrets) {
  if (secret.trim() !== '') {
    console.log(`::add-mask::${secret}`);
  }
}

const templateV1 = fs.readFileSync('./src/template.jinja2', 'utf-8');
const templateV2 = fs.readFileSync('./src/template_v2.jinja2', 'utf-8');

const template = config.template_version === 'v2' ? templateV2 : templateV1;
const renderedConfig = nunjucks.renderString(template, config);
fs.writeFileSync('device.yaml', renderedConfig);
