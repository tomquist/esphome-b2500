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
# written must wait for its terminator - it would read differently once the
# rest arrived, and the bytes already sent would no longer be a prefix of the
# log.
#
# Ninja separates its progress updates with CR rather than LF, because it means
# them to overwrite one another on a terminal. Deleting those CRs would run
# every update in a build phase together into one line tens of thousands of
# characters long, so they are converted to newlines instead: an overwrite and
# a newline both end a line, and only the newline survives being read back on a
# page. That is also what makes the output flow while a phase is compiling -
# held to LF alone, a phase says nothing until it ends.
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

# Every terminator becomes a newline before anything else looks at the bytes,
# so that "the last line" below means the same thing to ninja and to gcc. `tr`
# is what does it rather than sed, because sed would end the file with a
# newline the compiler had not written yet and make a half-line look finished.
LINES=$(mktemp)
trap 'rm -f "$LINES"' EXIT
tr '\r' '\n' <"$SOURCE" >"$LINES"

# A last line without its newline is one the compiler has not finished writing.
{ if [[ -n "$(tail -c 1 "$LINES")" ]]; then
    head -n -1 "$LINES"
  else
    cat "$LINES"
  fi; } |
  sed -E \
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
