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

Apply [`s3-cors.json`](./s3-cors.json) either by running the
[Apply S3 CORS configuration](../.github/workflows/s3-cors.yml) workflow or
manually:

```bash
aws s3api put-bucket-cors \
  --bucket esphome-b2500-images \
  --cors-configuration file://infra/s3-cors.json
```

Verify it with:

```bash
aws s3api get-bucket-cors --bucket esphome-b2500-images
```
