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
# Runs in the background alongside the build step. It stops when the stop file
# appears, which it only checks between polls: an upload in flight always
# finishes first, so a status it publishes can never land after the result the
# workflow publishes once this has exited. It must never exit on a transient
# error either.

set -uo pipefail

INTERVAL_SECONDS="${PROGRESS_INTERVAL_SECONDS:-15}"
STOP_FILE="${PROGRESS_STOP_FILE:-progress-watcher.stop}"
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
while [[ ! -e "$STOP_FILE" ]]; do
  completed=$(count_objects)
  if [[ "$completed" -gt 0 && "$completed" != "$last_completed" ]]; then
    STEP=compiling \
      PROGRESS_DONE="$completed" \
      PROGRESS_TOTAL="$(count_expected_objects)" \
      "$PUBLISH" building "Compiling the firmware" > /dev/null || true
    last_completed="$completed"
  fi
  # Sleep in short ticks so that stopping does not have to wait out a full
  # interval, and so that it is noticed here rather than during a publish.
  for _ in $(seq "$INTERVAL_SECONDS"); do
    if [[ -e "$STOP_FILE" ]]; then
      break 2
    fi
    sleep 1
  done
done
