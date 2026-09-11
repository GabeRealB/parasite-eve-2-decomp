# A working model of GCC 2.8.1 for this decomp

Use this model to identify compiler mechanisms, then search
`DECOMPILATION_LEARNINGS.md` for examples. The corpus contains many repeated
local solutions; their C spelling alone is not a general compiler rule.

The initial model came from the corpus. Sections 1, 2, 4, 10–12 and the
diagnostic ladder now incorporate the patched compiler source and direct GDB
observations of the bundled cc1. The 2026-09-09 investigation, reproducible
inputs and remaining uncertainties are in [COMPILER_ANALYSIS.md](COMPILER_ANALYSIS.md).
Distinguish a source rule, an observed decision, and a proposed source edit.
An exact scratch score still requires integration and build verification.

It sits beside `DECOMPILATION_LEARNINGS.md` deliberately: that file is the
corpus of instances, this one is the model they are instances of. Read this
first, then use `python3 tools/learn.py <terms>` to pull the specific entries
the model does not settle.

---

## 1. Register assignment has three decision stages

First identify which stage owns the mismatch:

1. **Local allocation** groups eligible pseudos into quantities. Suggestions,
   tied operands, live intervals, register classes and priority determine homes.
2. **Global allocation** handles the remaining allocnos, considering priorities,
   preferences, conflicts, already-used registers and caller-save eligibility.
3. **Reload** reserves scratch registers, evicts existing homes and can retry
   global allocation. Constants or memory equivalents can have no permanent home.

One useful global-allocation rule is:

> GCC's `global_alloc` orders allocnos by `floor_log2(n_refs) * n_refs /
> live_length`. Several early `return ret;` statements inside one branch add a
> reference each, which can push the return variable ahead of another
> whole-function local and swap their registers.

Three consequences that together account for most `regs=N` mismatches:

- **Reference count is a lever.** Adding or removing a mention of a variable —
  an early return, a redundant read, splitting one variable into two — changes
  its priority and can swap two registers. In the cited case rewriting a branch
  as nested `if`s dropped `ret` from 20 references to 17 and flipped the pair,
  while emitting identical instructions.
- **RTL live length is the other lever.** Moving a surviving definition or use
  can alter it; narrowing lexical C scope alone need not. Scheduling recounts
  liveness, and equivalent constants can have their global lifetime doubled.
- **Check suggestions and conflicts before changing rank.** Named temporaries
  can disappear during optimization. Explicit register asm changes the RTL and
  its hard-register constraints; it does not safely express allocator intent.

**Diagnostic:** `cc1 <flags> -dl -dg` writes `.lreg` / `.greg`; the `.lreg`
header prints how many times each register is used. Comparing that between your
build and a shape you expect is faster than guessing. 19 corpus entries work at
this level; 27 use RTL dumps generally.

**A value crossing a call needs preservation.** It may get a callee-saved
register, use caller-save code, spill, or be reconstructed from an equivalence.
Call crossing constrains the choice; it does not force `$s*`.

---

## 2. Scheduling uses RTL dependencies, priorities and order

The scheduler ranks ready instructions by priority, then their dependency
relationship to the last scheduled instruction, then original RTL order
(`sched.c:rank_for_schedule`). It schedules backward; the comparison's preferred
instruction is not necessarily the next one in final forward assembly. Only
instructions admitted by the dependency graph can compete in the ready queue.

That comparator does **not** make the final selection. `schedule_select` scans
equal-priority groups, queues instructions blocked by `actual_hazard`, then
prefers the survivor with the largest `potential_hazard` weight. A lower-ranked
instruction can therefore be selected. Potential weights are ranking values,
not latency cycles. Repeated stores can keep a ready load blocked on successive
cycles; changing its priority or original order cannot remove that hazard.

Trace a misplaced instruction through three decisions: its last dependency is
released in `schedule_insn`, its ready-list ranking, and hazard selection. In
sched1, `adjust_priority` can promote a newly ready register definition to
`LAUNCH_PRIORITY` through `birthing_insn_p`; promotion alone does not guarantee
selection. Replay's separate-top-coordinate counterfactual got the predicted
promotion, but the green load won on potential hazard. It also changed the
top coordinate to a local quantity in v1 and left the reused height in a3.
Scheduling and allocation requirements must be satisfied together.

Check block membership even when only reordering C statements. Replay's clut
expression expands signed division by four into a sign branch. Moving its
store before the flag assignment moves the shift to the following RTL block;
that changes the scheduler's available region and removes dependencies from
the preceding block. It improves one alternate, but transfers poorly to the
primary. A comparator model restricted to the old block cannot predict this.

Sched1 changes lifetimes **before** allocation. Sched2 works with hard-register
conflicts **after** reload and post-reload CSE. Reordering C has an effect only
if it changes the relevant RTL, dependency graph or tie order. Replay UID 370
wins on priority in sched1 but loses an original-order tie in sched2 (§12).

Branch delay filling happens in `reorg.c` (`.dbr`). MIPS final output and maspsx
also participate in load-delay handling and expansion. Compare `.sched`,
`.greg`, `.sched2`, `.dbr`, emitted `.s` and assembled `.o` to locate the stage
responsible for a misplaced instruction or nop. A missing nop has several
possible causes; C statement order alone cannot identify one.

---

## 3. Load width is decided at the load site, by the declared type

68 sections, and the mechanism is one line: `byte` is `typedef signed char` in
`include/decomp/types.h`, so reading a `byte` field into anything wider emits
`lb`. The target's `lbu` means the value was zero-extended, so cast at the load:

```c
val = (u8)ptr->field_4;   /* lbu */
val = ptr->field_4;       /* lb  */
```

Generalised: **the cast belongs at the point of load, not on the struct field.**
Changing the field's declared type will match locally and break every other
reader of that struct — a scratch env that invents `unsigned char field_4`
passes its own build and fails the real one. The same holds for `lhu` vs `lh`
via `(u16)`, and for `.h.hi` vs `.w >> 16` on packed fixed-point types.

A signed halfword that is both compared as SI and stored into a `u16` field
is two loads (`lh` + `lhu`) unless the value is first widened to `s32`. The
HI copy of an `s16` local still wants `movhi`/`lhu` for the store; the SI
temp lets `sh` truncate the same register the compares used.

---

## 4. The compiler will merge anything it can prove identical

217 sections touch cse or the jump pass. Two shapes recur:

- **Cross-jumping** collapses two identical tails into one. If the target keeps
  them separate and you get a branch to a shared tail, the two arms must differ
  in some way you have not reproduced — or you need a barrier to stop the merge.
- **CSE** can reuse a repeated subexpression, changing pressure and lifetimes.
  A shared constant can occupy a saved register or be rematerialized by reload.
  Inspect the equivalence and allocation before deciding whether to split it.

C spelling alone does not determine reuse: different temporaries can collapse,
and identical expressions can require separate loads after invalidation. Read
the MEM mode, address and flags and identify which optimizer performs the
reuse. Ordinary CSE and post-reload CSE have different invalidation rules (§11).

---

## 5. Control-flow shape is inferred from the dispatch, not the source

Switches and jump tables provide source-shape hypotheses. The following corpus
patterns are not unique reconstructions of the original C:

- A two-arm dispatch that falls through to a shared tail is a `switch`, not
  `if / else if`.
- A two-case switch may drop the `slti` range check entirely.
- An explicit `default:` changes what the delay slot can hold — 26 sections
  mention needing one.
- A shared `return 0` reached by `break` is not the same as an early return.

---

## 6. Generated rodata must live where the linker expects it

This is structural rather than codegen, and it is the class that wastes the most
work because the C can be perfect and still not land.

- A compiler-generated jump table is emitted into **its own object's**
  `.rodata`, and a unit's `.rodata` appears once in the linker script at the
  offset its subsegment names. So a function can only be decompiled in the unit
  that owns the block its table sits in.
- It must also **start** that block. GCC emits `.align 3` before a generated
  table, so a table sitting behind other data gets padded to the wrong address.
  Measured across the tree: 580 tables are correctly owned but not at the start
  of their block.
- `tools/rodata_triage.py <overlay>` reports both conditions;
  `tools/rodata_cut.py <overlay> --apply` fixes ownership.

Symptom to recognise early: a match that reaches 100.00% with all-zero penalties
in the scratch env and still fails the scoped build.

---

## 7. Loop shape decides the compare, and the counter's signedness decides it more

78 sections concern loops, and the recurring failure is not the body but the
*exit test*. A plain `for (i = 0; i < N; i++)` with a signed `i` often
strength-reduces to a countdown (`li v1, N-1` … `bgez`), which cannot match a
target that counts up. The lever is the counter's type and the loop form:

```c
u32 i = 0;
do { *ptr = 0; i++; ptr++; } while (i < 0x15U);   /* sltiu / bnez, counts up */
```

`u32` with `< N` yields `sltiu`; signed yields `slti` and invites the countdown.
Generalising the cluster:

- **`do`/`while` is the default shape**, not `for`. GCC 2.8.1 emits a
  bottom-tested loop; a `for` that must execute at least once compiles the same
  way, but writing `do`/`while` removes the guard the compiler would otherwise
  have to prove away.
- **`while (1)` with an internal break** is the shape for walks that re-enter at
  a null check — a linked-list traversal whose first test is the same as its
  last.
- **The induction variable competes for a register like anything else** (§1), so
  zeroing an index early or splitting one counter into two changes which
  register it gets. Several entries are register fixes wearing loop clothing.

## 8. Reading m2c output: what is real and what is an artifact

Only 8 titles name m2c, but the mistakes are systematic and they cost whole
attempts, because the seed *looks* authoritative.

- **A no-argument call is usually still a call with arguments.** m2c prints
  `f();` whenever the `jal`'s delay slot is a `nop`, because nothing visibly
  loads `$a0` — GCC knew `$a0` already held the value and emitted no `move`.
  Typing it as zero-argument is not merely wrong, it becomes impossible once the
  callee is prototyped in the same TU.
- **`func(0)` from a delay-slot `addu a1, zero, zero` is `func(arg0, 0)`** —
  same cause, one argument along.
- **`argN` names encode the register slot**, not the source order, so a dropped
  leading parameter shifts every name.
- **Do not hoist m2c's `temp_` variables reflexively.** A `temp_` that crosses a
  call is load-bearing (§1); one confined to a basic block is usually m2c
  spelling out something GCC would CSE, and keeping it pins the wrong operand.
- **`goto block_N` for a cross-jumped tail merges in the wrong direction** —
  m2c reconstructs the merged shape, not the original two arms (§4).

## 9. Struct typing: the layout is evidence, the name is not

63 titles concern struct inference, and the corpus is emphatic on one point that
no build can check: **a 100% checksum does not validate a type name.** Two
agents have reached 100% with the wrong struct because the spellings compiled
identically. Argue types from behaviour, allocation sites and callers.

What the codegen does constrain:

- **A mid-struct base is visible.** If the target keeps `arg + N` in a
  callee-saved register and loads at small offsets from it, plain field access
  from the struct base emits `move s0, a0` plus larger offsets. Same semantics,
  different code — take a typed pointer to the sub-object.
- **Element stride pins the struct size.** `i * 36` means the element is exactly
  0x24 bytes; that is a hard constraint, unlike a field's name.
- **The allocator pins the block size.** A literal `Mem_Calloc`/`Mem_Malloc`
  argument gives the work struct's size directly — grep `-B14 'jal.*Mem_'` over
  the overlay's asm.
- **Prefer a cast at the use over changing a field's type** (§3), because the
  struct is shared and the build that would catch the damage is not the one you
  are running.

---


---

## 10. Register allocation, mechanically

§1 separates local allocation, global allocation and reload. This section says what the sort keys
are, which of the two allocators runs first, what each one is allowed to take,
and how to read the answer off `.lreg` / `.greg`. It is derived from the compiler source (`local/gcc/gcc-2.8.1-psx/`:
`local-alloc.c`, `global.c`, `regclass.c`, `flow.c`, `config/mips/mips.h`) and
checked with probe compiles under `dump.sh`'s exact flags. Reload was subsequently
traced on the UI and HUD candidates (§11). Historical examples illustrate
mechanisms; current reproducible observations are linked in §12.

The flags that matter are all implied by `-O2` (`toplev.c`): `-fschedule-insns`
(sched1 runs *before* allocation), `-fcaller-saves`, `-fomit-frame-pointer`
(so `$fp` is a ninth callee-saved register), `-fexpensive-optimizations`.
`register` declarations do not steer allocation at any `-O` (`obey_regdecls` is cleared).

### 10.1 The pass order, and the three numbers every decision uses

```
flow → combine → sched1 → regclass → local_alloc → .lreg
     → global_alloc → reload → .greg
     → reload_cse_regs → prologue/epilogue RTL → sched2 → .sched2
     → jump2 → dbr → MIPS final output → maspsx → assembler
```

`flow` computes, per pseudo, the inputs both allocators sort on:

| name | printed in `.lreg` as | what counts |
|---|---|---|
| `REG_N_REFS` | `used R times` | every mention as a set *or* a use, weighted by loop depth: 1 outside loops, 2 inside one, 3 nested |
| `REG_LIVE_LENGTH` | `across L insns` | insns in which the pseudo is live, **recounted by sched1** after it reorders each block |
| `REG_N_CALLS_CROSSED` | `crosses C calls` | `CALL_INSN`s the pseudo is live across |
| `REG_BASIC_BLOCK` | `in block B` (absent = spans blocks) | |
| `REG_N_DEATHS` | `dies in D places` (absent = exactly 1) | number of `REG_DEAD` notes |

`.flow` prints the same lines before sched1; diffing the two shows what
scheduling did to a live range. Two adjustments happen after that, both inside
`local_alloc` (`update_equiv_regs`), so `.lreg` already reflects them and
`global_alloc` sees exactly the `.lreg` numbers:

- **An eligible initializing `REG_EQUIV` note doubles nonnegative live length.**
  `update_equiv_regs` has eligibility checks; inspect the actual note and
  statistics rather than assuming every parameter or constant qualifies.
  This includes register parameters (the arrival copy carries `REG_EQUIV (mem
  arg-slot)`: probe `p` went from `across 11` in `.flow` to `across 22` in
  `.lreg`), every `%hi(sym)` pseudo, and a single-block load from memory that
  nothing stores to before the pseudo dies. A plain `x = 0x12345;` gets no
  such note and is not doubled. The source comments that this "does not affect
  the priority in local-alloc" - it only demotes the pseudo in `global_alloc`.
- **A set-once pseudo with two references whose use is in another block is
  substituted into the use, or its defining insn is moved to just before the
  use** (outside loops, when the definition is not a call). Hoisting a
  single-use constant or address to the top of the function therefore does
  nothing: it comes back down and becomes block-local.

### 10.2 There is no `REG_ALLOC_ORDER` on this target

`config/mips/mips.h` defines neither `REG_ALLOC_ORDER` nor
`ORDER_REGS_FOR_LOCAL_ALLOC`. Both allocators walk hard registers **0..75 in
numeric order** and take the first one that is in class, free over the range,
and not excluded. What is excluded:

| value | local-alloc may use | global-alloc may use |
|---|---|---|
| crosses no call | 2-15, then **16-23**, then 24-25 (`$v0 $v1 $a0-$a3 $t0-$t7 $s0-$s7 $t8 $t9`) | pass 0: 2-15, 24-25 and any `$s` already in use; pass 1: 16-23 too |
| crosses a call | 16-23 only (`$fp` is eliminable, never local) | 16-23, then 30 (`$fp`) |

Fixed registers (`$zero $at $k0 $k1 $gp $sp $ra`) are never candidates. So the
corpus's "MIPS `REG_ALLOC_ORDER` hands out `$a3` before `$t0`" is true only
because 7 < 8. Two consequences that are not obvious from the names:

- In local-alloc a call-free temp lands in `$s0` before it lands in `$t8`, if
  `$v0`-`$t7` are all busy over its span (source; not isolated by a probe).
- In global-alloc the order is the other way round for a call-free allocno,
  because pass 0 only considers registers already used and every call-used
  register counts as used from the start: probe 10 put two `mult` results in
  `$t9`/`$t8` while `$s2`-`$s7` were untouched.

### 10.3 local-alloc: quantities, suggestions, then priority

**Eligibility.** A pseudo is local-allocated iff it appears in exactly one
block **and dies exactly once** and its preferred class is not a one-register
class. `dies in 0 places` or `dies in 2 places` in `.lreg` means the pseudo
goes to `global_alloc` even though it is `in block N` - a loop bound that is
still live when the block ends, or a value used in both arms of a branch. A
`mult` result (`pref LO_REG`) is always global for the class reason.

**Quantities.** Local-alloc does not colour pseudos, it colours *quantities*.
When a pseudo dies in an insn whose output is another eligible pseudo - a copy,
or a two-operand op - the output joins the dying input's quantity. The
quantity's refs are the **sum** and its span is the **union**. This is what
`.lreg`'s per-pseudo lines cannot show, and it decides a lot:

- `t = a * 7` before `f1(t); f3(t)` compiles to `sll $s0,$a0,3; subu
  $s0,$s0,$a0`: the shift temp died producing `t`, so it is *in* `t`'s
  call-crossing quantity and is computed straight into the callee-saved
  register (probe 3). There is no `move`.
- **The first-listed operand that dies is the one the result is tied to.**
  With `x = a[0]` (lbu) and `y = b[0]` (lw) both dying in `z = x + y`, the
  whole two-address chain `z <<= 2; z += 7;` lives in the `lbu`'s register;
  write `z = y + x` and it lives in the `lw`'s register (probe 9b:
  `lbu $2 / lw $3` versus `lbu $3 / lw $2`, nothing else changes). This is the
  lever for the "which of two loads owns `$v0`" swap, and it is invisible in
  the statistics.

**Order of allocation.** Two sorted passes over the block's quantities:

1. **Quantities with a suggested hard register go first**, and only try their
   suggestion. A suggestion is recorded when a quantity is tied to a hard
   register by a plain copy (`(set (reg N) (reg $a0))` at parameter arrival,
   `(set (reg $v0) (reg N))` at a return, `(set (reg $a1) (reg N))` at a call)
   - a *copy* suggestion - or when it is an operand of an op whose output is a
   hard register (`return t1 + t2;`, `f(a + b)`) - an *arithmetic*
   suggestion. Copy-suggested quantities go before arithmetic-suggested ones,
   fewer suggestions before more, and then by priority. In probe 1 `w->d`
   took `$a0` because it is an operand of the `addu` that produces `sink`'s
   argument; `w->c`, the other operand, tried `$a0` too, lost, and fell
   through to pass 2. This is the answer to "why is this temp in `$aN`".
2. **Everything else by priority**, highest first:

   ```
   QTY_CMP_PRI = floor_log2(refs) * refs * size / (death - birth)
   ```

   the *same* formula and direction as `global_alloc` (the source comment says
   so explicitly: "This is the identical prioritization as done by
   global-alloc"). `death - birth` is measured on the block's insn sequence
   after sched1, in half-insn units (`2 * insn_number`, clobbers at `2n - 1`,
   an output that is never read dies at `2n + 1`). The counter includes every
   non-NOTE node, including a leading CODE_LABEL. Exact ties are common in
   small blocks and go to the **lower quantity number, i.e. the one born
   first**. Each quantity takes the lowest numeric free register (§10.2) over
   its span.

The corpus entries that state this as `live_length / n_refs` ascending, or as
"the inverse of global_alloc", are wrong on both counts: the `floor_log2`
factor is there, and shorter-per-weighted-reference wins in both allocators. Both keys are
truncated integers after scaling by 10000; two unequal ratios can still tie.

**Worked: `$v0`/`$v1` flipping when a store is added (probe 2, 2c).**

```c
int t1 = p[0] * 3;   int t2 = p[1] - 7;                  return t1 + t2;   /* t2 in $v0 */
int t1 = p[0] * 3;   int t2 = p[1] - 7;   p[2] = t1;     return t1 + t2;   /* t1 in $v0 */
```

Both `t1` and `t2` get an arithmetic suggestion for `$v0` from the return, so
the order between them is the priority. `t1` is tied to the `p[0] << 1` temp
that dies producing it, `t2` to the `p[1]` load. Without the store: `t1`
quantity 4 refs over 8 half-insns = 1.0, `t2` 4 over 6 = 1.33, `t2` first.
With the store: `t1` 5 refs over 10 = 1.0, `t2` still 4 refs but now over 8
(the store sits inside its span) = 1.0 - a tie, and `t1`'s quantity was born
first. The per-pseudo `.lreg` lines (`t1` 3/4, `t2` 2/2) would have predicted
the opposite; only the tied quantities explain it. Note the general point:
**an insn inserted anywhere inside another value's range lengthens that range
by one**, whether or not it mentions the value.

**Register choice.** Call-free: lowest free of 2..25. Call-crossing: lowest
free of `$s0`-`$s7`; two disjoint call-crossing quantities in different blocks
both get `$s0` (probe 3: `t` in one arm and `r = f2()` after the join share
`$16`). If no `$s` is free over the span and `4 * calls < refs`, the quantity
takes a call-clobbered register and caller-save code is emitted around each
call (same rule as §10.4); otherwise local allocation leaves it unassigned for global allocation and
reload to handle.

### 10.4 global-alloc: rank, two passes, preferences, and what "spill" means

**Rank.** `;; N regs to allocate: ...` in `.greg` is the order, computed as

```
pri = floor_log2(n_refs) * n_refs / live_length * 10000 * size        (int)
```

descending, ties to the lower **allocno number**. Usually each allocno contains
one pseudo, but `reg_may_share` can merge them: refs and call counts are summed,
live length is the maximum, and the printed representative is the last member.
Use the actual allocno membership when reconstructing this sort. Pseudo numbers follow creation
order: parameters first, user locals in declaration order, compiler temporaries
(givs, CSE copies) after. Probe 1's header `95 108 84 83 85 81 80 82` is
exactly `2.00 1.75 1.13 1.00 0.47 0.43 0.27 0.09` from the `.lreg` lines.
Because parameters have their length doubled (§10.1) they usually rank last.

**Conflicts.** `;; R conflicts: <pseudos> <hard regs>` lists the pseudos live
at the same time and the hard registers R can never take: explicit hard-reg
uses *and every register local-alloc handed out over R's range*. That second
part is how a block-local temp bumps a whole-function value off `$v0`. The
list is printed before allocation starts; as allocnos are placed, their
registers are added to the conflict sets of everything they overlap, which the
dump does not show - you have to walk the order and keep score.

**`find_reg`, for each allocno in rank order:**

1. Candidates = in class, not fixed, not call-used if the allocno crosses a
   call, not in its hard-conflict set.
2. **Pass 0** also drops registers that are not yet in use (all call-used
   registers count as in use from the start; `$s` registers only once
   something has been placed in them) and registers that a *lower-ranked
   conflicting* allocno prefers. **Pass 1** drops only the conflicts. Take the
   lowest-numbered candidate as `best_reg`.
3. **Copy preference** overrides it: if the allocno is copied to or from a
   hard register (parameter arrival, argument setup, return) and that register
   is still a candidate, take it instead. `;; R preferences:` lists them.
4. **Arithmetic preference** next: the register of the *first operand* of the
   expression that computes the allocno - where a locally-allocated pseudo's
   register counts as a hard register. Probe 7: `d = p[0] + 1` with the load
   local in `$v1` gave `d` a preference for 3 and `d` landed in `$v1`,
   two-address style. Preferences also flow along pseudo-to-pseudo copies whose
   two sides do not conflict (`a = b;` where `b` dies).
5. Nothing left and the allocno crosses calls: if `4 * calls < refs`
   (`CALLER_SAVE_PROFITABLE`), retry allowing call-clobbered registers. The
   value then lives in a `$t`/`$a`/`$v` register and `caller-save.c` wraps
   **every call it crosses** with `sw`/`lw` to a dedicated stack slot. This is
   real in this codebase: `func_acropolis_roof_garden_8017E29C` keeps five
   such values in `$a3 $t0 $t1 $t2 $t4`, saved to `0x30..0x40($sp)` around
   each `jal Gp_AddTpageShift`, in both the target and the seed.
6. Still nothing: if some register holds only local-alloc pseudos whose summed
   `refs / length` is lower than this allocno's, evict them (scanning from
   `$t9` downward) and take it. (Source only; no probe.)
7. Otherwise the allocno gets no register. Reload may substitute a constant or
   memory equivalence, or assign a stack slot. Stack accesses use a scratch register (`$t8`,
   `$t7` in the example below). There is no cost comparison between spill
   candidates: **whoever ranks low enough to find nothing free is the one
   spilled**, and a call-crossing value with `refs <= 4 * calls` cannot even
   fall back to caller-save.

The `$fp` = `$s8` case follows from 1: it is the ninth callee-saved candidate,
after `$s7`, and only global-alloc can give it out.

### 10.5 Reading a dump: locate the decision before choosing a lever

1. Identify the value by its defining expression and UID. Pseudo IDs can change
   between source variants; do not compare equal numbers without checking meaning.
2. Inspect local allocation first. The `.lreg` header is per pseudo, while
   priority uses **tied quantities** and half-instruction birth/death positions.
   `lregwalk.py` helps inspect possible ties; `trace_gcc.py` observes the actual
   members, suggestions, intervals and each allocation attempt.
3. For a global value, use the actual allocno order, membership and conflicts.
   A preferred register may be unavailable. Caller-save profitability permits
   a retry; it does not guarantee the retry can find a register.
4. Compare the initial homes with reload's evictions and the final `.greg`
   dispositions. The global order and final dispositions in that same dump are
   observations from different moments.
5. If a load survives `.greg` but changes in `.sched2`, inspect
   `reload_cse_regs` before attributing the change to scheduling.
6. Record a prediction in terms of a specific quantity, conflict, substitution
   or scheduling comparison, then check both the intermediate decision and
   final assembly.

Per-pseudo priority correlations do not establish how often suggestions win.
They omit quantity grouping, eligibility, conflicts and later reload. The old
66%/38% measurements were not a validated explanation of those mechanisms.

### 10.6 Levers: what moves an allocation and what is folklore

Real, in rough order of how often it is the answer:

| lever | mechanism | typical form |
|---|---|---|
| weighted reference count | both priorities | add or drop a mention; a mention in a loop counts ×2, nested ×3 |
| live length | both priorities | move the definition later or the last use earlier; note that *any* inserted insn between another value's birth and death lengthens that value too (probe 2c) |
| where it dies | eligibility | a value that dies at a block boundary (`dies in 0 places`) is global; give it a last use in the block and it becomes local, with a different register pool |
| operand order of a two-operand op whose operands both die | tying (local) / arithmetic preference (global) | `y + x` instead of `x + y` decides which input's register the result inherits (probe 9b) |
| whether it crosses a call, and `refs` vs `4 * calls` | pool selection, caller-save, spill | move the computation or the last read across the `jal`; one extra reference turns a spilled value into a caller-saved one |
| one variable vs two | can change pseudo/quantity/allocno grouping; global shared allocnos sum refs and take maximum member length | split a reused local, or merge two into one to carry a preference between blocks (both in the corpus) |
| passing straight through a hard register | suggestion (local) / copy preference (global) | return `x` or pass it as an argument unchanged and it keeps `$v0`/`$aN` if the register is free over its range |
| statement order | through expansion, dependencies, operand tying, scheduling and liveness | works when the moved statement is a load or store sched1 keeps in place (probe 8); the corpus has cases where sched1 re-sorts everything back |

Folklore, tested:

- **Declaration order** can change pseudo creation order and global tie-breaks;
  it does not directly specify local quantity birth order. Probe 8 swapped `int a, b;` for
  `int b, a;` and produced identical code; swapping the two *statements* that
  load them flipped the registers, because the live lengths flipped.
- **An unused local** (`int unused = 3;`) is deleted by flow before either
  allocator runs.
- **`register`** does not steer allocation at `-O` (`obey_regdecls` is cleared).
- **Renaming** anything has no effect; only the RTL shape and the counts
  matter.
- **Explicit register asm** creates hard-register RTL. Function-local declarations
  do **not** call `globalize_reg`; only top-level register declarations do
  (`varasm.c:make_decl_rtl`). Model their actual uses/conflicts, not a blanket
  function-wide reservation. Eliminable `fp` is particularly problematic:
  `global_alloc` clears its conflicts, and the saved Replay pin probe allowed
  another live value into the same register. Keep match candidates unpinned.

### 10.7 Historical example: `func_acropolis_roof_garden_8017E29C` at 99.416%

The archived seed (`tools/giveups/.../base_3.c`) rebuilt in a scratch
directory reproduces the score exactly: `regs=84 reorder=2`, everything else 0.
The diff is four independent register-pair swaps, and every one of them has a
number attached in the dumps:

| swap (ours → target) | pseudos | what the dumps say | lever implied |
|---|---|---|---|
| `$t2`/`$t4` | `r428` = `%hi(Gpu_PrimCursor)` 13 refs/426, `r531` = `&Display_State` 11/414, both cross 2 calls | both are **caller-save** allocations (all of `$s0-$s7`,`$fp` are taken); `3·13/426 = 0.092 > 3·11/414 = 0.080` places `r428` first | the original ranked the `Display_State` address above the `%hi`: two more weighted mentions of the former, or one fewer `Gpu_PrimCursor` access through the same `%hi` pseudo, cross the threshold |
| `$s3`/`$s4` (local, block 8) | quantity {`r435` xor, `r437` mult result} 6+9 refs over 236 half-insns; quantity {`r550` = `0xff000000`} 13 refs over 220 | `3·15/236 = 0.191 > 3·13/220 = 0.177`, xor-quantity first | in the original the mask constant outranked the xor/mult chain: one more use of the mask in that block, or the xor operand not dying at the `mult` (which unties the chain and halves its refs) |
| `$s1`/`$s2` (local, block 8) | address quantities {`r444`,`r445`} 17 refs / 222 and {`r505`,`r506`} 16 refs / 210 | `int(4·17/222·10⁴) = 3063` vs `3047`: a 16-unit margin | one extra mention of the second pointer, or one fewer of the first, reverses it |
| `$v0`/`$v1` (local, block 8) | `lbu` result `r533` versus `lw` result `r534`, both dying in the op at `#80` | `r533` is operand 1, so the seven-insn result chain is tied to it and outranks the lone `lw` | write the `lw` value as the first operand of that op (probe 9b) |

Also visible, and worth knowing before touching anything: `r80` (the task
pointer, 7 refs over 7 calls), `r93`, `r421`, `r432`, `r433` have **no
disposition** - each has `refs <= 4 · calls`, so they were spilled to the
stack (`lhu $t8,32($sp)` feeds the xor) rather than caller-saved. `r421`
misses the threshold by one weighted reference. The target spills the same
values, so the seed's overall pressure is right; the four swaps are rank
inversions, not structure. Which source edit pulls exactly one lever without
disturbing the other three is the part this section does not do for you.

### 10.8 Limits of allocation reconstruction

The tracer observes actual allocation calls and results. Its displayed priority
is calculated from observed inputs; it is not a full simulator of every
machine constraint, global eviction or reload alternative. The bundled i386
compiler uses floating-point division in the priority calculation; do not infer
an unobserved exact tie solely from rounded printed ratios.

An interval measured in local half-instruction positions is different from
`REG_LIVE_LENGTH`. Constants' doubled global lengths must not be substituted
into local quantity ranking. See the HUD example in the analysis report.

## 11. Reload, aliasing, and empty asm

### Reload is a second allocation problem

`reload1.c:order_regs_for_reload` first considers unused registers, preferring
call-clobbered ones. Used registers are ordered by weighted use cost. A local
pseudo contributes `refs + (refs + 1) / 2` (integer division), since it cannot
be relocated as easily; a global pseudo contributes `refs`. Equal costs use
numeric hard-register order. Fixed, explicit and eliminable registers receive
additional penalties/exclusions. Register classes still constrain selection.

`spill_hard_reg` can evict pseudos and call `retry_global_alloc`; some block-local
homes survive if their block needs no spill registers. `REG_EQUIV` values may
be reconstructed without stack storage. In the HUD, lower-Y's changed **local**
priority puts it in t6 instead of t7; reload then reserves that register. The
final scratch swap is explained by both decisions, not a scratch-name preference.

### A late load replacement has its own pass

After the `.greg` dump, `reload_cse_regs` tracks known hard-register values.
`reload_cse_simplify_set` can replace a memory load with a register copy;
`reload_cse_simplify_operands` can replace individual operands. Labels and calls
invalidate knowledge; memory stores use the dependence checks in `sched.c`.

This is observed in UI candidate base_35: UID 1228 is still an unsigned byte
load at `.greg`, then becomes a copy of a0 before sched2. If the target needs
the reload, investigate why an equivalent value is still available there.

### Different passes have different memory knowledge

Ordinary CSE's `note_mem_written` / `invalidate_memory` conservatively
invalidate categories. A varying-address QI store can invalidate all memory
expressions. Scheduler/post-reload dependence analysis also reasons about
specific addresses, offsets, access widths, MEM_IN_STRUCT and volatility.
Do not apply ordinary CSE's category rule to every later optimization.

In `sched.c:true_dependence` / `anti_dependence`, a varying **non-byte**
structure access can be disambiguated from a fixed scalar access. A QI
structure access does not get that exemption. Changing a fixed brightness
read from scalar MEM to member MEM/s changes eligibility for this exemption.
Trace the particular access pair: marking one read volatile is not a universal
fence, and changing every field's type can introduce other dependencies.

### Empty asm is not uniformly soft

`stmt.c:expand_asm_operands` sets `vol = 1` whenever there are no outputs.
Thus input-only `SOFT_USE_REG` is implicitly volatile in this compiler despite
the macro spelling. Basic empty asm is also a scheduling boundary. A read/write
`SOFT_TOUCH_REG` has an output and avoids that particular rule, but can still
change copies, lifetimes and dependencies. Inspect the expanded RTL.

Extra empty nodes can change allocation while emitting no machine instructions.
Replay confirms this threshold effect. Such padding is diagnostic evidence;
a final source should explain the needed lifetime naturally.

## 12. Reproduce and observe a compiler decision

From the project root, after `dump.sh` has produced a preprocessed input:

```sh
python3 tools/trace_gcc.py nonmatchings/FUNC/base_N.i \
  --output-dir /tmp/gcc-observation --function FUNC --regs 138 425 \
  --uids 1228
```

Use a new output directory. Archived `.i.gz` inputs work directly and retain
the original header contents. The tool uses the bundled compiler, standard
scratch flags, GDB and readelf. GDB requires ptrace access; a sandbox may require
permission. It reads compiler state without changing it or calling functions
inside the inferior, and requires byte-identical emitted assembly from the
ordinary and observed compiles.

The output contains a compiler/input/source fingerprint manifest, complete
allocation events, selected scheduler comparisons, dependency releases, hazard
selection, post-reload substitutions,
RTL dumps and a filtered text report. `--regs` filters the report, not evidence;
`--uids` selects comparisons, releases involving those UIDs, and complete
ready-list/hazard decisions when one of them is ready; it also filters late
substitutions. UIDs must be taken from this compilation's dumps. Release events
show remaining dependency counts, priority changes and earliest issue ticks.
Selection events show actual blocking costs and potential ranking weights.
Cycles run backward and restart in each block. Consult the RTL dependency links
for the full graph; a UID absent from a ready list cannot win its comparison.

Only the audited cc1 hash is supported. A replacement binary needs an ABI/layout
audit before extending the profile. Source hashes record what was inspected;
they do not independently prove the provenance of the prebuilt compiler.

See [COMPILER_ANALYSIS.md](COMPILER_ANALYSIS.md) and
[the retained observations](tools/compiler_evidence/2026-09-09.json) for the
UI reload substitution, HUD quantity/reload chain and Replay's 909/911 boundary.

## Diagnostic ladder

Use exact penalties to locate differences, structural diagnostics to assess
control flow, and RTL/trace evidence to identify the responsible pass.

| observation | investigate |
|---|---|
| Different block connections or predicates | source control flow and jump-table interpretation |
| Register differences | local quantities/suggestions → global conflicts/preferences → reload |
| Instruction order differences | dependencies and priorities in sched1/sched2, then dbr/final output |
| Load present in greg, replaced in sched2 | post-reload CSE |
| Extra moves, loads, nops or shifted branch addresses | promotion, spill/rematerialization, scheduling, assembler expansion |
| Structurally eligible plateau | bounded permutation search with several distinct candidate outputs |
| Permuter improves generated code | preserve paired inputs → isolate the source delta → inspect RTL/trace → predict a controlled variation |
| Exact scratch score, failed integration | real headers/ABI, relocations, rodata ownership/alignment, build inputs |

Matching graph structure does not prove semantic equivalence. Raw branch and
insertion/deletion penalties alone do not establish changed control flow or
exclude a useful permutation search. Use the search router's recorded reasons.

Permuter discoveries are evidence for extending this model. The router retains
full-context outputs and verifies its best output against a paired baseline;
partial improvements matter too. Follow the bounded investigation in
`tools/claude-decomp-env/MATCH_LOOP.md`. A score gain establishes a changed result,
not its mechanism. Only promote a general rule after observing the relevant
compiler decision and checking a prediction on a controlled variation. Record
compiler/input hashes, scope and contrary observations. Keep unresolved cases
in session notes and `tools/permuter_findings/`, which survives match cleanup.

---

## What does not generalise

The corpus keeps its value for the residue, and the residue is real: handwritten
stubs that fall through into the next function on an incoming register, a dead
`lui/ori` in an entry block, big-endian halfwords assembled from two `u8`
stores, `%hi/%lo(sym+8)` vs `%hi/%lo(sym2)` assembling to identical words. These
are not instances of a rule; grep for them.

The split is the point: this file should stay short enough to read, and anything
that only ever happens once belongs in the corpus instead.
