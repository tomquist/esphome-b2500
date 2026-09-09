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
# most recently is the unit it just finished.
#
# The compiler's own output goes alongside it. The build step tees it to a file
# and publish-build-log.sh uploads whatever is new as its own object, so the
# status document carries only how many of those exist. That part is best
# effort - a build whose output cannot be read still reports progress.
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
# Slower than the poll: publishing a segment is an upload, and the compiler
# does not say enough in five seconds to be worth one. Zero turns it off.
LOG_INTERVAL_SECONDS="${PROGRESS_LOG_INTERVAL_SECONDS:-10}"
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

log_read_at=0
segments=0

# Uploads whatever the compiler has said since the last segment and reports how
# many now exist. Publishing is what advances that count, so the object is
# always in the bucket before the status document names it.
refresh_log() {
  [[ "$LOG_INTERVAL_SECONDS" -gt 0 ]] || return
  local moment published
  moment=$(now)
  if [[ $((moment - log_read_at)) -lt "$LOG_INTERVAL_SECONDS" ]]; then
    return
  fi
  log_read_at=$moment
  published=$("$LOG_COMMAND" 2>&1)
  if [[ "$published" =~ ^[0-9]+$ ]]; then
    segments=$published
  else
    # Not discarded: without it a build that published nothing says nothing
    # about why, which is exactly the hole the last attempt at this fell into.
    echo "publishing the build output: $published" >&2
  fi
}

last_completed=''
last_current=''
last_segments=''
while [[ ! -e "$STOP_FILE" ]]; do
  scan=$(scan_objects)
  completed=${scan%%$'\t'*}
  current=${scan#*$'\t'}
  # `sha256.c.obj` is the object; `sha256.c` is what was compiled to make it.
  current=${current%.obj}
  current=${current%.o}
  refresh_expected
  refresh_log
  # Any of the three: the name moves through files the count cannot
  # distinguish, and early on the compiler is talking before the first object
  # exists at all.
  if [[ "$completed" != "$last_completed" ||
        "$current" != "$last_current" ||
        "$segments" != "$last_segments" ]] &&
     [[ "$completed" -gt 0 || "$segments" -gt 0 ]]; then
    STEP=compiling \
      PROGRESS_DONE="$completed" \
      PROGRESS_TOTAL="$expected" \
      PROGRESS_CURRENT="$current" \
      LOG_SEGMENTS="$segments" \
      "$PUBLISH" building "Compiling the firmware" > /dev/null || true
    last_completed="$completed"
    last_current="$current"
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
