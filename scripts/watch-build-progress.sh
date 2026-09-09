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
# The same listing names what the compiler is on right now: the object written
# most recently is the unit it just finished. That is the only live signal
# available here - the compiler's own output goes to the job log, which the
# Actions API refuses to serve until the job has ended, so there is nothing to
# read while a build runs.
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
# The ninja graph is written once and then only grows by the second graph, so
# re-reading it every poll would spend most of the interval grepping megabytes.
TOTAL_REFRESH_SECONDS="${PROGRESS_TOTAL_REFRESH_SECONDS:-60}"

# How many objects exist and which was written last, from one traversal: the
# count alone already costs a walk of the build tree, so following the compiler
# comes free. Printed as one `count\tname` line so both survive a subshell.
scan_objects() {
  find "$BUILD_ROOT" -type f \( -name '*.obj' -o -name '*.o' \) \
    -printf '%T@\t%f\n' 2>/dev/null |
    awk -F'\t' '
      # Ties are ordinary: ninja compiles in parallel and mtimes are only so
      # fine, so several units can land in the same tick. Broken on the name
      # rather than left to the order the tree happens to be walked in, which
      # would let the reported unit flicker between two files that finished
      # together.
      {
        count++
        if ($1 > newest || ($1 == newest && $2 > name)) { newest = $1; name = $2 }
      }
      END { printf "%d\t%s\n", count + 0, name }'
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

last_completed=''
last_current=''
while [[ ! -e "$STOP_FILE" ]]; do
  scan=$(scan_objects)
  completed=${scan%%$'\t'*}
  current=${scan#*$'\t'}
  # `sha256.c.obj` is the object; `sha256.c` is what was compiled to make it.
  current=${current%.obj}
  current=${current%.o}
  refresh_expected
  # The name moves through files the count cannot distinguish - two units
  # finishing between polls advance it by one either way - so publish on either.
  if [[ "$completed" != "$last_completed" || "$current" != "$last_current" ]] &&
     [[ "$completed" -gt 0 ]]; then
    STEP=compiling \
      PROGRESS_DONE="$completed" \
      PROGRESS_TOTAL="$expected" \
      PROGRESS_CURRENT="$current" \
      "$PUBLISH" building "Compiling the firmware" > /dev/null || true
    last_completed="$completed"
    last_current="$current"
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
