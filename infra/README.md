# Firmware bucket configuration

Builds started from the [web builder](https://tomquist.github.io/esphome-b2500/)
upload two objects to the `esphome-b2500-images` bucket:

| Object                              | Purpose                                                              |
| ----------------------------------- | -------------------------------------------------------------------- |
| `firmware/<identifier>.zip`         | The password protected firmware archive.                             |
| `firmware/<identifier>.status.json` | Build status the web builder polls (`building`, `success`, `error`). |

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
