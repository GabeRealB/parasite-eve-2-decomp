# Matching loop (GCC 2.8.1)

Score with `./build.sh`. 100% is a match. Read the **Penalties:** line (`stack` / `branch` / `regs` / `reorder` / `insert` / `delete`).

1. First `./build.sh base.c` is a **baseline**. Minimal edits so it compiles. Do not rewrite m2c or a give-up seed from the asm before that score.
2. Prefer already-matched siblings in the same TU (BRIEF.md) over m2c gotos. Write each attempt as `base_N.c`.
3. Plan the next edit from **structural diagnostics + a compiler hypothesis + dumps**, not from the object-dump / asm-differ alone.
4. Search the learnings corpus with `python3 tools/learn.py <terms>` — it ranks whole
   sections, where a raw grep returns hundreds of context-free lines ("delay slot"
   appears in 286 sections but only 16 titles). `--show N` prints the top N in full.
   Never read `DECOMPILATION_LEARNINGS.md` end-to-end; it is 1.3 MB.
   Read `CODEGEN_MODEL.md` first — it is the short general model the corpus
   entries are instances of, and it settles most mismatches on its own.

`build.sh` keeps the `.s` with RTL insn uids (`# 31 movsi_internal2/5`). At ≥90% it runs `./dump.sh` and prints a **NEXT:** line naming dump files. The printed summary is not enough — **open those files** (`base_N.i.lreg`, …) before the next C edit. You can also run `./dump.sh base_N.c` by hand.

| leftover | file | what to do |
|---|---|---|
| `insert` / `delete` / `branch` ≠ 0 | `.diagnosis.json`, then relevant RTL dumps | Distinguish changed block connections/predicates from shifted addresses, spills, rematerialization and scheduling. These penalties alone do not identify the cause. |
| `regs` | `.lreg` `.greg` | **Check the suggestion first**, with `lregwalk.py`: is the value the direct result or operand of something whose output is a hard register? Measured over 98 dumps, the priority ratio orders only 66% of same-block pairs and 38% are exact ties — the rest is the suggestion pass, and inversions cluster on `$v0`. To move a value out of `$v0`, break the suggestion (name an intermediate, move where it is consumed). Only then shorten the loser's live range, **split a reused local**, or **unpin**. `CODEGEN_MODEL.md` §10.5. |
| `reorder` | `.sched` `.sched2` `.dbr` | Statement order and delay slots (store vs `mflo`/`lbu`/`jal`). |
| `stack` | extra locals / frame | Split or shrink locals. |
| fused const, `lb` vs `lh`, dropped `andi` | `.cse` `.cse2` `.combine` | |
| loop IV / one walking pointer | `.loop` then `.cse2` | |
| dead store / `REG_DEAD` | `.flow` | |

`build.sh` writes `.diagnosis.json` alongside the unchanged exact score. It compares
block connections (including delay slots and resolved switch destinations),
branch predicate kinds, call targets, and instruction counts. Matching topology
is not semantic equivalence; changed branch operand registers may themselves be
allocation differences. Unknown indirect transfers remain explicitly unknown.
Always inspect the corresponding dumps before deciding what caused a difference.

## Experiments and retry history

Read `HISTORY.md` on a retry. The archive keeps immutable session notes, sources,
compressed preprocessed inputs and experiment records even if the score declines.
Archived `seed_*.c` alternatives are copied into the scratch; their scores are
historical and must be rebuilt with the current headers and compiler.

Before an edit, record a prediction (the candidate file can be created afterward):

```
./attempt.py plan base_1.c --parent base.c --hypothesis 'piece dies too early' --expect 'piece conflicts with a1' --pass-name lreg
```

After the build and dump inspection, record the observation and next experiment:

```
./attempt.py conclude base_1.c --result 'lifetime extended, allocation unchanged' --next 'inspect suggestion pass'
```

`build.sh` journals success/failure, source/compiler/preprocessed-input hashes,
flags, score, duplicate assembly, and the earliest changed RTL dump versus the
parent. An early dump difference can be numbering noise: inspect its operands.
A failed prediction is useful evidence even when the percentage does not improve.
Repeated assembly is not a new search result. Ten distinct builds without a score
gain trigger a change of hypothesis or seed; do not repeat an exhausted experiment.
The default session budget is 40 builds, including failures and repeats, unless
the user authorized more. Repeats consume the budget without counting as progress.
Before stopping, complete conclusions and write unresolved questions plus evidence
in `LEARNINGS.md`. General compiler findings may also enter the shared corpus;
the session copy is preserved independently of whether a match lands.

## Pins

`register T x asm("s4")` is function-scope in GCC 2.8.1: it reserves that hard register for the **whole function**. Do not add pins because `$s4` is wrong in the object dump.

- Do not pin until dumps say a live range is the leftover **and** an unpinned attempt exists.

## Compiler source

The **patched GCC 2.8.1** that built the target (stock 2.8.1 plus the decompals
psx patches) is on disk at `local/gcc/gcc-2.8.1-psx/`, and a scratch env
symlinks it as `gcc/` in its own directory - use whichever your cwd is, since
this file is read both from a scratch dir and from the repo root. Read it when
a dump shows a
decision you cannot explain - `local-alloc.c` / `global.c` for which pseudo gets
which hard register, `reload1.c` for spills and why a pin misbehaves, `sched.c`
for sched1 tie-breaking, `combine.c` and `cse.c` for folding and operand order.

**Never fetch compiler source over the network, and never read another GCC
version.** Nine such fetches in one sweep pulled 2.95.3 four times: seven years
newer, different scheduler and CSE, and nothing here catches a conclusion drawn
from it. If neither path exists, run `tools/fetch_gcc_source.sh`, or work from
the dumps instead.

Prefer the dumps first regardless: they say what the compiler did to *this*
function, which is the question. The source only says what it does in general.
- If the seed already has pins, **unpin and rescore** as its own `base_N.c`. Unpinning is often the 100% move.
- Never treat a pinned ≥90% as the best seed. Leave an unpinned `base_N.c` in the scratch dir.

## Empty asm

Prefer the named helpers in `include/decomp/common.h` over raw empty `asm` / `asm volatile`. `TOUCH_REG(x)` is `"+r"` (blocks CSE / copy-prop). `TOUCH_REG_USE(x, y)` is `"+r"(x)` plus a keep-live `"r"(y)`. `USE_REG(x)` is input-only (keeps live); `SOFT_USE_REG` is the non-volatile form. `SCHED_BARRIER()` vs `SOFT_BARRIER()` is volatile vs not — that is a matching difference. Do not wrap them in `do { } while (0)` or extra braces. Instruction-emitting `lui`/`lo`/`sll` stays written out. `register T x asm("v0")` is still a pin, not these macros.

## Permuter

Once a primary reaches 95%, ask the search router to assess it and alternatives:

```
python3 tools/vacuum_permute.py --func $functionName --scratch <scratch> --timeout 360 --jobs 4
```

It rebuilds up to three distinct unpinned candidates, including promising lower
scores, and uses block/predicate/call diagnostics to decide whether to search.
Unknown structure requires further evidence or clean legacy register/scheduling
penalties. It shares one budget across rebuilding, setup and all searches.
`PERMUTER.json` / `PERMUTER.txt` record run and skip reasons. `--setup-only`
inspects cached eligibility without compiling or searching.
A zero score still requires porting and the full verification below.

If the kept `.s` matches and the `.o` does not, the bug is maspsx (`--expand-div`), not GCC.

## Shared bodies

Overlays in a family share a lot of code — 44% of the room functions are still
copies of another room's. Before matching one, check:

```
python3 tools/overlay_dup_index.py find <function>
```

If it reports copies, the body is worth matching **once**: put it in
`src/<family>/lib/<unit>.c`, add the span to `configs/USA/overlays.toml` and the
shared symbol to each sharer's `configs/USA/sym/<family>/<overlay>.txt`, and one
object is linked into every overlay that uses it. `=` in that listing means the
copies are byte-identical, `~` means the same body at a different link offset.

This is not hypothetical: of the first 158 room functions matched, only 29 were
distinct bodies — one two-instruction stub was matched 112 times.

## Verify

Worktrees copy `build/` binaries that still checksum from `INCLUDE_ASM`. `sha256sum --check` on `build/USA/out/SLUS_010.42` is **not** a match.

Verify in two steps, both from the **repo / worktree root**:

```
./tools/build-and-verify.sh --only <scope>    # inner loop, seconds
./tools/build-and-verify.sh                   # the finish line
```

`<scope>` is the `Build scope:` line in BRIEF.md — the overlay basename (`gameplay`, `m93r`), or a family (`core`, `weapons`). A scoped run splits, builds and checksums only those units and leaves every other overlay's `asm/` and `linkers/` alone, so it is safe to run repeatedly while iterating: 1.6s for one weapon overlay and 4s for gameplay, against 18s for the project.

A scoped `✅ SCOPED BUILD SUCCEEDED` says **nothing** about the overlays it skipped, and it says so in the message. Only the bare `./tools/build-and-verify.sh` printing `✅ BUILD SUCCEEDED` counts as a match — run it before committing or reporting one. Struct changes are the case that bites: they reach every overlay that shares the type, and the scoped run cannot see it.

If `python3 ninja_config.py` fails on splat/spimdisasm, use `venv/bin/python3`.

## Never rewrite a whole `.c`

Write the one function you matched. Do **not** replace a source file wholesale —
not from splat's regenerated output, not from a copy your worktree made before
another lane committed. Both drop every other matched body in that file, and the
build cannot see it: an `INCLUDE_ASM` assembles to exactly the bytes the C
compiled to, so the checksum still matches and every overlay still links. 39
functions have been lost this way.

If a step genuinely requires deleting and re-splitting a file — a `rodata` cut is
the usual reason — snapshot what it holds first (`bodies_of()` in
`tools/land_overlay.py` lists every C definition) and put those bodies back
afterwards. Reconstructing from the pre-change source beats starting from the
regenerated skeleton.

The unscoped `./tools/build-and-verify.sh` now runs `tools/check_lost_matches.py`
and fails if any function with a `matched` commit has reverted to `INCLUDE_ASM`
while its overlay still owns a `.s`. Do not commit through that failure -
recover the body from its matching commit.
