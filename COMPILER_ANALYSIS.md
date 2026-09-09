# GCC 2.8.1 decision analysis — 2026-09-09

This investigation explains compiler decisions in the three remaining difficult
functions and supplies a reusable observer. It does not claim an exact match
for any of them. The rules derived from it are incorporated into
[CODEGEN_MODEL.md](CODEGEN_MODEL.md).

## Method and evidence

Read the patched source in `local/gcc/gcc-2.8.1-psx`, especially `toplev.c`,
`local-alloc.c`, `global.c`, `reload1.c`, `sched.c`, `cse.c`, `expr.c`, `stmt.c`
and the MIPS backend. Replayed archived preprocessed inputs, preserving the
original headers. Observed the actual bundled i386 cc1 through GDB entry/return
breakpoints; no compiler replacement or compiler-state modification was needed.

The observer checks compiler SHA-256
`60d886cd75bbd7855fc7909224a15401de76bff21af8a629c2060290a073f5fd`
before reading its private data layouts. It resolves local symbols by their
source-file owner, since names such as `reg_qty` occur in several passes.
Every successful observation also produced byte-identical `.s` output in an
ordinary compilation. No test suite was added or run.

The three best saved candidates were also assembled and scored against the
target again: UI 99.249% (distance 535), HUD 97.419% (1985), Replay 98.438%
(770), reproducing their saved penalty breakdowns. The header edit changes
comments only; no game-function bodies or compiler behavior were modified.

[Retained evidence](tools/compiler_evidence/2026-09-09.json) includes input and
source hashes, selected actual events, archived-input locations and the precise
one-marker experiment. Full traces can be regenerated with `tools/trace_gcc.py`.
Displayed priority values are calculated from observed inputs; ordering,
membership, allocation results and substitutions are observed in the compiler.

Source availability is useful provenance evidence, but matching hashes of
inspected files do not establish that a prebuilt executable came from them.
Direct observation and ordinary/output comparison provide the additional check.

## UI: two mechanisms in the page-down clamp

Function: `func_80046EEC`. Compare the latest session's `base_35` and `base_34`.
The session is
`tools/giveups/func_80046EEC/sessions/00d13d380d6a44f5b8a62ae792fc276a/6f91ce972ba960221408/`.

### The early copy is a surviving promotion boundary

In base_35, UID 1182 loads the unsigned count byte into HI pseudo r138. The
initial RTL promotes that value for a signed SI comparison/subtraction with a
shift pair. Combine proves the extension redundant and turns UID 1201 into
an SI copy of a subreg of r138. It removes arithmetic, but preserves two
distinct pseudos with distinct later uses.

| Quantity/allocno | Observed references / interval | Allocation |
|---|---|---|
| r425, promoted count, allocno 44 | 4 / 13, priority 6153 | a1 |
| r138, raw count, allocno 32 | 3 / 18, priority 1666 | a2 |
| local q1, r438/r437/r434 | 6 / 10 half-instructions, priority 12000 | v0 |
| local q0, r430/r431 | 4 / 8 half-instructions, priority 10000 | v1 |

r138 is still consumed by the narrow subtraction at UID 1242. Its use does not
die at the earlier promotion, so the early copy cannot simply be treated as a
local dying-input tie. These are global allocation decisions for the count,
alongside local quantity decisions for the clamp arithmetic.

**Next hypothesis:** preserve the useful source comparison order while changing
which raw/promoted count expression survives into the clamp. Compare the direct
field form in base_34 by expression identity, since its pseudo numbers differ.
A useful prediction must state whether UID 1201 disappears, moves, or gets a
different source/destination; a score-only improvement cannot explain the cause.

### The missing unsigned load is post-reload CSE

UID 1228 provides a decisive pass boundary:

| Stage | Source of the assignment |
|---|---|
| combine, sched1, local allocation | unsigned `mem/s:QI(base + 5)` |
| `.greg`, after register allocation and reload | unsigned `mem/s:QI(s0 + 5)` |
| `reload_cse_simplify_set` | replaced with `reg:QI a0` |
| `.sched2` | retained register copy |

The GDB event directly records that substitution. The later dump confirms it
survives validation. This pass is called by `toplev.c` **after writing `.greg`**
and **before sched2**; the old model omitted it entirely.

`reload_cse_simplify_set` searches for an equivalent hard-register value and
uses a register copy when its costs and constraints allow it. Its memory
invalidation delegates to dependence analysis in `sched.c`. Ordinary CSE's
coarser rule for byte stores therefore does not explain this late replacement.

**Next hypothesis:** alter the lifetime or availability of the earlier unsigned
row value in a0 while retaining the correctly allocated signed row calculation.
Inspect the instructions that keep that equivalence alive, rather than applying
another generic byte-alias barrier to the whole region. This hypothesis remains
unmatched; the analysis establishes the responsible pass, not a final source fix.

## HUD: local ranking determines the register reload later reserves

Function: `func_800A57B0`. Compare `base_26` and `base_29` from
`tools/giveups/func_800A57B0/sessions/0e45e381017a4bfcbbc172cb77813a37/aa351635f2f7f19031b3/`.

The source change places each polygon's y2 store immediately after its y3
store. In block 46 it changes the death of lower-Y's local quantity:

| Candidate | Quantity | Refs | Birth → death | Integer priority | Initial home |
|---|---|---:|---|---:|---|
| base_26 | q28 / r434, lower Y | 5 | 144 → 356 | 471 | t7 |
| base_26 | q29 / r502 | 5 | 148 → 340 | 520 | t6 |
| base_29 | q28 / r434, lower Y | 5 | 144 → 332 | 531 | t6 |
| base_29 | q29 / r502 | 5 | 150 → 342 | 520 | t7 |

Neither quantity has a hard-register suggestion. The source change shortens
q28's interval from 212 to 188 half-instructions. Its priority crosses q29's
520, so q28 takes t6 first. This is a directly observed rank inversion.

Reload then reserves t8, s4, s5 and the register holding lower Y. The events
show r512 evicted from s4, r511 from s5 and r434 from t7/base_26 or t6/base_29.
Constants can rematerialize; the lower-Y value needs a reload. Its five
references contribute `5 + (5 + 1) / 2 = 8` to the cost of its hard register,
because it was allocated locally. The actual preference lists reflect this
change. The lower-Y scratch name follows the earlier allocation.

Two other quantities stay unchanged:

- Left coordinate q3/r430: refs 4, span 192, priority 416, home s3.
- Polygon code q31/r510: refs 3, span 180, priority 166, home s6.

Thus the t6 fix does not solve the left/code swap. The target wants those two
in s6/s3. Changing their priorities also changes which registers reload can
reserve for the flags copy. Track both local allocation and reload, including
the code/length/tpage quantities, before choosing another keepalive.

**Next hypotheses:** investigate the code quantity's grouping and suggestions,
and the pre-allocation live overlap of slide/display offset. Do not interpret
the globally doubled constant lifetimes in `.lreg` as local quantity intervals.
The tracer records the actual intervals, removing that ambiguity.

## Replay: a reversible allocator threshold experiment

Function: `func_replay_bonus_801183B8`. Inputs are from
`tools/giveups/func_replay_bonus_801183B8/sessions/1b6f7aef07844a02b7ca6d3bf6402bca/e18ee77eabc4be99270e/`.

Palette r98 is allocno 16 and has five references over 110 instructions. Its
calculated priority is 909. Brightness's high address is allocno 27; its pseudo
number changes from r224 to r225 between variants.

| Input | Brightness refs / length | Priority | First eligible winner of fp |
|---|---|---:|---|
| base_11, before padding | 19 / 824 | 922 | brightness |
| base_23, seven extra markers | 19 / 838 | 906 | palette |
| base_33, six extra markers | 19 / 836 | 909 | palette, by lower allocno |
| controlled edit of base_33, five extra markers | 19 / 834 | 911 | brightness |

The last row is a new intervention: remove exactly one basic empty asm from
the consecutive glyph group in base_33's saved `.i`, leaving all other input
unchanged. Prediction: brightness regains fp and palette loses its register
home. Both occurred. The input hashes and exact replacement are retained.

This validates the lifetime mechanism and the boundary. It does not establish
that the original source contained markers. Their absence from the emitted
instruction stream does not make them invisible to the optimizer. To remove
the padding naturally, find a surviving RTL definition/use or grouping change
with the same useful effect on the competing allocnos.

Brightness's failed caller-save retry in base_33 is also informative: 19 refs
across one call passes the profitability threshold, but its conflicts cover
all suitable general-purpose registers. Profitability alone does not allocate
a caller-saved home. Reload reconstructs the high address from its equivalence.

### Scheduling remains a separate problem

The tracer observes brightness-related comparisons involving UID 370:
it wins on priority against UIDs 359/362 in sched1, while those instructions
win original-RTL-order ties against it in sched2. The scheduler works backward,
so these are comparator preferences, not a forward execution trace.

Matching the palette's register is insufficient to fix brightness placement or
UV/coordinate-store interleaving. Post-reload values, hard-register dependencies
and memory-access flags must remain compatible with the target schedule.
The source's scalar/member distinction affects alias exemptions in `sched.c`;
the prior green-member experiment is preserved in session notes. This
investigation does not claim to have reconstructed every ready-queue dependency.

## Reusable corrections to the matching workflow

- Include post-reload CSE in the pass map. A `.greg`/`.sched2` difference is not
  automatically a scheduler decision.
- Use actual local quantities, then global allocnos, then reload evictions.
  Per-pseudo ratios cannot substitute for local quantity intervals.
- Treat global priority as a quantized integer with an allocno tie-break.
  Shared allocnos sum references and take maximum member live length.
- Separate ordinary CSE's memory categories from scheduler/post-reload alias
  analysis. A known non-overlapping byte store need not erase late equivalences.
- Input-only asm is implicitly volatile in this compiler. A `SOFT_` macro
  spelling is insufficient evidence that its RTL is movable.
- Function-local register asm creates hard-register RTL; only top-level
  register declarations call `globalize_reg`. An fp pin can be invalid even
  when its similarity score is high.

For future plateaus, use `trace_gcc.py` only after identifying relevant pseudos
and UIDs from the ordinary dumps. Preserve the important events and the input
hash in session notes. Read [CODEGEN_MODEL.md §12](CODEGEN_MODEL.md#12-reproduce-and-observe-a-compiler-decision)
for invocation, evidence files and platform limits. Compiler output comparison
checks observation neutrality; exact function matching and integration remain
separate checks.
