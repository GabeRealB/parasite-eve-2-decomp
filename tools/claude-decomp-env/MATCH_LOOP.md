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
| `regs` | `.lreg` `.greg`, compiler trace if needed | Identify local quantity membership/suggestions, then global conflicts/preferences, then reload evictions. `lregwalk.py` helps locate possible ties; `tools/trace_gcc.py` observes actual quantities and decisions. Per-pseudo priority ratios do not rank local quantities. `CODEGEN_MODEL.md` §10.5. |
| `reorder` | `.sched` `.sched2` `.dbr` | Check dependency release, ready-list ranking, actual/potential hazard selection, then delay-slot handling. The comparator alone does not select the instruction. |
| load survives `.greg`, becomes copy in `.sched2` | `reload_cse_regs`, then `.sched2` | Post-reload CSE runs between those dumps. Trace the substitution before changing scheduler barriers. |
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
The shortlist reserves room for a near-best source with fewer asm helpers and
a verified permuter gain, then different penalty tradeoffs. Equivalent observed
objects share one search slot. `PRIOR_SEEDS.json` carries this evidence across
restores. Reconsider older candidates with `tools/archive_giveup.py --func FUNC
--reindex` when selection policy changes; this leaves session snapshots intact.

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

When previous sessions already explain individual changes but remain stuck,
write the conflicting requirements in `LEARNINGS.md` before another variant:
desired instruction order, dependency/hazard that prevents it, required register
home or lifetime, and the first pass where each requirement fails. Predict both
the decision being changed and the allocation/scheduling property that must
survive it. Inspect those two results separately even if the score declines.
For example, Replay's subtraction can gain the expected launch priority yet
lose hazard selection and move to a different register. Repeating that priority
explanation or rearranging equal-priority statements is not a new hypothesis;
the next intervention must address the demonstrated conflict.

## Pins

Function-local `register T x asm("s4")` creates hard-register RTL; it does not
reserve the register through `globalize_reg` as a top-level declaration does.
Its uses, conflicts and elimination behavior can disturb other values. The fp
pin in the saved Replay probe was invalid despite a high similarity score.
Do not add pins because `$s4` is wrong in the object dump.

- Do not pin until dumps say a live range is the leftover **and** an unpinned attempt exists.

## Compiler source

The **patched GCC 2.8.1** source corresponding to the bundled matching compiler
(stock 2.8.1 plus the decompals psx patches) is at `local/gcc/gcc-2.8.1-psx/`, and a scratch env
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

When dumps omit a decisive quantity or reload transition, run from the root:

```
python3 tools/trace_gcc.py <scratch>/base_N.i --output-dir /tmp/gcc-observation --function $functionName --regs 138 425 --uids 1228
```

Use this compilation's pseudo/UID numbers and a new output directory. Archived
`.i.gz` inputs also work. The tracer requires GDB/ptrace access, supports the
audited bundled compiler hash, and checks that tracing leaves assembly unchanged.
Read `CODEGEN_MODEL.md` §12 and `COMPILER_ANALYSIS.md` for limits and examples.
Save the relevant events and input hash in `LEARNINGS.md` before scratch cleanup.
- If the seed already has pins, **unpin and rescore** as its own `base_N.c`. Unpinning is often the 100% move.
- Never treat a pinned ≥90% as the best seed. Leave an unpinned `base_N.c` in the scratch dir.

## Empty asm

Prefer the named helpers in `include/decomp/common.h` over raw empty asm.
`TOUCH_REG(x)` is `"+r"`; `TOUCH_REG_USE(x, y)` adds a keep-live `"r"(y)`.
Input-only asm, including `SOFT_USE_REG`, becomes implicitly volatile in GCC
2.8.1 because it has no outputs. Basic empty asm is a scheduling boundary too.
Read/write `SOFT_TOUCH_REG` avoids the no-output rule, but still changes RTL
dependencies and may move. Check the dump rather than inferring behavior from
the helper's name. Do not add `do/while` wrappers or extra braces. Instruction-
emitting `lui`/`lo`/`sll` stays written out; register asm remains a separate pin.

## Permuter

Once a primary reaches 95%, ask the search router to assess it and alternatives:

```
python3 tools/vacuum_permute.py --func $functionName --scratch <scratch> --timeout 360 --jobs 4
```

It rebuilds up to three distinct unpinned candidates, including promising lower
scores, and uses block/predicate/call diagnostics to decide whether to search.
Unknown structure requires further evidence or clean legacy register/scheduling
penalties. It shares one budget across rebuilding, setup, searches and paired
verification. `PERMUTER.json` / `PERMUTER.txt` record run and skip reasons.
`--setup-only` writes `PERMUTER_SETUP.json` / `.txt` without compiling or searching.

Every complete, source-distinct search output is retained with full declaration
context under `PERMUTER_EVIDENCE/<run>/`, before the next seed cleans the search
directory. The router rebuilds the normalized baseline and best output using the
scratch compiler/scorer. Search scores alone are not verified improvements.
Fresh builds enter the ordinary candidate journal and can become retry seeds.
Exit 0 means a candidate for follow-up: an exact scratch match (`PERMUTER_HIT`),
a verified partial improvement (`PERMUTER_IMPROVEMENT`), or a discovery whose
verification failed or exhausted its budget (`PERMUTER_REVIEW`). First reproduce
unverified leads. Read the report before integrating anything.

Treat discoveries as compiler experiments. Read `PERMUTER_ANALYSIS.md`, preserve
the original seed and improved candidate, and use new files to isolate the
source change. Compare the first meaningful RTL divergence; ignore numbering
noise. Distinguish preprocessing/normalization effects from the permutation.
First inspect the source delta for changed values, dropped masks or stores,
truncated constants and reads moved across writes. Permuter mutations need not
preserve behavior. A successful paired build verifies distance, not semantics.
Exclude a disproven candidate without deleting its evidence:

```
./attempt.py reject base_perm_ID.c --reason 'moves flag assignment before the coordinate store; y1 receives the flag'
```

Rejections follow the source hash through archival and restore; modifying the
source creates a new candidate for review. Inspect the next retained output
when the best-distance result is invalid, within the same follow-up budget.
Use `trace_gcc.py` when actual compiler decisions are needed; store traces under
`PERMUTER_EVIDENCE/<run>/analysis/`. Record a prediction with `attempt.py plan`
before compiling a controlled variation, then compare its actual result.

This investigation has **eight additional scratch builds**, enforced by
`build.sh`, including failures and repeats. Spend at most four on causal analysis
and reserve the rest for porting; at most two focused tracer invocations may
supplement the dumps. This allowance follows the router's automated verification
and supersedes the ordinary session limit for this follow-up only. Do not reset
it, bypass it with manual compiler calls, or rerun the router. Full integration
verification is separate. A mechanism may remain unresolved; preserve the gain.

After the final scratch build, record the conclusion:

```
./attempt.py conclude-permuter base_N.c --status unresolved \
  --result 'observations and remaining mechanism question' --next 'specific prediction to investigate' \
  --evidence PERMUTER_ANALYSIS.md --evidence PERMUTER_EVIDENCE/<run>/manifest.json
```

Use `supported` only with a mechanism backed by dump/trace evidence and a
successful controlled prediction; add `--prediction-source base_K.c`. The tool
requires its plan to precede its successful build. It cannot judge whether the
explanation follows from the evidence: document that reasoning in the notes.
The conclusion command copies cited files and the candidate/variation's build
outputs into the retained evidence, including files originally saved elsewhere.
Use `not-reproduced` when the claimed improvement fails the paired comparison.
Notes must distinguish observations, hypotheses, prediction/result, scope and
evidence paths. Promote reusable supported findings to `DECOMPILATION_LEARNINGS.md`
with input hashes and selected observations; change `CODEGEN_MODEL.md` only for
a supported general rule. Leave unresolved claims in the session notes.

Port useful transformations into the seed's normal C/header style. Partial
matches remain in scratch for archival with INCLUDE_ASM intact. A zero score
still requires porting and the full verification below; an unresolved mechanism
does not prevent landing a verified match. Concluding closes the scratch build
allowance. Before cleanup, including after success, run from the project root:

```
python3 tools/archive_giveup.py --func $functionName --scratch <scratch> --permuter-findings
```

Vacuum also does this automatically, including copying worktree findings to
trunk. `tools/permuter_findings/<function>/` keeps immutable local snapshots
after successful matches clear give-up seeds. Regular give-up archives include
these experiments and expose the latest conclusions in retry `HISTORY.md`.

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
