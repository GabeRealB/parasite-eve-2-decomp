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
(cd "$WT" && "${inner[@]}") 2>&1 | tee -a "$LOG_FILE"
log "inner vacuum finished"

# --- collect what it actually matched -----------------------------------------
mapfile -t MATCHED < <(git -C "$WT" log --format=%s "$BASE"..HEAD \
                       | awk '/^matched /{print $2}' | tac)
log "matched ${#MATCHED[@]} function(s)"
printf '  %s\n' "${MATCHED[@]}" | tee -a "$LOG_FILE"

if [[ "$DRY_RUN" == true || "$NO_LAND" == true || ${#MATCHED[@]} -eq 0 ]]; then
    log "not landing (dry-run, --no-land, or nothing matched); worktree kept at $WT"
    trap - EXIT
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

funcs_file=$(mktemp)
printf '%s\n' "${MATCHED[@]}" >"$funcs_file"
land_args=("$WT" "$OVERLAY" --functions-file "$funcs_file")
if [[ ${#EXTRAS[@]} -gt 0 ]]; then
    land_args+=(--extra "$(IFS=,; echo "${EXTRAS[*]}")")
fi

if ! python3 "$ROOT/tools/land_overlay.py" "${land_args[@]}" >>"$LOG_FILE" 2>&1; then
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

log "landed ${#MATCHED[@]}; difficult $(grep -c . <<<"$DIFFICULT" || echo 0); unattempted $(tr ',' '\n' <<<"$unattempted" | grep -c . || echo 0)"

if [[ "$KEEP" == false ]]; then
    "$ROOT/tools/overlay_batch.sh" --cleanup --overlay "$OVERLAY" --session "$SESSION" \
        >>"$LOG_FILE" 2>&1 || log "worktree cleanup refused; see $LOG_FILE"
fi

trap - EXIT
release_all
log "done: $OVERLAY"
