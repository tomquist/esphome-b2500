#!/usr/bin/env bash
#
# Publishes compile progress while the firmware builds, so the web builder can
# show something better than a spinner for the several minutes the ESPHome
# compile takes.
#
# ESP-IDF builds with ninja, which writes one object file per compile unit into
# the build tree and declares every one of them in the generated build.ninja.
# Counting both gives a progress fraction without having to parse the build log,
# which belongs to the action running the compile rather than to us.
#
# Runs in the background alongside the build step and is killed once it
# finishes, so it must never exit on a transient error.

set -uo pipefail

INTERVAL_SECONDS="${PROGRESS_INTERVAL_SECONDS:-15}"
BUILD_ROOT="${PROGRESS_BUILD_ROOT:-.esphome/build}"
PUBLISH="${PROGRESS_PUBLISH_COMMAND:-./scripts/publish-build-status.sh}"

count_objects() {
  find "$BUILD_ROOT" -type f \( -name '*.obj' -o -name '*.o' \) 2>/dev/null |
    wc -l
}

# Every object file the generated ninja graphs plan to build. Bootloader and
# app have separate graphs, so the counts are summed.
count_expected_objects() {
  find "$BUILD_ROOT" -type f -name 'build.ninja' -print0 2>/dev/null |
    xargs -0 --no-run-if-empty grep -hcE '^build [^:]+\.(obj|o): ' 2>/dev/null |
    awk '{ sum += $1 } END { print sum + 0 }'
}

last_completed=''
while true; do
  completed=$(count_objects)
  if [[ "$completed" -gt 0 && "$completed" != "$last_completed" ]]; then
    STEP=compiling \
      PROGRESS_DONE="$completed" \
      PROGRESS_TOTAL="$(count_expected_objects)" \
      "$PUBLISH" building "Compiling the firmware" > /dev/null || true
    last_completed="$completed"
  fi
  sleep "$INTERVAL_SECONDS"
done
