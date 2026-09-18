# Match overlay `actor_120500` (1 functions to match, 1 leased)

Worktree: `/home/gabriel/parasite-eve-2-decomp/../pe2-ov-actor_120500` — **build here, it needs no merge lock.**
Lease session: `ovb-actor_120500-3397953-1` (held via tools/vacuum_orch.py)

## Functions
```
func_actor_120500_80132028
```

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

## The matching loop — use it, do not read raw asm

Every function here gets its own scratch env inside this worktree. Make one
with:

```
./tools/claude --bootstrap-only <function>     # creates nonmatchings/<function>/
cd nonmatchings/<function> && ./build.sh base.c
```

The first few are already built. `build.sh` prints what you actually need:

```
Score: 97.903% (65 differences)
Penalties: stack=0 branch=0 regs=1 reorder=1 insert=0 delete=0
```

That score is the whole point. `build-and-verify --only` is pass/fail on the
overlay's checksum: it cannot tell you 60% from 99.9%, or which of two edits
regressed, and iterating against a binary signal is exactly where an earlier
run's tokens went. Iterate in the scratch env until 100.00% with all-zero
penalties, and only then paste the body into `src/` and run the scoped build.

`base.c` is already m2c-seeded and compiles. Each scratch dir also carries
`MATCH_LOOP.md` (the matching playbook — read it), `BRIEF.md` (this function's
callers and context), `diff.py`, and `dump.sh` for an RTL summary at >=90%.
Penalties that are only `regs`/`reorder` at >=95% are a permuter job, not a
typing job: `./permute.sh --run --timeout 360 -j4 <func> <asm> <c>`.

Delete the scratch dir when you are done with a function.

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
python3 tools/vacuum_orch.py adopt-overlay --session ovb-actor_120500-3397953-1 --pid $$
```

An agent session has no stable pid to adopt with - each of its shell commands
is a separate short-lived process - so re-run the same command periodically
instead; it refreshes the expiry. Without either, the lease lapses after
`--lease-minutes` (default 240) and another session may take these functions.

Then release the lease, recording outcomes in **three** buckets:

```
python3 tools/vacuum_orch.py finish-overlay --session ovb-actor_120500-3397953-1 \
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
tools/overlay_batch.sh --cleanup --session ovb-actor_120500-3397953-1 --overlay actor_120500
```

Landing rewrites trunk's files rather than merging this branch, so once the
commits are on main the branch still sits at the commit it was cut from and
`git -C <worktree> status` still lists every decompiled file as modified. That
looks exactly like unlanded work. The first run left its worktree behind and the
13 functions it had already landed on main read as 13 uncommitted matches - the
obvious recovery move would have applied all of them a second time. Delete it.

_Release without landing:_ `tools/overlay_batch.sh --release --session ovb-actor_120500-3397953-1`
