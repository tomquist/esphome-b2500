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
#   STEP             what the build is doing: preparing, compiling, packaging
#   PROGRESS_DONE    compile units finished so far (with PROGRESS_TOTAL)
#   PROGRESS_TOTAL   compile units the build expects in total
#   PROGRESS_CURRENT what the compiler is on right now
#   LOG_SEGMENTS     how many build output segments exist (publish-build-log.sh)
#   FIRMWARE_URL     download URL of the firmware ZIP (success only)
#   FIRMWARE_NAME    name of the firmware directory inside the ZIP
#   ESPHOME_VERSION  ESPHome version the firmware was built with

set -euo pipefail

STATUS="${1:?usage: publish-build-status.sh <status> [message]}"
MESSAGE="${2:-}"

# The build output itself is published as its own objects; this document only
# says how many of them there are. It is fetched every few seconds for the
# length of a build, so it stays small and stops growing with the compiler's
# appetite for talking.
SEGMENTS="${LOG_SEGMENTS:-0}"
[[ "$SEGMENTS" =~ ^[0-9]+$ ]] || SEGMENTS=0

if [[ ! "${IDENTIFIER:-}" =~ ^[a-z0-9-]{1,64}$ ]]; then
  echo "No usable build identifier, skipping status upload"
  exit 0
fi

jq -n \
  --arg status "$STATUS" \
  --arg identifier "$IDENTIFIER" \
  --arg message "$MESSAGE" \
  --arg step "${STEP:-}" \
  --arg run_url "${RUN_URL:-}" \
  --arg firmware_url "${FIRMWARE_URL:-}" \
  --arg firmware_name "${FIRMWARE_NAME:-}" \
  --arg esphome_version "${ESPHOME_VERSION:-}" \
  --arg current "${PROGRESS_CURRENT:-}" \
  --argjson done "${PROGRESS_DONE:-0}" \
  --argjson total "${PROGRESS_TOTAL:-0}" \
  --argjson segments "$SEGMENTS" \
  --arg updated_at "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
  '{status: $status, identifier: $identifier, updated_at: $updated_at}
    + ({run_url: $run_url, message: $message, step: $step,
        firmware_url: $firmware_url, firmware_name: $firmware_name,
        esphome_version: $esphome_version}
       | with_entries(select(.value != "")))
    + (if $done > 0 or $total > 0
       then {progress: ({completed: $done}
                        + (if $total > 0 then {total: $total} else {} end)
                        + (if $current != "" then {current: $current} else {} end))}
       else {} end)
    + (if $segments > 0 then {log_segments: $segments} else {} end)' \
  > build-status.json

cat build-status.json

aws s3 cp build-status.json "s3://${S3_BUCKET}/firmware/${IDENTIFIER}.status.json" \
  --content-type application/json \
  --cache-control "no-store, max-age=0"
