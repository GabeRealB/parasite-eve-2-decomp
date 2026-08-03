# Decompilation Learnings

Notes on the GCC 2.8.1 (`-O2 -mips1`, aspsx 2.77) toolchain used by this project.
Each entry was verified against real target assembly.

## Constant CSE across differently-sized stores

If the same small constant is stored to two struct fields of *different* widths,
cse rewrites the later, narrower use as a `subreg` of the register already
holding the earlier, wider one. The value then gets pinned in a callee-saved
register for the whole function, which shifts the stack frame and every
save/restore — a small logical difference that shows up as a large diff.

Symptom in the diff: target has two separate `li vN,K`; your build has one
`li sN,K` early plus an extra `sw/lw sN` in the prologue/epilogue.

Fix: route the *later* store through a wider temporary. cse can take a lowpart
but cannot widen, so an SImode constant cannot be unified with an existing
HImode/QImode one:

```c
s32 flag;          /* The indirection is required. */
...
p->field_120 = 1;  /* HImode constant */
...
flag = 1;
p->field_1f = flag; /* SImode constant — cse can't fold into the HI reg */
```

A cast alone (`p->field_1f = (s32)1;`) does **not** work: the front end folds it
back to the field's type before RTL is generated.

## `volatile` blocks delay-slot filling

`-fdelayed-branch` will not move a volatile memory access into a `jal` delay
slot. When the target has a `nop` in a delay slot that an adjacent store could
obviously have filled, the global involved is probably `volatile` — most often
because it is shared with an interrupt or VSync callback.

Corollary: this is a useful signal *about the game*, not just a matching trick.
`D_8005EC70` is written by the VSync callback `func_80027498` and read by the
main loop `func_8002785C`, so `volatile` is semantically correct there.

Inverse check: if the target *does* fill the slot with a store, that variable is
**not** volatile — don't add the qualifier to fix something else.

`volatile` also changes how field addresses are formed. A non-volatile store to
a field at a non-zero offset folds into a single split-address access:

```
lui  v0, %hi(D_xxx+4)
jr   ra
 sw  a0, %lo(D_xxx+4)(v0)
```

Making the global `volatile` forces a base-address materialization plus an
offset store, with the store *outside* the delay slot:

```
lui   v0, %hi(D_xxx)
addiu v0, v0, %lo(D_xxx)
sw    a0, 4(v0)
jr    ra
 nop
```

`func_8005791C` (`D_800827A0.field_4 = arg0`) is a pure example — only the
`volatile GStruct18` form matches.

`D_800680C0` is another interrupt-shared flag: the SPU timer callback
`func_8004D7D4` / `func_8004D820` reads and writes it while main-line
`func_8004CC58` does the same. Marking it `volatile` keeps stores out of
`jal` delay slots (target has `nop` after `D_800680C0 = 0`).

## Hold a global's address in a local pointer

When a function touches the same global struct across several calls, the target
usually loads its address once into a callee-saved register (`lui`/`addiu` into
`$s0`, then `off($s0)` everywhere). Referring to the global by name gives you a
fresh `lui %hi(...)` after every call instead, often stuffed into delay slots
that should hold a `nop`.

Assign it to a local pointer first — the same trick `func_8002D25C` in
`src/main/1C034.c` annotates as *"The indirection is required."*:

```c
GStruct3* state;

state = &D_80068FA0;
switch (state->field_4c) { ... }
```

Note the target may still use `%lo(sym)($sN)` for the field at offset 0 while
using `off($s0)` for the rest; that falls out of CSE on its own and is not a
sign that a second expression is needed.

**Inverse — skip the local pointer when all accesses are pre-call.** If every
read/write of the global happens *before* any `jal`, a bare `D_80070F68.field`
name matches fine: GCC loads the address into a temporary (`$v1`) once and
never needs to reload it. `func_8002BE0C` is an example — it reads
`field_101`, optionally writes `field_10b`, then only calls other functions.
A local `GStruct1*` would force a callee-saved register and a larger stack
frame for no benefit. Use the pointer only when the address is live across
calls.

**Post-call store that uses `$s0` for `%hi/%lo`.** Even a single
`global = func(...)` after a call can need the local-pointer form when the
target saves `$s0`, does `lui s0,%hi(global)` / `sh v0,%lo(global)(s0)`, then
restores `$s0`. A bare store picks `$v1` and drops the save/restore
(`func_80026178` / `D_8006EBF2`):

```c
s16 *ptr;

ptr = &D_8006EBF2;
/* ... fill stack args ... */
*ptr = func_8004DE18(&sp);
```

**Order: pointer first, then the shared constant.** When the target does
`lui %hi(global)` *then* `li aN,K` before the first `sw %lo(global)`, assign the
local pointer before the constant. Reversing those two lines swaps the
instructions and costs a near-match:

```c
/* Matches: lui of D_80070EE8, then li a1,0xA */
otCtx = D_80070EE8;
depth = 0xA;
otCtx->field_0 = depth;
/* ... otCtx[1].field_0 = depth reuses a1 */

/* Mismatches: li first, then lui */
depth = 0xA;
otCtx = D_80070EE8;
otCtx->field_0 = depth;
```

`func_800281D4` is the example — `depth` is shared across both OT buffers.

**Inverse — constant first, then pointer.** When the target does `li aN,K` *then*
`lui %hi(global)`, assign the constant before the address. A bare
`ptr->field = 1` often schedules `li` after the address load; force the order
with a local:

```c
/* Matches: li a1,1 then lui/addiu of D_8007EBF0 */
i = 0;
flag = 1;
for (ptr = D_8007EBF0; i < 0x40; i++, ptr++) {
    if (ptr->field_0 == 0) {
        ptr->field_0 = flag;
        ...
    }
}
```

`func_800509F4` is the example.

**Hybrid — pointer for early accesses, global name after a call.** When the
target keeps the address in `$s0` for pre-call loads/stores but reloads with a
fresh `lui`/`addiu %lo` for a *post-call* store (instead of `off($s0)`), use the
local pointer only up through the call and name the global for the later write:

```c
volatile GStruct32* p = &D_800828F0;
if (p->field_1 == 0) {
    p->field_4 = CdReadyCallback(arg0); /* uses $s0 */
} else {
    CdReadyCallback(arg0);
}
D_800828F0.field_1 = 1; /* reloads address into $v0 — not $s0 */
```

`func_8005B648` needs this plus `volatile` on the global (base+offset `sb`, not
`%lo(sym+1)`). Writing `p->field_1 = 1` keeps `$s0` and mismatches.

**Indexed volatile arrays — multiply before base load.** A direct
`arr[i].field = 0` on a `volatile` global often schedules the `lui`/`addiu` of
the array base *before* the stride multiply. The target for simple setters
usually does the multiply first, then materializes the base. Force that order
with a local pointer:

```c
/* Wrong schedule: lui/addiu base, then i*stride */
D_80071620[arg0].field_A = 0;

/* Right schedule: i*stride, then lui/addiu base */
volatile GStruct25* p;
p = &D_80071620[arg0];
p->field_A = 0;
```

Keep the local pointer `volatile` as well so the store stays out of the `jr`
delay slot (a plain `GStruct25*` still multiplies first but fills the slot with
`sb`). `func_8002C9E0` is the minimal example.

**Inverse — non-volatile array: base before index.** For a plain (non-volatile)
array, `&arr[(s8)i]` often schedules the signed index shift *before*
`lui`/`addiu` of the base. When the target materializes the base first, assign
the array to a local pointer, then index through that:

```c
/* Wrong schedule: sll/sra index, then lui/addiu base */
p = &D_80082148[(s8)arg0];

/* Right schedule: lui/addiu base, then sll/sra index */
GStruct31* base;
base = D_80082148;
p = &base[(s8)arg0];
```

`func_800561EC` needs this form so `F3D458_Free` can take `p->field_0` with the
base already in `$v0` before the stride multiply lands in `$s0`.

## `~x != 0` for `nor` + `sltu` (not `x != -1`)

When the target does:

```
nor  v0, zero, v0
sltu v0, zero, v0
```

write `return ~x != 0;` (or the same expression in a larger return). That is
semantically `x != -1`, but `x != -1` often compiles to a different compare
sequence. `func_8005462C` is a one-liner that only matches with the `~` form:

```c
return ~func_80055DAC(func_80053F00()) != 0;
```

## Store-then-reload for prologue scheduling

When the target opens with `lui %hi(global)` *before* the first `sw $s0` of the
prologue, assigning into a local and then storing is usually wrong:

```c
/* Wrong schedule: andi into $s0, then later lui */
flag = arg0 & 1;
D_8006EBBA = flag;
```

Force the address load first by writing the expression into the global and
reloading it for the rest of the function (`func_800260B0`):

```c
/* Matches: lui of D_8006EBBA, then sw $s0 / andi $s0 */
D_8006EBBA = arg0 & 1;
flag = D_8006EBBA;
```

CSE still reuses the masked value for later uses after the reload is combined,
but the early store pulls the `lui` ahead of the callee-saved saves.

## Count-up `do`/`while` with `u32` for `sltiu` clear loops

A plain `for (i = 0; i < N; i++) { *ptr++ = 0; }` with a signed `i` often
strength-reduces to a countdown (`li v1, N-1; ...; bgez`), which is wrong when
the target counts up:

```
move   v1, zero
sw     zero, 0(a0)
addiu  v1, v1, 1
sltiu  v0, v1, N
bnez   v0, loop
 addiu a0, a0, 4
```

Match with an unsigned counter and a `do`/`while` (`func_8004DDF0`):

```c
u32 i;
s32* ptr;

ptr = D_8007E2E0;
i = 0;
do {
    *ptr = 0;
    i++;
    ptr++;
} while (i < 0x15U);
```

`u32` + `< N` produces `sltiu`/`bnez`. Signed `i` or a counting-down `for`
does not.

## Finding which pass causes a mismatch

Rather than guessing at C-level rewrites, dump the RTL and find the pass that
introduces the difference:

```sh
cc1 <normal flags> -dr -ds -dc -o out.s in.i
```

Dumps land next to the `.i` input as `in.i.rtl` (post-expand), `in.i.cse`,
`in.i.combine`. Grepping for the offending `(const_int N)` or `(mem:QI ...)`
across the dumps shows exactly where a value got unified or rewritten. This is
how the constant-CSE `subreg` behaviour above was identified.

The dumps are deterministic — the same `.i` gives the same dumps — so they diff
cleanly between attempts.

## Local scratch environment gotchas

Use `tools/claude --bootstrap-only <fn>` to set up the environment without
launching a nested interactive session.

**`.set gp=64` (fixed).** `tools/claude` assembles `target.o` with
`-march=r3000`, and `prelude.inc` used to emit `.set gp=64`, which modern `gas`
rejects for a 32-bit CPU. The setup script reported the error and continued,
leaving **no `target.o`**, so `build.sh` failed later in a confusing way.
`prelude.inc` now emits `.set gp=32` and the environment builds on its own.

**Functions with jump tables still need a manual `target.o`.** `macro.inc`
defines `jlabel` as `.global`, so branches to the jump-table labels assemble to
`R_MIPS_PC16` relocations instead of resolved offsets, and `objdump.py` aborts
with `unknown relocation type ... R_MIPS_PC16`. The compiled side has no such
relocations, so the labels only need to be global in the real build. Make them
local in the scratch copy and reassemble:

```sh
sed -i 's/^  jlabel \(\.L[0-9A-F]*\)$/\1:/' nonmatchings/<fn>/target.s
mips-linux-gnu-as -EL -Iinclude -Iinclude/decomp -Iinclude/psyq -I build \
  -O2 -march=r3000 -mtune=r3000 -no-pad-sections -G0 \
  -o nonmatchings/<fn>/target.o nonmatchings/<fn>/target.s
```

The remaining diff will then be a single cosmetic pair — target references
`jtbl_XXXXXXXX` where your build references its own `.rodata`. That is expected
in the scratch environment; see the next section for the real build.

**`maspsx` hangs unless stdin is closed (fixed).** `maspsx.py` prints
`Warning, no input from stdin, will try to read from a file` and then blocks
forever waiting on stdin if stdin is still open — so `build.sh` appears to hang
with no output, and orphaned processes accumulate. `build.sh` now redirects
`< /dev/null` itself. If you invoke the toolchain by hand, or run `build.sh`
from a wrapper that keeps stdin open, redirect it yourself:

```sh
./build.sh base_1.c < /dev/null
```

## `register asm` pins

Pinning a local to a hard register is the practical tool for the register
allocation differences that remain after the expression-level ones are fixed.
Two rules learned the hard way:

- **The `register` keyword is required.** `u8* p asm("a2");` is silently
  ignored; only `register u8* p asm("a2");` binds. An inert pin looks like a
  working one in the source and wastes a lot of time.
- **Pin the variable the target *disagrees* about, not the one whose register
  looks wrong.** Diffs shift in blocks: if the whole allocation is off by one
  register, one pin on the earliest-allocated variable can realign everything
  downstream. Pinning the later ones individually usually makes the score worse.

Diagnostic worth knowing: dropping a pin can produce the *right instruction
form* with the *wrong register* (`andi v0,t0,0xff` where the target has
`andi v0,t1,0xff`), while keeping the pin produces the right register with the
wrong form (`andi t1,t1,0xff`). That means the variable is naturally allocated
elsewhere and the conflict is upstream — not that the pin is wrong.

## Fixing clusters together, not individually

When a large function is close but not matching, the remaining diff usually
splits into clusters that *look* independent. They often are not: applying the
obvious fix to one cluster in isolation can score worse than leaving it alone,
because it perturbs allocation everywhere else.

Concretely, while matching `F12D18_InitStage0TablesCb` the fix for one cluster
(a value being reloaded rather than cached) scored worse every time it was
applied alone — and then became unnecessary, because an unrelated change
elsewhere (hoisting an assignment above a test) made the cluster disappear on
its own. Treat a stubborn multi-cluster diff as one coupled problem and let
`permute.sh` search it rather than hand-fixing cluster by cluster.

## Running the permuter

`./permute.sh --clean -j <N> <fn> <asm-path> <c-path>` sets up
`permuter/<fn>/`, then run it with the project venv (the permuter needs `toml`,
which is in `requirements.txt`):

```sh
./venv/bin/python tools/decomp-permuter/permuter.py \
  -j 12 --better-only --stop-on-zero --algorithm levenshtein permuter/<fn>/
```

Check the reported `base score = N` against `dist.py`'s difference count for the
same file — if they disagree, the generated `compile.sh` is not compiling the
way `build.sh` does and the search is worthless.

Improvements land in `permuter/<fn>/output-<score>-<n>/source.c`, as the whole
preprocessed file reformatted by pycparser. Diff only the function against the
same slice of `permuter/<fn>/base.c` (`awk '/^<rettype> <fn>/,0'`) or the
reformatting drowns the real change. Reproduce the change in your own `base_N.c`
and re-measure before trusting it, then reseed the permuter from the improved
file — it searches from a fixed base and will not compound its own finds.

## Compiler-generated jump tables

A switch that compiles to a jump table means the extracted `jtbl_XXXXXXXX` in
`asm/USA/main/data/<file>.rodata.s` has to be replaced by the one GCC emits, or
the table ends up defined twice and the layout shifts.

In `configs/USA/main.yaml`, hand the table's address range to the C file by
adding a `.rodata` subsegment (the leading dot means "this range comes from the
C object"; `src/main/mem.c` was the pre-existing example):

```yaml
- [0x3764, rodata, C37C_1]  # tables still owned by assembly functions
- [0x37f4, .rodata, C37C]   # table generated by src/main/C37C.c
```

**The names must differ.** A `rodata` and a `.rodata` subsegment sharing a name
both write `asm/USA/main/data/<name>.rodata.s`, and the later one silently wins
— which deletes the earlier tables and fails at link time with `undefined
reference to jtbl_*`. Rename the assembly remainder (`C37C_1` above) and drop
the suffix once every table in the segment is compiler-generated.

`SUBALIGN(4)` in the linker script overrides the 8-byte alignment GCC gives the
table, so a table at a 4-mod-8 address still lands correctly.

## Reading switch statements

- **Jump table present** (`jtbl_*` in the `.rodata.s` file): read the table
  entries in order to get case-label → target-address, then read the order the
  case *bodies* appear in the function. GCC emits bodies in **source order**, so
  the body order tells you how to order the `case` arms in your C.
- **If-else chain** (`beq`/`slti` cascade, no table): GCC chose this for a small
  dense set of cases. Again, body order in the asm = source order.
- Unreferenced table slots map to the `break`/default label.

## Shared error block between success path and cleanup

When the target layout is:

```
[success helpers]  j check_ret   (andi in delay)
[error: F(0)]      j end
check_ret:         if (ret) cleanup
end:
```

early `return`s / nested `if`s will either (a) place the error *before* the
success path, or (b) merge the two `F(...)` calls into one `jal` with different
`$a0` setups. Force the order with gotos that jump *over* the error block:

```c
/* success path */
ret = helper();
goto check_ret;

on_error:
    F(0);
    goto end;

check_ret:
    if (ret != 0) { /* cleanup */ }
end:
    return;
```

Error sites earlier in the function `goto on_error`; soft-error sites that
must not share the `jal` call `F(2)` then `goto end` (or `return`). Same idea
as `F12D18_InitStage0TablesCb`, used for `func_80022BD0`.

## Empty body with a pure stack frame (`addiu sp` / `addiu sp` / `jr ra`)

Some table stubs look empty but are not `jr ra; nop`. The target only allocates
and frees a frame:

```
addiu sp, sp, -0x10
addiu sp, sp, 0x10
jr    ra
 nop
```

A bare `void f(void) {}` compiles to just `jr ra` (see `func_80033C38`,
`func_8002DEC4`). Unused automatics still force a frame under this toolchain:
size 1–8 → `-0x8`, size 9–16 → `-0x10`. Match with an unused buffer of the
frame size:

```c
void func_80036A1C(void)
{
    char pad[0x10];
}
```

No stores are required; do not save `$ra` if the target does not.

## Mid-struct pointer for `addiu sN, a0, N`

When the target keeps `arg + N` in a callee-saved register and then loads
relative to that base (`lw a0, 4(s0)` / `lbu a1, 1(s0)` for fields at
`arg+N+4` and `arg+N+1`), plain field access from the full struct base yields
`move s0, a0` plus larger offsets instead — same semantics, wrong codegen.

Symptom: only the base register and field offsets differ (e.g. `s0 = a0`
vs `s0 = a0 + 4`, `lw 8(s0)` vs `lw 4(s0)`).

Fix: take a local pointer to a typed overlay of the struct starting at offset
`N`, and access fields through that pointer. `func_80050C80` does this with
`GStruct16From4` overlaid at `&arg0->field_4`:

```c
GStruct16From4* mid = (GStruct16From4*)&arg0->field_4;
temp = func_80055DAC(mid->field_4); /* was arg0->field_8 */
if (temp >= 0) {
    func_80055B70(temp, mid->field_1); /* was arg0->field_5 */
}
```

Sibling helpers that only touch one field (`arg0->field_8`) do not need this;
use it when the target rebased the pointer and multiple fields are relative to
that new base.

## Early-exit for `beqz` with dual returns

When both arms return the same constant and the target uses `beqz` into the
failure epilogue (with an early `move v0,zero` on the success path so later
stores can free `$v0`), write the inverted early exit rather than
`if (cond) { ...; return K; } return K;`:

```c
/* Matches: beqz v0, fail; ...; move v0,zero; stores; j epilogue; fail: move v0,zero */
if (flag == 0) {
    return 0;
}
/* work */
return 0;
```

The `if (flag != 0) { ...; return 0; } return 0;` form often becomes
`bnez` + an explicit jump to the shared epilogue, which mis-schedules the
success-path setup. `func_8004D820` needs this shape (with `volatile`
`D_800680C0`) to free `$v0` for the return value before the `D_800680BC`
update.

## Fall-through return vs delay-slot return

When the target ends with:

```
bltz  v1, end
li    v0, -1      # delay: early-path return
li    v0, 3       # fall-through return (NOT in delay slot)
jr    ra
nop
```

plain early returns put the last constant in the `jr` delay slot:

```c
if (a >= 0) return 2;
if (b < 0) return -1;
return 3;           /* becomes: jr ra; li v0, 3 */
```

Fix: early-return the branched cases, but materialise the fall-through value in
a temporary before returning:

```c
if (a >= 0) {
    return 2;
}
if (b < 0) {
    return -1;
}
ret = 3;
return ret;         /* becomes: li v0, 3; jr ra; nop */
```

Also prefer a local pointer (`p = &global`) so the base lands in `$v1` and gets
overwritten by later field loads — matching the target's register reuse.
`func_8001D4F0` needs this pattern.

## `u16 x = arg0` for early `move v0,a0` + prologue `sw ra`

When the target opens with:

```
addiu sp,sp,-0x18
sw    ra,0x10(sp)
andi  a1,a1,0xff
bnez  a1, path
 move  v0,a0          # delay: copy arg0 for later andi/srl
...
andi  v0,v0,0xffff
srl   v0,v0,0xc
```

writing the index as `((u32)arg0 & 0xFFFF) >> 12` (or bare `arg0`) often puts
`sw ra` *in* the `bnez` delay slot and uses `andi v0,a0,0xffff` with no copy —
one instruction short and a mis-scheduled prologue.

Fix: truncate into a `u16` local first, then shift that:

```c
u16 x;

x = arg0;
if ((arg1 & 0xFF) == 0) {
    return other_path(...);
}
if (table[x >> 12] == -1) {
    return 0;
}
/* still use arg0 for later masks that need the full value */
switch (arg0 & 0xF000) { ... }
```

The `u16` assignment forces the early `sw ra` plus `move v0,a0` / `andi v0,v0,
0xffff` sequence. A plain `s32 temp = arg0` tends to land in `$a3` instead of
`$v0` and breaks the later `li v0,0x2000` delay-slot preload. `func_80053548`
needs the `u16` form.

## Volatile `u8` for `lbu` + `sll 24` / `sra 24` instead of `lb`

A non-volatile `u8` global cast to `s8` often collapses to a single `lb`
(sign-extending load). The target sometimes wants the longer form:

```
lbu  v0, %lo(sym)(v0)
li   a0, 1          /* other ops may interleave here */
sll  v0, v0, 24
sra  v1, v0, 24
```

Mark the global `volatile u8`. The volatile load forces `lbu`, and the cast
then emits the shift pair. Related stores that must keep program order around
that load (e.g. a `vol_a = 0; vol_b = vol_a;` chain just before the cast)
should also be `volatile`, or the load will sink into the middle of them.

`func_800537FC` needs this on `D_80082135` and the surrounding
`D_80082128` / `D_80082124` / `D_80082130` stores.

## Reuse formal parameters for live ranges that span early calls

When the target keeps `arg0`/`arg1` in `$s0`/`$s1` through a later loop
(`sll a0,s0,1` with `$s0` preserved), prefer:

```c
arg0 = arg0 & 0xFF;
...
if (table[var_v1 + arg0 * 2] == arg1)
```

over introducing new `temp_s0`/`temp_s1` locals. Fresh locals often free the
compiler to clobber `$s0` with the shift (`sll s0,s0,1`) and put the table
base in `$a0` instead of `$a1`.

## `do {} while (0)` to interleave `lui` with an early load

When unlinking from a doubly-linked list, the target often loads a local field
between the `lui` and `%lo` of a global head pointer:

```
lui  v0, %hi(D_head)
lw   v1, 0(a0)           /* next — between hi and lo */
lw   v0, %lo(D_head)(v0)
beqz v1, L
 addiu v0, v0, 4         /* &head->prev */
addiu v0, v1, 4          /* &next->prev */
```

Writing the natural form:

```c
next = state->node.next;
head = D_head;
pp = &head->prev;
if (next != NULL) {
    pp = &next->node.prev;
}
```

fully loads `D_head` before `next` (and inserts a load-delay `nop` before
`beqz`). Wrapping the address-of assignments in `do {} while (0)` restores the
hi/next/lo interleaving without changing semantics (`func_8002D444`):

```c
next = state->node.next;
head = D_800716D8;
do {
    pp = &head->prev;
    if (next != NULL) {
        pp = &next->node.prev;
    }
} while (0);
prev = state->node.prev;
*pp = prev;
prev->next = state->node.next;
```

Two separate `&…->prev` assignments (default head, then override) also produce
the `addiu …, 4` / `sw 0(pp)` form; a single `head->prev = …` after updating
`head` collapses to `sw 4(head)` and loses the delay-slot `addiu`.

## Returning two `s16`s packed as one `s32`

When the target stores two halfwords at `0(sp)` / `2(sp)` then `lw`s the word
back into `$v0`, GCC is packing a 2-component fixed-point result. Emitting
explicit shifts/masks (`(hi << 16) | (lo & 0xFFFF)`) does **not** match — it
produces a completely different instruction sequence.

Use adjacent stack halfwords (struct or array) and reload as a word:

```c
s32 func(void) {
    struct {
        s16 unk0;
        s16 unk2;
    } sp;

    sp.unk0 = D_x >> 8;
    sp.unk2 = D_y >> 8;
    return *(s32 *)&sp;
}
```

`func_80049950` is the pure example (fixed-point globals `D_80067648` /
`D_8006764C`, sra by 8, packed return).

## Walk word pairs with `s32*`, not struct `+8`

When the target fills an array of 8-byte slots as two consecutive word stores
with a `+4` pointer bump between them:

```
sw   a1, 0(a0)
addiu a0, a0, 4
sw   zero, 0(a0)
...
addiu a0, a0, 4   # delay slot of the loop branch
```

Writing through an 8-byte struct element produces a different schedule:

```
sw   a1, 0(a0)
sw   zero, 4(a0)
addiu a0, a0, 8
```

Match the target by treating the buffer as `s32*` and post-incrementing twice
per iteration (even if the high-level type is an 8-byte entry array):

```c
void init_slots(s32* arg0) {
    s32 i;

    if (arg0 != NULL) {
        for (i = 0; i < 0x10; i++) {
            *arg0++ = 0x407F4000;
            *arg0++ = 0;
        }
    }
}
```

`func_800528BC` (init of `GStruct22::field_484[16]`) is the pure example.

## Shared `return 0` via switch `break` (not early return)

When several paths end by returning 0 and the target routes them through a
single `move v0, zero` before the epilogue, write those paths as `break` out of
the switch (or fall through) into one trailing `return 0`. Early `return 0`
inside a case puts `move v0, zero` in a branch delay slot and jumps past the
shared label — wrong labels, wrong register pressure, and often a missed
delay-slot fill from the fall-through path.

```c
switch (state->field) {
case 0:
    if (ok) {
        state->field++;
        break; /* fall to shared return 0 */
    }
    return 2;
case 1:
    if (done()) {
        state->field = 0;
        return 2;
    }
    break;
default:
    return 0;
}
return 0;
```

`func_8001E6AC` needs this so the shell-open path can delay-slot-fill
`andi a1, s1, 0xFFFF` from the not-open fall-through instead of preloading
`v0 = 0`.

## `s16` return types force `sll; beqz` at call sites

Callers that test a halfword return emit:

```
jal  foo
nop
sll  v0, v0, 16
beqz v0, ...
```

Declare (or cast) the callee as returning `s16`, not `bool`/`s32`. A `bool`
definition still matches the callee body for 0/1 results, but call sites then
lose the `sll`. `E734_CDIsShellOpenBitSet` was retyped from `bool` to `s16` so
`func_8001E6AC` (and other CD helpers that already had the `sll` in target asm)
match at the call site.

## Two-case switch may drop the `slti` range check

A target of the form:

```
beq  x, 2, case2
slti t, x, 3
bnez t, default      /* x < 3 → default */
li   t, 3
beq  x, t, case3
j    default
case2 / case3 / default bodies...
```

looks like `switch (x) { case 2: ...; case 3: ...; default: ... }`, but GCC
2.8.1 with only those two cases often emits a plain equality chain *without*
the `slti`/`bnez` range check (delay slot of the first `beq` becomes `li 3`
instead). Score sits ~83% with identical case bodies.

Force the target layout with an explicit decision tree and gotos so each
comparison is a positive branch to a late body:

```c
s32 temp = x;
if (temp == 2) {
    goto case2;
}
if (temp < 3) {
    goto default_case;
}
if (temp == 3) {
    goto case3;
}
goto default_case;
case2:
    return 4;
case3:
    return 6;
default_case:
    return other;
}
```

`func_8003E698` needs this. Adding dummy cases (0/1) that share the default
body brings back `slti` but also inserts extra `bltz`/duplicate labels — not a
full match.

## Local pointer CSE for a shared byte-store address

When both arms of an `if`/`else` store to the same global `u8`, the target may
load `%hi(sym)` once into a register that both `sb`s reuse (often from the
branch delay slot). Naming the global twice emits a second `lui`:

```
bltz  v0, neg
lui   v1, %hi(D_xxx)
lui   v0, %hi(D_xxx)   /* extra — not in target */
j     end
 sb   a0, %lo(D_xxx)(v0)
```

Hold the address in a local first:

```c
u8* flag = &D_8007F2F0;
if ((s8)arg0 >= 0) {
    *flag = arg0;
} else {
    *flag = 0x7F;
}
```

`func_800517B4` needs this so both stores share one `lui v1, %hi(D_8007F2F0)`.

## One-iteration `for (i = 0; i <= 0; i++)` + array stride

Several audio helpers walk `D_8007F300` with stride `0x5DC` and loop condition
`blez` after `i++` from zero — i.e. exactly one iteration. A pointer `++` do/while
often becomes a countdown (`addiu -1` / `bgez`). Array indexing matches:

```c
i = 0;
val = 0xFFFF;
for (; i <= 0; i++) {
    (&D_8007F300)[i].field_C = val;
}
```

The element type must be size `0x5DC` (logical stride). BSS for `D_8007F300` is
still `0x5E0` (4 bytes of tail padding after the logical block).

## GTE inline macros: real COP2 opcodes, not DMPSX encodings

`include/psyq/inline_c.h` is DMPSX-oriented. Load/store helpers
(`gte_ldv0`, `gte_ldsvrtrow0`, `gte_stlvnl0`, `gte_SetRotMatrix`, …) emit real
MIPS `lwc2`/`ctc2`/`swc2` and match as-is. Command macros (`gte_rtv0`,
`gte_mvmva`, …) emit DMPSX placeholder `.word 0x00000xxx` values that this
toolchain never rewrites — they assemble to the wrong instruction.

For GTE *commands*, emit the real COP2 word:

```c
/* mvmva sf=0, mx=0 (rot), v=0 (V0), cv=3 (none), lm=0 → 0x4A406012 */
#define gte_rtv0sf0() __asm__ volatile("nop; nop; .word 0x4A406012")
```

`func_8003D000` is the template: `gte_ldsvrtrow0` + `gte_ldv0` + custom
command + `gte_stlvnl0`. Standard `gte_rtv0` is `mvmva 1,0,0,3,0`
(`0x4A486012`); the sf=0 variant drops the 12-bit shift (`0x4A406012`).

## Large sparse switches: case order and shared handlers

GCC 2.8.1 emits switch case *bodies* in an order tied to the binary-search
tree, not source order. When the decision tree already matches but the
handler tails (`j epilogue` / `addiu %lo(...)`) are shuffled, reorder the
`case` labels in the C source to match the target's leaf-emission order
(read it off the end of `target.s` / the dump). Case *value* set still
controls the tree; only body order changes.

Also watch for **shared-handler IDs** the tree folds together even when they
are far apart numerically — e.g. `0x21`/`0x121`, `0x61`/`0x161`, `0x31`/`0x39`/
`0x131`. If a `sltiu` + `beqz` lands on a handler without an equality check,
that gap value is a real case (or shares one). Missing them reshapes the
whole tree.

Local differ may report `lui v0,0x8001` vs `lui v0,%hi(D_8001xxxx)` as a
mismatch: the ROM disassembly hard-codes the common high half, while cc1
emits `%hi`. Linked output is identical when every symbol shares that half —
always confirm with `./tools/build-and-verify.sh`.

## Flag compare via pinned temp + xor

When the target loads a word, masks with `0xFFFF0000`, xors with a constant,
and `sltiu`s into the flag register (delay slot of a `j`):

```c
register u32 tmp asm("v0");
tmp = *(u32*)&p->byte_field_at_offset; /* or a real u32 field */
tmp = (tmp & 0xFFFF0000) ^ 0x02100000;
flag = tmp < 1;
```

A single `(x & mask) == cst` often allocates the intermediate into the flag's
eventual register (`a2`) instead of keeping it in `v0` through the xor.

## Null-check polarity: `!= NULL` for early `bnez` return

When the target opens with an early null return:

```
bnez  a0, body
 andi v0, a1, 0xff   # delay (shared setup)
jr    ra
 move v0, zero       # null return
body:
 ...
```

write the **positive** branch first, with the null return as the fall-through:

```c
if (arg0 != NULL) {
    return &arg0->field_4[arg0->field_10[arg1] + arg2];
}
return NULL;
```

The inverted early-exit form (`if (arg0 == NULL) return NULL; return ...;`)
produces `beqz` with the null epilogue at the end of the function — same
semantics, wrong layout. `func_8004EA60` only matches with the `!= NULL`
shape.

## Sign-extend a call result into `s32`, not `s8`

When the target does:

```
jal  foo
 ...
sll  v0, v0, 24
sra  a0, v0, 24
bltz a0, ...
...
sb   a0, 0(s0)   /* delay slot of a later jal — same sign-extended reg */
```

assign the result to an `s32` through an `(s8)` cast:

```c
s32 idx;
idx = (s8)foo(...);
if (idx < 0) {
    return NULL;
}
p->field_0 = idx;
bar(idx, ...);
```

A plain `s8 idx = foo(...);` keeps a copy of the raw return (`move v1,v0`)
and stores/`sb`s that copy instead of the sign-extended register, adding an
instruction and shifting every later label. `func_80056240` only matches with
the `s32` + `(s8)` form.

Note also that `D_80082148` is walked two ways: as `GStruct31[16]` (stride
`0x10`, via `func_800561C0` / `func_800561EC`) and as `GStruct43` slots
(stride `0x40`, via `func_80056240`). Cast the base rather than changing
`GStruct31`.

## Two-step table index for early `lw` into `$a0`

When the target loads `table[i]` into `$a0` *before* scaling `j`, then adds:

```
lw   a0, 0(v1)     /* a0 = table[i] */
sll  v0, a1, 1
addu v0, v0, a1
sll  v0, v0, 2
j    merge
 addu a0, a0, v0   /* a0 = table[i] + j * stride */
```

a single expression (`D_8005EF74[arg0] + arg1` or `&D_8005EF74[arg0][arg1]`)
schedules the multiply first and loads into `$v1` instead, breaking the match.

Split the load from the index:

```c
GStruct2* ptr;

if (arg0 >= 0) {
    ptr = D_8005EF74[arg0];
    ptr = &ptr[arg1];
} else {
    ptr = (GStruct2*)arg1;
}
return func_8002CB04(ptr, arg2, arg3, D_800716D8);
```

Also: use `if (arg0 >= 0)` (not `arg0 < 0`) so the fall-through is the table
path and the branch is `bltz` to the cast path — that matches the shared
post-merge arg shuffle (`a1=a2`, `a2=saved a3`, `a3=D_800716D8`) of
`func_8002CFDC`. Dual early returns force separate call setup and reg-shuffle
the args too early.

## `while (1)` for linked-list walks that re-enter at the null check

A normal `while (node != NULL)` puts the null test at the bottom of the loop
body as `bnez`/`beqz` after advancing the pointer. When the target instead does
an unconditional `j` back to a top-of-loop `beqz s0, end` (often because one
path assigns `node = remove(node)` and the other does `node = node->next`),
write:

```c
node = (GStruct8*)head->field_14;
while (1) {
    if (node == NULL) {
        break;
    }
    if (callback != NULL) {
        if (callback(node->field_c) == -1) {
            node = func_8004D94C(node);
            continue;
        }
    }
    node = (GStruct8*)node->field_14;
}
```

Also load `head = &global` *before* the enable-flag check so GCC materializes
both `%hi/%lo` pairs up front (see "Hold a global's address in a local
pointer"). Nested `if (flag) { if (head != NULL) { ... } }` rather than
`flag && head` keeps the second null test in the first's delay-slot region.
`func_8004D8BC` is the reference.

## K&R definition when a same-TU caller uses indeterminate args

A modern prototype definition (`s32 f(s32 x) { ... }`) is visible to later
call sites in the same translation unit. If an already-matched caller invokes
the function with a bare `f()` and a `nop` delay slot (so `$a0` is whatever
garbage was left), introducing a prototype makes that call a hard error
("too few arguments").

Use an old-style K&R definition instead — it does **not** create a prototype,
so the no-arg call stays legal and the callee still matches:

```c
s32 func_80053F00(arg0)
s32 arg0;
{
    /* ... */
}
```

Keep the header declaration unprototyped too (`extern s32 func_80053F00();`).
`func_8005462C` → `func_80053F00` is the reference.

## `switch` for equality chains that branch *to* case bodies

When the target does positive equality tests that jump *to* handlers
(`beq`/`beqz` to the case, default falls through then `j` past the bodies),
an `if` / `else if` chain usually emits the inverse (`bne` past an inlined
body). Sparse multi-way selection matches as a `switch` instead:

```c
/* Target: beqz x, case0 / beq x,5,case5 / default then j continue */
switch (arg0->field_C) {
case 0:
    table = D_8005EFB0;
    break;
case 5:
    table = D_800604B0;
    break;
default:
    table = D_8005FA30;
    break;
}
```

`func_8002EDFC` is a pure example: two independent `switch`es (glyph table by
`field_C`, centering by `field_D`) both needed this layout; the equivalent
`if`/`else if` form scored ~67%.

## If/else branch polarity: `bnez` fall-through is the `== 0` body

When the target starts with `bnez arg, else` (delay slot often hoists a load used by the else path) and falls through into the zero-arg setup before a `j join`, write:

```c
if (arg0 == 0) {
    /* fall-through body */
} else {
    /* branch target */
}
```

`if (arg0 != 0)` swaps the arms and GCC emits `beqz` with the non-zero path as fall-through — same code, inverted control flow, large score drop. `func_800338F4` is a short example (copy between `D_80060DD8` / `D_80060DF0`).

## Ring-buffer wrap: `x = x + 1; x = x % N` keeps both stores

For a power-of-two ring size, `x = (x + 1) & (N-1)` and `x++; x &= (N-1)` both
collapse to a single store under `-O2`. The target often keeps the unmasked
intermediate store, then masks:

```
addiu  v0, idx, 1
sh     v0, counter        # unmasked
andi   v1, v0, 0xffff
...
andi   v1, v1, 7
jr     ra
 sh    v1, counter        # masked
```

Writing the wrap as an unsigned modulo against the ring size preserves both
stores — GCC still lowers `% 8` to `& 7`, but only after materializing the
truncated u16 value of the first assignment:

```c
D_8006AC04 = index + 1;
D_8006AC04 = D_8006AC04 % 8;  /* not &= 7, not (index+1)&7 */
```

`func_8001D898` (8-entry queue walk of `D_80068FA0.entries`) is a pure example.
The same double-store shape appears on `field_1c8` / `field_1ca` updates in the
nearby ring producers (e.g. `func_8001D760`).

## `s8` globals load with `lb`, not `lbu`

`-funsigned-char` makes plain `char` unsigned, and a `u8` global always loads
with `lbu`. When the target uses `lb` to read a byte flag (then `beqz` /
compare), declare the global as `s8` even if it only holds 0/1.

A cast at the use site is not enough:

```c
if ((s8)D_800820E9 != 0)  /* still emits lbu, then sign-extends the reg */
```

```c
extern s8 D_800820E9;
if (D_800820E9 != 0)      /* emits lb */
```

`func_800518E0` / `func_80051DF4` both `lb` `D_800820E9`; stores remain `sb`
either way.

## `u8` temp for `srl` bit tests on `byte` fields

`byte` is `signed char`. Shifting a `byte` field directly as a condition inserts
an extra `negu` after the `srl`:

```
lbu  v0,1(v1)
srl  v0,v0,0x7
negu v0,v0        /* unwanted */
beqz v0,...
```

Symptom: target is plain `srl` + `beqz`; your build has `negu` between them and
a slightly larger branch distance.

Fix: load the byte into a `u8` temporary first, then shift that:

```c
u8 temp;

temp = p->unknown_0[1];
if (temp >> 7) {
    ...
}
```

That produces the clean `lbu` / `srl` / `beqz` sequence. Casting at the use site
(`if ((u8)p->unknown_0[1] >> 7)`) is not enough — the temporary is required.

`func_8005B920` is a pure example (bit 7 of `D_80082818.unknown_0[1]`).


## Early-load order among independent `&= ~mask` globals

When a function sets a bit in one global and clears it from two others
(`A |= mask; B &= ~mask; C &= ~mask`), and all three are *separate* symbols
(not fields of one struct), three codegen details interact:

1. **Local pointer on the `|=` target.** Assign `&A` to a local before the
   cast/shift. That forces the early `lui a1, %hi(A)` the target wants; without
   it GCC sign-extends the argument first and puts the mask in `a1` instead of
   `v1`.

2. **Write `~channel` twice, not a temp.** `B &= ~channel; C &= ~channel;`
   places `nor` *after* the `sw` of A. `inv = ~channel; B &= inv; C &= inv;`
   moves `nor` *before* that store and breaks the match.

3. **Source order of the two `&=` is not store order.** Writing
   `B &= ~mask; C &= ~mask;` can early-load C and late-load B (or vice versa).
   If the target early-loads one particular global into `$a2`, swap the two
   statements until that symbol is the early one — the final store order still
   ends up matching because of scheduling.

`func_8004E71C` is the pure example: pointer on `D648E0_8007EBB0`, then
`D648E0_8007EBA8 &= ~channel` before `D648E0_8007EBAC &= ~channel` so that
EBAC is the early-loaded `$a2` value.

## Unaligned 8-byte copy via nested `u8[8]` struct assignment

When the target copies 8 bytes with `lwl`/`lwr`/`swl`/`swr` pairs (not
`lw`/`sw`), both sides are being treated as alignment-1. Two traps:

1. **`*(s32*)` through a `byte` field** is *not* enough. GCC 2.8.1 still
   proves word alignment when the containing struct is aligned and the field
   offset is a multiple of 4, and emits `lw`/`sw`.

2. **Rebasing to the field address** (`*(Bytes8*)&obj->field_4 = ...`) produces
   the right unaligned ops but with base `obj+4` and offsets `0`/`3`/`4`/`7`.
   The target keeps the *object* base and uses offsets `4`/`7`/`8`/`0xB`.

Match by assigning a nested `u8[8]` field that lives at offset 4 inside an
overlay of the whole object:

```c
typedef struct { u8 data[8]; } GBytes8;
typedef struct {
    byte    pad[4];
    GBytes8 field_4;
} GStructOverlayAt4;

((GStructOverlayAt4*)dst)->field_4 = ((GStructOverlayAt4*)src)->field_4;
```

`func_8002BF10` is the pure example (`D4F564_8005ED64` ← `D_80072168`).

## Big-endian halfword from two `u8` fields (stack `sb`/`sb`/`lhu`)

When the target stores two bytes onto the stack and reloads them with `lhu`
before a `nor`/`andi`, it is assembling a 16-bit value with explicit byte
order rather than doing a native halfword load.

```
lbu  v1, 2(ptr)
sb   v1, 1(sp)   # high byte
lbu  v0, 3(ptr)
sb   v0, 0(sp)   # low byte
lhu  v0, 0(sp)
nor  v0, zero, v0
andi v0, v0, 0xffff
```

Match with a local `u16` and byte stores (a `union { u16 h; u8 b[2]; }` often
optimizes the stack stores away):

```c
u16 sp;
GStruct* base;

base = D_array;
((u8*)&sp)[1] = base[arg0].field_2; /* high */
((u8*)&sp)[0] = base[arg0].field_3; /* low */
return (u16)~sp;
```

### Array base first for `lui`/`addu v0,v0,v1`

When the target loads the array base into `$v1` *before* the index multiply and
ends with `addu v0, v0, v1` (pointer in `$v0`), write:

```c
base = D_array;
... base[arg0].field ...
```

Avoid `p = &D_array[arg0]` / `p = base + arg0` if that yields `addu v1, v0, v1`
(pointer in `$v1`) — reusing the base local via `base[arg0]` twice keeps the
pointer in `$v0` so the first `lbu` can land in `$v1`.

`func_8002CA0C` is the pure example (`D_800711C8`, stride 0x24).

## PsyQ GsOT layout without including libgs.h

`libgs.h` cannot be safely pulled into project-wide headers: it depends on
extra libgs types (and redeclares several GPU primitives) that break GCC 2.8.1
parse of units that only include `game.h` / `unknown_syms.h`.

For double-buffered ordering-table descriptors (size `0x14`, two entries =
`0x28`), define a local struct with the same layout as `GsOT`:

```c
typedef struct {
    u_long  length; /* OT depth as bit count; 6 → 2^6 = 64 tags */
    u_long* org;
    u_long  offset;
    u_long  point;
    u_long* tag;
} GStruct50; /* STATIC_ASSERT_SIZEOF(..., 0x14) */
```

Init pattern (see `func_8003E6E4`): hold `GStruct50* ot = D_8007A0E8`, write
`length`/`org` for both slots, with the second `org` as `tags + (1 << length)`.
OT tag storage of `0x200` bytes is two buffers of `0x100` (`u_long[0x80]`).

## Delay `i = 0` until after a special-case rewrite of the same constant

When the target puts `move a2, zero` (loop counter init) in the delay slot of a
compare, then on the fall-through does `move a0, zero` to rewrite the argument,
initializing `i = 0` *before* the `if` lets CSE rewrite the second zero as
`move a0, a2`. The target wants a fresh `addu a0, zero, zero`.

```c
/* BAD — CSE turns arg0 = 0 into move a0, a2 */
i = 0;
if (arg0 == 0xFFFF) {
    arg0 = 0;
}

/* GOOD — i lives in the for-init; delay slot still gets move a2, zero,
   but arg0 = 0 remains move a0, zero */
if (arg0 == 0xFFFF) {
    arg0 = 0;
}
for (i = 0, ptr = base; i < n; i++, ptr++) { ... }
```

### Pin `andi` before the array `lui` with an `s32` copy

After rewriting a `u16` arg to 0, the target recomputes `andi v1, a0, 0xFFFF`
*before* the `lui`/`addiu` of the array base. Comparing the `u16` directly
schedules that `andi` after the base load. Assigning through an `s32` id forces
the early mask:

```c
if (arg0 == 0xFFFF) {
    arg0 = 0;
}
id = arg0; /* s32 id — emits andi before lui */
for (i = 0, ptr = D_arr; i < n; i++, ptr++) {
    if (ptr->field == id) {
        return ptr;
    }
}
```

`func_8004D150` is the pure example.
