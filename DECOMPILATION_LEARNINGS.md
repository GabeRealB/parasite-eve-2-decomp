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
CdCmdQueue* state;

state = &CdCmd_Queue;
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

Concretely, while matching `Fs_InitStage0TablesCb` the fix for one cluster
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
as `Fs_InitStage0TablesCb`, used for `Fs_CdReadyCb`.

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
TaskDesc* ptr;

if (arg0 >= 0) {
    ptr = D_8005EF74[arg0];
    ptr = &ptr[arg1];
} else {
    ptr = (TaskDesc*)arg1;
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

`func_8001D898` (8-entry queue walk of `CdCmd_Queue.entries`) is a pure example.
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

## Hoist compare-constants as `s32` locals *inside* the early-exit `if`

When a loop compares a loaded byte against several fixed values (`'\n'`, `'N'`,
`'n'`, `'\\'`), the target loads all of those constants once *after* the early
`blez arg1, out` (first `li` lives in the delay slot), then reuses the registers
in the loop.

Two failure modes:

1. **Literals written inline** (`if (temp == 0xA)`) — GCC reloads a fresh `li`
   at each compare and never pins them in temps.
2. **`s32` locals declared before the `if`** — constants load *before* the
   `blez`, so the branch no longer owns the first `li` delay slot.

Fix: declare the `s32` constants at the top of the `if (arg1 > 0)` block so they
materialize only on the taken path, and use a goto-based loop (not `do`/`while`)
to keep the first iteration un-peeled:

```c
if (arg1 > 0) {
    s32 c_nl = 0xA;
    s32 c_N = 0x4E;
    s32 c_n = 0x6E;
    s32 c_bs = 0x5C;
loop:
    temp = *arg0;
    if (temp == 0) {
        goto end;
    }
    if (temp == c_nl) {
        arg1 -= 1;
    } else if (temp == c_N || temp == c_n) {
        if (arg0[-1] == c_bs) {
            arg1 -= 1;
        }
    }
    arg0 += 1;
    if (arg1 > 0) {
        goto loop;
    }
}
end:
return arg0;
```

`func_8002F528` is the pure example. A plain `do { ... } while (arg1 > 0)` with
the same locals peels the null check and reintroduces `andi` masks.

## `x % 8` (not `x & 7`) for u16 queue indices

When the target advances a u16 ring-buffer index with:

```
lhu  v0, idx
addiu v0, 1
sh   v0, idx
andi v0, 0xffff
andi v0, 7
j    ...
 sh  v0, idx
```

write two assignments using **modulo**, not bitwise and:

```c
state->field_1ca = state->field_1ca + 1;
state->field_1ca = state->field_1ca % 8;
```

`% 8` on a u16 forces the zero-extend `andi 0xffff` before `andi 7`. Writing
`x & 7` (or `(x + 1) & 7`) combines into a single `andi 7` and drops one store.
`func_8001D898` (`D_8006AC04 = index + 1; D_8006AC04 = D_8006AC04 % 8;`) is the
matched precedent; `func_8001C620` needs the same form for `field_1ca`.

## Route a `volatile u8` load through an existing `s32` temp for s-reg order

When `(s8)entry->field_5` must live in `$s3` while the constant `1` lives in
`$s2`, a direct `field5 = (s8)*(volatile u8*)&entry->field_5` often steals
`$s2` for `field5`. Assigning through an already-live `s32` first flips the
colors:

```c
status = *(volatile u8*)&entry->field_5; /* existing s32, used later too */
field5 = status;
step   = state->field_1d0;
field5 = (s8)field5;
```

The volatile load still yields `lbu` + `sll 24` / `sra 24` (not `lb`), and
`field5` lands in `$s3`. `func_8001C620` needs this for the case-0x54 prologue.

## Force `addu v0, v0, s0` (scaled-index + base) for `entries[i]`

Plain `state->entries[idx].field_4 = 0` often emits `addu v0, s0, v0` (base
first). When the target has the accumulate form after `move`/`sll`:

```
move v0, v1
sll  v0, v0, 3
addu v0, v0, s0   /* scaled index + base */
sb   zero, 4(v0)
```

build the address explicitly so the add folds onto the shifted temp:

```c
u32 t;
t = state->field_1ca << 3;
t += (u32)state;
((CdCmdEntry*)t)->field_4 = 0;
```

`func_8001C620` cleanup needs this; prefer struct indexing when the operand
order already matches.

## if/else ret assignment vs pre-set ret for delay-slot returns

When the target puts `li v0, K` in a `bnez`/`beqz` delay slot (early return of a
constant) and falls through to overwrite `v0` with the success value, an
if/else that assigns both return paths into one `ret` matches:

```c
if (flag != 0) {
    ret = 1;
} else {
    /* work */
    ret = 0;
}
return ret;
```

The pre-set form often miscolors the constant into a non-`v0` temp:

```c
ret = 1;               /* lands in $v1 */
if (flag == 0) {
    /* work */
    ret = 0;
}
return ret;            /* move v1,zero; move v0,v1 — mismatch */
```

Bare early `return 1;` / `return 0;` can also fail: the compiler may sink the
`return 1` path after the work block instead of filling the branch delay slot.
`func_80057724` is the pure example.

## Ternary keeps a second `return 1` from merging with an early exit

When the target has two separate `jr ra` / `li v0, 1` epilogues — one early
exit and one late — writing both as bare `return 1;` lets GCC share a single
epilogue and turn the late branch into a jump back to the early one:

```c
/* ~90%: late bne jumps to the early return-1 */
if (flag & 1) {
    return 1;
}
if (a == b) {
    return x != 0;
}
return 1;
```

A trailing ternary forces a distinct late epilogue:

```c
if (flag & 1) {
    return 1;
}
return (a != b) ? 1 : (x != 0);
```

`func_8005BB4C` is the pure example. An if/else that assigns into `ret` and
returns once can also work, but the ternary is the minimal rewrite.

## Statement order of independent increments fills load-delay slots

When a loop body does a dependent load chain (`lw` of a pointer, then `lbu`
through that pointer) plus two independent increments (`p++` and `i++`), GCC
2.8.1 fills the first load-delay with whichever independent op is *first* in
the source. The second increment lands after the second load (or in the
branch delay if it is the pointer step).

Target often wants:

```
lw    v0, 0(a0)
nop
lbu   v0, 0(v0)
addiu a1, a1, 1      # i++
addu  v1, v1, v0
...
bnez  ...
 addiu a0, a0, 0xC  # p++ in branch delay
```

Writing `i++` before `p++` fills the `lw` delay with `i++` and leaves a `nop`
after `lbu` — mismatch. Put the pointer step first:

```c
do {
    sum += *(u8*)p->field_0;
    p += 1;   /* scheduled into the branch delay */
    i += 1;   /* fills the lbu delay, leaves nop after lw */
} while (i < 9);
```

`func_80033D88` is the pure example. Same body with `i` then `p` scores ~94%.

## Separate s16 next/sum forces a2/v1 accumulator split

Checksum *write* loops that store both `sum` and `~sum` after iterating often
need the final sum in `$v1` (for the stores) while the running total lives in
`$a2`. A single in-place `sum += byte` collapses both into `$v1` and moves the
pointer step into the branch delay:

```
addu  v1, v1, v0
bnez  ...
 addiu a0, a0, 0xC
```

Target wants:

```
addu  v1, a2, v0
bnez  ...
 move  a2, v1
sh    v1, off(base)
nor   v1, zero, v1
sh    v1, off+2(base)
```

Force the split with a separate s16 temporary, and store that temp (not the
reassigned sum):

```c
s16 next;
s16 sum;
...
do {
    temp = p->field_0;
    p += 1;
    i += 1;
    next = sum + *(u8*)temp;
    sum = next;
} while (i < 9U);
D_80072168.field_940 = next;
D_80072168.field_942 = ~next;
```

Also keep an intermediate `base = D_800610FC; p = base + 1;` so the address
forms as `addiu v0, %lo(D_800610FC)` then `addiu a0, v0, 0xC` rather than a
folded `%lo(D_800610FC+0xC)`. `func_80033D3C` is the pure example; its verify
sibling `func_80033D88` uses a plain `s32 sum` and different scheduling.

## Statement order picks which local reuses `$a1`

When a function takes only `arg0`, `$a1` is free for the first assigned local.
A search over an array of structs often needs:

```
move  a1, zero          /* index */
lui   v0, %hi(arr)
addiu v1, v0, %lo(arr)  /* pointer */
```

Writing `p = arr; for (i = 0; ...)` gives the opposite assignment (`$a1` =
pointer, `$v1` = index). Initialize the counter first:

```c
i = 0;
p = D_80082248;
do {
    if ((p->field_16 & mask) && (p->field_0 == arg0)) {
        return i;
    }
    i++;
    p++;
} while (i < 8);
return -1;
```

`func_80055DAC` is the pure example. Signed `i` + `do`/`while` also produces
the target's `slti`/`bnez` count-up form.

## Cast away `volatile` for switch delay-slot constant CSE

When a `switch` is followed by a comparison against a small constant (e.g.
`if (arg1 == 2)`), GCC 2.8.1 can CSE that constant into the delay slot of the
last case `beq` — but only if the switch body loads are *not* volatile:

```
beq  a1, v0, case3
li   v0, 2          # final cmp constant, scheduled into switch dispatch
j    default
...
case bodies use v0=2 for `if (arg1 == 2)` without reloading on every path
```

With `volatile GStruct25* p` (or a volatile global accessed directly), the
loads pin scheduling and the `li 2` is emitted separately on each path
(~93% match: correct control flow, wrong delay slots). Strip the qualifier:

```c
/* Global stays volatile (other functions need it). */
GStruct25* p;
p = (GStruct25*)&D_80071620[arg0];
switch (arg1) {
case 1: val = p->field_6; break;
case 3: val = p->field_8; break;
default: val = p->field_4; break;
}
if (arg1 == 2) {
    return (val & arg2) == arg2;
}
return (val & arg2) != 0;
```

`func_8002C868` is the pure example. This is the inverse of the
"keep local pointer volatile" rule used by `func_8002C9E0` on the same array.

## Prefer bare global field names when target CSEs a mid-struct address

"Hold a global's address in a local pointer" is the right default when the
target loads `%lo(D_xxx)` into `$sN` and then uses `off($sN)`. The inverse
shows up when the first access is a non-zero-offset array field and later
fields are reached by adjusting that same register:

```
lui    s0, %hi(D_80070F68)
addiu  s0, s0, %lo(D_80070F68+0x48)   # DRAWENV array
...
addiu  a0, s0, -0x28                  # DISPENV array (= base+0x20)
...
addiu  s0, s0, -0x48                  # back to struct base
lbu    v0, 0x100(s0)
```

A local `GStruct1* p = &D_80070F68` forces the base into a callee-saved reg
and emits `addiu a0, a0, 0x48` / `addiu a0, s2, 0x20` instead — correct
offsets, wrong CSE (~85%). Write the accesses by name:

```c
PutDrawEnv(&D_80070F68.field_48[arg0]);
PutDispEnv(&D_80070F68.field_20[arg0]);
if (D_80070F68.field_100 != 0) { ... }
if (D_80070F68.field_104 == 0) { ... }
```

`func_800282D8` is the pure example.

## `byte` is signed — cast to `u8` for `lbu`

`typedef signed char byte` in `include/decomp/types.h`. Reading a `byte`
field and assigning it to a wider integer emits `lb` (sign-extend). When the
target uses `lbu` (zero-extend), cast through `u8`:

```c
/* target: lbu v1, 4(v1) ; sw v1, 0x20(a0) */
val = (u8)ptr->field_4;   /* not bare ptr->field_4 */
dst->field_20 = val;
```

Scratch envs that invent a local `unsigned char field_4` will match locally
but fail the full build once the real `byte` typedef is used. Prefer the
cast over changing the struct field type when other code relies on `byte`
(or takes its address).

Also: if the target loads the byte early but stores it late, hold it in a
local (`val = (u8)...`) so the load schedules before intervening stores.

`func_8003FB20` is the pure example (`D4F564_8005ED64->field_4` →
`D_80062698->field_20`).

## BSS adjacency: hold the later symbol, step back by typed size

When the target loads `$s0 = &LaterSymbol` and reaches an earlier BSS object
as `-0xN($s0)` / `addiu v0, s0, -0xN`, two separate `extern` names will **not**
CSE into that form — GCC keeps a second `lui`/`addiu` for the earlier symbol
(~82% match).

If the earlier block has a fixed size that ends exactly at the later symbol,
derive the parent pointer from the later one with a typed step-back. Split the
cast and the arithmetic so the pointer-arithmetic linter stays quiet:

```c
/* D_800827A0..D_800827B0 is 0x14 bytes immediately before D_800827B4 */
p = &D_800827B4;
parent = (volatile GStruct56*)p;
parent = parent - 1;   /* sizeof(GStruct56) == 0x14 */
func_8004D200(p, (parent->field_2 >> 7) & 0xFF, 0, arg0);
parent->field_0 = 3;   /* sb …, -0x14(s0) */
```

`volatile` on the parent pointer forces `addiu v0, s0, -0x14` + `lhu a1, 2(v0)`
instead of a folded `lhu a1, -0x12(s0)`.

`func_80057B24` is the pure example (`D_800827B4` / `D_800827A0`).

## Pre-advance a walk pointer before the loop bound check

When the target puts `addiu ptr, ptr, stride` in the **delay slot of `blez`**
(the loop-entry guard) and only then does `i = count - 2` / the empty-loop
`beq i, -1`, write the pointer advance *before* the `if (count - 1 > 0)` test:

```c
/* BAD — GCC puts i = count-2 in the blez delay, table++ in the later beq delay */
i = count - 1;
if (i > 0) {
    table++;
    i = count - 2;
    if (i != -1) { do { ... } while (i != -1); }
}

/* GOOD — table++ fills the blez delay slot */
table++;
i = count - 1;
if (i > 0) {
    i = count - 2;
    if (i != -1) { do { ... } while (i != -1); }
}
```

The advance is a no-op on the early-return path (local only). Same shape as the
classic countdown `for (i = n - 2; i != -1; i--)` body; only the hoist of the
pointer step matters.

`func_8004D19C` is the pure example (u16 prefix table + 4-byte group headers).

## Dual-global update: read first to pin `a3`/`a2` order

When a basic block loads the addresses of two globals into `$a3` then `$a2`
(e.g. `Fs_CdErrorCount` then `Fs_CdOpStatus`) and stores to both around a
`jal`, a bare:

```c
Fs_CdOpStatus = 0x80;
Fs_CdErrorCount += 1;
CdControlF(CdlPause, NULL);
```

often schedules the *OpStatus* address into `$a3` first (because that store is
written first), swapping the two `lui`s relative to the target.

Force the target order by reading the incremented global into a local first so
its address is materialised before the other store:

```c
u8 errCount;

errCount        = Fs_CdErrorCount; /* lui a3,%hi(ErrorCount) first */
Fs_CdOpStatus   = 0x80;            /* lui a2,%hi(OpStatus) second */
Fs_CdErrorCount = errCount + 1;    /* delay-slot store of count */
CdControlF(CdlPause, NULL);
```

`func_80025C94` is the pure example (sector-mismatch soft-error path).

## Two consecutive values: write `== a || == a+1`, not `(u32)(x-a) < 2`

Both forms compile to `addiu`/`sltiu` range checks, but instruction scheduling
differs when the true path returns a constant that equals the range width.

```c
/* Mismatch: CSE puts `li v0, 2` in the delay of the *first* range branch and
   turns the second compare into `sltu` against that register. */
if ((u32)(stage - 1) < 2U) return 1;
if ((u32)(stage - 4) < 2U) return 2;

/* Match: equality form still lowers to sltiu, but keeps `addiu …, -4` in the
   first branch delay and `li v0, 2` in the second. */
if (stage == 1 || stage == 2) return 1;
if (stage == 4 || stage == 5) return 2;
```

`func_80025898` needs the equality spelling.

## Unsigned divide by 65535 needs `(u32)` cast

Target pattern for `n / 65535` when the product is treated as unsigned:

```
mult   a1, v0
mflo   v1
lui    v0, 0x8000
ori    v0, v0, 0x8001
multu  v1, v0
mfhi   v1
srl    a1, v1, 0xf
```

A plain signed `/ 65535` emits the signed-magic sequence (`mult` + bias +
`sra`/`subu`), which never matches.

```c
/* BAD — signed division magic */
var = (var * scale) / 65535;

/* GOOD — unsigned division magic (multu + srl 15) */
var = (s32)((u32)(var * scale) / 65535);
```

`func_8004D298` is the pure example (GStruct55 linear interpolator scale).


## GStruct43 voice list (owner GStruct57)

`func_800562B4` inserts a `GStruct43` at the head of a doubly-linked list owned
by `GStruct57`:

| Offset | Role |
|--------|------|
| owner `+0x40` | list head (`GStruct43*`) |
| node `+0x34` | parent owner (`GStruct57*`) |
| node `+0x38` | prev |
| node `+0x3C` | next |

Insert-at-head: if head exists, rewire `new->next = old`, `old->prev = new`,
`new->prev = NULL`, `owner->head = new`, `new->parent = owner`. If owner is
NULL, only clear the node's three link fields. Pair with `func_80056068`
(unlink/free) and `func_800563B4` (walk via `+0x3C`).

## Local jump table via struct assignment of function pointers

Dispatchers that index a small fixed table of `GFunc0` callbacks often copy the
table onto the stack first, then call through the local copy. The target uses a
3-word multi-load / multi-store:

```
addiu t0, v0, %lo(D_xxx)
lw    a1, 0(t0)
lw    a2, 4(t0)
lw    a3, 8(t0)
sw    a1, 0x10(sp)
sw    a2, 0x14(sp)
sw    a3, 0x18(sp)
```

Element-wise assignment (`sp[0] = table[0]; …`) does **not** produce this:
it emits separate `%lo` loads and a different index form (`lw v0, 0x10(sp+idx)`).

Match with a struct-of-array and structure assignment, then index the local:

```c
typedef struct {
    GFunc0 funcs[3];
} GFunc0Table3;

extern GFunc0Table3 D_800134D0;

void dispatcher(GStruct0* arg0)
{
    GFunc0Table3 sp;

    sp = D_800134D0;
    /* … setup … */
    sp.funcs[arg0->field_30](arg0);
}
```

The index form then becomes `addiu v1,sp,0x10` / `sll` / `addu v1,v1,v0` /
`lw v0,0(v1)`. `func_8002C028` is the pure example (3 entries). The same idea
applies to `D_800134BC` (5 entries) for the sibling dispatcher `func_8002BEA8`.

## `while (j < n)` vs `if (n) do{}while` for counter/dest reg pair

A byte-copy loop that increments both a counter and a destination pointer can
allocate those two locals swapped (`$a0`/`$v1`) depending on loop shape.

Target wants the counter in `$v1` and dest in `$a0`:

```
move  v1, zero        /* j = 0 */
beqz  a2, end         /* size == 0 */
 addu a0, a1, a2      /* dest = src + size */
addiu v1, v1, 1
lbu   v0, 0(a1)
...
sltu  v0, v1, a2
bnez  v0, loop
 addiu a0, a0, 1
```

`if (size != 0) { do { j++; *dest++ = *src++; } while (j < (u32)size); }`
puts `j` in `$a0` and `dest` in `$v1` (98% match, pure reg swap).

Use a pre-tested loop instead, and declare `size` before `dest`:

```c
u8* src;
s32 size;
u8* dest;
...
j = 0;
dest = src + size;
while (j < (u32)size) {
    j += 1;
    *dest = *src;
    src += 1;
    dest += 1;
}
```

`func_80033E58` is the pure example (`D_800610FC[1..8]` buffer duplicate).

## Switch result phi via `temp` + goto join

When a `switch` on `CdSync` (or similar) must *assign* a status used by a later
switch — not return it — GCC 2.8.1 wants a two-register phi:

```
li    v0, N
...
move  v1, v0    /* join */
/* second switch on v1 */
```

Writing `status = N; break;` in each case often loads `v1` directly and skips
the join. Match the target with an explicit temp and shared join label:

```c
s32 temp;
s32 status;

switch (CdSync(1, NULL)) {
case CdlComplete:
    state->field = 0;
    temp = 1;
    goto join;
case CdlNoIntr:
    goto set0;
case CdlDiskError:
    ...
    temp = 2;
    goto join;
default:
    temp = 2;
    goto join;
}
set0:
    temp = 0;
join:
    status = temp;
switch (status) { ... }
```

Defaults must also go through the join (not assign `status` and jump past it),
or the delay-slot `move v1, v0` / `li v0, 2` pattern breaks.

## Final irregular switch: gotos for case body order

The target layout for an irregular status switch (`li s0,1; beq case1; slti;
bnez ret0; ... beq case2; beq case3`) places case bodies as
`[case1][case2][case3]` right after the dispatch. A plain `switch` or
`if (status != 1) { ... } else { case1 }` often emits case1 last.

Force source order of bodies with gotos:

```c
one = 1;
if (status == one) {
    goto L_case1;
}
if (status < 2) {
    goto L_ret0;
}
if (status == 2) {
    goto L_case2;
}
if (status == 3) {
    goto L_case3;
}
goto L_ret0;
L_case1:
    ...
    return 1;
L_case2:
    ...
    return 0;
L_case3:
    ...
    /* fall through to shared return 0 when target does */
L_ret0:
    return 0;
```

`one = 1` before the tests lets CSE put `1` in `$s0` for both the compare and
later `field = one` stores. Falling case3 into `L_ret0` avoids an extra
`move v0, zero` / `j` before the shared epilogue path.

`func_8001E2D4` is the full example (two copies of the CdSync status machine
plus this final switch).

## Dual `func(0)` / `func(1)` calls vs a computed argument

When the target loads a signed byte, compares it to `1`, and builds the
argument to a call as either `0` or `1` with:

```
lb    v1, flag
li    v0, 1
bne   v1, v0, L_one
li    a0, 1
move  a0, zero
jal   func
 nop
```

writing a single call with a computed argument:

```c
s32 a0 = (flag == 1) ? 0 : 1;
func(a0);
/* or: func(flag != 1); */
```

is optimised by GCC 2.8.1 into branchless `lb`/`xori`/`sltu` (or
`lbu`/`xori`/`sltu` when the flag is unsigned). That is shorter than the
target and fails to match.

Emit two separate calls so the compiler keeps the branch and delay-slot
`li a0, 1`:

```c
if (flag == 1) {
    func(0);
} else {
    func(1);
}
```

Declare the flag as `s8` (not plain `char` / `u8`) so the load is `lb`,
matching the target. `func_8002BD24` (`D_80072311`) is the example.

## Dual `return -1` paths: early-exit reuses a preloaded `$v0`

When the target does:

```
li   v0, -1
beq  cond, v0, L_early   # early exit reuses this -1
...
bne  other, ..., L_fail
 nop
... success: return 1 ...
L_fail:
jr   ra
 li  v0, -1              # separate failure path reloads -1
L_early:
jr   ra
 nop                     # $v0 already -1
```

writing the natural early-exit first:

```c
if (cond == -1) {
    return -1;
}
if (other != magic) {
    return -1;
}
return 1;
```

merges both failures: the compiler puts `li v0,-1` in the second branch's
delay slot and drops the trailing reload. Invert the first test so the
success/fail body is nested and each `return -1` stays a separate exit:

```c
if (cond != -1) {
    if (other == magic) {
        /* work */
        return 1;
    }
    return -1; /* L_fail */
}
return -1;     /* L_early — reuses preloaded $v0 */
```

`func_8005664C` is a pure example.

## Same byte mask across a call: `andi` vs CSE'd `and`

When the same immediate mask (e.g. `0xF7`) is applied to a `u8`/`byte` field
both *before* and *after* a function call, GCC 2.8.1 CSE's the mask into a
callee-saved register as `~bit` (`li s1,-9` for bit 3) and emits `and` instead
of two `andi` immediates. That also inflates the stack frame for the extra
`$s` save.

Symptom: target has two `andi v0,v0,0xf7`; your build has `li s1,-0x9` plus
`and v0,v0,s1` on both sides of the `jal`.

Fix: keep the pre-call clear as a direct field expression (so it stays
`lbu`/`andi` into `$v0`), and route the post-call clear through a `u8` temp
plus a local pointer for the base:

```c
D_80082818.unknown_0[0] = D_80082818.unknown_0[0] & 0xF7;
/* ... */
func(...);
p = &D_80082818;
temp = p->unknown_0[2];
p->unknown_0[2] = temp & 0xF7; /* separate andi — mask not CSE'd into $sN */
```

Using the temp form on *both* sides also yields `andi`, but loads into `$a1`
instead of reusing `$v0`.

`func_800588D8` is the example.

## Non-volatile store reordered past volatile field stores

A bare `global = 0` on a non-volatile `s16` can be scheduled *after* later
volatile field stores and even into the epilogue (after `lw s0`), even when
it appears earlier in the C source. The target often wants it strictly
between two volatile updates.

Symptom: target has `lui`/`sh zero` of the halfword between two `sb`s on a
`volatile` struct; your build moves the `sh` to just before `jr ra`.

Fix: declare the halfword `volatile`. That pins the store in source order
relative to the surrounding volatile accesses:

```c
extern volatile s16 D_80082808;
/* ... */
p->unknown_0[2] = temp & 0xF7;
D_80082808 = 0; /* stays here when volatile */
D_80082818.unknown_0[0] = D_80082818.unknown_0[0] | 1;
```

`func_800588D8` / `D_80082808` is the example.

## Default return value belongs in the fall-through branch

When the target puts `li a1,-1` in the *delay slot* of a `beq`/`bne` (then
`move v0,a1` on both exit paths), do **not** initialize `ret` before the
`if`. An early `ret = -1;` is scheduled *before* the address load of the
struct being tested, which steals the delay slot for the fall-through's
first real instruction (`li v0,4`, etc.) and drops the match a few percent.

Put the default assignment *inside* the fall-through arm instead. GCC still
lifts it into the branch delay slot (it is independent of the arm's stores),
but leaves the prologue as pure `lui`/`addiu`/`lbu`/`li`/`beq`:

```c
/* Wrong: li a1,-1 first, delay slot filled with li v0,4 */
ret = -1;
p = &D_80082798;
if (p->field_0 != 3) {
    p->field_1 = 4;
    p->field_2 = 1;
} else {
    ret = 0;
    /* ... */
}
return ret;

/* Right: li a1,-1 in beq delay slot */
p = &D_80082798;
if (p->field_0 != 3) {
    ret = -1;          /* lifted into delay slot */
    p->field_1 = 4;
    p->field_2 = 1;
} else {
    ret = 0;
    /* ... */
}
return ret;
```

`func_80057ACC` is the example. Pair with `if (x != K)` so the branch is
`beq` to the else arm and the `!=` arm is fall-through (failure-first layout).

## `register … asm("v1")` for `lui v1; addiu v1, v1, %lo`

When a local is assigned an address-of-global in one arm and a small constant
in another, GCC often materialises the address as:

```
lui   v0, %hi(sym)
addiu v1, v0, %lo(sym)
```

even when the hard register for that local is already `$v1`. The target may
instead want the same-reg form:

```
lui   v1, %hi(sym)
addiu v1, v1, %lo(sym)
```

Pinning the local forces the high part into the result register:

```c
register s32 flag asm("v1");

if (all_banks) {
    flag = arg0 & 1;
    /* … fill loop … */
} else {
    flag = (s32)D_80082138; /* lui v1 / addiu v1,v1 */
    ((volatile u8*)flag)[idx] = arg0 & 1;
}
```

Use a `volatile` cast on the store when the target keeps `sb` *before* the
following `bnez` (delay slot holds the next `lui`, not the store). The project
already uses `register … asm("reg")` elsewhere (`func_8005287C`, heap init).

`func_8005454C` is the pure example: dual-purpose `flag` (loop fill value vs
bank-table base) needs `asm("v1")` for the address load form.
