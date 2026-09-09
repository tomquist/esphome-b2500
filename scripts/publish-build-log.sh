#!/usr/bin/env bash
#
# Publishes the build output as append-only segments next to the status
# document, and prints how many exist.
#
# NOT WIRED UP: its only source of build output was fetch-job-log.sh, which
# cannot read this job's log while the job is running - see the header there.
# Everything below still holds for whatever does feed it next.
#
# S3 has no append: an object is replaced whole or not at all. Re-uploading one
# growing object would mean sending the entire log every few seconds, and the
# page re-reading it. So the log is published as a run of immutable segments -
# `firmware/<identifier>.log.0`, `.log.1`, ... - each holding only the bytes
# that appeared since the last one. Every byte is uploaded once and downloaded
# once, which is the best a bucket with no server in front of it can do.
#
# The page learns how many exist from `log_segments` in the status document, so
# a segment is always uploaded before the status that announces it. It never
# has to guess whether a missing segment means "not yet" or "no more".
#
# Correctness rests on the cleaned log being a growing prefix of itself: the
# job log the API returns only ever has lines appended, and cleaning is a
# per-line transform. Two things can break that, and both are guarded here
# rather than assumed away:
#
#   - a read that arrives cut mid-line, whose last line grows later. The fetch
#     command is what can see that, and drops the unfinished line; the check
#     below is what catches it having failed to.
#   - a read whose earlier bytes are not the ones already published, which is
#     what a moved start marker looks like. The bytes published so far are kept
#     and compared, not just counted, and a read that fails the comparison is
#     skipped. The page keeps a short log rather than a corrupt one.
#
# Two of these must never run at once. The workflow waits for the watcher to
# exit before the final flush, but that wait has a deadline and the kill that
# follows it reaches only the watcher, not a publisher it left mid-upload. Two
# publishers sharing this state would both write the same segment key, and the
# one that finished second would decide what it holds - dropping whatever the
# other had put there. So the whole read-and-publish is taken under a lock, and
# a publisher that cannot get it publishes nothing rather than racing.
#
# Usage: publish-build-log.sh
#
# Environment:
#   IDENTIFIER            build identifier from the repository dispatch
#   S3_BUCKET             target bucket
#   LOG_FETCH_COMMAND     what prints the cleaned log, default fetch-job-log.sh
#   LOG_STATE_PREFIX      where the published bytes and count are kept
#   LOG_MAX_TOTAL_BYTES   stop publishing once this much has been published
#   LOG_LOCK_WAIT_SECONDS how long to wait for another publisher to finish
#   plus everything fetch-job-log.sh reads

set -uo pipefail

FETCH="${LOG_FETCH_COMMAND:-./scripts/fetch-job-log.sh}"
STATE_PREFIX="${LOG_STATE_PREFIX:-build-log}"
# A build that somehow prints without end must not publish without end either.
MAX_TOTAL_BYTES="${LOG_MAX_TOTAL_BYTES:-4000000}"

COUNT_FILE="${STATE_PREFIX}.count"
PUBLISHED_FILE="${STATE_PREFIX}.published"
FULL_FILE="${STATE_PREFIX}.full"
SEGMENT_FILE="${STATE_PREFIX}.segment"
LOCK_FILE="${STATE_PREFIX}.lock"
LOCK_WAIT_SECONDS="${LOG_LOCK_WAIT_SECONDS:-60}"

read_count() {
  local value
  value=$(cat "$COUNT_FILE" 2> /dev/null || echo 0)
  [[ "$value" =~ ^[0-9]+$ ]] && echo "$value" || echo 0
}

# Whatever happens below, the caller needs the number of segments that exist,
# so that a failure to publish a new one still names the ones that do. Read
# back rather than remembered: another publisher may have moved it on.
report() {
  read_count
  exit 0
}

# Held for the life of the script, released when it exits and the descriptor
# closes. Where there is no flock to be had, carry on without one: this is best
# effort, and the alternative is publishing nothing at all.
if command -v flock > /dev/null 2>&1; then
  exec {lock_fd}> "$LOCK_FILE" 2> /dev/null || lock_fd=''
  if [[ -n "${lock_fd:-}" ]] && ! flock -w "$LOCK_WAIT_SECONDS" "$lock_fd"; then
    echo "Another build output publisher is still running, skipping" >&2
    report
  fi
fi

count=$(read_count)
offset=$(stat -c%s "$PUBLISHED_FILE" 2> /dev/null || echo 0)

if [[ ! "${IDENTIFIER:-}" =~ ^[a-z0-9-]{1,64}$ || -z "${S3_BUCKET:-}" ]]; then
  echo "No usable build identifier or bucket, skipping log upload" >&2
  report
fi

if [[ "$offset" -ge "$MAX_TOTAL_BYTES" ]]; then
  echo "Published $offset bytes of build output already, not publishing more" >&2
  report
fi

if ! "$FETCH" > "$FULL_FILE" 2> /dev/null || [[ ! -s "$FULL_FILE" ]]; then
  report
fi

total=$(stat -c%s "$FULL_FILE" 2> /dev/null || echo 0)
if [[ "$total" -le "$offset" ]]; then
  report
fi

# The bytes already published have to still be there, and still be these ones.
if [[ "$offset" -gt 0 ]] && ! cmp -s -n "$offset" "$PUBLISHED_FILE" "$FULL_FILE"; then
  echo "Build output no longer starts with what was published, skipping" >&2
  report
fi

# `tail -c +N` counts from one, so the byte after the offset is N = offset + 1.
# Bounded by what is left of the budget rather than by the budget alone: the
# check above only stops the publish *after* the cap is passed, so without this
# the first segment could carry a runaway log whole. Cutting mid-line is fine
# here - the page joins the segments back up, and the published bytes are still
# a prefix of the log.
tail -c "+$((offset + 1))" "$FULL_FILE" |
  head -c "$((MAX_TOTAL_BYTES - offset))" > "$SEGMENT_FILE"
[[ -s "$SEGMENT_FILE" ]] || report

if ! aws s3 cp "$SEGMENT_FILE" \
  "s3://${S3_BUCKET}/firmware/${IDENTIFIER}.log.${count}" \
  --content-type 'text/plain; charset=utf-8' \
  --cache-control 'public, max-age=31536000, immutable' > /dev/null; then
  # The state is left alone so the same bytes go out again next time.
  echo "Could not upload build output segment ${count}" >&2
  report
fi

cat "$SEGMENT_FILE" >> "$PUBLISHED_FILE"
echo "$((count + 1))" > "$COUNT_FILE"
published=$(stat -c%s "$PUBLISHED_FILE" 2> /dev/null || echo "$total")
echo "Published build output segment ${count}, $published bytes so far" >&2
report
