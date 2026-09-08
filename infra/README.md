# Firmware bucket configuration

Builds started from the [web builder](https://tomquist.github.io/esphome-b2500/)
upload two objects to the `esphome-b2500-images` bucket:

| Object                              | Purpose                                                                                                                                |
| ----------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------- |
| `firmware/<identifier>.zip`         | The password protected firmware archive.                                                                                               |
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

## Why the archive is still ZipCrypto

`zip -P` uses PKWARE's legacy stream cipher, which a known-plaintext attack
breaks - and a firmware archive has very predictable contents. AES-encrypted
ZIPs (`7z a -tzip -mem=AES256`) would fix that and `@zip.js/zip.js` reads them,
but Windows Explorer cannot, which would break the documented "download it and
flash it yourself" route. Short object lifetimes and an unguessable identifier
are what stand in for it; revisit if the manual route ever stops mattering.
