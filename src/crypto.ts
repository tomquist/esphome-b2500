import crypto from 'crypto';

export function aesEncrypt({
  password,
  input,
}: {
  password: string;
  input: Buffer;
}): Buffer {
  const key = crypto.createHash('sha256').update(password).digest();
  const iv = crypto.randomBytes(12);
  const cipher = crypto.createCipheriv('aes-256-gcm', key, iv);
  const encrypted = cipher.update(input);
  const final = cipher.final();
  return Buffer.concat([iv, cipher.getAuthTag(), encrypted, final]);
}

export const publicKey = `-----BEGIN PUBLIC KEY-----
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAvvRsA62G+HxFVteRvo9R
at3tt1hLzO1C//n0PgE0ljrc4Y5p+xfIrMe9lgo0Y0+/I5xtpOyzI6cCGiQfmpH8
v1/ZPwRJjG5Oezj910p7B90Z+bg2lS1H2BBPEq2CiDbUCtcd5RKZBiVv9Yjmwm/J
9DOGfiAB8lgmVReqe5fAc8IiZUnljp01LdCoxesp6HKSSEalet1pydj0joTR2xWy
80m58r69liFdLiZBzXYIlbpJRt91KSUyFEeRginm6mkrEjzu8bPKiZlXA3ZvbcSa
JSumPOJxYkcpse3uabXIEYRV3CL35R6CQN9YEYDYoGBrrQeAO5xgmgf76mMUBSWP
qQIDAQAB
-----END PUBLIC KEY-----`;

export interface EncryptParams {
  password: string;
  input: Buffer;
}

export interface ConfigData {
  secrets: string[];
  config: {
    template_version: string;
    [key: string]: any;
  };
}

export function encryptPassword(password: string): string {
  return crypto
    .publicEncrypt(
      {
        key: publicKey,
        padding: crypto.constants.RSA_PKCS1_OAEP_PADDING,
        oaepHash: 'sha256',
      },
      Buffer.from(password)
    )
    .toString('base64');
}

export function encryptConfig(config: ConfigData, password: string): string {
  const stringifiedConfig = JSON.stringify(config);
  const encrypted = aesEncrypt({
    password,
    input: Buffer.from(stringifiedConfig),
  });
  return encrypted.toString('base64');
}
