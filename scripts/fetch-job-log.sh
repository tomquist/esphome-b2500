#!/usr/bin/env bash
#
# Prints this workflow job's own log, cleaned.
#
# NOT WIRED UP, and read this before wiring it up again. The compile runs inside
# esphome/build-action, so its output belongs to that step: there is no file to
# tail and no pipe to tee, which leaves reading our own log back through the
# API. That does not work from inside the job:
#
#   GET /actions/jobs/<a running job>/logs    -> 404
#   GET /actions/jobs/<a finished job>/logs   -> 200
#
# The log is not served until the job has ended, and every step that could
# publish it runs before that - including the failure path. So this returned
# nothing for a whole build, five times, and then wrote itself off, which is
# exactly what it is built to do and produced no log at all.
#
# Two routes work, and this script is most of both. A workflow triggered on
# `workflow_run: completed` runs when the log is there, so it can publish the
# whole log and the failure reason a few seconds after the build ends - the
# page polls for thirty minutes, so it is still watching. Live output needs the
# compile to be ours rather than the action's, so that its stdout can be teed
# to a file, and then none of this is needed.
#
# Prints the whole log by default, because the caller publishes it as a growing
# prefix and needs the same bytes every time. `LOG_TAIL_LINES` and
# `LOG_MAX_BYTES` are for callers that want an excerpt instead.
#
# Everything here is best effort. A build that cannot read its own log still
# reports progress and still reports failure, only without the output - so no
# path may exit non-zero in a way that fails the step that called it, and a
# refusal that will not go away (a revoked token, an exhausted rate limit) must
# stop us from asking again for the rest of the build.
#
# Secrets never reach this: render.js registers them with `::add-mask::` before
# the compile starts, so the log the API returns already has them replaced.
#
# Usage: fetch-job-log.sh
#
# Environment:
#   GITHUB_TOKEN        token with `actions: read` (required)
#   GITHUB_REPOSITORY   owner/repo (required)
#   GITHUB_RUN_ID       the run whose job log to read (required)
#   GITHUB_RUN_ATTEMPT  which attempt of that run, defaults to 1
#   GITHUB_JOB          job id in the workflow file, used to pick the job
#   LOG_START_AT        drop everything up to and including the last line that
#                       starts with this text, and print nothing until some
#                       line does. A literal prefix, not a pattern - see below
#   LOG_END_AT_ERROR    stop at the first `##[error]` line, keeping it
#   LOG_TAIL_LINES      keep only this many lines from the end
#   LOG_MAX_BYTES       keep only this many bytes from the end
#   LOG_SOURCE_FILE     read this instead of the API (used by the tests)
#   LOG_JOB_ID_FILE     caches the resolved job id between calls
#   LOG_DISABLED_FILE   marks the log as unreadable, so we stop asking

set -uo pipefail

source_copy=$(mktemp)
trap 'rm -f "$source_copy"' EXIT

API_URL="${GITHUB_API_URL:-https://api.github.com}"
JOB_ID_FILE="${LOG_JOB_ID_FILE:-job-log-id}"
DISABLED_FILE="${LOG_DISABLED_FILE:-job-log-unavailable}"
MISSES_FILE="${DISABLED_FILE}.misses"
# A log that is not there yet reads exactly like one we will never be allowed
# to read, so give it a few tries before giving up on it for good.
MAX_MISSES="${LOG_MAX_MISSES:-5}"

ESC=$(printf '\033')
BOM=$(printf '\357\273\277')

# Turns a downloaded job log into something worth showing: no byte order mark,
# no timestamps, no terminal escapes, and no runner command markers, which are
# noise everywhere except on the error that ended the build.
#
# The escape pattern is the whole CSI form - parameter bytes 0x30-0x3F, then
# intermediates 0x20-0x2F, then a final byte 0x40-0x7E - rather than the colour
# codes alone. Docker's buildx output inside the compile step writes forms like
# `ESC[1:2m` that a digits-and-semicolons pattern walks straight past, and what
# it leaves behind is rendered as text on the page.
#
# Takes the log as a file rather than on stdin because the first thing it has to
# know is whether the last line has its newline yet. A line still being written
# is a line that will read differently next time - and since sed and awk end
# their output with a newline whether the input had one or not, that can only be
# told from the bytes as they arrived.
clean_log() {
  local source="$1"
  { if [[ -n "$(tail -c 1 "$source")" ]]; then
      head -n -1 "$source"
    else
      cat "$source"
    fi; } |
  sed -E \
    -e "1s/^${BOM}//" \
    -e 's/\r$//' \
    -e 's/^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]+Z //' \
    -e "s,${ESC}\\[[0-?]*[ -/]*[@-~],,g" \
    -e "s/${ESC}[()][A-B0-2]//g" \
    -e "s/${ESC}\\][^${ESC}]*(\\a|${ESC}\\\\)//g" |
    # Everything before the compile is the runner installing things. Nothing at
    # all until the marker shows up: the caller publishes this as a growing
    # prefix of itself, and falling back to the whole log would move where that
    # prefix starts the moment the marker appeared - which corrupts every
    # segment published after it. No log is a feature that quietly did not
    # happen; a shifting one is a log nobody can read.
    #
    # Compared as a literal prefix rather than matched as a regex. `awk -v`
    # processes escape sequences in the value it assigns, and the two awks in
    # play disagree about what survives: gawk turns `\[group\]` into `[group]`,
    # which is then a character class that cannot match the `[group]` it was
    # written to find, while mawk leaves the backslashes alone and matches.
    # The marker is a fixed string, so comparing it as one is both what we mean
    # and the same thing everywhere.
    { if [[ -n "${LOG_START_AT:-}" ]]; then
        awk -v prefix="$LOG_START_AT" '
          { line[NR] = $0; if (substr($0, 1, length(prefix)) == prefix) { start = NR } }
          END { if (start) for (i = start + 1; i <= NR; i++) print line[i] }'
      else
        cat
      fi; } |
    # The failing step is the last thing the log has to say that anyone cares
    # about; whatever the workflow does afterwards (publishing this log, among
    # other things) would only push it out of view.
    { if [[ -n "${LOG_END_AT_ERROR:-}" ]]; then
        awk '{ print } /^##\[error\]/ { exit }'
      else
        cat
      fi; } |
    sed -E \
      -e 's/^##\[error\]/ERROR: /' \
      -e 's/^##\[warning\]/WARNING: /' |
    grep -v '^##\[' |
    { if [[ -n "${LOG_TAIL_LINES:-}" ]]; then tail -n "$LOG_TAIL_LINES"; else cat; fi; } |
    { if [[ -n "${LOG_MAX_BYTES:-}" ]]; then tail -c "$LOG_MAX_BYTES"; else cat; fi; }
}

if [[ -n "${LOG_SOURCE_FILE:-}" ]]; then
  [[ -r "$LOG_SOURCE_FILE" ]] || exit 1
  # A process substitution is not seekable, so give `tail -c 1` a real file.
  cp "$LOG_SOURCE_FILE" "$source_copy" 2> /dev/null || exit 1
  clean_log "$source_copy"
  exit 0
fi

if [[ -z "${GITHUB_TOKEN:-}" || -z "${GITHUB_REPOSITORY:-}" || -z "${GITHUB_RUN_ID:-}" ]]; then
  exit 1
fi
if [[ -e "$DISABLED_FILE" ]]; then
  exit 1
fi

# Prints the response status and any redirect target, writes the body to $2.
api_get() {
  curl -sS --max-time 60 -o "$2" -w '%{http_code} %{redirect_url}' \
    -H "Authorization: Bearer $GITHUB_TOKEN" \
    -H "Accept: application/vnd.github+json" \
    -H "X-GitHub-Api-Version: 2022-11-28" \
    "$1" 2> /dev/null
}

give_up() {
  touch "$DISABLED_FILE" 2> /dev/null || true
  exit 1
}

# 404 while the job runs is normal enough to retry; the same answer over and
# over is not.
missed() {
  local misses
  misses=$(( $(cat "$MISSES_FILE" 2> /dev/null || echo 0) + 1 ))
  echo "$misses" > "$MISSES_FILE" 2> /dev/null || true
  if [[ "$misses" -ge "$MAX_MISSES" ]]; then
    give_up
  fi
  exit 1
}

body=$(mktemp)
trap 'rm -f "$body" "$source_copy"' EXIT

job_id=''
if [[ -s "$JOB_ID_FILE" ]]; then
  job_id=$(< "$JOB_ID_FILE")
fi

if [[ -z "$job_id" ]]; then
  read -r status _ < <(api_get \
    "$API_URL/repos/$GITHUB_REPOSITORY/actions/runs/$GITHUB_RUN_ID/attempts/${GITHUB_RUN_ATTEMPT:-1}/jobs?per_page=100" \
    "$body")
  case "$status" in
    200) ;;
    401 | 403 | 429) give_up ;;
    *) missed ;;
  esac
  # This job by name, the one that is running, then whatever came first: a
  # workflow with a single job resolves on the first of those every time.
  job_id=$(jq -r --arg name "${GITHUB_JOB:-}" '
    (.jobs // []) as $jobs
    | [ ($jobs[] | select(.name == $name)),
        ($jobs[] | select(.status == "in_progress")),
        $jobs[] ]
    | (first(.[].id) // empty)' "$body" 2> /dev/null)
  [[ "$job_id" =~ ^[0-9]+$ ]] || missed
  echo "$job_id" > "$JOB_ID_FILE" 2> /dev/null || true
fi

# The log itself lives in blob storage behind a signed redirect. Following it
# with the Authorization header still attached makes the storage reject the
# request for carrying two credentials, so the redirect is fetched bare.
read -r status redirect < <(api_get \
  "$API_URL/repos/$GITHUB_REPOSITORY/actions/jobs/$job_id/logs" "$body")
case "$status" in
  200) ;;
  30*)
    [[ -n "${redirect:-}" ]] || missed
    status=$(curl -sS --max-time 120 -o "$body" -w '%{http_code}' "$redirect" 2> /dev/null)
    [[ "$status" == "200" ]] || missed
    ;;
  401 | 403 | 429) give_up ;;
  *) missed ;;
esac

[[ -s "$body" ]] || missed
rm -f "$MISSES_FILE" 2> /dev/null || true
# A pipeline that stopped at the error, or that filtered everything out, is not
# a failure to report: the callers judge this by what it printed.
clean_log "$body"
exit 0
