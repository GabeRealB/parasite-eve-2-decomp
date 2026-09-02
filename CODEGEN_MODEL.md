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
