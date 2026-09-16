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
#   tools/vacuum_overlay.sh [--overlay NAME] [--profile NAME] [--times N]
#                           [--cli claude|grok|codex] [--keep] [--dry-run]
#                           [--no-land] [--max-difficulty 0..1]
#
# --max-difficulty bounds score_functions.py's P(does not match first try), so
# the sweep takes an overlay's easy work and stops rather than grinding into its
# hard tail. Pair it with a list sweep to skim many overlays cheaply; the
# remainder is still there for a later unbounded pass.
#
# --profile takes the same named cli/model/effort sets as tools/vacuum.sh, from
# the same table; --list-profiles prints it. The profile is applied here and
# exported, so the inner vacuum.sh run inside the worktree inherits it rather
# than resolving a default of its own.
#
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

OVERLAY=""
PRE_CLAIMED=false
SESSION_ARG=""
CLI="${VACUUM_CLI:-claude}"
CLI_EXPLICIT=0
PROFILE="${VACUUM_PROFILE:-}"
TIMES=""
# Upper bound on score_functions.py's difficulty, handed to the inner vacuum.
# Empty means no bound.
MAX_DIFFICULTY=""
KEEP=false
DRY_RUN=false
NO_LAND=false

# Profiles and the log directory, shared with tools/vacuum.sh.
# shellcheck source=tools/vacuum_profile.sh
. "$ROOT/tools/vacuum_profile.sh"

usage() { sed -n '2,24p' "$0" | sed 's/^# \{0,1\}//'; exit 1; }

while [[ $# -gt 0 ]]; do
    case $1 in
        --overlay) OVERLAY="$2"; shift 2 ;;
        # The list driver claimed and filtered this overlay already; adopt it.
        --pre-claimed) PRE_CLAIMED=true; shift ;;
        # Adopt the list driver's claim: same session name, and adopt-overlay
        # below re-points it at this pid so the lease follows the worker.
        --session)     SESSION_ARG="$2"; shift 2 ;;
        --cli)     CLI="$2"; CLI_EXPLICIT=1; shift 2 ;;
        --claude)  CLI=claude; CLI_EXPLICIT=1; shift ;;
        --grok)    CLI=grok; CLI_EXPLICIT=1; shift ;;
        --codex)   CLI=codex; CLI_EXPLICIT=1; shift ;;
        --profile) PROFILE="$2"; shift 2 ;;
        --profiles) PROFILES_FILE="$2"; shift 2 ;;
        --list-profiles) list_profiles; exit 0 ;;
        --times)   TIMES="$2"; shift 2 ;;
        --max-difficulty) MAX_DIFFICULTY="$2"; shift 2 ;;
        --keep)    KEEP=true; shift ;;
        --dry-run) DRY_RUN=true; shift ;;
        --no-land) NO_LAND=true; shift ;;
        -h|--help) usage ;;
        *) echo "unknown argument: $1" >&2; usage ;;
    esac
done

# A profile fills in cli/model/effort; with no --profile and no CLI flag the
# `default` row does, when the table has one. Same rule as tools/vacuum.sh.
if [[ -n "$PROFILE" ]]; then
    apply_profile "$PROFILE"
elif [[ $CLI_EXPLICIT -eq 0 ]] && profile_row default >/dev/null 2>&1; then
    apply_profile default
fi

orch() { python3 "$ROOT/tools/vacuum_orch.py" --root "$ROOT" "$@"; }

# --- lease + worktree ---------------------------------------------------------
# A shared unit is named "<family>/lib/<unit>"; the slashes cannot go into a
# filename, so every path derived from the overlay name is flattened.
OVERLAY_SLUG="${OVERLAY:-auto}"; OVERLAY_SLUG="${OVERLAY_SLUG//\//-}"
SESSION="${SESSION_ARG:-ovb-${OVERLAY_SLUG}-$$}"
LOG_FILE="$(vacuum_log_dir)/vacuum-overlay-${OVERLAY_SLUG}-$$.log"
: >"$LOG_FILE"
log() { echo "[$(date '+%H:%M:%S')] $*" | tee -a "$LOG_FILE"; }

log "session $SESSION, cli $CLI, model ${VACUUM_MODEL:-default}${PROFILE:+, profile $PROFILE}, log $LOG_FILE"

prep_args=(--session "$SESSION" --bootstrap 0)
[[ "$PRE_CLAIMED" == true ]] && prep_args+=(--pre-claimed)
[[ -n "$OVERLAY" ]] && prep_args+=(--overlay "$OVERLAY")
if ! "$ROOT/tools/overlay_batch.sh" "${prep_args[@]}" >>"$LOG_FILE" 2>&1; then
    log "could not lease an overlay (see $LOG_FILE)"
    exit 1
fi
OVERLAY=$(awk '/^leased /{print $2; exit}' "$LOG_FILE" | tr -d ':')
WT="$ROOT/../pe2-ov-$OVERLAY_SLUG"
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
#
# An archive trunk already has is updated, not skipped. The worktree's copy was
# seeded from trunk's (seed_giveups) and archive_giveup.py only ever adds
# sessions to it, so it is the newer superset. Skipping it is how
# func_mine_gorge_8017D5F8's second give-up (97.4%, 9 attempts) was lost: the
# first run had already created the directory on trunk.
migrate_giveups() {
    [[ -d "$WT/tools/giveups" ]] || return 0
    local carried=0 d name dst
    for d in "$WT"/tools/giveups/*/; do
        [[ -d "$d" ]] || continue
        name=$(basename "$d")
        dst="$ROOT/tools/giveups/$name"
        cmp -s "$d/meta.json" "$dst/meta.json" && continue
        mkdir -p "$dst"
        cp -au "$d/." "$dst/" && carried=$((carried+1))
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

# Give the worktree trunk's give-up archives for what it holds. tools/claude
# restores prior seeds from its own tree's tools/giveups, which is gitignored and
# so absent from a fresh worktree: without this every retry of a function under
# an overlay sweep restarts from m2c, and the archive it writes has none of the
# earlier sessions in it. vacuum.sh does the same for its per-function worktrees.
seed_giveups() {
    local fn seeded=0
    while read -r fn; do
        [[ -n "$fn" && -d "$ROOT/tools/giveups/$fn" ]] || continue
        [[ -d "$WT/tools/giveups/$fn" ]] && continue
        mkdir -p "$WT/tools/giveups"
        cp -a "$ROOT/tools/giveups/$fn" "$WT/tools/giveups/" && seeded=$((seeded+1))
    done <<<"$CLAIMED"
    [[ $seeded -gt 0 ]] && log "seeded $seeded give-up archive(s) from trunk"
    return 0
}
seed_giveups

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
# A difficulty bound belongs in this denominator too. Without it the log counts
# every attemptable function while the sweep will only ever pick those under the
# bound, so "[1 of 49]" describes work it is not going to do and disagrees with
# what the monitor reports.
VACUUM_TOTAL=$(python3 - "$WT" "$OVERLAY" "$MAX_DIFFICULTY" <<'PYEOF' 2>/dev/null || echo ""
import os, re, subprocess, sys, pathlib
wt, ov = sys.argv[1], sys.argv[2]
bound = sys.argv[3] if len(sys.argv) > 3 else ""
inc = []
src = pathlib.Path(wt, "src")
# "<family>/lib/<unit>" is one .c file; every other overlay is a directory of them.
if "/lib/" in ov:
    fam, unit = ov.split("/lib/", 1)
    files = [src / fam / "lib" / f"{unit}.c"]
else:
    files = list(src.rglob(f"{ov}/*.c"))
for c in files:
    if not c.is_file():
        continue
    inc += re.findall(r'INCLUDE_ASM\("[^"]+",\s*(\w+)\)', c.read_text(errors="replace"))
try:
    solved = set(subprocess.run(
        ["python3", "tools/overlay_dup_index.py", "solved"],
        cwd=wt, capture_output=True, text=True, timeout=900).stdout.split())
except Exception:
    solved = set()
owner_list = os.environ.get("VACUUM_OWNER_LIST", "")
if owner_list and os.path.isfile(owner_list):
    try:
        solved |= set(subprocess.run(
            ["python3", "tools/overlay_dup_index.py", "ceded", "--list", owner_list],
            cwd=wt, capture_output=True, text=True, timeout=900).stdout.split())
    except Exception:
        pass
keep = [f for f in inc if f not in solved]
if bound and keep:
    base = pathlib.Path(wt, "asm", "USA")
    if "/lib/" in ov:
        fam, unit = ov.split("/lib/", 1)
        d = base / fam / "nonmatchings" / "lib" / unit
    else:
        hits = list(base.glob(f"*/nonmatchings/{ov}"))
        d = hits[0] if hits else None
    if d and d.is_dir():
        # --scores, not --max-score: a bounded run exits non-zero both when
        # nothing meets the bound and when it failed, and those need opposite
        # handling. Unfiltered, non-zero means only "could not score".
        r = subprocess.run(["python3", "tools/score_functions.py",
                            "--scores", str(d)],
                           cwd=wt, capture_output=True, text=True, timeout=900)
        if r.returncode == 0 and r.stdout.strip():
            lim = float(bound)
            ok = {ln.split("\t")[1] for ln in r.stdout.splitlines()
                  if "\t" in ln and float(ln.split("\t")[0]) <= lim}
            keep = [f for f in keep if f in ok]
print(len(keep))
PYEOF
)
export VACUUM_TOTAL
log "${VACUUM_TOTAL:-?} function(s) to attempt (of $CLAIMED_N claimed; the rest are duplicates matched${VACUUM_OWNER_LIST:+ or owned} elsewhere)"

# decomp_overlay resolves a nested name relative to the family's asm tree, so
# it knows "lib/<unit>" but not "<family>/lib/<unit>". The lease keeps the
# family qualifier; the inner vacuum gets the tree-relative form.
INNER_OVERLAY="$OVERLAY"
[[ "$OVERLAY" == */lib/* ]] && INNER_OVERLAY="lib/${OVERLAY##*/}"
inner=(./tools/vacuum.sh --cli "$CLI" --overlay "$INNER_OVERLAY")
[[ -n "$TIMES" ]] && inner+=(--times "$TIMES")
[[ -n "$MAX_DIFFICULTY" ]] && inner+=(--max-difficulty "$MAX_DIFFICULTY")
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
mapfile -t ALL_MATCHED < <(git -C "$WT" log --format=%s "$BASE"..HEAD \
                           | awk '/^matched /{print $2}' | tac)
# land_overlay.py --overlay X maps bodies by walking trunk's src/**/X directory,
# so only a function whose body lives in THIS overlay can be landed that way.
# Two kinds of match end up elsewhere and make it refuse the whole batch:
#   * a promoted shared body, which moves to src/<family>/lib/<unit>.c;
#   * a sibling overlay's copy matched in passing - an actor_503500 sweep
#     matched func_dryfield_dilapidated_house_80181290 like this.
# Both still reach trunk through EXTRAS; they just must not be in the
# per-function list. (The replay path below is unaffected: it cherry-picks
# commits, so it carries them correctly either way.)
# A shared unit "<family>/lib/<unit>" is a file inside src/<family>/lib, not a
# directory of its own; everything else is src/<family>/<overlay>.
if [[ "$OVERLAY" == */lib/* ]]; then
    WT_SRC="src/${OVERLAY%/*}"
else
    WT_SRC=$(cd "$WT" && ls -d src/*/"$OVERLAY" 2>/dev/null | head -1)
fi
MATCHED=()
for _fn in "${ALL_MATCHED[@]}"; do
    if [[ -n "$WT_SRC" ]] && grep -qlE "^[A-Za-z_][A-Za-z0-9_ *]*\b${_fn}[[:space:]]*\(" \
         "$WT/$WT_SRC"/*.c 2>/dev/null; then
        MATCHED+=("$_fn")
    else
        log "  not landing $_fn here: body is not in $WT_SRC (promoted, or another overlay's)"
    fi
done
log "matched ${#ALL_MATCHED[@]} function(s), ${#MATCHED[@]} landable in $OVERLAY"
printf '  %s\n' "${MATCHED[@]}" | tee -a "$LOG_FILE"

carry_bookkeeping() {
    local files=() f added subject hard
    for f in tools/difficult_functions DECOMPILATION_LEARNINGS.md; do
        git -C "$WT" diff --quiet "$BASE" HEAD -- "$f" || files+=("$f")
    done
    [[ ${#files[@]} -gt 0 ]] || return 0
    if ! orch merge-acquire --session "$SESSION" --pid $$ --wait "${VACUUM_MERGE_WAIT:-3600}" \
         >>"$LOG_FILE" 2>&1; then
        log "could not take the merge lock; give-up bookkeeping for $OVERLAY is lost"
        return 0
    fi
    if python3 - "$WT" "$ROOT" "$BASE" "${files[@]}" <<'PYEOF' >>"$LOG_FILE" 2>&1
import subprocess, sys
from pathlib import Path
wt, root, base = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3]
sys.path.insert(0, str(root / "tools"))
from land_overlay import merge_sections
for f in sys.argv[4:]:
    src, dst = wt / f, root / f
    if f.endswith("difficult_functions"):
        # Same union land_overlay.py applies: keyed by name, latest line wins.
        have = {l.split()[0]: l for l in dst.read_text().splitlines() if l.strip()}
        for l in src.read_text().splitlines():
            if l.strip():
                have[l.split()[0]] = l
        dst.write_text("\n".join(have[k] for k in sorted(have)) + "\n")
    else:
        r = subprocess.run(["git", "-C", str(root), "show", f"{base}:{f}"],
                           capture_output=True, text=True)
        dst.write_text(merge_sections(dst.read_text(), src.read_text(),
                                      r.stdout if r.returncode == 0 else ""))
PYEOF
    then
        added=$(git -C "$WT" diff "$BASE" HEAD -- tools/difficult_functions \
                | awk '/^\+[^+]/{sub(/^\+/,""); print}' | paste -sd, | sed 's/,/, /g')
        subject="${added:+difficult: $added}"
        subject="${subject:-learnings: carried from $OVERLAY}"
        if git -C "$ROOT" diff --quiet -- "${files[@]}"; then
            log "give-up bookkeeping already on trunk"
        elif git -C "$ROOT" commit -q -m "$subject" \
                 -m "Carried from $OVERLAY's sweep, which matched nothing." \
                 -- "${files[@]}" >>"$LOG_FILE" 2>&1; then
            log "carried to trunk: $subject"
        else
            log "could not commit give-up bookkeeping for $OVERLAY; see $LOG_FILE"
        fi
    else
        log "merging give-up bookkeeping for $OVERLAY failed; see $LOG_FILE"
    fi
    orch merge-release --session "$SESSION" >/dev/null 2>&1 || true
    # Tell the orchestrator too, so the lease release does not return them to
    # the pool: finish-overlay records them, where relinquish would not.
    hard=$(git -C "$WT" diff "$BASE" HEAD -- tools/difficult_functions \
           | awk '/^\+[^+]/{sub(/^\+/,""); print $1}' | sort -u | paste -sd,)
    [[ -n "$hard" ]] && orch finish-overlay --session "$SESSION" --difficult "$hard" \
        >>"$LOG_FILE" 2>&1
    return 0
}

# Only "nothing matched at all" is a no-op. A batch whose every body was
# promoted to src/<family>/lib has MATCHED empty but ALL_MATCHED full, and this
# branch used to discard it: worktree deleted, branch deleted, verified work
# gone. Thirteen matches went that way in one day. Those commits land perfectly
# well through the replay below - cherry-picking needs no per-function mapping -
# so fall through instead of dropping them.
if [[ "$DRY_RUN" == true || "$NO_LAND" == true || ${#ALL_MATCHED[@]} -eq 0 ]]; then
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
    # A run that matched nothing can still have committed something worth
    # keeping: the give-up line in tools/difficult_functions and any learnings
    # sections. Deleting the branch threw both away, so the function was never
    # marked difficult and the next pass of the list claimed it again -
    # func_mine_gorge_8017D5F8 gave up at 98.8% and 97.4% and was started a
    # third time. Merge those two files onto trunk the same way a landing does;
    # nothing else a failed run changed is carried.
    carry_bookkeeping
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
                      | grep -v "^src/.*/${OVERLAY%/*}/" || true)
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
  - DECOMPILATION_LEARNINGS.md   (the worktree's new '## ' sections AND its edits
                                  inside existing sections - diff it against $BASE)
  - tools/difficult_functions    (union by function name)
  - configs/USA/overlays.toml    (only the entries the worktree changed)

Then:
  1. If configs/USA/overlays.toml changed, re-split: venv/bin/python3 ninja_config.py
  2. ./tools/build-and-verify.sh  - require the '✅ BUILD SUCCEEDED' line, and check
     the exit status explicitly. Never pipe it into tail or head under set -e.
     Run it, and everything else, in the foreground and wait for it. This is a
     one-shot session: it ends when you stop, and that kills any background job
     - actor_521100's landing died mid-merge that way, build killed, no commit.
  3. Preserve one commit per function with its original attempt count from the
     branch; that number is training data for fit_difficulty_model.py.
  4. If you cannot land it safely, change nothing on trunk and say so.

Do not modify the worktree. Do not touch any overlay other than $OVERLAY."

    # This arm is written against claude's flags, so a land profile on another
    # api cannot be honoured here; say so rather than silently ignoring it.
    if [[ -n "${VACUUM_LAND_API:-}" && "$VACUUM_LAND_API" != "claude" ]]; then
        log "warning: land profile uses api '$VACUUM_LAND_API', but the drift landing agent only speaks claude; using claude"
    fi
    land_cmd=(claude)
    if [[ -n "${VACUUM_LAND_LAUNCH:-}" ]]; then
        read -ra land_cmd <<<"$VACUUM_LAND_LAUNCH"
    fi
    if command -v "${land_cmd[0]}" >/dev/null 2>&1; then
        # Landing is mechanical next to matching, so it can run on a cheaper
        # model; the land profile selects it, falling back to the session's.
        # A launch wrapper names the model itself, so do not pass it twice.
        land_model="${VACUUM_LAND_MODEL-${VACUUM_MODEL:-}}"
        [[ -n "${VACUUM_LAND_LAUNCH:-}" ]] && land_model=""
        log "drift landing agent, model ${land_model:-default}"
        # Cap it. The agent holds the global merge lock for its whole run, so
        # one that stops converging stalls every other lane's landing behind it:
        # actor_105700's held the lock 51 minutes with a queue behind it. The
        # cap is generous because a legitimate 20-file reconcile is slow; it
        # exists to bound a hang, not to hurry a working agent.
        timeout --signal=TERM --kill-after=60 "${VACUUM_LAND_TIMEOUT:-5400}" \
            "${land_cmd[@]}" -p ${land_model:+--model "$land_model"} \
            --verbose --output-format stream-json --dangerously-skip-permissions \
            "$port_prompt" >>"$LOG_FILE" 2>&1
        rc=$?
        [[ $rc -eq 124 ]] && log "landing agent hit the ${VACUUM_LAND_TIMEOUT:-5400}s cap"
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
        if git -C "$ROOT" rev-parse -q --verify MERGE_HEAD >/dev/null; then
            log "warning: trunk is left mid-merge (MERGE_HEAD set) - the landing agent stopped before committing"
        fi
        if [[ $rc -ne 0 ]]; then
            log "LANDING FAILED: $OVERLAY - landing agent failed; ${#MATCHED[@]} match(es) kept on $BRANCH_NAME (worktree $WT)"
            exit 3
        fi
        # A `matched` commit with no counterpart on trunk is still landed when
        # trunk no longer has the function as unmatched assembly. That happens
        # when trunk promoted the body into src/<family>/lib after this worktree
        # was cut: the agent correctly drops the now-redundant copy, and there is
        # no commit to replay. actor_113100 was reported stranded that way, with
        # func_actor_113100_80132E00 already served by actors_shared_80132390.
        # The agent's unscoped build has just re-split trunk, so asm/ is current.
        # Lines added between two branch revisions (optionally only in the
        # given paths) that the branch still has must all be on trunk.
        lines_on_trunk() {              # $1 from, $2 to, [$3.. pathspec]
            python3 - "$WT" "$ROOT" "$@" <<'PYEOF'
import subprocess, sys
from pathlib import Path
wt, root, a, b, *spec = sys.argv[1:]
def git(*args):
    return subprocess.run(["git", "-C", wt, *args], capture_output=True, text=True).stdout
lost = 0
for f in git("diff", "--name-only", a, b, "--", *spec).split():
    added = {l[1:] for l in git("diff", a, b, "--", f).splitlines()
             if l.startswith("+") and not l.startswith("+++") and l[1:].strip()}
    dst = Path(root) / f
    have = set(dst.read_text().splitlines()) if dst.is_file() else set()
    gone = (added & set(git("show", f"HEAD:{f}").splitlines())) - have
    if gone:
        lost += 1
        print(f"  {f}: {len(gone)} line(s) not on trunk, e.g. {sorted(gone)[0][:90]!r}")
sys.exit(1 if lost else 0)
PYEOF
        }
        content_on_trunk() {            # $1 = subject of a branch commit
            local sha
            sha=$(git -C "$WT" log --format='%H %s' "$BASE..HEAD" \
                  | awk -v s="$1" '{h=$1; sub(/^[^ ]+ /,"")} $0 == s {print h; exit}')
            [[ -n "$sha" ]] && lines_on_trunk "$sha^" "$sha" >/dev/null
        }
        missing=$(git -C "$WT" log --format=%s "$BASE..HEAD" \
                  | grep -vxFf <(git -C "$ROOT" log --format=%s "$BASE..HEAD") || true)
        # Any other commit can reach trunk folded into a content merge rather
        # than replayed - a learnings commit merged into the doc keeps its text
        # but not its subject. actor_521100 reported 7 of those as stranded
        # next to 2 commits that really were. So judge those by content: every
        # line the commit added that the branch still has must be on trunk.
        stranded=""
        while read -r subject; do
            [[ -n "$subject" ]] || continue
            fn=$(awk '$1 == "matched" {print $2}' <<<"$subject")
            if [[ -n "$fn" && -d "$ROOT/asm" ]] \
               && [[ -z "$(find "$ROOT/asm" -path '*/nonmatchings/*' -name "$fn.s" -print -quit)" ]]; then
                log "  $fn has no commit on trunk but is no longer unmatched there (promoted)"
                continue
            fi
            if [[ -z "$fn" ]] && content_on_trunk "$subject"; then
                log "  '$subject' has no commit on trunk but its content is there (merged)"
                continue
            fi
            stranded="$stranded; $subject"
        done <<<"$missing"
        # The docs are checked whole as well, because a `matched` commit carries
        # learnings too and the subject test above passes it by name. The
        # section-append merge lost a paragraph actor_521100's
        # func_actor_521100_80136290 commit added inside an existing section.
        if ! doc_loss=$(lines_on_trunk "$BASE" HEAD '*.md'); then
            log "doc content from the branch is missing on trunk:"
            log "$doc_loss"
            stranded="$stranded; doc edits (above)"
        fi
        if [[ -n "$stranded" ]]; then
            log "LANDING FAILED: $OVERLAY - agent reported success but commits are missing on trunk:${stranded#;}; worktree $WT"
            exit 3
        fi
        log "agent landing complete; every branch commit is on trunk"
        cleanup_worktree
        trap - EXIT; release_all; exit 0
    fi
    log "LANDING FAILED: $OVERLAY - no ${land_cmd[0]:-claude} CLI for the landing agent; worktree $WT"
    exit 3
fi

log "acquiring merge lock"
if ! orch merge-acquire --session "$SESSION" --pid $$ --wait "${VACUUM_MERGE_WAIT:-3600}" \
     >>"$LOG_FILE" 2>&1; then
    # Matches are already verified and committed on the branch by this point,
    # so a lock timeout strands them exactly as a failed landing does. Exit 1
    # here told the driver "no work left" and it moved on without a word -
    # which is how actors/lib/actor_101600_text's 19 matches went unnoticed.
    log "LANDING FAILED: $OVERLAY - could not take the merge lock; matches kept on $BRANCH_NAME (worktree $WT)"
    trap - EXIT
    exit 3
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
    # Ignore the same shared files the drift check excepts. Without this the
    # replay path is dead in practice: DECOMPILATION_LEARNINGS.md and
    # tools/difficult_functions are touched by every sweep and every landing, so
    # the intersection is essentially never empty and CAN_REPLAY never becomes
    # true. actor_503500 lost its replay to exactly that and fell through to the
    # file rewrite, which cannot map bodies across a promotion's renumbered
    # units - 68 verified matches stranded on the branch.
    touched=""
    while read -r f; do
        [[ -n "$f" ]] || continue
        case " $MERGEABLE " in *" $f "*) continue ;; esac
        touched="$touched $f"
    done < <(git -C "$WT" diff --name-only "$BASE"..HEAD)
    if [[ -z "$(git diff --name-only "$BASE"..HEAD -- $touched 2>/dev/null)" ]]; then
        CAN_REPLAY=true
    fi
fi

# Replay the branch, resolving the one conflict class that is not a real
# disagreement. The manifest is one line per overlay and git resolves by hunk,
# so a commit editing actor_400100 conflicts with a trunk that edited
# actor_161500 two lines above it. Aborting there sent the landing to the file
# rewrite, which is the path that cannot survive a promotion's renumbered units
# - three overlays stranded 61 verified matches in one morning that way, and
# every one of those conflicts was this shape. tools/resolve_manifest.py
# decides each entry against the picked commit's own parent and refuses (exit 2)
# only when both sides really changed the same overlay.
replay_branch() {                       # $1 = range, returns 0 if fully replayed
    local range="$1" rc
    git cherry-pick "$range" >>"$LOG_FILE" 2>&1 && return 0
    while :; do
        local unmerged
        unmerged=$(git diff --name-only --diff-filter=U)
        if [[ "$unmerged" != "configs/USA/overlays.toml" ]]; then
            [[ -n "$unmerged" ]] && log "replay conflict outside the manifest: $unmerged"
            break
        fi
        python3 "$ROOT/tools/resolve_manifest.py" --root "$ROOT" >>"$LOG_FILE" 2>&1
        rc=$?
        if [[ $rc -ne 0 ]]; then
            log "manifest conflict needs a human (resolve_manifest rc=$rc)"
            break
        fi
        git add configs/USA/overlays.toml
        if GIT_EDITOR=true git cherry-pick --continue >>"$LOG_FILE" 2>&1; then
            return 0
        fi
    done
    git cherry-pick --abort >/dev/null 2>&1 || true
    return 1
}

if [[ "$CAN_REPLAY" == true ]]; then
    log "replaying $(git rev-list --count "$BASE".."$BRANCH_NAME") commit(s) from $BRANCH_NAME"
    if replay_branch "$BASE".."$BRANCH_NAME"; then
        log "replayed cleanly"
    else
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

# An all-promoted batch has no per-function mapping to make, so the file
# rewrite has nothing to work with; replay is the only route. Say so plainly
# rather than letting land_overlay.py fail on an empty function list.
if [[ ${#MATCHED[@]} -eq 0 && "$CAN_REPLAY" != true ]]; then
    log "every match was promoted out of $OVERLAY and trunk has moved; \
replay is the only way to land them"
    log "LANDING FAILED: $OVERLAY - ${#ALL_MATCHED[@]} promoted match(es) are \
stranded on $BRANCH_NAME (worktree $WT)"
    rm -f "$funcs_file"; exit 3
fi

if [[ "$CAN_REPLAY" != true ]] \
   && ! python3 "$ROOT/tools/land_overlay.py" "${land_args[@]}" >>"$LOG_FILE" 2>&1; then
    # Last resort: replay the branch's own commits. Cherry-picking needs no
    # per-function correspondence with trunk, so it survives the renumbering a
    # promotion causes, and it keeps each commit's attempt count. If the picks
    # conflict it aborts and changes nothing.
    log "file rewrite failed; trying to replay $BRANCH_NAME"
    if replay_branch "$BASE".."$BRANCH_NAME"; then
        log "replayed cleanly after the rewrite failed"
    else
        git cherry-pick --abort >/dev/null 2>&1 || true
        # Exit 3, not 1: a stranded batch is not the same as a refused lease,
        # and the driver used to log both as "no work left" - which is how 68
        # matches sat unnoticed for hours.
        log "LANDING FAILED: $OVERLAY - ${#MATCHED[@]} verified match(es) are stranded on $BRANCH_NAME (worktree $WT)"
        rm -f "$funcs_file"; exit 3
    fi
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
            | grep -F "_${OVERLAY##*/}_" || true)
# Every function in ALL_MATCHED was matched and reached trunk - the promoted
# ones through EXTRAS and the replay rather than the per-function mapping.
# Reporting only MATCHED told the orchestrator a promoted body was never
# attempted, which is how it came to be offered again on a later sweep.
matched_csv=$(IFS=,; echo "${ALL_MATCHED[*]}")
diff_csv=$(tr '\n' ',' <<<"$DIFFICULT" | sed 's/,$//')
unattempted=$(comm -23 <(sort <<<"$CLAIMED") \
                       <(printf '%s\n%s\n' "$matched_csv" "$diff_csv" | tr ',' '\n' | sort -u) \
              | tr '\n' ',' | sed 's/,$//')

orch finish-overlay --session "$SESSION" \
    --matched "$matched_csv" \
    ${diff_csv:+--difficult "$diff_csv"} \
    ${unattempted:+--unattempted "$unattempted"} >>"$LOG_FILE" 2>&1 || true

_promoted=$(( ${#ALL_MATCHED[@]} - ${#MATCHED[@]} ))
log "landed ${#ALL_MATCHED[@]}$( ((_promoted > 0)) && echo " (${_promoted} promoted out of $OVERLAY)"); difficult $(grep -c . <<<"$DIFFICULT" || echo 0); unattempted $(tr ',' '\n' <<<"$unattempted" | grep -c . || echo 0)"

cleanup_worktree

trap - EXIT
release_all
log "done: $OVERLAY"
