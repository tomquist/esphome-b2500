import crypto from "crypto";

export function aesEncrypt({ password, input }: { password: string; input: Buffer }): Buffer {
    const key = crypto.createHash("sha256").update(password).digest();
    const iv = crypto.randomBytes(12);
    const cipher = crypto.createCipheriv("aes-256-gcm", key, iv);
    const encrypted = cipher.update(input);
    const final = cipher.final();
    return Buffer.concat([iv, cipher.getAuthTag(), encrypted, final]);
}
