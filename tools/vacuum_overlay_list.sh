#!/usr/bin/env bash
#
# Drive tools/vacuum_overlay.sh through an ordered list of overlays, optionally
# with several workers at once.
#
# Usage:
#   tools/vacuum_overlay_list.sh --list FILE [--profile NAME] [--jobs N]
#                                [--stagger SECONDS] [--max-difficulty 0..1]
#                                [--cli claude|grok|codex] [--times N] [--keep]
#                                [--difficult]
#
# --difficult walks the same list but sweeps only the give-ups in
# tools/difficult_functions, passing through to vacuum_overlay.sh and to the
# claim filter - so an overlay with no parked work is skipped before it costs a
# worktree, exactly as an overlay with nothing under --max-difficulty is.
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
DIFFICULT=false
PASSTHRU=()

usage() { sed -n '2,26p' "$0" | sed 's/^# \{0,1\}//'; exit 1; }

while [[ $# -gt 0 ]]; do
    case $1 in
        --list)    LIST="$2"; shift 2 ;;
        --stagger) STAGGER="$2"; shift 2 ;;
        --jobs)    JOBS="$2"; shift 2 ;;
        --profile) PROFILE_ARG="$2"; PASSTHRU+=(--profile "$2"); shift 2 ;;
        --cli|--times|--max-difficulty) PASSTHRU+=("$1" "$2"); shift 2 ;;
        --claude|--grok|--codex|--keep|--no-land) PASSTHRU+=("$1"); shift ;;
        # Both halves need it: the sweep picks difficult functions, and the
        # claim filter has to keep them rather than drop them as parked.
        --difficult|--only-difficult) DIFFICULT=true; PASSTHRU+=(--difficult); shift ;;
        -h|--help) usage ;;
        *) echo "unknown argument: $1" >&2; usage ;;
    esac
done

[[ -n "$LIST" && -f "$LIST" ]] || { echo "need --list FILE" >&2; usage; }
# The list also decides which overlay owns each shared body: the first carrier
# in list order, or its promoted lib copy. Every sweep this driver starts - the
# claim filter, vacuum_overlay.sh and the inner vacuum - skips the other copies,
# so parallel workers never match one body twice or give it up once per carrier.
# Absolute, because the inner vacuum runs in a worktree without local/.
VACUUM_OWNER_LIST="$(realpath "$LIST")"
export VACUUM_OWNER_LIST
[[ "$JOBS" =~ ^[0-9]+$ ]] || { echo "--jobs must be a number" >&2; exit 1; }

LOG_DIR="$(vacuum_log_dir)"
RUN="$LOG_DIR/overlay-list-$$"
mkdir -p "$RUN"
STAGGER="${STAGGER:-30}"   # seconds between worker starts; 0 disables
CURSOR="$RUN/queue"
fill_queue() { grep -vE '^\s*#|^\s*$' "$LIST" | awk '{print $1}' >"$CURSOR"; }
fill_queue
TOTAL=$(wc -l <"$CURSOR")

# One pass over the list is not enough. Workers walk a shared queue in order, so
# an overlay that is leased at the moment a worker reaches it is recorded as
# skipped and never revisited - and with several workers contending, most names
# are leased when someone gets to them. A worker that reached the end simply
# exited, so a run decayed to nothing while work remained: one run ended with 43
# free sub-0.3 functions and no workers left, another swept 1 overlay and
# skipped 39.
#
# So re-walk while the last pass achieved something. "Achieved something" means
# a sweep ran to completion, which releases a lease and can free an overlay for
# the next pass; a pass of pure skips changes nothing and ends the run. The cap
# is a backstop against a pathological cycle, not the expected exit.
#
# "Nothing claimable" is only final when no sweep is in flight. Every in-flight
# sweep will release its lease, and a worker that found the queue empty used to
# exit on the spot: a 12-worker run lost a worker each time one finished its
# overlay while the rest were still busy, and was down to 7 within the hour.
# So an idle worker waits for the next sweep to finish and re-walks after it,
# and only exits once the queue is dry with nothing running. Because a refill
# now follows every completed sweep rather than every burst of them, the pass
# cap has to be sized in sweeps, not in walks of the list.
PASS_FILE="$RUN/pass";  echo 1 >"$PASS_FILE"
SWEPT_FILE="$RUN/swept"; echo 0 >"$SWEPT_FILE"
BUSY_FILE="$RUN/busy";   echo 0 >"$BUSY_FILE"
MAX_PASSES="${VACUUM_MAX_PASSES:-200}"
IDLE_POLL="${VACUUM_IDLE_POLL:-60}"   # seconds an idle worker waits between re-checks

STOP=0
trap 'echo ""; echo "Interrupt received; workers stop after the overlay in flight."; STOP=1' INT

log() { echo "[$(date '+%H:%M:%S')] $*" | tee -a "$RUN/driver.log"; }
# Same, but never on stdout. next_overlay's stdout IS the overlay name - it is
# read with name=$(next_overlay) - so a log line written there is captured as
# part of the name and the worker tries to sweep it.
log_q() { echo "[$(date '+%H:%M:%S')] $*" | tee -a "$RUN/driver.log" >&2; }
log "list $LIST: $TOTAL overlays, $JOBS worker(s)${PROFILE_ARG:+, profile $PROFILE_ARG}"
log "run dir $RUN"

# Pop the next name atomically. flock keeps two workers from picking the same
# overlay in the same instant; the lease is what actually guarantees exclusion.
# Add $2 to the counter in file $1, under the cursor lock.
bump() {
    exec 8>"$CURSOR.lock"
    flock 8
    echo $(( $(cat "$1" 2>/dev/null || echo 0) + $2 )) >"$1"
    flock -u 8
    exec 8>&-
}
note_swept() { bump "$SWEPT_FILE" 1; }

# Prints a name, WAIT (queue dry but sweeps are still in flight), or nothing
# (exhausted: the worker should exit).
next_overlay() {
    local name="" swept pass busy
    exec 9>"$CURSOR.lock"
    flock 9
    if [[ ! -s "$CURSOR" ]]; then
        swept=$(cat "$SWEPT_FILE" 2>/dev/null || echo 0)
        pass=$(cat "$PASS_FILE" 2>/dev/null || echo 1)
        busy=$(cat "$BUSY_FILE" 2>/dev/null || echo 0)
        if [[ "$swept" -gt 0 && "$pass" -lt "$MAX_PASSES" ]]; then
            fill_queue
            echo $((pass + 1)) >"$PASS_FILE"
            echo 0 >"$SWEPT_FILE"
            log_q "pass $pass swept $swept overlay(s); re-walking the list (pass $((pass + 1)))"
        elif [[ "$busy" -gt 0 && "$pass" -lt "$MAX_PASSES" ]]; then
            name=WAIT
        elif [[ "$pass" -ge "$MAX_PASSES" ]]; then
            log_q "pass cap $MAX_PASSES reached; stopping"
        else
            log_q "pass $pass claimed nothing and no sweep is in flight; list is exhausted"
        fi
    fi
    if [[ -s "$CURSOR" ]]; then
        name=$(head -1 "$CURSOR")
        tail -n +2 "$CURSOR" >"$CURSOR.tmp" && mv "$CURSOR.tmp" "$CURSOR"
    fi
    flock -u 9
    exec 9>&-
    echo "$name"
}

worker() {
    local id=$1 name rc done=0 skipped=0 stranded=0
    # Stagger the first claim. A worker's first act is a 235MB asm copy and
    # four submodule clones; N of those at once is an I/O storm - eight of them
    # drove load to 88 and took available memory from 17G to 3G, all of it page
    # cache the kernel then had to reclaim. Subsequent claims desynchronise on
    # their own, so only the opening needs spreading.
    if [[ $id -gt 1 && ${STAGGER:-0} -gt 0 ]]; then
        log "worker $id: waiting $(( (id - 1) * STAGGER ))s before its first claim"
        sleep $(( (id - 1) * STAGGER ))
    fi
    local idle=0
    while [[ $STOP -eq 0 ]]; do
        name=$(next_overlay)
        [[ -n "$name" ]] || break
        if [[ "$name" == WAIT ]]; then
            [[ $idle -eq 1 ]] || log "worker $id: nothing claimable; waiting for an in-flight sweep to finish"
            idle=1
            sleep "$IDLE_POLL"
            continue
        fi
        idle=0
        # Busy from the moment a name is taken, not from when the sweep starts:
        # a claim in progress can still produce a sweep, and counting only the
        # sweep let an idle worker see busy=0 between another's claim and its
        # launch, and exit.
        bump "$BUSY_FILE" 1
        log "worker $id -> $name"
        # Claim here, not in the worker: claiming is cheap and filtering
        # cheaper still, so an overlay whose remaining functions are all shared
        # bodies is dropped before anything pays for a 235MB worktree.
        #
        # Exit 1 is a decision about the overlay (skip it); anything else means
        # the filter could not decide, and the sweep must fall back to letting
        # the worker claim for itself. Treating a broken helper as "skip" would
        # silently do nothing at all, overlay after overlay.
        sess="ovb-${name//\//-}-$$-$id"
        claim_args=()
        if [[ -x "$ROOT/tools/claim_filter.py" ]]; then
            # Pass the bound too: without it the filter clears an overlay whose
            # only remaining work is above the bound, and the worker pays a full
            # worktree and split to match nothing.
            bound=""
            for ((bi=0; bi<${#PASSTHRU[@]}; bi++)); do
                [[ "${PASSTHRU[$bi]}" == "--max-difficulty" ]] && bound="${PASSTHRU[$((bi+1))]}"
            done
            # $BASHPID, not $$: in this subshell $$ is the driver's pid, so a
            # lease taken here outlived the worker that took it and the
            # orchestrator's liveness sweep could never reclaim it.
            dflag=""
            [[ "$DIFFICULT" == true ]] && dflag="--difficult"
            claim=$("$ROOT/tools/claim_filter.py" "$name" "$sess" $BASHPID $bound $dflag 2>>"$RUN/worker-$id.log")
            crc=$?
            if [[ $crc -eq 1 ]]; then
                skipped=$((skipped + 1))
                log "worker $id: $name skipped (unclaimable, or nothing landable here)"
                bump "$BUSY_FILE" -1
                continue
            elif [[ $crc -eq 0 ]]; then
                claim_args=(--pre-claimed --session "$sess")
                log "worker $id: $name claimed as $sess"
            else
                log "worker $id: claim filter unusable (rc=$crc); letting the worker claim $name"
            fi
        fi
        "$ROOT/tools/vacuum_overlay.sh" --overlay "$name" ${claim_args[@]+"${claim_args[@]}"} \
            "${PASSTHRU[@]}" >>"$RUN/worker-$id.log" 2>&1
        rc=$?
        if [[ $rc -eq 0 ]]; then
            done=$((done + 1))
            note_swept
        elif [[ $rc -eq 4 ]]; then
            # Completed, matched nothing. It is done - the lease is released and
            # the give-up bookkeeping carried - but it is not progress, so it
            # must not justify another walk. Counting it did: a --difficult pass
            # re-picked the same function every walk, since re-marking it
            # difficult leaves it exactly as claimable as before.
            done=$((done + 1))
            log "worker $id: $name swept, nothing matched"
        elif [[ $rc -eq 3 ]]; then
            # Exit 3 means the sweep matched functions it could not land: the
            # work is verified and committed on an overlay/* branch, and the
            # worktree is still there. That is nothing like a refused lease, and
            # collapsing the two is how actor_503500's 68 matches went unnoticed.
            stranded=$((stranded + 1))
            log "worker $id: LANDING FAILED for $name - matches are stranded on overlay/$name; see the log"
        else
            # A refused lease, or an overlay whose work vanished while queued,
            # is not a failure of this driver - take the next name.
            skipped=$((skipped + 1))
            log "worker $id: $name returned $rc (leased elsewhere, or no work left)"
            # If we claimed for it, the lease is ours to drop: overlay_batch.sh
            # leaves a --pre-claimed lease alone on purpose. Without this, one
            # transient failure - a half-extracted asset tree took out 50 sweeps
            # at once - parks every function in the overlay for the full lease,
            # and the retry is refused by our own orphaned claims.
            if [[ ${#claim_args[@]} -gt 0 ]]; then
                "$ROOT/tools/vacuum_orch.py" relinquish-overlay --session "$sess" \
                    >>"$RUN/worker-$id.log" 2>&1 || true
                log "worker $id: released $name's lease after rc=$rc"
            fi
        fi
        # Belt and braces, independent of the exit code: if the branch survived
        # with commits ahead of main, matches are stranded whatever rc said. An
        # exit code can only report a path the script actually took - a crash, a
        # kill, or a path that still exits 1 reports nothing at all, and that is
        # how 19 verified matches sat unnoticed behind a plain "returned 1".
        b="overlay/$name"
        if git -C "$ROOT" rev-parse --verify -q "$b" >/dev/null 2>&1; then
            ahead=$(git -C "$ROOT" log --oneline "main..$b" 2>/dev/null | grep -c ' matched ')
            if [[ "${ahead:-0}" -gt 0 ]]; then
                log "worker $id: STRANDED - $b still holds $ahead verified match(es) after rc=$rc"
            fi
        fi
        bump "$BUSY_FILE" -1
    done
    log "worker $id finished: $done swept, $skipped skipped, $stranded stranded"
}

for i in $(seq 1 "$JOBS"); do
    worker "$i" &
done
wait
log "all workers done; $(wc -l <"$CURSOR") overlays left unclaimed in the queue"
log "logs: $RUN"
