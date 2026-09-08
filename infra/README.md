# Firmware bucket configuration

Builds started from the [web builder](https://tomquist.github.io/esphome-b2500/)
upload two objects to the `esphome-b2500-images` bucket:

| Object                              | Purpose                                                                                                                                |
| ----------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------- |
| `firmware/<identifier>.zip.enc`     | The firmware archive, AES-256-GCM encrypted (see below).                                                                               |
| `firmware/<identifier>.status.json` | Build status the web builder polls (`building`, `success`, `error`), including which step is running and how far along the compile is. |

Both live under the same `firmware/` prefix so that a single public-read bucket
policy covers them.

## CORS

The web builder downloads the status document and the firmware archive with
`fetch()` so it can decrypt the firmware and flash it via Web Serial. That
requires CORS on the bucket - without it the browser blocks the responses and
the builder falls back to the manual download instructions.

[`s3-cors.json`](./s3-cors.json) is the applied configuration. It is applied by
hand rather than from a workflow: the IAM user the build workflow authenticates
as can write objects but not change bucket configuration, which is the way
round it should be.

Applying the configuration needs `s3:PutBucketCORS` and reading it back needs
`s3:GetBucketCORS`, both on `arn:aws:s3:::esphome-b2500-images` - the bucket ARN
itself, with no `/*` suffix, because CORS is bucket-level configuration rather
than an object action.

With credentials that hold those permissions:

```bash
aws s3api put-bucket-cors \
  --bucket esphome-b2500-images \
  --cors-configuration file://infra/s3-cors.json
```

The S3 console works too, under Permissions > Cross-origin resource sharing
(CORS). It expects the bare array of rules, so paste the value of `CORSRules`
rather than the whole file.

Verify it with:

```bash
aws s3api get-bucket-cors --bucket esphome-b2500-images
```

Or check what a browser sees, against any firmware object that exists:

```bash
curl -sS -o /dev/null -D - \
  -H "Origin: https://tomquist.github.io" \
  https://esphome-b2500-images.s3.eu-west-1.amazonaws.com/firmware/<identifier>.zip \
  | grep -i access-control
```

## Object lifetime

Objects under `firmware/` are readable by anyone who knows the identifier, and a
firmware image embeds the WiFi and MQTT credentials it was built with. Bucket
listing is denied, so the identifier is the only thing guarding an object -
`generateRandomIdentifier()` therefore ends in 96 bits of `randomBytes`, and the
bucket should expire the objects rather than keep them forever.

The IAM user the build workflow uses cannot change bucket configuration, so this
is applied by hand like the CORS rules above:

```bash
aws s3api put-bucket-lifecycle-configuration \
  --bucket esphome-b2500-images \
  --lifecycle-configuration '{
    "Rules": [{
      "ID": "expire-firmware",
      "Status": "Enabled",
      "Filter": {"Prefix": "firmware/"},
      "Expiration": {"Days": 1}
    }]
  }'
```

A day is generous: the web builder downloads the archive as soon as the build
finishes, and the manual route is a link the user follows in the same sitting.

## How the build is keyed

Every build gets an ephemeral P-256 key pair that the browser generates and
never transmits. The dispatch payload carries only its public half, so there is
no secret in it at all - and the build never has to move one through a shell
variable, which is where the worst bug in this workflow used to live.

Two directions, each with its own HKDF-SHA256 derived AES-256-GCM key over an
ECDH shared secret, separated by a per-direction `info` string:

| Direction | ECDH between | Framing |
| --------- | ------------ | ------- |
| config → build | browser ephemeral private ↔ repo static public | `iv (12) \|\| tag (16) \|\| ciphertext` |
| firmware → browser | runner ephemeral private ↔ browser ephemeral public | `public key (65) \|\| iv (12) \|\| tag (16) \|\| ciphertext` |

The build has to read the config - rendering the YAML is the whole job - so that
direction is addressed to the repo's long-lived key. The return direction is
not: the packaging step generates a throwaway key pair, ships its public half in
the object header, and holds no secret at all. Once the job ends nothing on the
runner can read what it published.

`scripts/buildCrypto.js` is the build side, `src/crypto.ts` and
`src/firmware/archiveCrypto.ts` the browser side, and
`src/firmware/archiveCrypto.test.ts` runs the two against each other in both
directions so they cannot drift apart.

### Setting the key pair

    node scripts/generateBuildKeypair.js

- **Secret** `BUILD_PRIVATE_KEY` - the PEM. Only the render step reads it.
- **Variable** `BUILD_PUBLIC_KEY` - the base64 point. Public; the deploy
  workflow bakes it into the page as `REACT_APP_BUILD_PUBLIC_KEY`.

The public half is deployment configuration rather than a constant in the
source, so rotating costs a variable, a secret and a redeploy - no code change.
Rotate it: it decrypts every configuration ever submitted, and there is no
forward secrecy to be had here. `repository_dispatch` is fire-and-forget, so
there is no round trip in which the runner could offer an ephemeral key before
the browser encrypts, which means the recipient key is necessarily long-lived
and anyone who kept old payloads can read them if it ever leaks.

Deploy the page with the new public key *before* switching the secret, or the
handful of builds in flight will fail to decrypt.

## Why the archive is not a password-protected ZIP

`zip -P` uses PKWARE's stream cipher: a dozen bytes of known plaintext recover
the internal keys and unlock the whole archive. A firmware archive is close to
worst case for that, because anyone can run a build of their own at the pinned
ESPHome version and get byte-identical bootloader and partition images to use as
that plaintext.

The AES-encrypted ZIP format (`7z a -tzip -mem=AES256`) was the other candidate.
`@zip.js/zip.js` reads it, but Windows Explorer, macOS Archive Utility and
Info-ZIP `unzip` all refuse it, so flashing manually would have started with
"install 7-Zip". Encrypting the container instead keeps the manual route on an
ordinary ZIP: the page decrypts in the browser and offers the plain archive as a
download, so nobody has to type anything.

One thing this gives up: the object served from the bucket is no longer useful
on its own. If a browser cannot fetch it there is no "download it by hand"
fallback - the page holds the only key, and only until it is closed.
