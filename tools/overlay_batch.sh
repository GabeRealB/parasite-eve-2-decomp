#!/usr/bin/env bash
#
# Prepare a whole-overlay matching job: lease the overlay, build an isolated
# worktree for it, and write the brief an agent needs.
#
# This is deliberately a *preparer*, not a loop. Spawning the agent, retrying a
# failed landing, falling back to the permuter, marking give-ups difficult and
# auto-committing when an agent forgets are all things tools/vacuum.sh already
# solves after a lot of iteration; reimplementing them here would produce a
# second, worse copy of that loop. When this shape has proven itself, it belongs
# in vacuum.sh as an --overlay-batch mode rather than as a parallel driver.
#
# Why a whole overlay is the right unit:
#   Overlays holding 20+ functions carry 60% of the remaining work, and their
#   functions are the *shortest* in the tree (median 31 instructions against 118
#   in one- and two-function overlays). They resist the mechanical path not
#   because they are complex but because they share one untyped state struct.
#   Working that struct out is a cost paid once and spent across the whole
#   overlay: agents doing exactly this landed 155 of 156 attempted, no reverts.
#
# Usage:
#   tools/overlay_batch.sh [--overlay NAME] [--session S] [--keep]
#   tools/overlay_batch.sh --release --session S
#   tools/overlay_batch.sh --cleanup --overlay NAME [--session S]
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

OVERLAY=""
SESSION=""
RELEASE=false
KEEP=false
CLEANUP=false

usage() {
    sed -n '2,25p' "$0" | sed 's/^# \{0,1\}//'
    exit 1
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --overlay) OVERLAY="$2"; shift 2 ;;
        --session) SESSION="$2"; shift 2 ;;
        --release) RELEASE=true; shift ;;
        --cleanup) CLEANUP=true; shift ;;
        --keep)    KEEP=true; shift ;;
        -h|--help) usage ;;
        *) echo "unknown argument: $1" >&2; usage ;;
    esac
done

orch() { python3 "$ROOT/tools/vacuum_orch.py" --root "$ROOT" "$@"; }

if [[ "$RELEASE" == true ]]; then
    [[ -n "$SESSION" ]] || { echo "--release needs --session" >&2; exit 1; }
    orch relinquish-overlay --session "$SESSION"
    exit $?
fi

# --- cleanup -----------------------------------------------------------------
# Landing rewrites trunk's files instead of merging this branch, so afterwards
# the branch still points at the commit it was cut from and every decompiled
# file still reads as modified in the worktree. That is indistinguishable from
# unlanded work, and re-applying it would land the same functions twice. Delete
# the worktree once its functions are demonstrably on main.
if [[ "$CLEANUP" == true ]]; then
    [[ -n "$OVERLAY" ]] || { echo "--cleanup needs --overlay" >&2; exit 1; }
    WT="$ROOT/../pe2-ov-$OVERLAY"
    BRANCH="overlay/$OVERLAY"

    if [[ ! -d "$WT" ]]; then
        echo "no worktree at $WT; nothing to clean"
    else
        # Refuse to discard anything trunk does not already have. Every function
        # the worktree decompiled must have a body on main and no INCLUDE_ASM.
        unlanded=()
        while read -r fn; do
            [[ -n "$fn" ]] || continue
            if grep -rq "INCLUDE_ASM(.*\b${fn})" "$ROOT/src" 2>/dev/null \
               || ! grep -rqE "[ *]${fn}\(" "$ROOT/src" 2>/dev/null; then
                unlanded+=("$fn")
            fi
        done < <(git -C "$WT" diff -- src \
                 | sed -nE 's/^-.*INCLUDE_ASM\([^,]*, *([A-Za-z0-9_]+)\).*/\1/p' | sort -u)

        if [[ ${#unlanded[@]} -gt 0 ]]; then
            echo "refusing to delete $WT: these are not on main yet:" >&2
            printf '  %s\n' "${unlanded[@]}" >&2
            echo "land them first, or pass --keep to leave the worktree alone" >&2
            exit 1
        fi

        git worktree remove --force "$WT"
        echo "removed worktree $WT"
    fi

    git branch -D "$BRANCH" >/dev/null 2>&1 && echo "deleted branch $BRANCH" || true
    if [[ -n "$SESSION" ]]; then
        orch relinquish-overlay --session "$SESSION" >/dev/null 2>&1 || true
    fi
    exit 0
fi

SESSION="${SESSION:-overlay-batch-$$}"

# --- lease -------------------------------------------------------------------
# The lease is one claim per function, so the vacuum sessions already running -
# which know nothing about overlays - skip these when they pick work.
claim_args=(claim-overlay --session "$SESSION" --pid $$ --cli agent)
[[ -n "$OVERLAY" ]] && claim_args+=(--overlay "$OVERLAY")

CLAIM_JSON="$(orch "${claim_args[@]}")" || {
    echo "$CLAIM_JSON" >&2
    echo "could not lease an overlay (see error above)" >&2
    exit 1
}

read -r OVERLAY COUNT <<<"$(python3 -c '
import json, sys
d = json.load(sys.stdin)
print(d.get("overlay", ""), d.get("count", 0))' <<<"$CLAIM_JSON")"

FUNCS="$(python3 -c '
import json, sys
print("\n".join(json.load(sys.stdin).get("functions", [])))' <<<"$CLAIM_JSON")"

echo "leased $OVERLAY: $COUNT functions (session $SESSION)"

# From here on a failure must not strand the lease: every function in the
# overlay would stay unclaimable until the pid died.
cleanup_lease() { orch relinquish-overlay --session "$SESSION" >/dev/null 2>&1 || true; }
trap cleanup_lease ERR

# --- isolated worktree -------------------------------------------------------
# Verification in the trunk checkout is almost always blocked by another
# session; in a worktree build-and-verify needs no lock at all, and the lock is
# taken once at the end to land. Agents that worked this way held the merge lock
# for ~96 seconds to land 19 functions, against 40+ minutes of queueing for
# those that worked in the trunk checkout directly.
WT="$ROOT/../pe2-ov-$OVERLAY"
BRANCH="overlay/$OVERLAY"

if [[ -d "$WT" ]]; then
    echo "reusing existing worktree $WT"
else
    git worktree add -f "$WT" -b "$BRANCH" main >/dev/null
    echo "worktree: $WT ($BRANCH)"
fi

# The gitignored inputs a fresh worktree lacks. Without the submodule links the
# cc1 gate silently rejects every header set and every seed comes out untyped -
# a failure with no error message attached to it.
ln -sfn "$ROOT/assets" "$WT/assets"
ln -sfn "$ROOT/venv" "$WT/venv"
for d in m2c maspsx asm-differ decomp-permuter; do
    # `[[ ... ]] && continue` returns 1 when the test is false, which fires the
    # ERR trap and silently released the lease while the script carried on
    # reporting success. Use a plain if.
    if [[ ! -L "$WT/tools/$d" ]]; then
        rm -rf "$WT/tools/$d"
        ln -sfn "$ROOT/tools/$d" "$WT/tools/$d"
    fi
done

if [[ ! -d "$WT/asm" ]]; then
    echo "splitting (a few minutes)..."
    (cd "$WT" && venv/bin/python3 ninja_config.py >/dev/null 2>&1) \
        || { echo "split failed in $WT" >&2; exit 1; }
fi

# --- rodata triage -----------------------------------------------------------
# A function whose compiler-generated jump table belongs to another unit's
# .rodata cannot land, however good the C is, until a `rodata`/`units` cut is
# made in the manifest. Discovering that after matching wastes the whole match:
# 26 of the 49 functions the orchestrator has recorded as matched but which are
# still unmatched in the tree are blocked this way. Triage before the agent
# picks, not after.
TRIAGE_JSON="$(mktemp)"
trap 'rm -f "$TRIAGE_JSON"' EXIT
python3 "$ROOT/tools/rodata_triage.py" "$OVERLAY" --json >"$TRIAGE_JSON" 2>/dev/null || echo '{}' >"$TRIAGE_JSON"
BLOCKED="$(python3 - "$TRIAGE_JSON" <<'PYEOF'
import json, sys
try:
    d = json.load(open(sys.argv[1]))
except Exception:
    d = {}
for b in d.get("blocked", []):
    print("\t".join((b["func"], b["unit"], b["table"], b["owner"])))
PYEOF
)"

# Filter the leased list here, next to the triage that produced the block list,
# so the brief body is pure formatting and the counts in the heading come from
# the same computation as the list under it.
FUNCS_FREE="$(FUNCS_IN="$FUNCS" BLOCKED_IN="$BLOCKED" python3 - <<'PYEOF'
import os
blocked = {l.split("\t")[0] for l in os.environ.get("BLOCKED_IN", "").splitlines() if l.strip()}
for f in os.environ.get("FUNCS_IN", "").splitlines():
    f = f.strip()
    if f and f not in blocked:
        print(f)
PYEOF
)"
FREE_COUNT="$(grep -c . <<<"$FUNCS_FREE" || echo 0)"

if [[ -n "$BLOCKED" ]]; then
    echo "rodata triage: $(wc -l <<<"$BLOCKED") function(s) blocked on a split change"
fi

# --- brief -------------------------------------------------------------------
BRIEF="$WT/OVERLAY_BRIEF.md"
{
    echo "# Match overlay \`$OVERLAY\` ($FREE_COUNT functions to match, $COUNT leased)"
    echo
    echo "Worktree: \`$WT\` — **build here, it needs no merge lock.**"
    echo "Lease session: \`$SESSION\` (held via tools/vacuum_orch.py)"
    echo
    echo "## Functions"
    echo '```'
    echo "$FUNCS_FREE"
    echo '```'

    if [[ -n "$BLOCKED" ]]; then
        echo
        echo "## These need a split change first - make it, then match them"
        echo
        echo "Each carries a compiler-generated jump table that splat has placed in a"
        echo "*different* unit's \`.rodata\`. A unit's \`.rodata\` appears once in the linker"
        echo "script, at the offset its subsegment names, so decompiling the function emits"
        echo "the table at the wrong address and the overlay stops matching. No amount of"
        echo "work on the C body changes it - one of these reached 100.00% with all-zero"
        echo "penalties in an earlier run and still could not land."
        echo
        echo '| function | its unit | table | table owned by |'
        echo '|---|---|---|---|'
        while IFS=$'\t' read -r fn unit tbl owner; do
            [[ -n "$fn" ]] && echo "| \`$fn\` | $unit | \`$tbl\` | $unit -> **$owner** |"
        done <<<"$BLOCKED"
        echo
        echo "Fix it **before matching anything**, with:"
        echo
        echo '```'
        echo "python3 tools/rodata_cut.py $OVERLAY --apply"
        echo "./tools/build-and-verify.sh --only $OVERLAY"
        echo '```'
        echo
        echo "Ownership is not a judgement call - a generated table must live in the"
        echo "\`.rodata\` of the object whose function uses it - so the tool derives the"
        echo "cuts and rewrites \`configs/USA/overlays.toml\` itself. **Do not do this by"
        echo "hand.** The procedure deletes and re-splits the overlay's \`src/\`, and a"
        echo "function reverted to \`INCLUDE_ASM\` still matches, so lost work leaves the"
        echo "build green and tells you nothing. Done by hand once, it silently reverted"
        echo "two units the change did not even touch. The tool re-applies every"
        echo "decompiled body, refuses to finish if any file's \`INCLUDE_ASM\` count rose,"
        echo "and keeps the originals when it bails."
        echo
        echo "It runs a full split twice, so give it a few minutes. Afterwards these are"
        echo "ordinary work: re-run \`python3 tools/rodata_triage.py $OVERLAY\` to confirm"
        echo "and add them to your list. The manifest edit lands with the match commits."
    fi
    cat <<'BODY'

## Why these are worth doing together

Functions in a large overlay are short — median 31 instructions — but they
share one work struct that nothing declares, which is why the mechanical
seeding pass matches only ~4% of them. Work that struct out once and it pays
across the whole overlay.

`arg0` is usually a `Task*` (`include/main/task.h`, 0x48); 0x1C/0x2C/0x30 are
`idMap`/`extra`/`state`. Actors park their own work block in `idMap` and its
size is pinned by a literal `Mem_Calloc`/`Mem_Malloc`/`Mem_Set` argument —
`grep -B14 'jal.*Mem_' ` over the overlay's `.s`. Expect more than one
allocator; overlays have had 1, 2, 4 and 12 distinct blocks. If none of your
functions dereferences `idMap`, record the anchor and do not invent a struct.

Rooms differ: 80 of 168 allocate nothing at all, and the anchor is
`Task::field_24`, which 167 of 168 rooms load with a `GpMsgEntry[]` table
(`include/gameplay/D4.h` — use it, do not invent a room-local type).

`Task::extra` is a `TmdObject*`; `GameActorExt` was merged into it.

## Traps, all measured

- Retyping a global reinterprets arithmetic in code you did not touch: a
  `global + 0x47C` became scaled pointer arithmetic and broke an already-landed
  function. Grep every use before retyping.
- `lhu` does not imply `u16`; a halfword down-counter compared to zero wants a
  signed *local*.
- A `temp_` crossing a call is load-bearing; one confined to a basic block
  usually pins the wrong `addu` operand order.
- m2c's `f()` is often really `f(task)` — GCC dropped a redundant `move $a0`.
- `byte` is `signed char` and `-funsigned-char` does not override it, so a
  `byte` read may need `(u8)` to emit `lbu` rather than `lb`.
- A store to a bare `extern` beside pointer struct traffic may stop matching.
  `SOFT_BARRIER()` fixes a byte store; a pointer store needs `extern T x[1];`.
  A barrier that improves the score but does not reach 100% is the wrong
  remedy, not a near miss to permute.
- **A 100% checksum does not validate a type name.** Two agents scored 100%
  with the wrong struct because the spellings compiled identically. Argue types
  from behaviour, allocation sites and callers.

## Verifying and landing

Iterate freely here: `./tools/build-and-verify.sh --only <overlay>` (~3s) and
the bare unscoped build (~60s) need no lock in this worktree. Prove every body
at 100.00% with all-zero penalties *before* landing.

To land, in **one** script (the lock is valid only while the acquiring pid
lives, so acquire, work and release in the same process):

1. `python3 tools/vacuum_orch.py merge-acquire --session "$SESSION" --pid $$ --wait 5400`
   with `trap ... EXIT` releasing it.
2. Wait for `git status --porcelain -- src include` to be empty on trunk.
3. Apply by **rewriting trunk's files** — never merge or cherry-pick this
   branch; trunk moves continuously.
4. Bare unscoped build on trunk; require `✅ BUILD SUCCEEDED`, checking the
   status explicitly. Never pipe it into `tail` under `set -e` — the pipeline
   succeeds even when the build failed, which committed a broken tree once.
5. One commit per function, `matched <func> 1`, with an explicit pathspec.

## Holding the lease

The lease was taken by a tool that has already exited, so it is guarded by a
clock rather than a process. If a long-lived process owns this work, bind the
lease to it on startup so it lasts exactly as long as the work does and is
released the moment that process dies:

```
python3 tools/vacuum_orch.py adopt-overlay --session __SESSION__ --pid $$
```

An agent session has no stable pid to adopt with - each of its shell commands
is a separate short-lived process - so re-run the same command periodically
instead; it refreshes the expiry. Without either, the lease lapses after
`--lease-minutes` (default 240) and another session may take these functions.

Then release the lease, recording outcomes in **three** buckets:

```
python3 tools/vacuum_orch.py finish-overlay --session __SESSION__ \
    --matched a,b,c --difficult d,e --unattempted f,g
```

`--difficult` is only for functions you actually fought and lost, and it parks
them. Everything you simply ran out of time for goes in `--unattempted`, which
releases it back to the pool untouched. The distinction is the whole point: a
session runs out of clock long before it runs out of functions, and the first
run of this workflow parked three ordinary functions as difficult purely
because time expired - one of them a near-clone of a function already matched.
Every leased name should appear in exactly one of the three.

## After landing, delete the worktree

```
tools/overlay_batch.sh --cleanup --session __SESSION__ --overlay __OVERLAY__
```

Landing rewrites trunk's files rather than merging this branch, so once the
commits are on main the branch still sits at the commit it was cut from and
`git -C <worktree> status` still lists every decompiled file as modified. That
looks exactly like unlanded work. The first run left its worktree behind and the
13 functions it had already landed on main read as 13 uncommitted matches - the
obvious recovery move would have applied all of them a second time. Delete it.
BODY
    echo
    echo "_Release without landing:_ \`tools/overlay_batch.sh --release --session $SESSION\`"
} > "$BRIEF"

sed -i "s|__SESSION__|$SESSION|g; s|__OVERLAY__|$OVERLAY|g" "$BRIEF"

echo
echo "brief: $BRIEF"
echo
echo "next:  hand $BRIEF to an agent, or work it yourself in $WT"
echo "done:  python3 tools/vacuum_orch.py finish-overlay --session $SESSION --matched ... --difficult ... --unattempted ..."
echo "clean: tools/overlay_batch.sh --cleanup --session $SESSION --overlay $OVERLAY"
echo "abort: tools/overlay_batch.sh --release --session $SESSION"

trap - ERR
