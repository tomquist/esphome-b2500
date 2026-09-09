#!/usr/bin/env bash
#
# Prints a cleaned tail of this workflow job's own log.
#
# The compile runs inside esphome/build-action, so its output belongs to that
# step rather than to us: there is no file to tail and no pipe to tee. The
# Actions API does serve a job's log while the job is still running, though, so
# reading our own log back is the one way to get the compiler's output out of
# the runner and in front of the person waiting for the build. The watcher
# publishes what this prints every few seconds, and the failure path publishes
# a longer tail ending at the error.
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
#   LOG_TAIL_LINES      how many lines to print
#   LOG_MAX_BYTES       hard cap on what is printed, applied after the lines
#   LOG_END_AT_ERROR    stop at the first `##[error]` line, keeping it
#   LOG_SOURCE_FILE     read this instead of the API (used by the tests)
#   LOG_JOB_ID_FILE     caches the resolved job id between calls
#   LOG_DISABLED_FILE   marks the log as unreadable, so we stop asking

set -uo pipefail

TAIL_LINES="${LOG_TAIL_LINES:-40}"
MAX_BYTES="${LOG_MAX_BYTES:-8000}"
API_URL="${GITHUB_API_URL:-https://api.github.com}"
JOB_ID_FILE="${LOG_JOB_ID_FILE:-job-log-id}"
DISABLED_FILE="${LOG_DISABLED_FILE:-job-log-unavailable}"
MISSES_FILE="${DISABLED_FILE}.misses"
# A log that is not there yet reads exactly like one we will never be allowed
# to read, so give it a few tries before giving up on it for good.
MAX_MISSES="${LOG_MAX_MISSES:-5}"

ESC=$(printf '\033')

# Turns a downloaded job log into something worth showing: no timestamps, no
# terminal escapes, and no runner command markers, which are noise everywhere
# except on the error that ended the build.
clean_log() {
  sed -E \
    -e 's/\r$//' \
    -e 's/^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]+Z //' \
    -e "s/${ESC}\\[[0-9;?]*[a-zA-Z]//g" \
    -e "s/${ESC}[()][A-B0-2]//g" \
    -e "s/${ESC}\\][^${ESC}]*(\\a|${ESC}\\\\)//g" |
    # The failing step is the last thing the log has to say that anyone cares
    # about; whatever the workflow does afterwards (publishing this log, among
    # other things) would only push it out of the tail.
    { if [[ -n "${LOG_END_AT_ERROR:-}" ]]; then
        awk '{ print } /^##\[error\]/ { exit }'
      else
        cat
      fi; } |
    sed -E \
      -e 's/^##\[error\]/ERROR: /' \
      -e 's/^##\[warning\]/WARNING: /' |
    grep -v '^##\[' |
    tail -n "$TAIL_LINES" |
    tail -c "$MAX_BYTES"
}

if [[ -n "${LOG_SOURCE_FILE:-}" ]]; then
  [[ -r "$LOG_SOURCE_FILE" ]] || exit 1
  clean_log < "$LOG_SOURCE_FILE"
  exit 0
fi

if [[ -z "${GITHUB_TOKEN:-}" || -z "${GITHUB_REPOSITORY:-}" || -z "${GITHUB_RUN_ID:-}" ]]; then
  exit 1
fi
if [[ -e "$DISABLED_FILE" ]]; then
  exit 1
fi

# Prints the response status, writes the body to $2.
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
trap 'rm -f "$body"' EXIT

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
# A pipeline that stopped at the error, or that filtered everything out,
# is not a failure to report: the callers judge this by what it printed.
clean_log < "$body"
exit 0
