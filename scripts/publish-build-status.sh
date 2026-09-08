#!/usr/bin/env bash
#
# Publishes the status document that the web builder polls while a firmware
# build is running. It lives next to the firmware ZIP so that it is covered by
# the same bucket policy.
#
# Usage: publish-build-status.sh <status> [message]
#
# Environment:
#   IDENTIFIER       build identifier from the repository dispatch (required)
#   S3_BUCKET        target bucket (required)
#   RUN_URL          link to this workflow run
#   FIRMWARE_URL     download URL of the firmware ZIP (success only)
#   FIRMWARE_NAME    name of the firmware directory inside the ZIP
#   ESPHOME_VERSION  ESPHome version the firmware was built with

set -euo pipefail

STATUS="${1:?usage: publish-build-status.sh <status> [message]}"
MESSAGE="${2:-}"

if [[ ! "${IDENTIFIER:-}" =~ ^[a-z0-9-]{1,64}$ ]]; then
  echo "No usable build identifier, skipping status upload"
  exit 0
fi

jq -n \
  --arg status "$STATUS" \
  --arg identifier "$IDENTIFIER" \
  --arg message "$MESSAGE" \
  --arg run_url "${RUN_URL:-}" \
  --arg firmware_url "${FIRMWARE_URL:-}" \
  --arg firmware_name "${FIRMWARE_NAME:-}" \
  --arg esphome_version "${ESPHOME_VERSION:-}" \
  --arg updated_at "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
  '{status: $status, identifier: $identifier, updated_at: $updated_at}
    + ({run_url: $run_url, message: $message, firmware_url: $firmware_url,
        firmware_name: $firmware_name, esphome_version: $esphome_version}
       | with_entries(select(.value != "")))' > build-status.json

cat build-status.json

aws s3 cp build-status.json "s3://${S3_BUCKET}/firmware/${IDENTIFIER}.status.json" \
  --content-type application/json \
  --cache-control "no-store, max-age=0"
