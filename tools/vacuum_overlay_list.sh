#!/usr/bin/env bash
#
# Drive tools/vacuum_overlay.sh through an ordered list of overlays, optionally
# with several workers at once.
#
# Usage:
#   tools/vacuum_overlay_list.sh --list FILE [--profile NAME] [--jobs N]
#                                [--cli claude|grok|codex] [--times N] [--keep]
#
# Why a list rather than the built-in order: vacuum_orch.rank_overlays sorts by
# unmatched-function count alone, which knows nothing about duplication. In
# actors the leading digit is a load-address bucket, so actor_207000 holds 44
# functions but contributes one body that actor_107000 has not already covered.
# A list computed from the duplicate index (see local/ACTORS_SWEEP_ORDER.md)
# spends each match where it retires the most work.
#
# Concurrency is safe without any locking of its own: vacuum_orch leases every
# function in an overlay at once, so a second worker asking for a leased overlay
# is refused and moves to the next name. The cursor below only stops two workers
# starting the same overlay at the same instant and wasting a worktree build.
#
# SIGINT finishes the overlay in flight and then stops, matching vacuum.sh.

set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
. "$ROOT/tools/vacuum_profile.sh"

LIST=""
JOBS=1
PROFILE_ARG=""
PASSTHRU=()

usage() { sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 1; }

while [[ $# -gt 0 ]]; do
    case $1 in
        --list)    LIST="$2"; shift 2 ;;
        --jobs)    JOBS="$2"; shift 2 ;;
        --profile) PROFILE_ARG="$2"; PASSTHRU+=(--profile "$2"); shift 2 ;;
        --cli|--times) PASSTHRU+=("$1" "$2"); shift 2 ;;
        --claude|--grok|--codex|--keep|--no-land) PASSTHRU+=("$1"); shift ;;
        -h|--help) usage ;;
        *) echo "unknown argument: $1" >&2; usage ;;
    esac
done

[[ -n "$LIST" && -f "$LIST" ]] || { echo "need --list FILE" >&2; usage; }
[[ "$JOBS" =~ ^[0-9]+$ ]] || { echo "--jobs must be a number" >&2; exit 1; }

LOG_DIR="$(vacuum_log_dir)"
RUN="$LOG_DIR/overlay-list-$$"
mkdir -p "$RUN"
CURSOR="$RUN/queue"
grep -vE '^\s*#|^\s*$' "$LIST" | awk '{print $1}' >"$CURSOR"
TOTAL=$(wc -l <"$CURSOR")

STOP=0
trap 'echo ""; echo "Interrupt received; workers stop after the overlay in flight."; STOP=1' INT

log() { echo "[$(date '+%H:%M:%S')] $*" | tee -a "$RUN/driver.log"; }
log "list $LIST: $TOTAL overlays, $JOBS worker(s)${PROFILE_ARG:+, profile $PROFILE_ARG}"
log "run dir $RUN"

# Pop the next name atomically. flock keeps two workers from picking the same
# overlay in the same instant; the lease is what actually guarantees exclusion.
next_overlay() {
    local name=""
    exec 9>"$CURSOR.lock"
    flock 9
    if [[ -s "$CURSOR" ]]; then
        name=$(head -1 "$CURSOR")
        tail -n +2 "$CURSOR" >"$CURSOR.tmp" && mv "$CURSOR.tmp" "$CURSOR"
    fi
    flock -u 9
    exec 9>&-
    echo "$name"
}

worker() {
    local id=$1 name rc done=0 skipped=0
    while [[ $STOP -eq 0 ]]; do
        name=$(next_overlay)
        [[ -n "$name" ]] || break
        log "worker $id -> $name"
        "$ROOT/tools/vacuum_overlay.sh" --overlay "$name" "${PASSTHRU[@]}" \
            >>"$RUN/worker-$id.log" 2>&1
        rc=$?
        if [[ $rc -eq 0 ]]; then
            done=$((done + 1))
        else
            # A refused lease, or an overlay whose work vanished while queued,
            # is not a failure of this driver - take the next name.
            skipped=$((skipped + 1))
            log "worker $id: $name returned $rc (leased elsewhere, or no work left)"
        fi
    done
    log "worker $id finished: $done swept, $skipped skipped"
}

for i in $(seq 1 "$JOBS"); do
    worker "$i" &
done
wait
log "all workers done; $(wc -l <"$CURSOR") overlays left unclaimed in the queue"
log "logs: $RUN"
