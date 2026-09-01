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
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

OVERLAY=""
SESSION=""
RELEASE=false
KEEP=false

usage() {
    sed -n '2,25p' "$0" | sed 's/^# \{0,1\}//'
    exit 1
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --overlay) OVERLAY="$2"; shift 2 ;;
        --session) SESSION="$2"; shift 2 ;;
        --release) RELEASE=true; shift ;;
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

# --- brief -------------------------------------------------------------------
BRIEF="$WT/OVERLAY_BRIEF.md"
{
    echo "# Match overlay \`$OVERLAY\` ($COUNT functions)"
    echo
    echo "Worktree: \`$WT\` — **build here, it needs no merge lock.**"
    echo "Lease session: \`$SESSION\` (held via tools/vacuum_orch.py)"
    echo
    echo "## Functions"
    echo '```'
    echo "$FUNCS"
    echo '```'
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

Then release the lease, recording outcomes:

```
python3 tools/vacuum_orch.py finish-overlay --session __SESSION__ \
    --matched a,b,c --difficult d,e
```

Anything you could not match goes in `--difficult` so it is not re-picked.
BODY
    echo
    echo "_Release without landing:_ \`tools/overlay_batch.sh --release --session $SESSION\`"
} > "$BRIEF"

sed -i "s|__SESSION__|$SESSION|g" "$BRIEF"

echo
echo "brief: $BRIEF"
echo
echo "next:  hand $BRIEF to an agent, or work it yourself in $WT"
echo "done:  python3 tools/vacuum_orch.py finish-overlay --session $SESSION --matched ... --difficult ..."
echo "abort: tools/overlay_batch.sh --release --session $SESSION"

trap - ERR
