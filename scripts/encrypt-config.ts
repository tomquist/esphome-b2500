import fs from 'fs';

import { encryptConfig } from '../src/crypto';
import type { ConfigData } from '../src/crypto';

const [, , configPath, password] = process.argv;

try {
  const config = JSON.parse(fs.readFileSync(configPath, 'utf8'));
  const configData: ConfigData = {
    secrets: [],
    config,
  };
  const encryptedConfig = encryptConfig(configData, password);
  console.log(encryptedConfig);
} catch (error) {
  console.error('Error encrypting config:', error);
  process.exit(1);
}
