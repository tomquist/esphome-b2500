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
# It reads that output too, when the API lets it: fetch-job-log.sh pulls this
# job's own log back, and the tail of it goes into the same status document, so
# the page can show the build talking rather than only a bar moving. That part
# is best effort - a build whose log cannot be read still reports progress.
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
LOG_COMMAND="${PROGRESS_LOG_COMMAND:-./scripts/fetch-job-log.sh}"
# Slower than the poll: reading the log costs an API request against a limit
# every build in the repository shares, while counting files costs nothing.
# Zero turns log reporting off.
LOG_INTERVAL_SECONDS="${PROGRESS_LOG_INTERVAL_SECONDS:-10}"
LOG_FILE="${PROGRESS_LOG_FILE:-build-log.tail}"
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

refresh_log() {
  [[ "$LOG_INTERVAL_SECONDS" -gt 0 ]] || return
  local moment
  moment=$(now)
  if [[ $((moment - log_read_at)) -lt "$LOG_INTERVAL_SECONDS" ]]; then
    return
  fi
  log_read_at=$moment
  local pending="${LOG_FILE}.pending"
  if "$LOG_COMMAND" > "$pending" 2>/dev/null && [[ -s "$pending" ]]; then
    mv -f "$pending" "$LOG_FILE"
  else
    rm -f "$pending"
  fi
}

log_fingerprint() {
  [[ -s "$LOG_FILE" ]] || return 0
  cksum < "$LOG_FILE" 2>/dev/null
}

last_completed=''
last_log=''
while [[ ! -e "$STOP_FILE" ]]; do
  completed=$(count_objects)
  refresh_expected
  refresh_log
  log=$(log_fingerprint)
  # Republish for either half: early on there are no object files yet but the
  # log already has the configure step to show, and late in a link step the
  # count stands still while the output keeps moving.
  if [[ "$completed" != "$last_completed" || "$log" != "$last_log" ]] &&
     [[ "$completed" -gt 0 || -s "$LOG_FILE" ]]; then
    STEP=compiling \
      PROGRESS_DONE="$completed" \
      PROGRESS_TOTAL="$expected" \
      BUILD_LOG_FILE="$LOG_FILE" \
      "$PUBLISH" building "Compiling the firmware" > /dev/null || true
    last_completed="$completed"
    last_log="$log"
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
