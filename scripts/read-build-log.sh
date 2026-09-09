#!/usr/bin/env bash
#
# Prints the compiler's output from the file the build step tees it to.
#
# The build runs esphome/build-action's own docker invocation directly rather
# than through the action, for this: a composite action's steps are `run:`
# steps we cannot tee, and the job log its output lands in is the one place it
# then exists. Reading that back through the Actions API does not work from
# inside the job - `GET /actions/jobs/<a running job>/logs` is a 404 until the
# job has ended, which is after every step that could publish it, the failure
# path included. Owning the invocation means owning stdout, so the log is an
# ordinary growing file and none of that applies.
#
# What it prints is whole lines only. publish-build-log.sh sends the difference
# since last time and the page joins those back up, so a line still being
# written must wait for its newline - it would read differently once the rest
# arrived, and the bytes already sent would no longer be a prefix of the log.
#
# Usage: read-build-log.sh
#
# Environment:
#   BUILD_LOG_FILE      what the build step tees to, default build.log
#   LOG_END_AT_ERROR    stop at the first `::error::` line, keeping it
#   LOG_TAIL_LINES      keep only this many lines from the end
#   LOG_MAX_BYTES       keep only this many bytes from the end

set -uo pipefail

SOURCE="${BUILD_LOG_FILE:-build.log}"
ESC=$(printf '\033')

[[ -r "$SOURCE" && -s "$SOURCE" ]] || exit 1

# A last line without its newline is one the compiler has not finished writing.
{ if [[ -n "$(tail -c 1 "$SOURCE")" ]]; then
    head -n -1 "$SOURCE"
  else
    cat "$SOURCE"
  fi; } |
  sed -E \
    -e 's/\r$//' \
    -e "s,${ESC}\\[[0-?]*[ -/]*[@-~],,g" \
    -e "s/${ESC}[()][A-B0-2]//g" \
    -e "s/${ESC}\\][^${ESC}]*(\\a|${ESC}\\\\)//g" |
  # The failing step is the last thing anyone needs from a failed build;
  # whatever the workflow does afterwards would only push it out of view.
  { if [[ -n "${LOG_END_AT_ERROR:-}" ]]; then
      awk '{ print } /^::error::/ { exit }'
    else
      cat
    fi; } |
  # entrypoint.py writes workflow commands to stdout, so they arrive here in
  # their `::` form rather than as the runner renders them. The group titles
  # are worth keeping as headings; the rest is machinery.
  sed -E \
    -e 's/^::error::/ERROR: /' \
    -e 's/^::warning::/WARNING: /' \
    -e 's/^::group:://' |
  grep -v '^::' |
  { if [[ -n "${LOG_TAIL_LINES:-}" ]]; then tail -n "$LOG_TAIL_LINES"; else cat; fi; } |
  { if [[ -n "${LOG_MAX_BYTES:-}" ]]; then tail -c "$LOG_MAX_BYTES"; else cat; fi; }
exit 0
