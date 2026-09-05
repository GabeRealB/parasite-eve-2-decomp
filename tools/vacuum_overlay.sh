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

log "session $SESSION, cli $CLI, model ${VACUUM_MODEL:-default}, log $LOG_FILE"

prep_args=(--session "$SESSION" --bootstrap 0)
[[ -n "$OVERLAY" ]] && prep_args+=(--overlay "$OVERLAY")
if ! "$ROOT/tools/overlay_batch.sh" "${prep_args[@]}" >>"$LOG_FILE" 2>&1; then
    log "could not lease an overlay (see $LOG_FILE)"
    exit 1
fi
OVERLAY=$(awk '/^leased /{print $2; exit}' "$LOG_FILE" | tr -d ':')
WT="$ROOT/../pe2-ov-$OVERLAY"
log "leased $OVERLAY, worktree $WT"

# Carry give-up archives out of the worktree and remove it. tools/giveups/ is
# gitignored, so no landing path moves it, and it holds the best compiling C for
# every function that stalled - the whole point being that a later retry does
# not restart from m2c.
# Copy give-up archives to trunk. Idempotent, and called as soon as the inner
# vacuum returns rather than only from cleanup_worktree, because everything
# between those two points can fail: acropolis_bridge died on a syntax error
# after 70 matches, never reached cleanup, and its archives were then lost with
# the worktree - 95 attempts at 98.783% and 32 at 99.406% thrown away, which is
# exactly what the archive exists to prevent.
migrate_giveups() {
    [[ -d "$WT/tools/giveups" ]] || return 0
    local carried=0 d name
    for d in "$WT"/tools/giveups/*/; do
        [[ -d "$d" ]] || continue
        name=$(basename "$d")
        if [[ ! -d "$ROOT/tools/giveups/$name" ]]; then
            mkdir -p "$ROOT/tools/giveups"
            cp -r "$d" "$ROOT/tools/giveups/$name" && carried=$((carried+1))
        fi
    done
    [[ $carried -gt 0 ]] && log "carried $carried give-up archive(s) to trunk"
    return 0
}

cleanup_worktree() {
    migrate_giveups
    if [[ "$KEEP" == false ]]; then
        "$ROOT/tools/overlay_batch.sh" --cleanup --overlay "$OVERLAY" --session "$SESSION" \
            >>"$LOG_FILE" 2>&1 || log "worktree cleanup refused; see $LOG_FILE"
    fi
}


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

# Bind the lease to *this* process. overlay_batch.sh claims as a preparer that
# exits immediately, so the lease is guarded only by its expiry - and a sweep
# outlives it: mist_parking ran 8.5 hours against a 240-minute lease, was swept
# mid-run, and its 58 functions became claimable by anything else while it was
# still matching them. This driver lives exactly as long as the work does, so
# adopting makes the lease do the same.
orch adopt-overlay --session "$SESSION" --pid $$ >>"$LOG_FILE" 2>&1 \
    || log "could not adopt the lease; it will expire on the clock instead"

# And refresh it while the sweep runs, so a run longer than --lease-minutes is
# not swept out from under itself.
(
  while kill -0 $$ 2>/dev/null; do
    sleep 1800
    orch adopt-overlay --session "$SESSION" --pid $$ >/dev/null 2>&1 || true
  done
) &
LEASE_REFRESHER=$!
trap 'kill $LEASE_REFRESHER 2>/dev/null || true; release_all' EXIT

# --- match, one function at a time -------------------------------------------
# vacuum.sh picks the easiest remaining function in the overlay each iteration
# (tools/score_functions.py), so the ordering by difficulty is already its job.
# How many of the claimed functions the sweep will actually attempt. Not the
# claimed count: a body already matched in another overlay is skipped rather
# than re-matched, and that is a large share of the tail - mist_r18 stopped with
# 4 of 17 untouched, mist_parking is heading for 12 of 58. Counting them in the
# denominator would make every sweep look like it stalled short of the end.
VACUUM_TOTAL=$(python3 - "$WT" "$OVERLAY" <<'PYEOF' 2>/dev/null || echo ""
import re, subprocess, sys, pathlib
wt, ov = sys.argv[1], sys.argv[2]
inc = []
for c in pathlib.Path(wt, "src").rglob(f"{ov}/*.c"):
    inc += re.findall(r'INCLUDE_ASM\("[^"]+",\s*(\w+)\)', c.read_text(errors="replace"))
try:
    solved = set(subprocess.run(
        ["python3", "tools/overlay_dup_index.py", "solved"],
        cwd=wt, capture_output=True, text=True, timeout=900).stdout.split())
except Exception:
    solved = set()
print(len([f for f in inc if f not in solved]))
PYEOF
)
export VACUUM_TOTAL
log "${VACUUM_TOTAL:-?} function(s) to attempt (of $CLAIMED_N claimed; the rest are duplicates already matched elsewhere)"

inner=(./tools/vacuum.sh --cli "$CLI" --overlay "$OVERLAY")
[[ -n "$TIMES" ]] && inner+=(--times "$TIMES")
[[ "$DRY_RUN" == true ]] && inner+=(--dry-run)

log "running: ${inner[*]}  (in $WT)"
BASE=$(git -C "$WT" rev-parse HEAD)
BRANCH_NAME=$(git -C "$WT" rev-parse --abbrev-ref HEAD)
# No `| tee` here: the inner vacuum appends to this same file itself (via
# VACUUM_LOG_FILE), so the log stays live during a match instead of filling in
# one burst when the function finishes. Piping as well would duplicate it.
(cd "$WT" && VACUUM_LOG_FILE="$LOG_FILE" "${inner[@]}") 2>&1
log "inner vacuum finished"

# Before anything that can fail. The landing, the rebase and the cleanup are
# all downstream of here, and a give-up archive is worth more than any of them:
# it is the only copy of the best compiling C for a function that stalled.
migrate_giveups

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

# --- has trunk moved under us? ----------------------------------------------
# Same rule tools/vacuum.sh uses to decide between a fast port and a port agent:
# copying files is only safe when trunk has not touched them since this worktree
# was cut. Copying blindly is what reverted mist_r18's promotion span - the
# gallery's worktree predated it, and its overlays.toml went over the top.
#
# Three shared files are excepted because every parallel sweep touches them and
# a deterministic merge exists: the learnings file merges by section, the
# difficult list by function name, the manifest by overlay entry. Anything else
# that drifted needs judgement, so it gets an agent rather than a heuristic.
# The learnings file and the difficult list merge unconditionally: appending
# sections and unioning lines cannot lose either side's work. The manifest is
# different - entry-wise merging is only correct when each entry has at most one
# modifier - so it is exempt only when the two sides changed *disjoint* overlay
# entries. Both editing the same entry is a real case (a promotion rewrites the
# entry of every overlay carrying the body), and there it goes to the agent.
MERGEABLE="DECOMPILATION_LEARNINGS.md tools/difficult_functions"
manifest_is_mergeable() {
    local f=configs/USA/overlays.toml
    git -C "$WT" diff --quiet "$BASE" -- "$f" 2>/dev/null && return 0   # we did not touch it
    MANIFEST_CONFLICT=$(python3 - "$WT" "$BASE" "$ROOT" <<'PYEOF' 2>/dev/null
import re, subprocess, sys
wt, base, root = sys.argv[1], sys.argv[2], sys.argv[3]
f = "configs/USA/overlays.toml"
def show(repo, rev):
    return subprocess.run(["git","-C",repo,"show",f"{rev}:{f}"],
                          capture_output=True, text=True).stdout
E = lambda t: {m.group(1): m.group(0) for m in re.finditer(r"^(\w+) = \{.*$", t, re.M)}
b = E(show(root, base))
mine = {k for k, v in E(open(f"{wt}/{f}").read()).items() if b.get(k) != v}
theirs = {k for k, v in E(open(f"{root}/{f}").read()).items() if b.get(k) != v}
print(" ".join(sorted(mine & theirs)))
PYEOF
)
    [[ -z "$MANIFEST_CONFLICT" ]]
}
if manifest_is_mergeable; then
    MERGEABLE="$MERGEABLE configs/USA/overlays.toml"
else
    log "manifest: both sides changed ${MANIFEST_CONFLICT} - not auto-mergeable"
fi
DRIFTED=""
while read -r f; do
    [[ -n "$f" ]] || continue
    case " $MERGEABLE " in *" $f "*) continue ;; esac
    if ! git diff --quiet "$BASE" -- "$f" 2>/dev/null; then
        DRIFTED="$DRIFTED $f"
    fi
done < <(git -C "$WT" diff --name-only --diff-filter=ACDMR "$BASE")

if [[ -n "$DRIFTED" ]]; then
    log "trunk diverged in:$DRIFTED"
    log "handing the landing to an agent"
    port_prompt="Land a finished overlay sweep onto the trunk checkout at $ROOT.

The work is committed on branch \`$BRANCH_NAME\` in the worktree \`$WT\`, as one
\`matched <function> <attempts>\` commit per function. It is verified there.

**Trunk has changed since that worktree was cut** ($BASE), in exactly these files:
$DRIFTED

That is why this is not a file copy. Reconcile them - take trunk's changes and the
worktree's, do not discard either side. Landing by overwriting is what reverted
another overlay's manifest span earlier today and broke the build.

Also carry across, merging rather than replacing:
  - DECOMPILATION_LEARNINGS.md   (append the worktree's new '## ' sections)
  - tools/difficult_functions    (union by function name)
  - configs/USA/overlays.toml    (only the entries the worktree changed)

Then:
  1. If configs/USA/overlays.toml changed, re-split: venv/bin/python3 ninja_config.py
  2. ./tools/build-and-verify.sh  - require the '✅ BUILD SUCCEEDED' line, and check
     the exit status explicitly. Never pipe it into tail or head under set -e.
  3. Preserve one commit per function with its original attempt count from the
     branch; that number is training data for fit_difficulty_model.py.
  4. If you cannot land it safely, change nothing on trunk and say so.

Do not modify the worktree. Do not touch any overlay other than $OVERLAY."

    if command -v claude >/dev/null 2>&1; then
        # Landing is mechanical next to matching, so it can run on a cheaper
        # model; VACUUM_LAND_MODEL selects it, falling back to the session's.
        land_model="${VACUUM_LAND_MODEL-${VACUUM_MODEL:-}}"
        log "drift landing agent, model ${land_model:-default}"
        claude -p ${land_model:+--model "$land_model"} \
            --verbose --output-format stream-json --dangerously-skip-permissions \
            "$port_prompt" >>"$LOG_FILE" 2>&1
        rc=$?
        log "port agent finished (rc=$rc)"

        # Do NOT re-verify here. The agent already ran an unscoped build while
        # holding the merge lock, and re-running it afterwards is outside the
        # lock: another session that commits in that window makes trunk look
        # broken when the landing was fine. acropolis_promenade landed all 12
        # commits and verified 449/449, then this build failed on
        # `cannot find build/USA/src/rooms/lib/room_script05.c.o` - a shared
        # unit a concurrent grok session had just added. rc=1, worktree kept,
        # nothing actually wrong. The queue verifies trunk after every room
        # anyway, so the second check bought nothing and cost a false alarm.
        if [[ $rc -ne 0 ]]; then
            log "landing agent failed; worktree kept at $WT"
            exit 1
        fi
        if git -C "$WT" log --oneline "$BASE..HEAD" --format=%s \
             | grep -qvxFf <(git -C "$ROOT" log --oneline "$BASE..HEAD" --format=%s); then
            log "agent reported success but some commits are missing on trunk; worktree kept at $WT"
            exit 1
        fi
        log "agent landing complete; every branch commit is on trunk"
        cleanup_worktree
        trap - EXIT; release_all; exit 0
    fi
    log "no claude CLI available; leaving the worktree at $WT for a manual landing"
    exit 1
fi

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

log "landed ${#MATCHED[@]}; difficult $(grep -c . <<<"$DIFFICULT" || echo 0); unattempted $(tr ',' '\n' <<<"$unattempted" | grep -c . || echo 0)"

cleanup_worktree

trap - EXIT
release_all
log "done: $OVERLAY"
