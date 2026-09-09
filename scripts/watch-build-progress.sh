#!/usr/bin/env bash
#
# Publishes compile progress while the firmware builds, so the web builder can
# show something better than a spinner for the several minutes the ESPHome
# compile takes.
#
# ESP-IDF builds with ninja, which writes one object file per compile unit into
# the build tree and declares every one of them in the generated build.ninja.
# Counting both gives a progress fraction that does not depend on being able to
# read the compiler's output.
#
# It publishes that output too, when the API lets it: publish-build-log.sh
# uploads whatever the compiler has said since the last time as its own object,
# and the status document carries only how many of those exist. That part is
# best effort - a build whose log cannot be read still reports progress.
#
# Runs in the background alongside the build step. It stops when the stop file
# appears, which it only checks between polls: an upload in flight always
# finishes first, so a status it publishes can never land after the result the
# workflow publishes once this has exited. It must never exit on a transient
# error either.

set -uo pipefail

INTERVAL_SECONDS="${PROGRESS_INTERVAL_SECONDS:-5}"
STOP_FILE="${PROGRESS_STOP_FILE:-progress-watcher.stop}"
BUILD_ROOT="${PROGRESS_BUILD_ROOT:-.esphome/build}"
PUBLISH="${PROGRESS_PUBLISH_COMMAND:-./scripts/publish-build-status.sh}"
LOG_COMMAND="${PROGRESS_LOG_COMMAND:-./scripts/publish-build-log.sh}"
# Slower than the poll: reading the log costs an API request against a limit
# every build in the repository shares, while counting files costs nothing. At
# ten seconds a busy hour of builds stays well inside it. Zero turns log
# reporting off.
LOG_INTERVAL_SECONDS="${PROGRESS_LOG_INTERVAL_SECONDS:-10}"
# The ninja graph is written once and then only grows by the second graph, so
# re-reading it every poll would spend most of the interval grepping megabytes.
TOTAL_REFRESH_SECONDS="${PROGRESS_TOTAL_REFRESH_SECONDS:-60}"

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

now() { date +%s; }

expected=0
expected_read_at=0

# Keeps the cached total until it is worth paying for again: while it is still
# unknown, once the refresh interval has passed, and whenever the build has
# overtaken it - which is what a graph read before the second one existed looks
# like.
refresh_expected() {
  local moment
  moment=$(now)
  if [[ "$expected" -gt 0 &&
        "$expected" -ge "$completed" &&
        $((moment - expected_read_at)) -lt "$TOTAL_REFRESH_SECONDS" ]]; then
    return
  fi
  expected=$(count_expected_objects)
  expected_read_at=$moment
}

log_read_at=0
segments=0

# Uploads whatever the build has said since the last segment and reports how
# many segments now exist. Publishing the log is what advances that count, so
# the object is always in the bucket before the status document names it.
refresh_log() {
  [[ "$LOG_INTERVAL_SECONDS" -gt 0 ]] || return
  local moment published
  moment=$(now)
  if [[ $((moment - log_read_at)) -lt "$LOG_INTERVAL_SECONDS" ]]; then
    return
  fi
  log_read_at=$moment
  published=$("$LOG_COMMAND" 2>/dev/null)
  if [[ "$published" =~ ^[0-9]+$ ]]; then
    segments=$published
  fi
}

last_completed=''
last_segments=''
while [[ ! -e "$STOP_FILE" ]]; do
  completed=$(count_objects)
  refresh_expected
  refresh_log
  # Republish for either half: early on there are no object files yet but the
  # log already has the configure step to show, and late in a link step the
  # count stands still while the output keeps moving.
  if [[ "$completed" != "$last_completed" || "$segments" != "$last_segments" ]] &&
     [[ "$completed" -gt 0 || "$segments" -gt 0 ]]; then
    STEP=compiling \
      PROGRESS_DONE="$completed" \
      PROGRESS_TOTAL="$expected" \
      LOG_SEGMENTS="$segments" \
      "$PUBLISH" building "Compiling the firmware" > /dev/null || true
    last_completed="$completed"
    last_segments="$segments"
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
