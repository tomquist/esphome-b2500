#!/usr/bin/env bash
#
# Stops the compile progress watcher, and does not return until nothing it
# started can publish again. The workflow publishes the build result right
# after this, and a survivor would overwrite that result with a stale
# "building" status - leaving the web builder showing compile progress for a
# firmware that is already built.
#
# The watcher stops on the stop file, which it only checks between polls, so
# the ordinary path lets an upload in flight finish first. The signals below
# are for a publish wedged well past its own timeout: the watcher leads its own
# process group, so they reach the upload too rather than only its shell.
#
# Environment:
#   PROGRESS_WATCHER_PID           pid of the watcher, which leads its group
#   PROGRESS_STOP_FILE             the stop file the watcher polls for
#   PROGRESS_STOP_TIMEOUT_SECONDS  how long to wait for it to stop on its own
#   PROGRESS_KILL_TIMEOUT_SECONDS  how long to wait after signalling it

set -uo pipefail

STOP_FILE="${PROGRESS_STOP_FILE:-progress-watcher.stop}"
STOP_TIMEOUT_SECONDS="${PROGRESS_STOP_TIMEOUT_SECONDS:-60}"
KILL_TIMEOUT_SECONDS="${PROGRESS_KILL_TIMEOUT_SECONDS:-10}"
PID="${PROGRESS_WATCHER_PID:-}"

touch "$STOP_FILE"

if [[ -z "$PID" ]]; then
  exit 0
fi

# The watcher itself, or anything still running in the group it leads.
alive() {
  kill -0 "$PID" 2> /dev/null || kill -0 -"$PID" 2> /dev/null
}

gone_within() {
  local remaining="$1"
  while ((remaining > 0)); do
    alive || return 0
    sleep 1
    remaining=$((remaining - 1))
  done
  ! alive
}

if gone_within "$STOP_TIMEOUT_SECONDS"; then
  exit 0
fi

echo "Progress watcher did not stop on its own; signalling its process group"
kill -TERM -"$PID" 2> /dev/null || kill -TERM "$PID" 2> /dev/null || true
if gone_within "$KILL_TIMEOUT_SECONDS"; then
  exit 0
fi

kill -KILL -"$PID" 2> /dev/null || kill -KILL "$PID" 2> /dev/null || true
if ! gone_within 5; then
  echo "Progress watcher process group is still running"
fi
