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
