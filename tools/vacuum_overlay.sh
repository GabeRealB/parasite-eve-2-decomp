#!/usr/bin/env bash
#
# Vacuum one whole overlay: claim every function in it, match them one at a
# time, then land the batch onto the main tree.
#
# This is tools/vacuum.sh with a different unit of *claiming*, not a different
# unit of *work*. Handing one agent a whole overlay was tried and cost 25k-47k
# tokens per matched function, because that agent has to reinvent the loop
# vacuum.sh already has: scratch env, m2c seed, a per-function score with
# penalties, MATCH_LOOP.md as rules, the permuter fallback on a high give-up,
# difficult-list bookkeeping. So the agent still sees exactly one function at a
# time; only the lease and the landing are batched.
#
# The inner vacuum runs inside the overlay's own worktree, so its per-function
# `matched <fn> <attempts>` commits land on that branch and the main tree is
# untouched until the batch is ready. Nothing here takes the merge lock until
# then, which is the whole point of doing it in a worktree.
#
# Usage:
#   tools/vacuum_overlay.sh [--overlay NAME] [--cli claude|grok] [--times N]
#                           [--keep] [--dry-run] [--no-land]
#
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

OVERLAY=""
CLI="${VACUUM_CLI:-claude}"
TIMES=""
KEEP=false
DRY_RUN=false
NO_LAND=false

usage() { sed -n '2,24p' "$0" | sed 's/^# \{0,1\}//'; exit 1; }

while [[ $# -gt 0 ]]; do
    case $1 in
        --overlay) OVERLAY="$2"; shift 2 ;;
        --cli)     CLI="$2"; shift 2 ;;
        --claude)  CLI=claude; shift ;;
        --grok)    CLI=grok; shift ;;
        --times)   TIMES="$2"; shift 2 ;;
        --keep)    KEEP=true; shift ;;
        --dry-run) DRY_RUN=true; shift ;;
        --no-land) NO_LAND=true; shift ;;
        -h|--help) usage ;;
        *) echo "unknown argument: $1" >&2; usage ;;
    esac
done

orch() { python3 "$ROOT/tools/vacuum_orch.py" --root "$ROOT" "$@"; }

# --- lease + worktree ---------------------------------------------------------
SESSION="ovb-${OVERLAY:-auto}-$$"
LOG_FILE="$ROOT/tools/vacuum-overlay-${OVERLAY:-auto}-$$.log"
: >"$LOG_FILE"
log() { echo "[$(date '+%H:%M:%S')] $*" | tee -a "$LOG_FILE"; }

log "session $SESSION, cli $CLI, log $LOG_FILE"

prep_args=(--session "$SESSION" --bootstrap 0)
[[ -n "$OVERLAY" ]] && prep_args+=(--overlay "$OVERLAY")
if ! "$ROOT/tools/overlay_batch.sh" "${prep_args[@]}" >>"$LOG_FILE" 2>&1; then
    log "could not lease an overlay (see $LOG_FILE)"
    exit 1
fi
OVERLAY=$(awk '/^leased /{print $2; exit}' "$LOG_FILE" | tr -d ':')
WT="$ROOT/../pe2-ov-$OVERLAY"
log "leased $OVERLAY, worktree $WT"

release_all() {
    orch relinquish-overlay --session "$SESSION" >/dev/null 2>&1 || true
}
trap release_all EXIT

CLAIMED=$(orch status 2>/dev/null | python3 -c '
import json, sys
d = json.load(sys.stdin)
print("\n".join(sorted(f for f, c in d["claims"].items()
                       if c.get("session") == sys.argv[1])))' "$SESSION")
CLAIMED_N=$(grep -c . <<<"$CLAIMED" || echo 0)
log "$CLAIMED_N function(s) claimed"

# --- match, one function at a time -------------------------------------------
# vacuum.sh picks the easiest remaining function in the overlay each iteration
# (tools/score_functions.py), so the ordering by difficulty is already its job.
inner=(./tools/vacuum.sh --cli "$CLI" --overlay "$OVERLAY")
[[ -n "$TIMES" ]] && inner+=(--times "$TIMES")
[[ "$DRY_RUN" == true ]] && inner+=(--dry-run)

log "running: ${inner[*]}  (in $WT)"
BASE=$(git -C "$WT" rev-parse HEAD)
# No `| tee` here: the inner vacuum appends to this same file itself (via
# VACUUM_LOG_FILE), so the log stays live during a match instead of filling in
# one burst when the function finishes. Piping as well would duplicate it.
(cd "$WT" && VACUUM_LOG_FILE="$LOG_FILE" "${inner[@]}") 2>&1
log "inner vacuum finished"

# --- collect what it actually matched -----------------------------------------
mapfile -t MATCHED < <(git -C "$WT" log --format=%s "$BASE"..HEAD \
                       | awk '/^matched /{print $2}' | tac)
log "matched ${#MATCHED[@]} function(s)"
printf '  %s\n' "${MATCHED[@]}" | tee -a "$LOG_FILE"

if [[ "$DRY_RUN" == true || "$NO_LAND" == true || ${#MATCHED[@]} -eq 0 ]]; then
    if [[ "$DRY_RUN" == true || "$NO_LAND" == true ]]; then
        log "not landing (dry-run or --no-land); worktree kept at $WT"
        trap - EXIT
        exit 0
    fi
    # Nothing matched. That is a legitimate outcome - mist_r21's only function
    # is a 22-instruction body with 238 copies that is already matched
    # elsewhere, so the vacuum correctly refuses to match it again and wants it
    # promoted instead. But the run is over either way, so hold nothing: an
    # abandoned lease keeps those functions unclaimable until it expires, and
    # `trap - EXIT` here used to skip the release entirely.
    log "nothing matched; releasing the lease"
    if [[ "$KEEP" == false ]]; then
        "$ROOT/tools/overlay_batch.sh" --cleanup --overlay "$OVERLAY" --session "$SESSION" \
            >>"$LOG_FILE" 2>&1 || log "worktree cleanup refused; see $LOG_FILE"
    fi
    trap - EXIT
    release_all
    exit 0
fi

# --- land the batch -----------------------------------------------------------
# Everything the branch touched outside src/<overlay> - new headers, learnings,
# a manifest cut - has to travel with the bodies or trunk will not build.
mapfile -t EXTRAS < <(git -C "$WT" diff --name-only "$BASE"..HEAD \
                      | grep -v "^src/.*/$OVERLAY/" || true)
log "extra paths: ${EXTRAS[*]:-none}"

log "acquiring merge lock"
if ! orch merge-acquire --session "$SESSION" --pid $$ --wait "${VACUUM_MERGE_WAIT:-3600}" \
     >>"$LOG_FILE" 2>&1; then
    log "could not take the merge lock; worktree kept at $WT"
    trap - EXIT
    exit 1
fi
release_all() {
    orch merge-release --session "$SESSION" >/dev/null 2>&1 || true
    orch relinquish-overlay --session "$SESSION" >/dev/null 2>&1 || true
}

# Prefer replaying the worktree's own commits. They are already one
# `matched <fn> <attempts>` per function, so the attempt counts survive - and a
# promotion, which moves bodies into src/<family>/lib and re-partitions the
# overlay's units, leaves trunk and the worktree with no per-function
# correspondence at all. mist_r18 promoted a shared body into
# rooms_shared_8017df80 and land_overlay.py refused all 13 functions with "no
# INCLUDE_ASM slot on trunk"; cherry-picking the same 13 commits applied
# cleanly and kept a 15-attempt and a 9-attempt count that the rewrite path
# would have recorded as 1.
#
# It is only safe when trunk has not moved underneath: the base must still be an
# ancestor, and no path the branch touched may have changed on trunk since. Fall
# back to the file rewrite otherwise, which is what handles a drifted trunk.
BRANCH_NAME=$(git -C "$WT" rev-parse --abbrev-ref HEAD)
CAN_REPLAY=false
if git merge-base --is-ancestor "$BASE" HEAD 2>/dev/null; then
    touched=$(git -C "$WT" diff --name-only "$BASE"..HEAD)
    if [[ -z "$(git diff --name-only "$BASE"..HEAD -- $touched 2>/dev/null)" ]]; then
        CAN_REPLAY=true
    fi
fi

if [[ "$CAN_REPLAY" == true ]]; then
    log "replaying $(git rev-list --count "$BASE".."$BRANCH_NAME") commit(s) from $BRANCH_NAME"
    if git cherry-pick "$BASE".."$BRANCH_NAME" >>"$LOG_FILE" 2>&1; then
        log "replayed cleanly"
    else
        git cherry-pick --abort >/dev/null 2>&1 || true
        log "replay failed; falling back to the file rewrite"
        CAN_REPLAY=false
    fi
fi

funcs_file=$(mktemp)
printf '%s\n' "${MATCHED[@]}" >"$funcs_file"
land_args=("$WT" "$OVERLAY" --functions-file "$funcs_file" --base "$BASE")
if [[ ${#EXTRAS[@]} -gt 0 ]]; then
    land_args+=(--extra "$(IFS=,; echo "${EXTRAS[*]}")")
fi

if [[ "$CAN_REPLAY" != true ]] \
   && ! python3 "$ROOT/tools/land_overlay.py" "${land_args[@]}" >>"$LOG_FILE" 2>&1; then
    log "landing failed; trunk left alone, worktree kept at $WT"
    rm -f "$funcs_file"; exit 1
fi
rm -f "$funcs_file"

# A manifest change only takes effect after a re-split.
if printf '%s\n' "${EXTRAS[@]}" | grep -q 'configs/USA/overlays.toml'; then
    log "manifest changed; re-splitting"
    venv/bin/python3 ninja_config.py >>"$LOG_FILE" 2>&1 || {
        log "re-split failed"; exit 1; }
fi

log "verifying trunk (unscoped)"
if ! ./tools/build-and-verify.sh >>"$LOG_FILE" 2>&1; then
    log "TRUNK BUILD FAILED after landing $OVERLAY - not resetting, inspect $LOG_FILE"
    exit 1
fi
log "trunk verified"

# --- bookkeeping --------------------------------------------------------------
DIFFICULT=$(awk '{print $1}' "$ROOT/tools/difficult_functions" 2>/dev/null \
            | grep -F "_${OVERLAY}_" || true)
matched_csv=$(IFS=,; echo "${MATCHED[*]}")
diff_csv=$(tr '\n' ',' <<<"$DIFFICULT" | sed 's/,$//')
unattempted=$(comm -23 <(sort <<<"$CLAIMED") \
                       <(printf '%s\n%s\n' "$matched_csv" "$diff_csv" | tr ',' '\n' | sort -u) \
              | tr '\n' ',' | sed 's/,$//')

orch finish-overlay --session "$SESSION" \
    --matched "$matched_csv" \
    ${diff_csv:+--difficult "$diff_csv"} \
    ${unattempted:+--unattempted "$unattempted"} >>"$LOG_FILE" 2>&1 || true

# tools/giveups/ is gitignored, so nothing in the landing path moves it and the
# worktree is about to be deleted. It holds the best compiling C for every
# function that stalled - mist_parking's 8017F764 reached 98.846% over 21
# attempts - and its whole purpose is to stop a later retry restarting from m2c.
# Copy any archive trunk does not already have.
if [[ -d "$WT/tools/giveups" ]]; then
    carried=0
    for d in "$WT"/tools/giveups/*/; do
        [[ -d "$d" ]] || continue
        name=$(basename "$d")
        if [[ ! -d "$ROOT/tools/giveups/$name" ]]; then
            mkdir -p "$ROOT/tools/giveups"
            cp -r "$d" "$ROOT/tools/giveups/$name" && carried=$((carried+1))
        fi
    done
    [[ $carried -gt 0 ]] && log "carried $carried give-up archive(s) to trunk"
fi

log "landed ${#MATCHED[@]}; difficult $(grep -c . <<<"$DIFFICULT" || echo 0); unattempted $(tr ',' '\n' <<<"$unattempted" | grep -c . || echo 0)"

if [[ "$KEEP" == false ]]; then
    "$ROOT/tools/overlay_batch.sh" --cleanup --overlay "$OVERLAY" --session "$SESSION" \
        >>"$LOG_FILE" 2>&1 || log "worktree cleanup refused; see $LOG_FILE"
fi

trap - EXIT
release_all
log "done: $OVERLAY"
