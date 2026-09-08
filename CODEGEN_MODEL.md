# A working model of GCC 2.8.1 for this decomp

`DECOMPILATION_LEARNINGS.md` is 1,275 sections and 1.29 MB, and the match prompt
tells agents to grep it rather than read it. That is the right way to use a
corpus of instances, but it means the *model* underneath never gets stated: 90
sections restate "the value crosses a call", 68 restate "cast to force `lbu`",
53 restate "a temp pins the operand order". This file is an attempt at the
model, so that the corpus is only consulted for the residue the model does not
explain.

It is derived from the corpus, not from the GCC source. Where an entry cites a
pass or a formula I have kept its wording; where I generalised from several
instances I say so. Treat a rule here as a strong prior, not as ground truth —
verify against `build.sh`'s score, which is the only authority.

It sits beside `DECOMPILATION_LEARNINGS.md` deliberately: that file is the
corpus of instances, this one is the model they are instances of. Read this
first, then use `python3 tools/learn.py <terms>` to pull the specific entries
the model does not settle.

---

## 1. Register assignment is a priority sort, not a choice about your code

The entry that explains the rest:

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
- **Live length is the other lever.** Narrowing a variable's scope shortens its
  live range and raises its priority; hoisting an address out of a loop
  lengthens one and lowers it.
- **You cannot name a register, only change its rank.** This is why so many
  entries take the form "introduce a temp so X lands in `$v0`" rather than
  "pin X to `$v0`". A `register … asm("v0")` pin is a last resort and usually
  papers over a rank problem.

**Diagnostic:** `cc1 <flags> -dl -dg` writes `.lreg` / `.greg`; the `.lreg`
header prints how many times each register is used. Comparing that between your
build and a shape you expect is faster than guessing. 19 corpus entries work at
this level; 27 use RTL dumps generally.

**Corollary — a value that crosses a call is callee-saved.** Not a separate
rule, just the live-range rule at its most predictable: if the value is needed
after a `jal`, the allocator must give it `$s*`. So the lever for "target uses
`$s3`, I get `$v1`" is usually *where the value is computed and last read*,
relative to the call.

---

## 2. Delay slots are filled from source order

287 sections touch the delay-slot pass — the largest single cluster.

> GCC 2.8.1 fills the first load-delay with whichever independent op is *first*
> in the source.

So statement order among *independent* operations is a real degree of freedom,
and often the only one that matters. Writing `i++` before `p++` fills the `lw`
delay with `i++` and leaves a `nop` after the `lbu`; swapping them matches.

Practical form:

- If the target has a `nop` you do not, you have too few independent operations
  before the use — move one earlier.
- If the target fills a slot you leave empty, reorder the independent statements
  so the one it uses comes first.
- `asm("")` is the blunt instrument: it stops a specific instruction winning a
  slot. Several entries use it precisely (`asm("")` after a move that must own
  the next `beqz` delay slot). Reach for ordering first.
- The branch delay is filled *last*, from whatever remains, which is why a
  pointer increment so often ends up there.

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

---

## 4. The compiler will merge anything it can prove identical

217 sections touch cse or the jump pass. Two shapes recur:

- **Cross-jumping** collapses two identical tails into one. If the target keeps
  them separate and you get a branch to a shared tail, the two arms must differ
  in some way you have not reproduced — or you need a barrier to stop the merge.
- **CSE** hoists a repeated subexpression into one register, which then needs a
  callee-saved slot and grows the frame. If the target re-materialises a
  constant twice (`li v0,8` in two places) and you emit one `s1`, you have
  written it as one value where the original had two.

The general lever: *identical text in C becomes identical code*. To keep two
things apart, make them genuinely different — different temporaries, different
order — rather than trying to suppress the optimisation.

---

## 5. Control-flow shape is inferred from the dispatch, not the source

218 sections concern switches and jump tables. Recurring recognitions:

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

§1 says allocation is a priority sort. This section says what the sort keys
are, which of the two allocators runs first, what each one is allowed to take,
and how to read the answer off `.lreg` / `.greg`. Unlike the rest of this file
it is derived from the compiler source (`local/gcc/gcc-2.8.1-psx/`:
`local-alloc.c`, `global.c`, `regclass.c`, `flow.c`, `config/mips/mips.h`) and
checked with probe compiles under `dump.sh`'s exact flags. `reload1.c` was not
read; where reload matters it is said in a sentence. Every claim below is
either quoted from the source or reproduced by a probe; the few that are
source-only are marked.

The flags that matter are all implied by `-O2` (`toplev.c`): `-fschedule-insns`
(sched1 runs *before* allocation), `-fcaller-saves`, `-fomit-frame-pointer`
(so `$fp` is a ninth callee-saved register), `-fexpensive-optimizations`.
`register` declarations do not steer allocation at any `-O` (`obey_regdecls` is cleared).

### 10.1 The pass order, and the three numbers every decision uses

```
flow  →  combine  →  sched1  →  regclass  →  local_alloc  ─ .lreg ─▶  global_alloc  →  reload  ─ .greg ─▶  sched2 → jump2 → dbr
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

- **Every pseudo with a `REG_EQUIV` note has its live length doubled.** That
  is every register parameter (the arrival copy carries `REG_EQUIV (mem
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
   an output that is never read dies at `2n + 1`). Exact ties are common in
   small blocks and go to the **lower quantity number, i.e. the one born
   first**. Each quantity takes the lowest numeric free register (§10.2) over
   its span.

The corpus entries that state this as `live_length / n_refs` ascending, or as
"the inverse of global_alloc", are wrong on both counts: the `floor_log2`
factor is there, and shorter-per-reference wins in both allocators.

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
call (same rule as §10.4); otherwise it is left for reload, which puts it on
the stack.

### 10.4 global-alloc: rank, two passes, preferences, and what "spill" means

**Rank.** `;; N regs to allocate: ...` in `.greg` is the order, computed as

```
pri = floor_log2(n_refs) * n_refs / live_length * 10000 * size        (int)
```

descending, ties to the lower pseudo number. Pseudo numbers follow creation
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
7. Otherwise the allocno gets no register, and reload gives it a stack slot;
   every use becomes `lw`/`sw` through a scratch register reload picks (`$t8`,
   `$t7` in the example below). There is no cost comparison between spill
   candidates: **whoever ranks low enough to find nothing free is the one
   spilled**, and a call-crossing value with `refs <= 4 * calls` cannot even
   fall back to caller-save.

The `$fp` = `$s8` case follows from 1: it is the ninth callee-saved candidate,
after `$s7`, and only global-alloc can give it out.

### 10.5 Reading a dump: why did pseudo P land in H

1. Find P in `.lreg`. If there is a `;; Register P in H.` line at the end of
   the statistics, local-alloc placed it; otherwise it is in `.greg`'s
   `;; N regs to allocate:` line and the `;; Register dispositions:` table
   (`P in H`). A pseudo in neither place got no register at all (spilled).
2. **Local.** Look at P's block in the RTL that follows (the insns are in
   sched1 order, which is what local-alloc numbered). Work out P's quantity:
   follow `REG_DEAD` notes into two-operand ops and copies to see what it is
   tied to. Check for a suggestion first (is the quantity copied to/from a
   hard register, or an operand of an op whose output is one?). If not,
   compute `floor_log2(refs) * refs / span` for it and for the quantities it
   overlaps, using summed refs and the union span; the one that ranks higher
   picked first, and each took the lowest numeric free register (§10.2) - the
   registers of quantities already placed, and of global pseudos live across
   the block (`Registers live at start`), are busy.
3. **Global.** P's position in the order line is its rank; `.lreg` gives the
   two inputs. Walk the order line from the front: for each allocno take the
   lowest register not in its `conflicts` list, not call-used if it `crosses
   calls`, not taken by an overlapping allocno placed earlier; then apply its
   `preferences` line if the preferred register is still available. When you
   reach P you have H and the reason.
4. **Wrong H.** The target's assignment tells you the order the original
   source produced. Which rank needs to change is then a matter of arithmetic
   on the two ratios - the same computation the corpus entries under
   `floor_log2` do by hand.

`summarize_dumps.py` already prints rank, `used R/L`, disposition and hard
conflicts per allocno; it does not print quantities or per-block insn
positions, which is the missing piece for local cases. `lregwalk.py`, symlinked
into the scratch env beside it, supplies them: per block, the insns in
local-alloc's own order with their `set` destinations and `REG_DEAD` notes.
That is what step 2 needs - the `REG_DEAD` chain gives the tying, the `set`
destinations give the suggestions, and the position gives birth order for a tie.

    python3 lregwalk.py base_N.i.lreg 111 154     # blocks mentioning those pseudos

**Do step 2 in that order: suggestion, then priority, then birth.** Priority is
the part that is easy to compute and the least often decisive. Measured over 86
`.lreg` dumps from two functions, comparing only pseudos that competed in the
same block, with ties and same-register pairs excluded:

| | |
|---|---|
| pairs the priority ratio orders correctly | 66% |
| pairs at an exact priority tie, where birth order decides | 38% of all pairs |

The disagreement is not noise, and it is not spread evenly. Counting how often
a pseudo took a *lower* register than a higher-priority competitor, per register:

```
$v0  4.8 inversions per assignment      $a3  0.8
$v1  2.3                                $t0  0.9
$a0  1.5                                $t2  0.3
$a1  1.4                                $t3  0.0
```

It decays monotonically to zero by `$t3`. That is the shape of the suggestion
pass, not of a ranking error: `$v0` is the return register and the destination
of most arithmetic results, so it is the most-suggested register in any block by
a wide margin, and a value that is suggested into it takes it before priority is
consulted at all.

The practical consequence is a different first move. To get a value *out* of
`$v0`, break its suggestion - stop it being the direct result or operand of
something whose output is a hard register, usually by giving the expression a
named intermediate or by moving where the value is consumed. Adjusting refs and
live length to lose on priority is the wrong lever there, and it is the lever a
matching agent reaches for first because the ratio is the part the dump prints.
Priority is decisive only once no competitor is suggested.

### 10.6 Levers: what moves an allocation and what is folklore

Real, in rough order of how often it is the answer:

| lever | mechanism | typical form |
|---|---|---|
| weighted reference count | both priorities | add or drop a mention; a mention in a loop counts ×2, nested ×3 |
| live length | both priorities | move the definition later or the last use earlier; note that *any* inserted insn between another value's birth and death lengthens that value too (probe 2c) |
| where it dies | eligibility | a value that dies at a block boundary (`dies in 0 places`) is global; give it a last use in the block and it becomes local, with a different register pool |
| operand order of a two-operand op whose operands both die | tying (local) / arithmetic preference (global) | `y + x` instead of `x + y` decides which input's register the result inherits (probe 9b) |
| whether it crosses a call, and `refs` vs `4 * calls` | pool selection, caller-save, spill | move the computation or the last read across the `jal`; one extra reference turns a spilled value into a caller-saved one |
| one variable vs two | one allocno with summed refs and unioned length vs two ranks; block-local halves go to local-alloc | split a reused local, or merge two into one to carry a preference between blocks (both in the corpus) |
| passing straight through a hard register | suggestion (local) / copy preference (global) | return `x` or pass it as an argument unchanged and it keeps `$v0`/`$aN` if the register is free over its range |
| statement order | only through sched1 changing live lengths | works when the moved statement is a load or store sched1 keeps in place (probe 8); the corpus has cases where sched1 re-sorts everything back |

Folklore, tested:

- **Declaration order** of locals changes pseudo numbers and therefore breaks
  exact priority ties, nothing else. Probe 8 swapped `int a, b;` for
  `int b, a;` and produced identical code; swapping the two *statements* that
  load them flipped the registers, because the live lengths flipped.
- **An unused local** (`int unused = 3;`) is deleted by flow before either
  allocator runs.
- **`register`** does not steer allocation at `-O` (`obey_regdecls` is cleared).
- **Renaming** anything has no effect; only the RTL shape and the counts
  matter.
- **`register x asm("$s1")`** does work, but by reserving `$s1` function-wide
  it edits every other allocno's conflict set, so it moves other registers as
  a side effect. The existing rule stands: pin only when both ends of the
  losing range are fixed by the target and the ratios cannot be changed.

### 10.7 Worked example: `func_acropolis_roof_garden_8017E29C` at 99.416%

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

### 10.8 Where this model is thin

- **reload** was not read. It chooses the scratch registers for spilled
  values, can re-home an allocno after a spill (`retry_global_alloc`), and
  splits `li` into `lui/ori`. If the `.greg` dispositions agree with the
  target and the final assembly does not, the difference is reload's.
- The eviction step (§10.4 item 6) and the "skip registers a lower-ranked
  allocno prefers" rule in pass 0 are from the source only.
- `across L insns` is sched1's recount and can differ by one or two from the
  span you would count by hand; for a tied quantity it is only a per-pseudo
  component. When two ratios are within a few percent, compute from the RTL
  positions, not from the header lines.
- Nothing here changes §1's advice on *when* to reach for the permuter; it
  changes what to try first when the permuter has stopped finding anything.

## Diagnostic ladder

Work from the penalty breakdown `build.sh` prints, not from the diff:

| penalties | most likely mechanism | first lever |
|---|---|---|
| `regs` only | §1 priority sort | change reference count or scope of the mismatched value |
| `reorder` only | §2 delay slots | reorder independent statements |
| `regs`+`reorder`, ≥95% | allocator noise | the permuter — this is what it is for |
| `branch`≠0 | §5 control-flow shape | the C shape is wrong; the permuter cannot help |
| `insert`/`delete`≠0 | missing or extra work | not a codegen problem; re-read the asm |
| 100% in scratch, fails build | §6 rodata placement | `rodata_triage.py` |

The `branch`/`insert`/`delete` row is worth internalising: those three being
zero is the documented precondition for the permuter being any use at all.

---

## What does not generalise

The corpus keeps its value for the residue, and the residue is real: handwritten
stubs that fall through into the next function on an incoming register, a dead
`lui/ori` in an entry block, big-endian halfwords assembled from two `u8`
stores, `%hi/%lo(sym+8)` vs `%hi/%lo(sym2)` assembling to identical words. These
are not instances of a rule; grep for them.

The split is the point: this file should stay short enough to read, and anything
that only ever happens once belongs in the corpus instead.
