# GCC 2.8.1 decision analysis — 2026-09-09

This investigation explains compiler decisions in the three remaining difficult
functions and supplies a reusable observer. It does not claim an exact match
for any of them. The rules derived from it are incorporated into
[CODEGEN_MODEL.md](CODEGEN_MODEL.md).

## Replay rerun follow-up: observe selection, preserve competing constraints

The later Astra and Fable sessions still leave the best distance at 770
(98.438%). Fable supplies a useful 1215-distance source with two asm helpers,
and Astra's repaired permuter transformation improves a weaker alternate from
4185 to 3112. Neither constitutes an improvement over 770. Both useful shapes
were being omitted from the retry shortlist; they are now retained and given
search priority, with equivalent observed objects sharing a slot.

The next step is to combine the already-known requirements, rather than repeat
their individual explanations. A source intervention can fix one compiler
decision and break a later one. The following constraints remain relevant:

| Requirement | Evidence or competing requirement |
|---|---|
| Palette must retain fp | The previous 909/911 allocation threshold still applies; source-shape padding can affect it without changing emitted instructions. |
| Glyph subtraction must fall between the brightness loads | Ready-list priority does not settle its placement; actual hazards can queue a load and potential hazards can override the comparator. |
| Height, top coordinate and final flag must use the target register at their respective lifetimes | Splitting top makes it a local quantity in v1 and leaves height/flag in a3; splitting only the flag preserves top in a0 but moves the shift early and flag to t0. |
| Sprite piece must stay live long enough for t3, while page uses a1 | The previous runs show that removing the sprite barrier improves constant placement but moves the piece update early and changes allocation. A compatible dependency/lifetime intervention remains unresolved. |

`trace_gcc.py --uids` now observes `schedule_insn` dependency releases and
`schedule_select`, including `actual_hazard` and `potential_hazard`, in addition
to the comparator. The scheduler operates backward. Actual hazard costs are
delays; potential costs are weights derived from unit blockage and remaining
unit users, not cycle counts. The compiler source is `sched.c`, particularly
`adjust_priority`, `schedule_insn`, `schedule_select` and the hazard functions.

The controlled subtraction experiment reproduced a concrete counterexample:

1. Rebuild Fable base_19 as `compiler-residue/base_1.c`: distance 1215.
2. Predict that separating `top = y - y0` from the reused `y0` will promote
   subtraction UID 370 when released. Build `base_2.c`: distance 1809.
3. Observe the predicted promotion from priority 3 to LAUNCH_PRIORITY
   (2130706433). At sched1 cycle 11, the comparator prefers subtraction 370 to
   green load 358. Both have zero actual hazard. The load wins the subsequent
   potential-hazard choice, 8650752 versus 0.
4. Observe top as local quantity b19/q12, nine weighted references over 16
   half-instructions, allocated to v1. Remaining height/flag r90 receives a3
   through global allocation. Thus the predicted promotion is real and
   insufficient; the source split also violates the register requirement.

A second counterfactual, `base_3.c`, assigns the final glyph flag to the existing
`page` variable. It keeps top in a0 but moves the shift before the subtraction,
does not fix X/X/Y/Y store grouping, and increases distance from 770 to 2970.
The reorder penalty improves from nine to three while the register penalty
increases from six to 118. A better individual penalty cannot establish that a
candidate is a better starting point without examining these tradeoffs.

Both observed compilations have byte-identical assembly with and without GDB.
The [retained counterfactual evidence](tools/compiler_evidence/2026-09-09-replay-residue.json)
contains compiler/source/input hashes, selected raw events, scores and plans
recorded before compilation. Full traces and sources remain in the
`compiler-residue` scratch and its session archive. No game source was changed.

The search validity defect was also repaired at its source. The vendored parser
misparsed `sizeof(T) + value` as a cast inside `sizeof`; explicit type-form
precedence and regeneration of cached yacc tables repair it. The actual Replay
seed now survives parser emission with the same normalized object fingerprint
and distance 770. The generated compiler wrapper stops on an archived malformed
output's cc1 exit status 33 and creates no object. These checks establish a valid
search baseline; they do not establish a new matching source.

A subsequent three-minute search across all three seeds illustrates why source
review must follow scoring. The best numerical results were 770 → 710,
1215 → 1161 and 3112 → 2149. The first writes the glyph flag into `p->y1`
instead of its top coordinate; the third truncates the `0xE1000200` GPU command
constant to 16 bits. Both were rejected. `attempt.py reject SOURCE --reason ...`
now preserves that decision by source hash through archives and restores, while
keeping the bad output as evidence. The other primary output reproduced the
original 770 object in the scratch scorer despite a small search-score gain.

The valid Fable alternate improves to **1161 (97.645%)**. Two controlled builds
isolate its edits: casting sprite x to unsigned before subtraction produces
identical object assembly; moving `p->clut` before the flag assignment reproduces
the complete gain. The cast is unnecessary and omitted from the readable port,
`compiler-residue/base_4.c`. Its stored halfword is equivalent modulo 65536 for
all signed-16 x values, but equivalence alone does not imply a compiler effect.

The clut expression contains signed division by four. Its generated sign branch
creates a scheduling boundary: after the source reorder, the flag shift is in
the following block, so its output/anti-dependencies no longer constrain the
previous glyph block. This improves green/subtraction/blue/add and Y/X/Y/X
ordering, but moves the flag shift later than the target and swaps gh/gv's hard
registers across both loops. This is more specific than a ready-list tie change.
An isolated transfer to the primary (`base_8.c`) predicted and reproduced that
branch/shift boundary, but worsened distance to 1741. The reusable rule is to
check RTL block membership before reasoning about scheduling priorities.

The valid primary remains **770 (98.438%)**. The improved alternate and all
counterexamples are archived. No test suite was added or run; validation used
the actual compiler, object scorer, controlled source variations, observer
output comparisons, syntax checks and source review.

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
