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

## `s16 ret = K` keeps `slti` while still pinning K in a callee-saved reg

When a function needs the same small constant K both as an early live return
value (`li s2,K` reused across calls via `move v0,s2` / `sb s2,...`) *and* as a
switch/range compare that the target emits as `slti ...,K`, an `s32 ret = K`
is too good: reload substitutes the SImode register into the compare and you
get `slt v0,v1,s2` plus `beq v1,s2,...` instead of `slti` / `li v0,K; beq`.

Symptom: prologue and stack frame already match (including `li s2,K`), but the
dispatch still differs by `slt` vs `slti` and a missing `li v0,K` before the
case-K equality test.

Fix: hold the return value in a narrower temporary so REG_EQUAL is HImode and
cannot supply the SImode compare operand:

```c
s16 ret;

ret = 3;
switch (p->field_2) {
case 1:
    /* ...; early exits reuse s2 via move v0,s2 */
    break;
case 2:
    p->field_2 = 3; /* still sb s2, ... — low byte of the HI reg */
    /* fallthrough */
case 3:
    if (func() == 0) {
        ret = 0;
    }
    break;
}
return ret; /* promotes to s32 at the return */
```

`CdAudio_DrivePhase0` is the pure example. Leaving `ret` as `s32` stuck at ~96% with
an otherwise identical switch.

Same fix for jump-table index shifts: `s32 ret = 2` is CSE'd into the
`index << 2` as `sllv v1,v1,s0` (shift amount is already in `$s0`), while the
target wants `sll v1,v1,0x2`. An `s16 ret` keeps the HImode register out of
SImode shift-amount CSE. `CdAudio_DrivePhase1` is the pure example — otherwise a
100% body with only the `sll`/`sllv` line wrong.

## Compute else-only address temps so they fill the `bne` delay slot in `$v0`

When the target does:

```
bne  v1, v0, else
 addiu v0, s1, -0x14   /* delay: parent = interp - 1 */
move  a0, zero         /* if-body: a0 for the next call */
...
else:
lhu   a1, 2(v0)
```

declaring the parent pointer *before* the `if` often allocates it to `$a0`,
which the if-body immediately clobbers with `move a0,zero`. Computing it only
inside the `else` lets delay-slot filling hoist the `addiu` while keeping the
result in `$v0`:

```c
if (interp->field_0 == interp->field_4) {
    /* equal path — never mentions parent */
    func_X(0);
} else {
    parent = (volatile CdAudioLocEx*)interp - 1;
    func_Y(parent->field_2);
}
```

`CdAudio_DrivePhase0` needs this (together with the `s16 ret` tip above) for the
`LinInterp_CdStream` / `CdAudio_Loc` pair linked by CdAudioLocEx.

## `volatile` blocks delay-slot filling

`-fdelayed-branch` will not move a volatile memory access into a `jal` delay
slot. When the target has a `nop` in a delay slot that an adjacent store could
obviously have filled, the global involved is probably `volatile` — most often
because it is shared with an interrupt or VSync callback.

Corollary: this is a useful signal *about the game*, not just a matching trick.
`D_8005EC70` is written by the VSync callback `Display_VSyncCallback` and read by the
main loop `GameMain_Loop`, so `volatile` is semantically correct there.

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

`CdAudio_SetLocBase` (`CdAudio_Loc.field_4 = arg0`) is a pure example — only the
`volatile CdAudioLoc` form matches.

`D_800680C0` is another interrupt-shared flag: the SPU timer callback
`Spu_TimerCallback` / `Spu_TimerReentryWork` reads and writes it while main-line
`Spu_InitSystem` does the same. Marking it `volatile` keeps stores out of
`jal` delay slots (target has `nop` after `D_800680C0 = 0`).

`D_8006EC30` / `D_80070E38` are the same shape for the draw path: main-line
`Display_FrameFlipDraw` writes them (copies of `Display_State.field_100` /
`field_103`) and the VSync callback `Display_VSyncCallback` → `Display_FlipDraw` reads
them. Without `volatile`:

- successive `if (D_8006EC30 == …)` arms CSE the load (target reloads via a
  `%hi` kept in `$v1` from the earlier branch delay slot);
- `(s8)D_80070E38 < 0x10` collapses to a single `lb` instead of
  `lbu` + `sll 24` + `sra 24`.

**Writer/reader conflict on the same global.** The reader (`Display_FlipDraw`)
needs `D_8006EC30` volatile, but the writer (`Display_FrameFlipDraw`) must put the
store in the `jal ExitCriticalSection` / `jal Display_FlipDraw` delay slot.
Keep the global `volatile` and store through a non-volatile lvalue:

```c
*(u8*)&D_8006EC30 = temp->field_100; /* fills jal delay slot */
ExitCriticalSection();
```

`D_8005EC74` / `D_8005EC78` are the same VSync-shared pair on the lag path
(`Display_VSyncCallback` writes `D_8005EC74` and reads `D_8005EC78`; `Display_FrameFlipDraw`
does the inverse). Mark both `volatile` so the draw path reloads `D_8005EC74`
twice and keeps `D_8005EC78 = 0` *outside* the following `jal VSync` delay
slot.

## Interleave an independent load with `*org = CONST`

When the target builds a wide constant (`lui`/`ori` of `0xFFFFFF`) around a
pointer store and also needs an unrelated global for a later multiply, stage
the accesses so GCC fills the `lw org` delay with the global load:

```c
org  = ot[i].org;
size = D_8007A0E4;           /* load fills the org-load delay; claims $a0 */
*org = C5F414_OTAG_END_PRIM; /* 0xFFFFFF stays in $a1 */
size /= 2;                   /* signed /2 after the store */
saved      = D_800710A0;
D_800710A0 = ot[i].org;      /* second load of org — do not reuse `org` */
D_80071190 = base + i * size;
```

Putting `saved = D_800710A0` immediately after `*org = …` steals the delay
slot for `%hi(D_800710A0)` and parks the constant in `$a0` instead. Computing
`D_8007A0E4 / 2` in one expression before the store also mis-orders the
divide relative to the store. `Display_FrameFlipDraw` is the pure example.

## Hold a global's address in a local pointer

When a function touches the same global struct across several calls, the target
usually loads its address once into a callee-saved register (`lui`/`addiu` into
`$s0`, then `off($s0)` everywhere). Referring to the global by name gives you a
fresh `lui %hi(...)` after every call instead, often stuffed into delay slots
that should hold a `nop`.

Assign it to a local pointer first — the same trick `Task_ExecList` in
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
read/write of the global happens *before* any `jal`, a bare `Display_State.field`
name matches fine: GCC loads the address into a temporary (`$v1`) once and
never needs to reload it. `GameFlow_SpawnMainWhenReady` is an example — it reads
`field_101`, optionally writes `field_10b`, then only calls other functions.
A local `DisplayState*` would force a callee-saved register and a larger stack
frame for no benefit. Use the pointer only when the address is live across
calls.

**Hybrid — direct on the no-call arm, pointer only on the call arm.** When the
target holds the address in `$v1` through an early branch that never calls, then
reloads it into `$s0` only on the fallthrough path that *does* call, do not
share one local pointer for both arms. Access the global by name (or return
early) on the no-call arm, and introduce `p = &global` only after joining the
call path. A single `p` live across both arms pins `$s0` for the whole function
and mismatches prologue/early stores (`CdStream_Continue` / `CdStream_State`).

**Also: capture a dest address before a call that you assign after.** Writing
`Fs_ChunkWritePtr = (u8*)&Fs_CdSector` after `CdSync` materialises the address
post-call. Pre-assigning `dest = (u8*)&Fs_CdSector` before `CdSync` puts the
address in `$s1` and fills the call delay slot with `addiu s1, v0, %lo(...)`,
which also forces `a1` for the call to be set earlier (matching the target
prologue). `Fs_SelectStage` is the example — same shape as `Fs_ReadSectorEx`
where the dest is already a parameter.

**Post-call store that uses `$s0` for `%hi/%lo`.** Even a single
`global = func(...)` after a call can need the local-pointer form when the
target saves `$s0`, does `lui s0,%hi(global)` / `sh v0,%lo(global)(s0)`, then
restores `$s0`. A bare store picks `$v1` and drops the save/restore
(`CdVol_RegisterCallbacks` / `D_8006EBF2`):

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
/* Matches: lui of Gpu_OtBuffers, then li a1,0xA */
otCtx = Gpu_OtBuffers;
depth = 0xA;
otCtx->field_0 = depth;
/* ... otCtx[1].field_0 = depth reuses a1 */

/* Mismatches: li first, then lui */
depth = 0xA;
otCtx = Gpu_OtBuffers;
otCtx->field_0 = depth;
```

`Gfx_InitGraph` is the example — `depth` is shared across both OT buffers.

**Inverse — constant first, then pointer.** When the target does `li aN,K` *then*
`lui %hi(global)`, assign the constant before the address. A bare
`ptr->field = 1` often schedules `li` after the address load; force the order
with a local:

```c
/* Matches: li a1,1 then lui/addiu of SndEvt_Pool */
i = 0;
flag = 1;
for (ptr = SndEvt_Pool; i < 0x40; i++, ptr++) {
    if (ptr->field_0 == 0) {
        ptr->field_0 = flag;
        ...
    }
}
```

`SndEvt_Alloc` is the example.

**Hybrid — pointer for early accesses, global name after a call.** When the
target keeps the address in `$s0` for pre-call loads/stores but reloads with a
fresh `lui`/`addiu %lo` for a *post-call* store (instead of `off($s0)`), use the
local pointer only up through the call and name the global for the later write:

```c
volatile CdReadyQueue* p = &CdReady_Queue;
if (p->field_1 == 0) {
    p->field_4 = CdReadyCallback(arg0); /* uses $s0 */
} else {
    CdReadyCallback(arg0);
}
CdReady_Queue.field_1 = 1; /* reloads address into $v0 — not $s0 */
```

`CdReady_InstallCallback` needs this plus `volatile` on the global (base+offset `sb`, not
`%lo(sym+1)`). Writing `p->field_1 = 1` keeps `$s0` and mismatches.

**Indexed volatile arrays — multiply before base load.** A direct
`arr[i].field = 0` on a `volatile` global often schedules the `lui`/`addiu` of
the array base *before* the stride multiply. The target for simple setters
usually does the multiply first, then materializes the base. Force that order
with a local pointer:

```c
/* Wrong schedule: lui/addiu base, then i*stride */
Pad_States[arg0].field_A = 0;

/* Right schedule: i*stride, then lui/addiu base */
volatile PadState* p;
p = &Pad_States[arg0];
p->field_A = 0;
```

Keep the local pointer `volatile` as well so the store stays out of the `jr`
delay slot (a plain `PadState*` still multiplies first but fills the slot with
`sb`). `Pad_ClearCooldown` is the minimal example.

**Inverse — non-volatile array: base before index.** For a plain (non-volatile)
array, `&arr[(s8)i]` often schedules the signed index shift *before*
`lui`/`addiu` of the base. When the target materializes the base first, assign
the array to a local pointer, then index through that:

```c
/* Wrong schedule: sll/sra index, then lui/addiu base */
p = &SndBank_Slots[(s8)arg0];

/* Right schedule: lui/addiu base, then sll/sra index */
SndBankSlot* base;
base = SndBank_Slots;
p = &base[(s8)arg0];
```

`SndBankSlot_Free` needs this form so `F3D458_Free` can take `p->field_0` with the
base already in `$v0` before the stride multiply lands in `$s0`.

## `~x != 0` for `nor` + `sltu` (not `x != -1`)

When the target does:

```
nor  v0, zero, v0
sltu v0, zero, v0
```

write `return ~x != 0;` (or the same expression in a larger return). That is
semantically `x != -1`, but `x != -1` often compiles to a different compare
sequence. `SndVoice_HasActiveId` is a one-liner that only matches with the `~` form:

```c
return ~SndVoice_FindById(SndBank_RemapId()) != 0;
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
reloading it for the rest of the function (`CdVol_SetMixMode`):

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

Match with an unsigned counter and a `do`/`while` (`AsyncCb_Reset`):

```c
u32 i;
s32* ptr;

ptr = AsyncCb_Queue;
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

## Dummy pin that outlives a short constant (Ui_SpawnTextBlock)

When the target holds a small constant K in `$s5` only during early init, then
reuses a *different* callee-saved (`$s3`) for a later accumulator that would
otherwise coalesce with K's color, a plain `s32 flag = K` lets CSE put both in
the same register and steals `$s3` from the `%hi` of a global.

Pin a dummy to the short-lived constant's register and keep it live through the
second half without using it for real work. That reserves `$s5` for K, forces
the accumulator into `$s3` (sharing with the dead `%hi`), and leaves the rest of
the coloring alone:

```c
register s32 dummy asm("s5");
...
dummy = 1; /* before if (task) — also fills beqz delay like the target */
if (task != NULL) {
    obj->field_0  = dummy;
    ...
    obj->field_16 = dummy;
}
...
if (result != NULL) {
    maxWidth = 0;      /* lands in $s3, not $s5 */
    dummy    = dummy;  /* keep dummy live into this block */
    ...
    asm volatile("" ::"r"(dummy)); /* and through the end */
}
```

Assigning the constant *before* `if (task != NULL)` matches the MIPS delay-slot
semantics of `beqz task; li s5, 1` (the `li` always runs). An `asm volatile`
barrier right after `dummy = 1` inside the `if` often inserts a `nop` in the
delay slot when the variable is hard-pinned — prefer the pre-branch assign.

## Stack-reuse `union` for sequential TaskDesc / RECT

When the target reuses one stack slot as a `TaskDesc` then as a `RECT`, declare:

```c
union {
    TaskDesc desc;
    RECT     rect;
} sp;
```

Separate locals (even in nested scopes) often get simultaneous slots under
GCC 2.8.1 and blow the frame / shift every access.

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
- [0x3764, rodata, cdcmd_1]  # tables still owned by assembly functions
- [0x37f4, .rodata, cdcmd]   # table generated by src/main/cdcmd.c
```

**The names must differ.** A `rodata` and a `.rodata` subsegment sharing a name
both write `asm/USA/main/data/<name>.rodata.s`, and the later one silently wins
— which deletes the earlier tables and fails at link time with `undefined
reference to jtbl_*`. Rename the assembly remainder (`cdcmd_1` above) and drop
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

## Force prologue moves before the first load (delay-slot fill)

When the target opens with several independent `move`s *then* an `lbu`/`lw`
with a hard `nop` delay, the scheduler will gladly fill that delay with one of
the moves (or park a late-live arg copy in the `beqz` delay slot). Symptom:

```
# target                         # your build
move  t5,a0                      move  t5,a0
move  t0,zero                    move  t0,zero
move  t1,a2                      lbu   a3,0(a1)
lbu   a3,0(a1)                   move  t1,a2   /* filled load delay */
nop                              beqz  a3,end
beqz  a3,end                     move  t2,t0
 move t2,t0
```

Pin the early registers and emit a multi-output empty asm *after* the moves so
nothing that follows can be scheduled before them (`Text_MeasureGlyphWidth`):

```c
register TextDrawReq* ctx asm("t5");
register s32 width asm("t0");
FontGlyph* glyph;

ctx   = arg0;
width = 0;
glyph = (FontGlyph*)arg2;
asm("" : "+r"(ctx), "+r"(width), "+r"(glyph));
c = *arg1; /* only now may the load be emitted */
```

A single `+r` on one variable is not enough if the others are still free to
slide past the load.

## Shared flag block that reloads from memory

When the target joins two early exits into:

```
li   a0, 1
lbu  v0, 0(a1)   /* reload — same address, CSE would drop it */
beq  v0, a3, ...
```

writing `end_flag = 1; ch = *arg1;` is not enough: CSE knows the load is
redundant. Defeat it with a memory clobber between the store-to-reg and the
reload (`Text_MeasureGlyphWidth`):

```c
if (ch != 0 && ch != nl) {
    goto check_bs;
}
end_flag = 1;
asm("" ::: "memory");
ch = *arg1;
check_bs:
...
```

The `if (ch != 0 && ch != nl) goto` / fallthrough shape is what produces the
shared `li`+`lbu` block; an `if (ch == 0 || ch == nl) { ... }` tends to either
drop the reload or emit an `andi` on a `u8` temporary.

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
`N`, and access fields through that pointer. `SndEvt_HandleVolumeRamp` does this with
`SndEvtFrom4` overlaid at `&arg0->field_4`:

```c
SndEvtFrom4* mid = (SndEvtFrom4*)&arg0->field_4;
temp = SndVoice_FindById(mid->field_4); /* was arg0->field_8 */
if (temp >= 0) {
    SndVoice_SetVolumeRamp(temp, mid->field_1); /* was arg0->field_5 */
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
success-path setup. `Spu_TimerReentryWork` needs this shape (with `volatile`
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
`CdCmd_GetOverlayStatus` needs this pattern.

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
`$v0` and breaks the later `li v0,0x2000` delay-slot preload. `SndLoad_AllocBuffer`
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

`Snd_InitFromStage` needs this on `D_80082135` and the surrounding
`D_80082128` / `D_80082124` / `D_80082130` stores.

Halfword analogue: a plain `s16` load is usually `lh`, but the target may want

```
lhu  v0, %lo(sym)(v0)
li   v1, 1
sll  v0, v0, 16
sra  v0, v0, 16
```

Use a volatile load through the existing symbol rather than flipping its type
(other matched sites may store through `s16`):

```c
if (*(volatile s16*)&D_800689EC == 1) { ... }
```

`Midi_IsChannelFree` needs this form for `D_800689EC`.

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
hi/next/lo interleaving without changing semantics (`Task_Unlink`):

```c
next = state->node.next;
head = Task_ActiveList;
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

`Ui_GetCursorFixed` is the pure example (fixed-point globals `D_80067648` /
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

`Midi_InitChannelTable` (init of `MidiOpcodeCtx::field_484[16]`) is the pure example.

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

`CdCmd_PollStatus` needs this so the shell-open path can delay-slot-fill
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
lose the `sll`. `CdSync_IsShellOpenBitSet` was retyped from `bool` to `s16` so
`CdCmd_PollStatus` (and other CD helpers that already had the `sll` in target asm)
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

`Midi_SetMasterVolume` needs this so both stores share one `lui v1, %hi(D_8007F2F0)`.

## One-iteration `for (i = 0; i <= 0; i++)` + array stride

Several audio helpers walk `Midi_Song` with stride `0x5DC` and loop condition
`blez` after `i++` from zero — i.e. exactly one iteration. A pointer `++` do/while
often becomes a countdown (`addiu -1` / `bgez`). Array indexing matches:

```c
i = 0;
val = 0xFFFF;
for (; i <= 0; i++) {
    (&Midi_Song)[i].field_C = val;
}
```

The element type must be size `0x5DC` (logical stride). BSS for `Midi_Song` is
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

/* mvmva sf=1, mx=0 (rot), v=3 (IR), cv=3 (none), lm=0 → RTIR */
#define gte_rtir_real() __asm__ volatile("nop; nop; .word 0x4A49E012")
```

`Gfx_ApplyMatrixNoSf` is the template: `gte_ldsvrtrow0` + `gte_ldv0` + custom
command + `gte_stlvnl0`. Standard `gte_rtv0` is `mvmva 1,0,0,3,0`
(`0x4A486012`); the sf=0 variant drops the 12-bit shift (`0x4A406012`).

`gte_MulMatrix0` from `gtemac.h` is fine if `gte_rtir` is swapped for
`gte_rtir_real` — load/store helpers (`gte_SetRotMatrix`, `gte_ldclmv`,
`gte_stclmv`) already emit real MIPS. `Gfx_MatrixToEuler` is the template.

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
semantics, wrong layout. `Snd_GetNote` only matches with the `!= NULL`
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
instruction and shifting every later label. `SndVoice_Alloc` only matches with
the `s32` + `(s8)` form.

Note also that `SndBank_Slots` is walked two ways: as `SndBankSlot[16]` (stride
`0x10`, via `SndBankSlot_Get` / `SndBankSlot_Free`) and as `SndVoice` slots
(stride `0x40`, via `SndVoice_Alloc`). Cast the base rather than changing
`SndBankSlot`.

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

a single expression (`Task_DescBanks[arg0] + arg1` or `&Task_DescBanks[arg0][arg1]`)
schedules the multiply first and loads into `$v1` instead, breaking the match.

Split the load from the index:

```c
TaskDesc* ptr;

if (arg0 >= 0) {
    ptr = Task_DescBanks[arg0];
    ptr = &ptr[arg1];
} else {
    ptr = (TaskDesc*)arg1;
}
return Task_SpawnFromDesc(ptr, arg2, arg3, Task_ActiveList);
```

Also: use `if (arg0 >= 0)` (not `arg0 < 0`) so the fall-through is the table
path and the branch is `bltz` to the cast path — that matches the shared
post-merge arg shuffle (`a1=a2`, `a2=saved a3`, `a3=Task_ActiveList`) of
`Task_Spawn`. Dual early returns force separate call setup and reg-shuffle
the args too early.

## `while (1)` for linked-list walks that re-enter at the null check

A normal `while (node != NULL)` puts the null test at the bottom of the loop
body as `bnez`/`beqz` after advancing the pointer. When the target instead does
an unconditional `j` back to a top-of-loop `beqz s0, end` (often because one
path assigns `node = remove(node)` and the other does `node = node->next`),
write:

```c
node = (AudioTickNode*)head->field_14;
while (1) {
    if (node == NULL) {
        break;
    }
    if (callback != NULL) {
        if (callback(node->field_c) == -1) {
            node = AudioTick_Remove(node);
            continue;
        }
    }
    node = (AudioTickNode*)node->field_14;
}
```

Also load `head = &global` *before* the enable-flag check so GCC materializes
both `%hi/%lo` pairs up front (see "Hold a global's address in a local
pointer"). Nested `if (flag) { if (head != NULL) { ... } }` rather than
`flag && head` keeps the second null test in the first's delay-slot region.
`AudioTick_Process` is the reference.

## K&R definition when a same-TU caller uses indeterminate args

A modern prototype definition (`s32 f(s32 x) { ... }`) is visible to later
call sites in the same translation unit. If an already-matched caller invokes
the function with a bare `f()` and a `nop` delay slot (so `$a0` is whatever
garbage was left), introducing a prototype makes that call a hard error
("too few arguments").

Use an old-style K&R definition instead — it does **not** create a prototype,
so the no-arg call stays legal and the callee still matches:

```c
s32 SndBank_RemapId(arg0)
s32 arg0;
{
    /* ... */
}
```

Keep the header declaration unprototyped too (`extern s32 SndBank_RemapId();`).
`SndVoice_HasActiveId` → `SndBank_RemapId` is the reference.

## `switch` for equality chains that branch *to* case bodies

When the target does positive equality tests that jump *to* handlers
(`beq`/`beqz` to the case, default falls through then `j` past the bodies),
an `if` / `else if` chain usually emits the inverse (`bne` past an inlined
body). Sparse multi-way selection matches as a `switch` instead:

```c
/* Target: beqz x, case0 / beq x,5,case5 / default then j continue */
switch (arg0->field_C) {
case 0:
    table = Font_Glyphs0;
    break;
case 5:
    table = Font_Glyphs2;
    break;
default:
    table = Font_Glyphs1;
    break;
}
```

`Text_MeasureAndCenter` is a pure example: two independent `switch`es (glyph table by
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

`if (arg0 != 0)` swaps the arms and GCC emits `beqz` with the non-zero path as fall-through — same code, inverted control flow, large score drop. `Mc_CopyFileName` is a short example (copy between `Mc_FileName` / `Mc_FileNameBuf`).

**`return` in the `== 0` arm defeats this.** GCC sinks return blocks to the
function end and rewrites the test as `beqz` → exit, with the continue path as
fall-through — even when you wrote `if (x == NULL)`. Prefer `break` out of a
`do { ... } while (cond != NULL)` so the null arm stays the fall-through of
`bnez` and the non-null arm ends with an unconditional `j` back to the loop
(GCC proves `cond` is true after the null check and folds the while test):

```c
do {
    /* ... */
    next = cur->field_18;
    free_node(cur);
    if (next == NULL) {
        tail = NULL;
        head = NULL;
        break;              /* not return — keeps bnez fall-through */
    }
    head = next;
} while (next != NULL);      /* folds to `j loop` on the non-null path */
```

`SndEvt_Process` only matches with this shape; `return` in the null arm stuck at
~94% with inverted `beqz` and a missing `j` to the shared epilogue.

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
CdCmd_EntryIter = index + 1;
CdCmd_EntryIter = CdCmd_EntryIter % 8;  /* not &= 7, not (index+1)&7 */
```

`CdCmd_NextEntry` (8-entry queue walk of `CdCmd_Queue.entries`) is a pure example.
The same double-store shape appears on `field_1c8` / `field_1ca` updates in the
nearby ring producers (e.g. `CdCmd_CommitReplace`).

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

`SndEvt_FlushType5Pending` / `Midi_UpdateVoiceVolumes` both `lb` `D_800820E9`; stores remain `sb`
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

`CdStream_SetFlag14` is a pure example (bit 7 of `CdStream_State.unknown_0[1]`).


## Early-load order among independent `&= ~mask` globals

When a function sets a bit in one global and clears it from two others
(`A |= mask; B &= ~mask; C &= ~mask`), and all three are *separate* symbols
(not fields of one struct), three codegen details interact:

1. **Local pointer on the `|=` target.** Assign `&A` to a local before the
   cast/shift. That forces the early `lui a1, %hi(A)` the target wants; without
   it GCC sign-extends the argument first and puts the mask in `a1` instead of
   `v1`.

2. **`nor` placement is target-dependent.** Compare where `nor` sits relative
   to the `sw` of A:
   - Target has `nor` *after* `sw` of A → write `~channel` twice inline:
     `B &= ~channel; C &= ~channel;`. A shared temp (`inv = ~channel`) moves
     `nor` too early. (`Spu_KeyOff`)
   - Target has `nor` *before* `sw` of A → assign into the channel temp:
     `channel = ~channel; B &= channel; C &= channel;`. (`Spu_KeyOn`,
     where B is a struct field via the same local pointer as other accesses)

3. **Source order of the two `&=` is not store order.** Writing
   `B &= ~mask; C &= ~mask;` can early-load C and late-load B (or vice versa).
   If the target early-loads one particular global into `$a2`, swap the two
   statements until that symbol is the early one — the final store order still
   ends up matching because of scheduling.

`Spu_KeyOff` is the pure late-`nor` example: pointer on `D648E0_8007EBB0`,
then `D648E0_8007EBA8 &= ~channel` before `D648E0_8007EBAC &= ~channel` so
that EBAC is the early-loaded `$a2` value. `Spu_KeyOn` is the pure
early-`nor` counterpart: pointer on the `|=` target plus `channel = ~channel`
before the two clears.

`Spu_ArmKeyOn` is late-`nor` with an extra struct-field clear in the middle
(`A |= mask; field &= ~mask; B &= ~mask; C &= ~mask`). Same recipe as E71C:
local pointer on A (`&D648E0_8007EBAC`) and three inline `~channel` uses — not
`channel = ~channel`. Dropping the pointer alone leaves only `li a1,1`
mis-scheduled before the `sb`; using `channel = ~channel` with the pointer
falls back to ~70%.

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
} SessionBytesAt4;

((SessionBytesAt4*)dst)->field_4 = ((SessionBytesAt4*)src)->field_4;
```

`GameFlow_CopySaveIds` is the pure example (`Game_Session` ← `Mc_SaveData`).

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

`Pad_ReadButtonsInv` is the pure example (`Pad_RawPorts`, stride 0x24).

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
} GameOt; /* STATIC_ASSERT_SIZEOF(..., 0x14) */
```

Init pattern (see `Gpu_InitOtSmall`): hold `GameOt* ot = Gpu_OrderingTables`, write
`length`/`org` for both slots, with the second `org` as `tags + (1 << length)`.
OT tag storage of `0x200` bytes is two buffers of `0x100` (`u_long[0x80]`).

When calling PsyQ `GsClearOt`, declare it with `GameOt*` (in `game.h`) rather
than including `libgs.h` or casting through `GsOT*`:

```c
void GsClearOt(unsigned short offset, unsigned short point, GameOt* otp);
/* ... */
GsClearOt(0, 0, &ot[temp->field_118]);
*ot[temp->field_118].org = C5F414_OTAG_END_PRIM;
D_800710A0 = ot[temp->field_118].org;
```

`Gpu_InitOt` is the reference: sets both `Gpu_OrderingTables` slots to depth `0xA`
with `D5F414_OrderingTables` / `+ C5F414_OTAG_ENTRIES`, clears the active buffer
(`Display_State.field_118`), then points `D_800710A0` at the OT base.

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

`Snd_FindBank` is the pure example.

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

`Text_SkipLines` is the pure example. A plain `do { ... } while (arg1 > 0)` with
the same locals peels the null check and reintroduces `andi` masks.

## "Dead" `andi reg, 0xffff` before `jr` is often a u16 return

When the epilogue looks like:

```
lhu  v0, idx
addiu v1, v0, 1
sh   v1, idx
andi v1, v1, 0xffff
andi v1, v1, 7
andi v0, v0, 0xffff   /* appears unused */
jr   ra
 sh  v1, idx
```

the final `andi v0, v0, 0xffff` is **not** a scheduler leftover — it is
zero-extending a `u16` return value that already lives in `$v0`. The function
is not `void`; it returns the pre-increment index (or another u16 that stayed
in `$v0`). Callers that ignore the result do not prove the prototype is
`void`.

```c
s32 CdCmd_Enqueue(...)  /* not void */
{
    u16 writeIdx;
    u16 next;

    writeIdx = p->writeIdx;
    next = writeIdx + 1;
    p->writeIdx = next;
    next = p->writeIdx % 8;
    p->writeIdx = next;
    return writeIdx;  /* andi v0, v0, 0xffff */
}
```

Without the `return`, GCC reuses `$v0` for the increment and the extra `andi`
never appears. `CdCmd_Enqueue` is the pure example — stuck at ~97% with a
`void` signature until the return type was corrected.

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
`CdCmd_NextEntry` (`CdCmd_EntryIter = index + 1; CdCmd_EntryIter = CdCmd_EntryIter % 8;`) is the
matched precedent; `CdCmd_HandleMount` needs the same form for `field_1ca`.

Same rule applies to **loop indices** that index `entries[i]` each iteration. A
u16 walk of the ring:

```c
i = p->readIdx + 1;
i = i % 8;
if (i != writeIdx) {
    do {
        p->entries[i].cmd = 0;
        i = i + 1;
        i = i % 8;
    } while (i != p->writeIdx);
}
```

needs `% 8` on both the initial wrap and the loop step. Using `i = (i + 1) & 7`
lets GCC fold the zero-extend into the address calc (`sll` in the branch delay
slot, pointer in `$a1` instead of `$a0`) and breaks the
`andi v0, v1, 0xffff` / `addiu v1, v1, 1` / `sll v0, v0, 3` shape.
`CdCmd_DropPending` is the pure example.

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
`field5` lands in `$s3`. `CdCmd_HandleMount` needs this for the case-0x54 prologue.

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

`CdCmd_HandleMount` cleanup needs this; prefer struct indexing when the operand
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
`CdAudio_PrepareNextEntry` is the pure example.

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

`CdStream_IsBusy` is the pure example. An if/else that assigns into `ret` and
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

`Mc_VerifyFirstByteChecksum` is the pure example. Same body with `i` then `p` scores ~94%.

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
Mc_SaveData.field_940 = next;
Mc_SaveData.field_942 = ~next;
```

Also keep an intermediate `base = Mc_BufferSlots; p = base + 1;` so the address
forms as `addiu v0, %lo(Mc_BufferSlots)` then `addiu a0, v0, 0xC` rather than a
folded `%lo(Mc_BufferSlots+0xC)`. `Mc_WriteFirstByteChecksum` is the pure example; its verify
sibling `Mc_VerifyFirstByteChecksum` uses a plain `s32 sum` and different scheduling.

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
p = SndScript_Slots;
do {
    if ((p->field_16 & mask) && (p->field_0 == arg0)) {
        return i;
    }
    i++;
    p++;
} while (i < 8);
return -1;
```

`SndVoice_FindById` is the pure example. Signed `i` + `do`/`while` also produces
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

With `volatile PadState* p` (or a volatile global accessed directly), the
loads pin scheduling and the `li 2` is emitted separately on each path
(~93% match: correct control flow, wrong delay slots). Strip the qualifier:

```c
/* Global stays volatile (other functions need it). */
PadState* p;
p = (PadState*)&Pad_States[arg0];
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

`Pad_CheckButtons` is the pure example. This is the inverse of the
"keep local pointer volatile" rule used by `Pad_ClearCooldown` on the same array.

## Prefer bare global field names when target CSEs a mid-struct address

"Hold a global's address in a local pointer" is the right default when the
target loads `%lo(D_xxx)` into `$sN` and then uses `off($sN)`. The inverse
shows up when the first access is a non-zero-offset array field and later
fields are reached by adjusting that same register:

```
lui    s0, %hi(Display_State)
addiu  s0, s0, %lo(Display_State+0x48)   # DRAWENV array
...
addiu  a0, s0, -0x28                  # DISPENV array (= base+0x20)
...
addiu  s0, s0, -0x48                  # back to struct base
lbu    v0, 0x100(s0)
```

A local `DisplayState* p = &Display_State` forces the base into a callee-saved reg
and emits `addiu a0, a0, 0x48` / `addiu a0, s2, 0x20` instead — correct
offsets, wrong CSE (~85%). Write the accesses by name:

```c
PutDrawEnv(&Display_State.field_48[arg0]);
PutDispEnv(&Display_State.field_20[arg0]);
if (Display_State.field_100 != 0) { ... }
if (Display_State.field_104 == 0) { ... }
```

`Display_PutEnvAndDraw` is the pure example.

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

`Display_BeginTransition` is the pure example (`Game_Session->field_4` →
`Stage_Ctx->field_20`).

## BSS adjacency: hold the later symbol, step back by typed size

When the target loads `$s0 = &LaterSymbol` and reaches an earlier BSS object
as `-0xN($s0)` / `addiu v0, s0, -0xN`, two separate `extern` names will **not**
CSE into that form — GCC keeps a second `lui`/`addiu` for the earlier symbol
(~82% match).

If the earlier block has a fixed size that ends exactly at the later symbol,
derive the parent pointer from the later one with a typed step-back. Split the
cast and the arithmetic so the pointer-arithmetic linter stays quiet:

```c
/* CdAudio_Loc..D_800827B0 is 0x14 bytes immediately before LinInterp_CdStream */
p = &LinInterp_CdStream;
parent = (volatile CdAudioLocEx*)p;
parent = parent - 1;   /* sizeof(CdAudioLocEx) == 0x14 */
LinInterp_Setup(p, (parent->field_2 >> 7) & 0xFF, 0, arg0);
parent->field_0 = 3;   /* sb …, -0x14(s0) */
```

`volatile` on the parent pointer forces `addiu v0, s0, -0x14` + `lhu a1, 2(v0)`
instead of a folded `lhu a1, -0x12(s0)`.

`CdAudio_StartVolumeRamp` is the pure example (`LinInterp_CdStream` / `CdAudio_Loc`).

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

`Snd_BuildGroupIndex` is the pure example (u16 prefix table + 4-byte group headers).

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

`Fs_StreamReadyCb` is the pure example (sector-mismatch soft-error path).

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

`Fs_GetStageDiskKind` needs the equality spelling.

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

`LinInterp_Apply` is the pure example (LinInterp linear interpolator scale).


## SndVoice voice list (owner SndVoiceOwner)

`SndVoice_Attach` inserts a `SndVoice` at the head of a doubly-linked list owned
by `SndVoiceOwner`:

| Offset | Role |
|--------|------|
| owner `+0x40` | list head (`SndVoice*`) |
| node `+0x34` | parent owner (`SndVoiceOwner*`) |
| node `+0x38` | prev |
| node `+0x3C` | next |

Insert-at-head: if head exists, rewire `new->next = old`, `old->prev = new`,
`new->prev = NULL`, `owner->head = new`, `new->parent = owner`. If owner is
NULL, only clear the node's three link fields. Pair with `SndVoice_Detach`
(unlink/free) and `SndScript_TickVoices` (walk via `+0x3C`).

## Local jump table via struct assignment of function pointers

Dispatchers that index a small fixed table of `TaskFunc` callbacks often copy the
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
    TaskFunc funcs[3];
} TaskFuncTable3;

extern TaskFuncTable3 D_800134D0;

void dispatcher(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_800134D0;
    /* … setup … */
    sp.funcs[arg0->field_30](arg0);
}
```

The index form then becomes `addiu v1,sp,0x10` / `sll` / `addu v1,v1,v0` /
`lw v0,0(v1)`. `GameFlow_DispatchTable` is the pure example (3 entries). The same idea
applies to `D_800134BC` (5 entries) for the sibling dispatcher `GameFlow_DispatchTable5`.

Two-arg handlers (e.g. `UiPanelFunc` / `D_80013F2C` / `Ui_DispatchObjectState`) use the same
struct-assignment pattern. When the object that supplies the index is also the
first call argument, keep it in a temp so both the index and the call share it:

```c
temp = arg0->field_20;
sp.funcs[temp->field_8](temp, arg0);
```

Six entries still multi-load in two groups of three (`lw`/`sw` at 0/4/8 then
0xC/0x10/0x14).

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

`Mc_DuplicateBuffers` is the pure example (`Mc_BufferSlots[1..8]` buffer duplicate).

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

`CdCmd_PausePoll` is the full example (two copies of the CdSync status machine
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
matching the target. `GameFlow_WaitMenuDone` (`D_80072311`) is the example.

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

`SndScript_FindOneA` is a pure example.

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
CdStream_State.unknown_0[0] = CdStream_State.unknown_0[0] & 0xF7;
/* ... */
func(...);
p = &CdStream_State;
temp = p->unknown_0[2];
p->unknown_0[2] = temp & 0xF7; /* separate andi — mask not CSE'd into $sN */
```

Using the temp form on *both* sides also yields `andi`, but loads into `$a1`
instead of reusing `$v0`.

`CdStream_CleanupIrq` is the example.

## `0xFE` byte clear vs `~1` word mask: CSE to `li -2`

`x & 0xFE` on a zero-extended byte and `flags & ~1` on a word are the same
SImode constant (`-2` / `0xFFFFFFFE`). When both appear in one function, GCC
CSE's them into a single `li reg,-2` plus `and`, replacing the target's
`andi …,0xfe`.

Symptom: target has `lbu` / `nop` / `andi v0,v0,0xfe` early and
`li v0,-2` / `and` only for a later `flags & ~1`; your build uses `li`/`and`
for both.

Fix: force the byte clear through a `u8` temporary so the mask stays an
`andi` immediate, while leaving `(flags & ~1) | 4` alone for the `li -2`
form:

```c
u8 t;

t = p->unknown_0[2];
t = t & 0xFE;
p->unknown_0[2] = t;
/* ... */
e->field_0 = (flags & ~1) | 4; /* still li -2; and */
```

`t = p->field & 0xFE; p->field = t` (load into temp, mask into temp, store)
can put the `lbu` in `$a1`; the split `t = field; t = t & 0xFE; field = t`
keeps `lbu`/`andi` on `$v0`.

`CdStream_TeardownVoices` is the pure example (pairs with the `CdReady_Queue` entry flag
update used by `CdStream_Stop`).

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
CdStream_State.unknown_0[0] = CdStream_State.unknown_0[0] | 1;
```

`CdStream_CleanupIrq` / `D_80082808` is the example.

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
p = &CdAudio_Phase;
if (p->field_0 != 3) {
    p->field_1 = 4;
    p->field_2 = 1;
} else {
    ret = 0;
    /* ... */
}
return ret;

/* Right: li a1,-1 in beq delay slot */
p = &CdAudio_Phase;
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

`CdAudio_RequestStop` is the example. Pair with `if (x != K)` so the branch is
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
already uses `register … asm("reg")` elsewhere (`Midi_ReadVlq`, heap init).

`SndBank_SetEnableFlags` is the pure example: dual-purpose `flag` (loop fill value vs
bank-table base) needs `asm("v1")` for the address load form.

## if/else field stores reuse `$v0` for large constants better than a temp addend

When the target folds a large constant into `$v0` after a compare that also
used `$v0` (typical pattern: `li v0,1; bne; lui v0,0xHHHH` in the delay slot,
then `ori` / `addu v0,a0,v0`), a temporary addend variable often lands in
`$a1` instead:

```c
/* Mismatch: addend in $a1 */
addend = 0xFFFF0000;
if (flag == 1) {
    addend = 0xFFFF6667;
}
p->field = temp + addend;
```

Writing the store once per arm keeps the constant in `$v0`:

```c
/* Match: lui/ori into $v0, addu v0,a0,v0 */
if (flag == 1) {
    p->field = temp + 0xFFFF6667;
} else {
    p->field = temp + 0xFFFF0000;
}
```

`SndVoice_Tick` is the pure example (`field_4 += 0xFFFF6667` vs `0xFFFF0000`
gated on `Display_State.field_124 == 1`).

## `s16` accumulator forces `sll/sra 16` on each add

When a running total is added to an `s8` and then compared (either to another
narrow value or a constant), an `s32` local often drops the `(s16)` truncation:
GCC 2.8.1 proves the sum of two sign-extended bytes already fits in 16 bits and
CSEs the cast away. The target still has the classic

```
addu  v0, a0, v1
move  a0, v0
sll   v0, v0, 16
sra   v0, v0, 16
slt/slti ...
```

pattern. Declaring the accumulator as `s16` restores it — each store truncates
and each use re-extends, matching the split `a0` (full add result) / `v0`
(truncated compare operand) form:

```c
s16 level;
s8  delta;
s8  bound;

level = (s8)func();
if (delta > 0) {
    level = level + delta;   /* addu; move; sll/sra 16 */
    bound = limit;
    if (bound < level) { ... }
} else if (delta < 0) {
    level = level + delta;
    if (level < 0x30) { ... }
}
```

`SndVoice_StepMasterLevel` is the pure example. `s32 level` with an explicit `(s16)` cast
in the compare still scored only ~81% — the cast was deleted.

## Same global, `lb` in one function and `lbu` in another

`D_80082749` is loaded with `lb` by `SndVoice_TickRefCount` (`if (D_80082749 != 0)`)
and with `lbu` (+ `sll/sra 24` sign-extend) by `SndVoice_StepMasterLevel`. Declaring the
symbol `s8` matches the first; the second needs an unsigned load:

```c
bound = *(u8*)&D_80082749;  /* forces lbu, then s8 assignment sign-extends */
```

Flipping the global to `u8` breaks the already-matched `lb` site. Prefer
keeping the type that matches the majority of call sites and type-pun only at
the mismatched load.

## Shared `s16` phi + `(s8)(u8)` reload

When two success arms write the same halfword field (one from a signed byte,
one from a constant like `-1`) and the target has a single shared `sh` after a
join label, use an `s16` temporary with a `goto` join — not an early `return`
and not an `s32`/`s8` temp:

```c
s16 val;

if (check_a()) {
    ...
    val = (s8)(u8)arg0->field_8;  /* lbu; nop; sll 24; j; sra 24 */
    goto store;
}
if (check_b()) {
    ...
    val = -1;                     /* li v0,-1  (no extra sign-extend) */
store:
    arg1->field_2C = val;         /* shared sh */
}
```

- `s8 val` re-extends `-1` with `sll/sra 24` after the join.
- `s32 val` lets CSE sink each `sh` into its arm (and often drop the `lbu` path
  back to a plain `lb`).
- Plain `val = arg0->field_8` with an `s8` field emits `lb`, not the target's
  `lbu` + sign-extend. The `(s8)(u8)` cast (or `*(u8*)&`, see above) forces it.

Temps for the call that precedes this block may also be required so `a3 = 0`
is scheduled early and the 5th-arg `sw` fills the `jal` delay slot
(`McMenu_ConfirmWithRender`).

## `beq` register order for call-result vs field compare

`if (call() != p->field)` and `if (p->field != call())` are not always
equivalent under GCC 2.8.1 register assignment. The inline form often emits
`beq v0,v1` (return value first). When the target has `beq v1,v0` after
`lw v1,off(sN)` of a struct field, assign the call result to a temp first:

```c
pos = CdPosToInt(loc);
if (state->field_4 != pos) {  /* beq v1,v0 — field in v1, pos in v0 */
    ...
}
```

`CdAudio_ReadyCallback` is the example. Also mark interrupt-shared flags like
`D_80082770` (written by a `CdReadyCallback`, polled on the main path)
`volatile` so the post-call store stays out of a `j` delay slot.

## Volatile global: index via global name, not a local pointer

For a `volatile` global struct with an embedded array (e.g. `CdReady_Queue.entries`),
taking a local `p = &CdReady_Queue` and then forming `&((T*)((u8*)p + off))[idx]`
(or `p->entries[idx]` through that pointer) often folds the field offset into
the scaled index:

```
sll  v1, idx, ...
addiu v1, v1, 8      /* offset fused with scale */
addu  v1, v1, a3
```

The target often materializes `base + offsetof` first, then adds the scale:

```
sll   v1, idx, ...
addiu v0, a3, 8      /* base + offsetof(entries) */
addu  v1, v1, v0
```

Use the global directly (no local pointer) so the address of the struct is
shared between the `%lo(sym)` field_0 access and the array base:

```c
temp = CdReady_Queue.field_0; /* lui/addiu + %lo lbu */
if (arg0 != 0) {
    idx = arg0 - 1;
    entry = (CdReadyEntry*)&CdReady_Queue.entries[idx];
    ...
    CdReady_Queue.field_0 = temp;
}
```

`CdReady_Cancel` is the minimal example. A local `volatile CdReadyQueue* p` was the
sole difference between a 99% and a 100% match.

## Early-return `move v0,zero` vs `move a1,zero` with a live sum

When a checksum-style function zeros an accumulator, early-returns 0 on a
range check, then zeros a loop index, CSE of the constant 0 creates a
two-sided trap:

```
/* sum = 0 before the if: */
bnez  valid, body
 move  v1, zero      /* sum */
jr    ra
 move  v0, v1        /* return reuses sum — want move v0,zero */
...
 move  a1, zero      /* i is independent — good */

/* sum = 0 after the if (sunk into the branch delay): */
bnez  valid, body
 move  v1, zero      /* sum */
jr    ra
 move  v0, zero      /* return independent — good */
...
 move  a1, v1        /* i reuses sum — want move a1,zero */
```

s32 sum lands in one of those two 99.8% states. Declaring the accumulator as
`register s16 sum asm("v1")` (initialized *after* the early return) gives both
independent zeros, keeps the sum in `$v1`, and still yields `lbu` + `sll 24` /
`sra 24` when the pointer is `volatile u8*`.

## `sum = sum + tmp` vs `sum += expr` for `addu` operand order

With an s16 accumulator pinned in `$v1`, a direct

```c
sum += (s8)*ptr;   /* or sum = sum + (s8)*ptr */
```

often emits `addu v1, v0, v1` (addend first). The target usually wants
`addu v1, v1, v0`. Route the byte through an s32 temporary and write the add
as `sum = sum + tmp`:

```c
register s16 sum asm("v1");
volatile u8* ptr;
s32 tmp;
...
tmp = (s8)*ptr;
sum = sum + tmp;   /* addu v1, v1, v0 */
```

`Mc_VerifySaveHdrChecksum` needs both this and the s16/`volatile u8*` pairing above.

## Parallel dead offset temp for `p + offset` bank loops

When the target walks banks of a fixed-size array with:

```
li    a1, 0x10
...
addu  v1, a3, a1      /* entries = p + offset */
...
addiu a1, a1, 0x20    /* delay of outer branch */
```

writing the clean form `entries = p->field_10[i]` alone often loses the offset
register and rewrites the address as `sll`/`addu` on `i`. Keep a parallel
offset temporary that starts at the first bank's byte offset and advances by
the bank stride each outer iteration — even if it is never read in C. GCC CSE
equates `field_10[i]` with `p + offset` and emits the target's `addu` /
`addiu …, 0x20` shape:

```c
i = 0;
offset = 0x10; /* first bank at struct offset 0x10 */
for (; i < 2; i++) {
    entries = p->field_10[i]; /* not (Entry*)((u8*)p + offset) */
    for (j = 0; j < 8; j++) {
        entries[j].field_0 = 0;
        entries[j].field_1 = 0;
        entries[j].field_2 = 0;
    }
    offset += 0x20; /* bank stride; keeps a1 live for addu */
}
```

Also: prefer `entries[j].field = …` over `entry->field = …; entry++`. Pointer
increment tends to CSE a separate address for a mid-struct halfword field
(`addiu v1, a0, 2` then `sb -1(v1)` / `sh 0(v1)`), while array indexing keeps
one base and `sb 0` / `sb 1` / `sh 2` plus `addiu base, 4` in the branch delay.

`Pad_ClearEvents` (`PadState::field_10[2][8]`) is the example.

## Capture a reused halfword field so `%lo` wins and `$a0` stays free

When the same global halfword is tested and then compared against another
value, writing both accesses as bare `global.field` can make GCC materialise
`&global` into `$a0`:

```
lui    v0,%hi(Display_State)
addiu  a0,v0,%lo(Display_State)
lhu    v1,0x12a(a0)
```

That steals `$a0` from another live value the target keeps there (e.g. an
earlier `lhu a0, %lo(other_global)`), and it also breaks the pure
`lhu v1,%lo(Display_State+0x12a)(v0)` form.

Fix: load the field into a local once and reuse that local for both the
equality-to-constant test and the later compare:

```c
ac14 = D_8006AC14;          /* stays in $a0 */
f12a = Display_State.field_12a; /* lhu v1, %lo(...+0x12a)(v0) */
if (f12a == 1) {
    if (ac14 == f12a) { /* bne a0, v1 — both already live */
        ...
    }
}
Display_State.field_106 = 0; /* separate lui after calls; delay-slot-friendly */
```

`CdCmd_StopMdec` is the pure example (`D_8006AC14` vs `Display_State.field_12a`).

## Equality comparison operand order controls `beq` register order

Target often has `beq a0, v0` after `lbu v0, field(ptr)` (loaded value in
`$v0`, compare arg first). Writing `field == arg` tends to emit
`beq v0, a0`; write `arg == field` to get `beq a0, v0`:

```c
/* target: beq a0, v0 after lbu v0, 1(v1) */
if (arg0 == (&Midi_Song)[i].field_1) {
```

`Midi_IsBusy` needed this (99.6% → 100%).

## Long-lived step in `$v1`: avoid intermediate field temps

For small update/clamp helpers that load one long-lived value first (e.g. step
from `arg0->field_8`) and use it in both arms, **do not** extract the other
fields into locals. Temps for `cur`/`end` push the step into `$a1`/`$a2`, which
then frees `$v1` for the sum and lets GCC put the store in a branch delay slot:

```
# BAD (~71%): step in a2, sum in v1, store delayed
addu  v1, v0, a2
sltu  v0, v1, a1
bnez  v0, end
 sw   v1, 0(a0)    # delay slot
```

Direct field access keeps step in `$v1` and reuses `$v0` for sum **and** the
`sltu` result, forcing the store before the compare (matches target):

```
# GOOD: step in v1, sum/compare share v0
addu  v0, v0, v1
sw    v0, 0(a0)
sltu  v0, v0, a1
bnez  v0, end
 nop
```

```c
/* GOOD — step local only; fields accessed directly */
s32 step = arg0->field_8;
if (step) {
    if (arg0->field_C < 0) {
        if ((u32)(arg0->field_4 + step) >= (u32)arg0->field_0)
            arg0->field_0 = arg0->field_4;
        else
            arg0->field_0 = arg0->field_0 - step;
    } else {
        arg0->field_0 = arg0->field_0 + step;
        if ((u32)arg0->field_0 >= (u32)arg0->field_4)
            arg0->field_0 = arg0->field_4;
    }
}
```

Branch polarity for the decreasing arm: write `if (end + step >= cur) clamp;
else subtract` so fall-through is clamp and `bnez` targets subtract (matches
`sltu`/`bnez`). Inverting to `<` swaps the arms.

`LinInterp_Step` is the pure example (LinInterp linear interpolator tick).

## Empty switch case as binary-search pivot to shared default

When the target opens a small dense switch with:

```
li   v0, 2
beq  v1, v0, shared_default
slti v0, v1, 3
beqz v0, check_hi
...
```

case 2 is intentionally in the case set even though its body is the same as
`default` — it is the binary-search pivot. Listing only the non-default cases
(`1`, `3`/`4`) drops the `== 2` check and reshapes the tree (~83–86%).

Force the pivot with an empty case that falls into the shared default body:

```c
switch (p->field_8) {
case 1:
    /* unique body */
    return;
case 2:
    break; /* empty — falls into shared default */
case 3:
case 4:
    /* unique body */
    return;
}
/* shared default / case 2 body */
*out = p->field_C;
```

`Ui_ComputeAnimRect` is the pure example.

## Dual large constants: call in both if/else arms for shared `lui` + `j`

When two multi-instruction constants share the same high 16 bits (e.g.
`0x4A800` vs `0x45400`, both `lui a0,4` + different `ori`), assigning either to
a temp then calling once:

```c
if (flag == 0) {
    size = 0x4A800;
} else {
    size = 0x45400;
}
ptr = Mem_Malloc(size, 1);
```

lets GCC hoist the shared `lui` before the branch and emit `bnez`/`beqz` with a
re-`lui` in the fall-through arm (~95%). The target often wants:

```
bnez  cond, L_if
  lui   a0, HI        /* delay: shared high half */
j     L_join
  ori   a0, a0, LO_else
L_if:
  ori   a0, a0, LO_if
L_join:
  jal   Mem_Malloc
```

Force that layout by writing the call in **both** arms (same destination). GCC
CSEs the calls back to one `jal` while keeping the `j` + shared-`lui`-in-delay
form:

```c
if (flag == 0) {
    ptr = Mem_Malloc(0x4A800, 1);
} else {
    ptr = Mem_Malloc(0x45400, 1);
}
```

Polarity still matters: fall-through must be the `== 0` arm (`bnez` to the
non-zero constant). `Mem_AllocAuxWithImages` is the pure example.

## `ptr = (u8*)&global; ptr += 4` for shared-`%hi` base then offset

When a function both walks bytes at a non-zero offset of a global struct and
writes nearby fields via the bare global name, a direct

```c
ptr = global.field_4;          /* or p = &global; ptr = p->field_4 */
```

often folds to `%lo(D_xxx+4)` and then rebuilds the base as `addiu v1, a0, -4`.
Keeping a live `GStruct* p = &global` through the end stores pins the base in
`$a0` and steals the walk-pointer register.

Write the walk pointer as a two-step from the global's address, and keep the
field stores as bare `global.field_…` so the base dies after the prologue
clears:

```c
sum = 0;
ptr = (u8*)&Mc_SaveData;
ptr += 4;                      /* addiu a0, v1, %lo(D); addiu a0, a0, 4 */
limit = 0x38;
i = 0;
Mc_SaveData.field_1C = 0;       /* completes v1 with second %lo(D) */
Mc_SaveData.field_1E = 0xFFFF;
do {
    i += 1;
    tmp = (s8)*ptr;
    sum = sum + tmp;
    ptr += 1;
} while (i < limit);
Mc_SaveData.field_1C = sum;
Mc_SaveData.field_1E = ~sum;
Mc_VerifySaveHdrChecksum(&Mc_SaveData);
```

That emits the shared-`%hi` shape:

```
lui    v1, %hi(Mc_SaveData)
addiu  a0, v1, %lo(Mc_SaveData)
addiu  a0, a0, 4
...
addiu  v1, v1, %lo(Mc_SaveData)
```

After the loop, reloading `&Mc_SaveData` as `%lo(D+4)` / `addiu -4` is fine —
it links to the same address as a splat `D_xxx+4` symbol (e.g. `D_8007216C`).

`Mc_WriteSaveHdrChecksum` is the pure example (checksum writer for `McSaveData::field_1C` /
`field_1E`; pair with the s16 / `sum = sum + tmp` notes used by `Mc_VerifySaveHdrChecksum`).

## Signed division needs `--expand-div` on the TU

Retail ASPSX expands signed `div` into the full trap sequence (`bnez` / `break 7`
/ overflow `break 6` / `mflo`). GCC 2.8.1 emits a bare `div $d,$s,$t`; maspsx
only re-emits that sequence when `--expand-div` is passed.

Symptom in the scratch: target starts with the trap block after `div`, your
build has `div` then immediate `mflo` (and the rest of the function shifts by
~10 instructions). Full-project checksum also fails without the expansion.

Fix: enable `--expand-div` for the translation unit in `ninja_config.py`
(`EXPANDIVFLAG`), and use the same flag in the scratch `build.sh`. Power-of-two
divides that become shifts do not need this. Known TUs: `tmd.c`
(`Mdec_StripCallback`), `sndbank.c` (`LinInterp_Setup`).

## Keep the `- 1` outside the div assignment for schedule

```c
/* BAD — value of counter is hoisted before the stack frame / CdCmd_Queue setup */
temp = 0x140 / (scale * 16) - 1;
if (counter == temp) { ... }

/* GOOD — mflo stays in $v1; -1 is delayed until after address loads */
temp = 0x140 / (scale * 16);
if (counter == temp - 1) { ... }
```

Folding the `- 1` into `temp` changes register pressure enough that GCC loads
the counter early (`lhu a0, counter` before `addiu sp`) and puts `&CdCmd_Queue`
only in the branch delay slot. Splitting keeps:

```
mflo   v1
addiu  sp,sp,-0x18
lui    a0,%hi(counter)
sw     ra,...
lui    v0,%hi(CdCmd_Queue)
addiu  a1,v0,%lo(CdCmd_Queue)
lhu    v0,%lo(counter)(a0)
addiu  v1,v1,-1
bne    v0,v1,else
 addiu  v0,v0,1
```

Also type strip counters that the target loads with `lhu` as `u16` (not `s16`),
or you get a second `lh` and sign-extend on the index path. `Mdec_StripCallback`.

## Pin `val` to `$a0` and abs temp to `$v0` for in-place `$a1` diff

When a function saves a transformed arg into one register and then mutates the
arg register in place (classic `move a0,a1` / `subu a1,a1,v0` / `sll;sra a1`),
writing `arg0 = arg1; arg1 -= …` is not enough if later control flow prefers
putting the diff in `$a0` and leaving val in `$a1`. That shows up as a clean
~95% match with every branch correct but all `sb val` / `bgtz diff` using the
swapped registers.

Fix with hard-register pins (both need the `register` keyword):

```c
register s32 val asm("a0");
register s32 t asm("v0");

arg1 = (~arg1) & 0x7F;
val  = arg1;           /* move a0, a1 */
/* … load + sign-extend into t … */
arg1 -= t;
arg1 <<= 16;
arg1 >>= 16;           /* in-place s16 truncate on a1 */
```

For the following abs that the target emits as:

```
bgez  a1, skip
 move  v0, a1
negu  v0, v0
```

compare the *source* register and negate the *pinned* temp — not the same name
for both:

```c
t = arg1;
if (arg1 < 0) {   /* bgez a1 — not bgez v0 */
    t = -t;       /* negu v0, v0 because t is asm("v0") */
}
```

`if (t < 0) t = -t` alone either keeps `negu v0, a1` (CSE) or moves the compare
onto `$v0` and breaks the delay-slot form.

## `*(volatile u8*)&field` then `(s8)` for `lbu` + `sll/sra`

`(s8)p->u8_field` collapses to a single `lb`. When the target has early
`lbu` scheduled before independent work (e.g. `nor`/`andi` on another arg) and
a later `sll 24; sra 24`, split the load and the cast:

```c
t = *(volatile u8*)&p->field_13; /* lbu, may schedule early */
/* … unrelated arg transforms … */
t = (s8)t;                       /* sll; sra */
```

Same pattern as `C37C.c`'s `status = *(volatile u8*)&entry->param0` followed by
`field5 = (s8)field5`. `SndVoice_SetVolumeRamp` needs this for `SndScript.field_13`.

## Three-way sign with `<= 0` outer for `bgtz` fall-through

Shared zero-store between "close" and "far-and-zero" paths:

```c
if (abs_diff >= threshold) {
    p->target = val;
    if (diff <= 0) {
        if (diff < 0) {
            p->step = -8;
            goto end;
        }
        /* zero: fall through to shared store */
    } else {
        p->step = 8;
        goto end;
    }
} else {
    p->current = val;
}
p->step = 0;
end:
p->dirty = 1;
```

`if (diff <= 0)` (not `if (diff > 0)` first) makes the positive arm the `bgtz`
branch target and the negative arm fall through after `bgez` fails — matching
the `bgtz` / `bgez` / shared-zero label shape of `SndVoice_SetVolumeRamp` / `SndVoice_SetPanRamp`.

## Booleanize `(x & mask)` via `== mask`, not `!= 0`

After `temp = x & mask`, writing `if (temp != 0) temp = 1; else temp = 0;` (or
`temp = 1; if (!(x & mask)) temp = 0;`) is jump-threaded away when the only use
of `temp` is a later `if (temp)`. The target then has a short
`andi` / `beqz` / store form instead of the longer materialization:

```
andi  v0, v0, mask
bnez  v0, merge
li    v0, 1
move  v0, zero
merge:
beqz  v0, skip
...
```

Fix: compare against the mask itself. Because `x & mask` is either `0` or
`mask`, `temp == mask` is equivalent to `temp != 0`, but GCC 2.8.1 does not
fold it into the control-only form:

```c
temp = result[0] & CdlStatShellOpen;
if (temp == CdlStatShellOpen) {
    temp = 1;
} else {
    temp = 0;
}
if (temp != 0) {
    p->field = 0;
}
```

`CdCmd_RecoverDisk` case 1 is the example (`CdlStatShellOpen == 0x10`).

## Reassign call result to force `li`/`bne` equality tests

`if (func() == K) { body }` often becomes `xori` / `bnez` (or a fused compare).
To get the longer `li v1,K` / `bne` / `li v0,1` / `beqz v0,...` shape, capture
the call result and reassign:

```c
temp = CdDiskReady(1);
if (temp == CdlComplete) {
    temp = 1;
} else {
    temp = 0;
}
if (temp != 0) {
    /* body */
}
```

`CdCmd_RecoverDisk` case 0 is the example.

## Place loop-invariant table load inside the `if` to order `andi` before `lui`

When a `u8` parameter is used only in a loop condition, GCC 2.8.1 schedules its
zero-extend `andi` next to that use — *after* independent prologue loads of a
table address. The target often wants:

```
move  i, zero
andi  a0, a0, 0xff     /* early */
lui   / addiu table
andi  a1, a1, 0xff
li    sentinel, 0xFFFF
```

Assigning the table *inside* the conditional that first uses `arg0` still lets
`-O2` hoist the loop-invariant load into the prologue, but now after the `andi`:

```c
i   = 0;
arr = &Midi_Song;
for (; i <= 0; i++) {
    if ((arg0 == arr[i].field_1) || (arg0 == 0)) {
        table = D_800689F0; /* hoisted after andi a0 */
        product = table[arr[i].field_1] * arg1;
        arr[i].field_C = 0xFFFF;
        arr[i].field_8 = product;
    }
}
```

Do **not** fix this by early `arg0 &= 0xFF` on an `s32` parameter: that gets the
`andi` order right but kills delay-slot fill of `addu index, table` on the
equality branch (and the matching reload of `field_1` on the `arg0 == 0` path).
Keeping `u8` params and moving the table assignment is what preserves both.

`Midi_SetVolumeScale` is the pure example. Pair with the one-iteration
`for (i = 0; i <= 0; i++)` + `MidiSong` array pattern for `Midi_Song`.

## Stack-struct pointer: RMW via temp forces `lw v1` then `lw a0`

When the target updates two fields of a pointer loaded from a stack struct
(`SpuVoiceRef sp10`, pointer at `sp+0x14`) as:

```
lw   v1, 0x14(sp)
lhu  v0, 0x3C(v1)
...
sh   v0, 0x3C(v1)
lw   a0, 0x14(sp)   /* reload into a different reg */
lw   v0, 4(a0)
...
sw   v0, 4(a0)
```

a local pointer CSE folds both accesses into one register (`lw a0` once or
reused). Force the double-reload and the `v1`/`a0` split by:

1. Reading the halfword into a `u16` temporary
2. Writing both fields through `((Type*)sp10.field_4)->member` — no local
   pointer held across the two updates

```c
u16 temp;

Spu_GetVoiceRef(idx, &sp10);
temp = ((SpuVoiceAttr*)sp10.field_4)->adsr2;
temp = (temp & 0xFFE0) | 5;
((SpuVoiceAttr*)sp10.field_4)->adsr2 = temp;
((SpuVoiceAttr*)sp10.field_4)->mask |= SPU_VOICE_ADSR_ADSR2;
```

Also init the loop counter *before* the slot base pointer when the target
prologue does `move s2,zero` then `addiu s0,a0,0x504`:

```c
i = 0;
slot = arg0->voiceSlots;
do { ...; i++; slot++; } while (i < 0x12);
```

`Midi_KeyOffVoices` is the pure example.

## Call-arg width: wrong prototype gives `lb` instead of `lw`

When the target loads a struct field into `$aN` with `lw` for a call argument,
but your build emits `lb`/`lh`/`lhu`, the callee's prototype is almost always
too narrow. GCC loads only as much as the parameter type requires.

```c
/* Wrong — third param is s8, so menu->field_10 (s32) becomes lb a2,0x10(v0) */
extern void func_800330D8(void* a0, s32 a1, s8 a2, s32 a3, s32 a4);

/* Right — s32 third param yields lw a2,0x10(v0) */
extern void func_800330D8(void* a0, s32 a1, s32 a2, s32 a3, s32 a4);
```

`McMenu_FileInformation` is the pure example: the header had `s8` for arg2, but the
target always used `lw` of `UiList::field_10`. Callers that pass an `s8`
local still match after the widen (default argument promotion).

## Pull a call arg into a local so the stack-arg store fills the `jal` delay

For a 5-arg call whose last two args are zeros, the target often does:

```
move  a0, ...
move  a1, ...
lw    a2, off(v0)     /* field used as 3rd arg */
move  a3, zero
jal   func
 sw   zero, 0x10(sp)  /* 5th arg in delay slot */
```

Writing `func(obj, data, menu->field_10, 0, 0)` can schedule the stack store
*before* the field load and put `move a3,zero` in the delay slot instead.
Forcing the field into a local first restores the target order:

```c
val = menu->field_10;
func_800330D8(obj, data, val, 0, 0);
```

`McMenu_FileInformation` is the pure example.

## Advance a global pointer via a local after a store through it

`*global_ptr = val; global_ptr += N` often reloads the global after the store
(GCC 2.8.1 treats the store as a possible clobber of the pointer itself):

```
lw   v0, %lo(global)(s1)
sw   v1, 0(v0)
lw   v0, %lo(global)(s1)   /* unwanted reload */
addiu v0, v0, 0x80
sw   v0, %lo(global)(s1)
```

Target reuses the loaded register for the advance. Capture into a local first:

```c
ot = D_800710A0;
*ot = C5F414_OTAG_END_PRIM;
D_800710A0 = ot + 0x20; /* addiu reuses ot — no reload */
```

`Display_FlipOt` is the pure example (double-buffer OT flip).

## Volatile on independent BSS stores preserves assignment order

A tail of independent stores to distinct globals (`g1 = 0; g2 = 0; g3 = g2; …`)
can be reordered when some targets are non-volatile: GCC 2.8.1 may hoist the
volatile chain (e.g. `D_80082808 = 0; D_80082810 = D_80082808`) ahead of an
earlier non-volatile `D_80068B58 = 0`, and may sink the `D_80082810` store past
later non-volatile stores.

Symptom: body matches except the final store sequence is permuted (and `%hi`
temps may switch from `$v0` to `$v1`).

Fix: declare every global in that ordered sequence `volatile` so each access is
a sequence point the scheduler cannot cross:

```c
extern volatile s32 D_80068B58;
extern volatile u16 D_80082808;
extern volatile u16 D_80082810;
/* ... */
D_80068B58 = 0;
D_80082808 = 0;
D_80082810 = D_80082808; /* lhu requires unsigned half on the source */
D_80068B6A = 0;
D_80068B5C = 0;
```

Also match load width to the source type: `lhu` ⇒ `volatile u16` (not `s16`,
which yields `lh`). `CdStream_Reset` is the pure example.

## Local bitmask for correct s-reg assignment across a call

When a function tests a flag bit, calls something, then ORs the same bit back
into a field, CSE reuses the constant — but the s-reg it lands in is not always
the one the target wants. A bare:

```c
if (!(p->field_1c & 0x40000000)) {
    func(0);
    p = D_xxx;
    p->field_20 = arg0;   /* may get $s3 */
    p->field_11 = arg1;   /* may get $s1 */
    p->field_1c |= 0x40000000; /* mask may get $s2 */
}
```

often rotates the callee-saved assignment (`arg0→s3`, `mask→s2`, `arg1→s1`)
versus the target (`arg0→s2`, `mask→s1`, `arg1→s3`). Instruction shape and
stack frame can still look ~98% correct.

Fix: materialize the mask into a local *before* the test, and use that local for
both the `and` and the later `or`:

```c
s32 mask;

mask = 0x40000000;
if (!(Stage_Ctx->field_1c & mask)) {
    Pad_SetCooldown(0);
    temp = Stage_Ctx;
    temp->field_20 = arg0;
    temp->field_24 = 0;
    temp->field_28 = 0;
    temp->field_11 = arg1;
    temp->field_1c |= mask;
}
```

That pins the mask in `$s1` and shifts `arg0`/`arg1` into `$s2`/`$s3` to match
the target prologue (`move s2,a0` early, `lui s1,0x4000` after the field load,
`move s3,a1` in the `bnez` delay slot). `Stage_BeginTransition` is the pure example.

## Live `0xFFFF` register for ones-complement stores (`subu` not `nor`)

When a checksum write stores both `sum` and its ones-complement, `field = ~sum`
usually matches (`nor v0, zero, sum`). Some loops instead keep a live
`0xFFFF` across the whole function and subtract:

```
li    t2, 0xffff          /* once, outside the loop */
...
subu  v0, t2, a2          /* each iteration */
sh    v0, 2(buf)
sh    a2, 0(buf)
```

`~sum` (or a folded `0xFFFF - sum` constant expression) becomes `nor` and
drops the `li t2`. Force the live register with an outer-scope temporary:

```c
s32 inv = 0xFFFF;
...
temp->field_2 = inv - sum;  /* subu v0, t2, sum */
temp->field_0 = sum;
```

Also form the slot pointer as `base = Mc_BufferSlots; p = base + 1;` (not
`p = Mc_BufferSlots + 1`) so the address is `addiu v0, %lo(...)` then
`addiu t0, v0, 0xC` rather than a folded `%lo(+0xC)`.

For the per-slot size path that does `lw a1, 4(p); addiu a1, a1, -4`, split
the subtract and interleave the payload pointer setup:

```c
count = p->field_4;
ptr   = temp->field_4;
count = count - 4;
```

`count = p->field_4 - 4` alone often routes through `$v0` and swaps the sum /
count registers. If sum ends up correct in `$a2` but the loop index and count
are swapped (`$a1`/`$a0`), pin the index: `register u32 j asm("a0")`.

`Mc_WriteSlotChecksums` is the pure example (batch write over `Mc_BufferSlots[1..8]`;
contrast `Mc_WriteBlockChecksum` which uses `~sum` for a single buffer).

## Signed `/ 2` chain must land in `$a0` via the call argument

When the target does signed division by 2 entirely in `$a0` before a call:

```
jal    DecDCTBufSize
 nop
srl    a0,v0,0x1f
addu   a0,a0,v0
sra    a0,a0,0x1
addiu  a0,a0,2
jal    DecDCTvlcSize2
```

assigning through a local first (`size = x / 2 + 2; foo(size)`) often computes
the shift chain in `$v1` and only the final `+ 2` into `$a0`. Pass the expression
directly as the call argument so the whole chain is the argument:

```c
/* BAD — intermediates in $v1, only +2 in $a0 */
size = DecDCTBufSize(frame) / 2 + 2;
DecDCTvlcSize2(size);

/* GOOD — entire signed-div sequence in $a0 */
DecDCTvlcSize2(DecDCTBufSize(frame) / 2 + 2);
```

Pair with a separate `DecDCTvlcSize2(0)` on the other branch (rather than a
shared `size` phi) so the zero path still fills the `bnez` delay with
`move a0,zero` and both paths share no local. `Mdec_DecodeFrame` is the example.

## Reload a global (not the local pointer) to fill a branch delay with `lui`

When the target ends a status-check cascade with:

```
bne  v1,v0,shared
 lui  v0,%hi(D_xxx)     /* delay: start rematerialising &D_xxx */
...
shared:
addiu v0,v0,%lo(D_xxx)
lbu   v0,2(v0)
```

and an earlier local `p = &D_xxx` is still live in `$a0`, writing
`if (p->field_2 != 0)` reuses `$a0` and leaves a `nop` in the delay slot. Access
the same field through the global so the compiler rematerialises the address:

```c
/* BAD — reuses p in $a0; delay slot is nop */
if (p->field_2 != 0) {
    return 1;
}

/* GOOD — lui %hi(D_xxx) fills the prior bne delay; fallthrough path
   re-issues lui when the delay value was clobbered */
if (D_xxx.field_2 != 0) {
    return 1;
}
func(...);
return 0;
```

Pair with early `return 1` / `return 0` (not a `ret` phi) so the `bnez` delay
holds `li v0,1` and the call path ends in `move v0,zero`. `CdAudio_Begin` is
the pure example.

## Prefer Psy-Q GPU macros for OT prim insertion

Hand-written `0xFFFFFF` / `0xFF000000` AND/OR for `addPrim`-style OT linking
often lands near-matches (~91%) with wrong register assignment (`$a3`/`$t0`
swapped for the two masks; OT pointer in `$a1` instead of `$a0`). The
canonical Psy-Q macros expand through `P_TAG` bitfields and match the retail
codegen:

```c
DR_TPAGE* p;

p          = D_80071190;
D_80071190 = p + 1;
setDrawTPage(p, 0, 1, 0x1E | ((abr & 3) << 5));
addPrim(D_800710A0 + otz, p);
```

`setDrawTPage` → `setlen` + `_get_mode`; `addPrim` → `setaddr`/`getaddr` on
`P_TAG`. Same pattern as `Prim_DrawTPage` (which uses `AddPrim` the function
instead of the macro — that one is a real call). `Ui_InsertDrawTPage` is the pure
inline-macro example.

## Array index vs intermediate pointer for `addu` operand order

When the target indexes a struct array and wants the scaled offset added
as `addu v0, v0, base` (offset first), an intermediate pointer often flips
the operands to `addu v0, base, v0`:

```c
/* BAD near-match — addu v0, t3, v0 (base first) */
p = base + idx;
src = (u8*)p->field_0;
size = p->field_4;

/* GOOD — addu v0, v0, t3 (offset first); CSE still loads address once */
src = (u8*)base[idx].field_0;
size = base[idx].field_4;
```

Pair with `base = Mc_BufferSlots` kept live (not `Mc_BufferSlots[idx]` alone) so the
`%lo` address stays in a temp across the loop. `Mc_CompareBufferHalves` is the pure
example (reverse walk of `Mc_BufferSlots[8..1]` comparing each buffer to its
duplicate half).

## Store both halfwords first, then reload one into `s16` for a clamp

When a function writes two related halfwords and then clamps the first against
a bound that uses the second, a natural left-to-right order:

```c
p->x = a + b + 8;
temp = 0x96 - (p->x + p->w);   /* CSE: sll/sra of the just-written reg */
p->y = c + d - 2;
if (temp < 0) {
    p->x = (u16)p->x + temp;   /* y store sinks into bgez delay slot */
}
```

scores ~79%: GCC sign-extends the store register with `sll`/`sra` instead of
`lh`, and sinks the `y` store into the `bgez` delay slot (leaving a `nop`
before the clamp `addu`).

Write *both* stores first, then reload `x` into an `s16` local used for the
clamp math and the `(u16)` adjust:

```c
s16 new_var;

p->x = a + b + 8;
p->y = c + d - 2;
new_var = p->x;                /* forces lh after both stores */
temp = 0x96 - (new_var + p->w);
if (temp < 0) {
    p->x = (u16)new_var + temp;
}
```

The early `y` store is free to schedule around the clamp setup; the compiler
then emits target order (`lh` of `x`/`w`, compute `temp`, store `y`, `lhu` +
`bgez` with `addu` in the delay slot). `Ui_ClampDialogRect` is the pure example
(dialog RECT clamp to 0x96 × 0x5A).

## `s16` accumulator forces `lbu`+sign-extend (not `lb`) in checksum loops

When summing signed bytes from a `u8*` buffer under `-funsigned-char`:

```c
sum += (s8)*ptr;
```

an `s32 sum` collapses the cast to a single `lb`. The target often wants the
longer form (`lbu` / `sll 24` / `sra 24`) that `Mc_WriteSlotChecksums` and its verify
sibling `Mc_VerifySlotChecksums` use over `Mc_BufferSlots[1..8]`.

Declare the accumulator `s16`:

```c
s16 sum;
...
sum = 0;
j = 0;
...
sum += (s8)*ptr;   /* lbu + sll/sra, not lb */
```

Side effect: with `s32 sum`, the following `j = 0` CSE's as `move a0,a2`
instead of `move a0,zero`. The `s16` width also keeps the two zeros independent.

## Force `addu rd, offset, base` with an integer cast

Pointer + index usually emits `addu s0, base, offset`. The target sometimes
wants the operands swapped (`addu s0, offset, base`). Casting the pointer to
`s32` and adding the pre-scaled byte offset as integers preserves the source
operand order:

```c
/* Matches: sll v0,v0,1 ; addu s0,v0,v1  (offset then base) */
temp  = ((D_80062738 + product) & 0xFFFF) * 2;
entry = (GPairU8*)(temp + (s32)D_8006273C[idx]);

/* Mismatches: addu s0,v1,v0 */
entry = D_8006273C[idx] + ((D_80062738 + product) & 0xFFFF);
```

`Stage_RequestMidiFromMap` is the pure example. Pair with a `register ... asm("v1")` pin
on the stage pointer when the target loads `Game_Session` into `$v1` (with
an argument live in `$s1`) rather than `$a0`.

## Pre-increment store `*++p = f()` fills `jal` delay with the previous store

When walking a buffer and writing values that each depend on a call (e.g.
`rand()`), splitting the step as `p++; *p = table[f() & mask]` schedules the
call *before* the store and leaves a `nop` after `lbu`:

```
jal   rand
 addiu p, p, 1      # delay: advance
andi  ...
lbu   ...
nop
sb    v0, 0(p)
```

The target wants the *previous* byte stored in the delay slot of the *next*
`jal`, with the pointer advance between load and call:

```
lbu   v0, 0(v0)
addiu p, p, 1
jal   rand
 sb   v0, 0(p)      # delay: store previous char
```

Write a single pre-increment assignment so the call and the pending store are
adjacent in the same expression:

```c
/* BAD — call first, store after nop */
*p = table[arg1];
p++;
*p = table[rand() & 0x3F];
p++;
*p = table[rand() & 0x3F];

/* GOOD — *++p keeps store in jal delay of the next rand */
*p = table[arg1];
*++p = table[rand() & 0x3F];
*++p = table[rand() & 0x3F];
/* ... */
p[1] = 0;
```

`Mc_BuildFileName` is the pure example (memcard filename: product-code prefix +
selector char + 7 random chars + NUL).

## Dual same-function calls beat `sltiu` for boolean `0`/`1` args

When the target sets `$a0` with a branch then makes a single call:

```
lb    v1, flag
...
bnez  v1, skip
 move  a0, zero
li    a0, 1
skip:
jal   func
 nop
```

writing the boolean into a temp (or a ternary) collapses to `sltiu` in the
`jal` delay slot:

```c
/* BAD — emits jal / sltiu a0,a0,1 */
a0 = 0;
if (flag == 0) {
    a0 = 1;
}
func(a0);
/* also BAD: func(flag == 0); */
```

Write two calls with inverted constants and let GCC merge them:

```c
/* GOOD — bnez + move/li then one jal */
if (flag == 0) {
    func(1);
} else {
    func(0);
}
```

`Snd_ApplyVolumeTable` (`D_80072311` → `CdVol_SetMixMode`) is the pure `== 0` example.
`GameFlow_WaitMenuDone` is the sibling `== 1` form (`CdVol_SetMixMode(0)` vs `(1)`).

## Goto-forced block order for shared-default multi-way branch

When the target dispatches on a small integer with a shared default tail that
*falls through* into the next code (no `j` after the default assignment), and a
special case block sits *before* that default in the object file:

```
beq   mode, 2, case2
slti  v0, mode, 3
beqz  v0, default          # mode >= 3
...
# case1 body; j done
case2:
  # assign special; j done
default:
  # assign default — falls into done
done:
  # rest of function
```

plain `if`/`else` or `switch` often places the special-case `else` *after*
default, which inserts an extra `j` on the default path and mismatches offsets.

Force the order with gotos (same pattern as `func_8003E698`):

```c
if (mode == 2) {
    goto case2;
}
if (mode >= 3) {
    goto default_case;
}
if (mode != 1) {
    goto default_case;
}
menu = &special_1;
goto done;
case2:
menu = &special_2;
goto done;
default_case:
menu = &fallback;
done:
/* ... */
```

`if (mode >= 3) goto default` is what emits the `slti` / `beqz` pair; do not
collapse the two default entries into one `else` if that reorders blocks.
`McMenu_InitByMode` is the pure example (menu pointer select among three
`UiList` data objects).

## Table index: `offset = 0` then `if (hi) offset = hi << 1`

When the target zeros a register, branch-skips an `sll` on a nonzero index, then
`addu`s that byte offset onto a table base (so index 0 never emits the shift),
match with an explicit offset local rather than `base[hi]` or `base += hi`:

```
andi  v0, temp, 0xFF
srl   v1, v0, 1          /* lo index */
move  a0, zero           /* offset = 0 */
andi  a1, temp, 0xFFFF
lui   v0, %hi(table)
srl   a1, a1, 8          /* hi index */
beqz  a1, L
 addiu v0, v0, %lo(table)
sll   a0, a1, 1          /* offset = hi << 1 — only when hi != 0 */
L:
addu  a0, a0, v0
lhu   a0, 0(a0)
```

```c
lo = (temp & 0xFF) >> 1;
offset = 0;
hi = temp & 0xFFFF;
do {
    base = table;
    hi >>= 8;
    if (hi != 0) {
        offset = hi << 1;
    }
} while (0);
val = *(u16 *)((u8 *)base + offset);
```

`base[hi]` / `offset = hi` then `base[offset]` emits `move`+`sll` or always-shifts
and misses the branchy form. Split `hi = temp & 0xFFFF` from `hi >>= 8` so the
`andi` and `srl` are separate statements; wrap `base = table` + the shift/`if` in
`do {} while (0)` so `lui %hi(table)` sits between `andi` and `srl` with
`addiu %lo` in the `beqz` delay slot (same interleave idea as the list-unlink
`do {} while (0)` note above).

Reuse the lo-index local for the multiply result so the product/`0x3FFF` clamp
lands in `$v1` (the register that held the second table address) instead of
`$a0`. Use `u32` temps so `>>` is `srl`, not `sra`.

`Spu_CalcVolume` is the example (volume-style lookup: `D_80068BB8[hi] *
D_80068C78[lo] >> 8`, clamp to `0x3FFF`).

## Early halfword temp so `lh` stays live across intervening stores

When the target does `lh v1, field` early, fills other stack/struct fields, then
only later does `addiu v1, v1, 1` / `sw v1, ...`, writing the expression inline
at the store site reloads late (`lh` just before use) and scrambles schedule.

Hoist the signed load into a temporary at the early point:

```c
sp.field_2 = ...;
temp = (s16)arg0->field_14; /* early lh into a live temp */
sp.field_8 = arg4;
/* ... more stores ... */
sp.field_4 = temp + 1;      /* addiu + sw near the call */
sp.field_E = (s8)arg5;
func(&sp, arg3);            /* field_E often lands in the jal delay slot */
```

Pair with `u16` fields that the target loads via `lhu` for unsigned arithmetic
(`field_14 - 1` / `+ 1`) and `(s16)` only where the target uses `lh`.

`Ui_DrawTextAtLayout` is the pure example.

## `s32` save temp forces `lb` for pure byte save/restore

Saving an `s8` global into an `s8` local and only writing it back with `sb`
lets GCC emit `lbu` — the sign bits are dead. The target often uses `lb`
anyway (same pattern as interrupt-flag save/restore around a call).

Hold the saved value in an `s32` so the load must sign-extend:

```c
s32 saved;

saved = D_80072189;   /* lb, not lbu */
/* ... call that may clobber the global ... */
D_80072189 = saved;   /* sb */
```

`Game_ResetSessionAndBuffers` is the pure example.

## `do {} while (0)` keeps a post-call store before a later load

When the target restores a saved global *before* touching another object
(with a load-delay `nop`):

```
jal  func
 ...
lui  v0, %hi(flag)
sb   s0, %lo(flag)(v0)
lw   v0, 0x30(s1)
nop
addiu v0, v0, 1
```

a plain sequential write is often reordered so the `lw` fills the slot and the
`sb` uses a different register. Wrapping only the restore in `do {} while (0)`
pins the store first:

```c
Mc_InitBufferSlots();
do {
    D_80072189 = saved;
} while (0);
arg0->field_30 = arg0->field_30 + 1;
```

`Game_ResetSessionAndBuffers` is the pure example.

## Per-branch stores beat a phi-merged store for load/store ordering

When both arms of an if/else write the same field and the target then loads a
*different* field of the same object:

```
beqz  v0, else
 ...
addiu v0, v0, 1     /* if path: state + 1 */
j     join
else:
li    v0, 6
join:
sw    v0, 0x30(s2)  /* store first */
li    a1, 1
lw    s0, 0x20(s2)  /* then load sibling field */
```

writing a shared store after the if often lets the subsequent `lw` sink above
the `sw` (same base, different offset — no dependence):

```c
/* sinks: lw field_20, then sw field_30 */
if (ok) {
    state = p->field_30 + 1;
} else {
    state = 6;
}
p->field_30 = state;
obj = p->field_20;
```

Store in each arm instead. GCC still emits the shared `sw` after the join, but
keeps it *before* the sibling load:

```c
if (ok) {
    p->field_4 = 0;
    p->field_30 = p->field_30 + 1;
} else {
    p->field_30 = 6;
}
obj = p->field_20;
```

`Mc_StateOpenDirEntry` is the pure example (with `register asm` pins for `s0`/`s1`/`s2`).

## `u32` switch discriminator for `sltiu` range split

When a switch on a byte field is compiled as equality-on-2, then
`sltiu`/`beqz` for the `>= 3` group, then equality-on-1, a `u8` temporary
often lowers the range check to signed `slti`:

```
beq   v1, v0, case2
slti  v0, v1, 3     /* wrong — target wants sltiu */
beqz  v0, high
```

Hold the discriminator in a `u32` (or compare via an unsigned cast) so the
range check is `sltiu`:

```c
u32 mode;

mode = ptr->field_11; /* u8 load still lbu */
switch (mode) {
case 3:
case 0x20:
    /* ... */
    break;
case 2:
    /* ... */
    break;
case 1:
    /* ... */
    break;
}
```

`Display_FlipOtAndDispatch` is the pure example (`u8 mode` → 97.6%, `u32 mode` → 100%).

## Reassign `ptr = base + i` instead of `ptr++` to avoid mid-struct IV

When a loop walks a large struct array and also takes the address of a mid-struct
field for a call (`func(&ptr->field_14, ...)`), writing `ptr++` (or a for-loop
`ptr++`) lets GCC strength-reduce `&ptr->field_14` into a second induction
variable (`s1 = s0 + 0x14`, advanced by the same stride). Symptoms:

- Extra callee-saved reg and larger stack frame
- `move a0, s1` instead of `addiu a0, s0, 0x14`
- `lbu v0, -0x13(s1)` for earlier fields instead of `lbu v0, 1(s0)`
- Constant compares pinned in s-regs (`li s5, 2`)

Branch-local `ptr++` after the call can avoid the IV but schedules the increment
*before* the `jal` (capturing `a0` early), so the target's `j` delay
`addiu s0, s0, stride` becomes `addiu s1, s1, 1` instead.

Fix: recompute the element pointer from the index each iteration, with no
`ptr++`:

```c
for (i = 0; i <= 0; i++) {
    ptr = &Midi_Song + i; /* not ptr++ */
    if ((id == ptr->field_1) || (id == 0)) {
        if (ptr->field_0 == 2) {
            ptr->field_0 = 0x80;
            LinInterp_Setup(&ptr->field_14, vol, 0, fade);
        } else {
            ptr->field_0 = 4;
        }
    }
}
```

GCC still walks with `addiu s0, s0, 0x5DC` and fills the post-call `j` delay
with that step, but emits a one-shot `addiu a0, s0, 0x14` for the call.
`Midi_StartFadeOut` is the pure example.

## `field <<= 16` reloads; `field = saved << 16` CSE's into a callee-saved

When a handler saves `temp = p->field_0`, then later both (a) stores a shifted
copy into the field and (b) compares the field against `temp << 16` to restore:

```c
temp = p->field_0;
/* … other work that does not touch field_0 … */
p->field_0 = temp << 0x10;   /* BAD for some targets */
func(p);
if (p->field_0 == (temp << 0x10)) {
    p->field_0 = temp;
}
```

GCC CSE's the shift into a callee-saved (`sll s0, s2, 0x10; sw s0, …` then
`bne v0, s0`), which grows the stack frame and mismatches targets that reload:

```
lw   v0, 0(s0)
sll  v0, v0, 0x10
jal  …
 sw   v0, 0(s0)
…
lw   v1, 0(s0)
sll  v0, s1, 0x10
bne  v1, v0, …
```

Write the store as an in-place shift so the load is re-issued and the compare
recomputes from the saved original only:

```c
temp = p->field_0;
/* … */
p->field_0 <<= 0x10;
func(p);
if (p->field_0 == (temp << 0x10)) {
    p->field_0 = temp;
}
```

Contrast with `Ui_DrawAndCallback`, where the target *does* keep the shifted value in
an s-reg — there `p->field_0 = temp << 0x10` is correct. `Ui_AnimCloseStep` is the
reload form; pick based on whether the target reuses a shifted s-reg after the
call or re-shifts from the original.

## `tmp = (s32)base` barrier + `register … asm("a2")` for dual-pointer init loops

When the target opens a fixed-base + walking-pointer init as:

```
lui    v0, %hi(arr)
addiu  a3, v0, %lo(arr)   /* base */
move   a1, a3             /* p = base */
move   a2, zero           /* offset = 0 */
addiu  t0, a1, 0xb8       /* end = p + n */
…
addu   a0, a2, a3         /* ptr = offset + base */
```

two coupled problems appear:

1. **`offset = 0` between `base = arr` and `p = base`** forces the load into `$a3`
   (needed so `addu a0, a2, a3` and `move a1, a3` match) but GCC hoists the
   independent `move a2, zero` *before* the `lui` of the array.
2. **`p = base` immediately after the load** schedules `move a2, zero` correctly
   but steals the load into `$a1` (p is the heavier user).

Fix both with a live integer copy of the base as a scheduling barrier, then
assign `p` and `offset`, and pin the offset register:

```c
register s32 offset asm("a2");
s32 tmp;
volatile PadState* base;
volatile PadState* p;

base   = Pad_States;
tmp    = (s32)base;   /* barrier: completes base before p, load stays in $a3 */
p      = base;
offset = 0;           /* now after move a1, a3 — not hoisted before lui */
do {
    ptr = (u8*)(offset + tmp); /* addu a0, a2, a3 */
    /* byte-clear 0x5C; set fields via p */
    p++;
    offset += 0x5C;
} while (p < base + 2); /* end materialises as addiu t0, a1, 0xb8 */
```

`tmp` must stay live for the whole loop (used in `offset + tmp`) so it is not
DCE'd as a dead store. Pinning `offset` to `$a2` keeps the zero and the
`addiu …, 0x5C` on that register. `Pad_Init` is the pure example.

### Companion: non-volatile load, volatile stores for pad buffers

`PadInitDirect((u8*)pad, (u8*)(pad + 1))` wants `lui s0` / `addiu s0, s0, %lo`
on a plain `PadRawPort*`. Field stores `pad->field_2 = 0xFF` without `volatile`
rebase the pointer (`addiu s0, 3` / `sb -1(s0)`). Load into a non-volatile
pointer for the call, then assign a `volatile PadRawPort*` for the init loop:

```c
PadRawPort* pad;
volatile PadRawPort* vpad;

pad = Pad_RawPorts;
PadInitDirect((u8*)pad, (u8*)(pad + 1));
vpad = pad;
for (j = 0; j < 2; j++) {
    vpad->field_2 = 0xFF;
    vpad->field_3 = 0xFF;
    vpad++;
}
```

## Dense dummy cases force jump tables for sparse result maps

A switch that maps only a few status codes (e.g. 0→0xA, 1→0x14, 4→0x15,
7→0x19) with everything else → default will often lower to an if/else cascade
even when the target uses a jump table over 0..N-1. GCC 2.8.1 only emits the
`sltiu` + table form when enough case labels fill the span:

```c
/* Sparse — binary search / equality chain, no jtbl */
switch (status) {
case 0:  arg0->field_30 = 0xA;  break;
case 1:  arg0->field_30 = 0x14; break;
case 4:  arg0->field_30 = 0x15; break;
case 7:  arg0->field_30 = 0x19; break;
default: arg0->field_30 = 0x2A; break;
}

/* Dense — sltiu + jtbl; unused slots share the default body */
switch (status) {
case 0:  arg0->field_30 = 0xA;  break;
case 1:  arg0->field_30 = 0x14; break;
case 4:  arg0->field_30 = 0x15; break;
case 7:  arg0->field_30 = 0x19; break;
case 2:
case 3:
case 5:
case 6:
default: arg0->field_30 = 0x2A; break;
}
```

Hold the discriminator in a `u32` so the range check is `sltiu` (see also
"u32 switch discriminator"). Assign the destination field *inside* each arm;
routing through an intermediate then storing once can pick the wrong register
for the case immediates (`v1` vs target `v0`).

`Mc_StateCreateFile` is the pure example (MemCardCreateFile status → UI state).
Remember to hand the jtbl's address range to the C file via a `.rodata`
subsegment in `configs/USA/main.yaml` (split surrounding asm tables into
`mc_1` / `mc_2` style siblings).

When only **two** unique non-default results fill the span (e.g. 0→0x1A,
5→0x7, everything else →0x18), fall-through dummy labels alone still lower to
a binary-search cascade (`sltiu` 5 / `beq` case5 / `bnez` case0). Give each
in-range slot its own assignment + `break` so the case-cost estimator sees
enough nodes; GCC then emits the jump table and merges the identical bodies
back to a shared label:

```c
/* Two unique results over 0..5 — fall-through dummies stay if/else */
switch (status) {
case 0:  arg0->field_30 = 0x1A; break;
case 5:  arg0->field_30 = 0x7;  break;
case 1:
case 2:
case 3:
case 4:
default: arg0->field_30 = 0x18; break;
}

/* Separate arms force jtbl; identical 0x18 bodies share one label */
switch (status) {
case 0:  arg0->field_30 = 0x1A; break;
case 1:  arg0->field_30 = 0x18; break;
case 2:  arg0->field_30 = 0x18; break;
case 3:  arg0->field_30 = 0x18; break;
case 4:  arg0->field_30 = 0x18; break;
case 5:  arg0->field_30 = 0x7;  break;
default: arg0->field_30 = 0x18; break;
}
```

`Mc_StateOpenRead` is the pure example (MemCardOpen status → UI state).

## `register s32 idx asm("v1")` for `andi v1,a1,0xff` + delay-slot `-1`

When the target opens with:

```
andi   v1, a1, 0xff
bnez   v1, nonzero
 addiu  v1, v1, -1    # delay: idx-1 always computed
```

a plain `s32 idx = arg1 & 0xFF` often coalesces into `andi a1,a1,0xff` (clobbering
`$a1`). Pin the masked index:

```c
register s32 idx asm("v1");
idx = arg1 & 0xFF;
if (idx != 0) {
    idx = idx - 1;
    /* use idx as original-1; GCC keeps addiu in the bnez delay slot */
    p = base->entries[idx].field;
}
```

Pair with `base->entries[idx].field` (not `*(T*)(base + idx*stride + off)`) so
the scaled add is `addu v0, a0, v0` (base first). For the fall-through `== 0`
path that builds a big-endian u32 then adds the base, write
`offset = offset + (u32)ptr; return (void*)(offset + N);` to get
`addu v0, v0, a2` (offset first).

`Midi_ResolveTrackData` is the pure example (track data pointer resolve from
`MidiSong` / `field_10`).

## Force `move aN, s0` for a known-zero live return value

When the target sets `s0 = 0` early as a live return flag and later passes that
same register three times into a call (`move a0,s0; move a1,s0; move a2,s0`),
plain C with `ret = 0; foo(ret, ret, ret, …)` collapses to
`move a0,zero; move a1,a0; move a2,a0` via REG_EQUAL of const 0.

Two pieces are required:

1. **Kill REG_EQUAL** with an empty operand asm so the value is still in a
   register but no longer a proven constant:
   ```c
   asm("" : "+r"(ret));
   foo(ret, ret, ret, 5);
   ```
2. **Pin the register** so the empty asm does not reshuffle the earlier
   frame/arg allocation:
   ```c
   register s32 ret asm("s0");
   ```

Without the pin, `asm("" : "+r"(ret))` alone often copies `arg0` into `$v1`
early and shifts the stack loads. Without the asm, CSE substitutes `$zero`.

### Callee parameter width must not re-extend at the call site

If the callee is declared `s16`/`s16`/`s16`, an "unknown" `s32` (post-asm)
gets `sll`/`sra` sign-extension at the *call site*, which the target does not
have (extension lives inside the callee for `GetTPage` etc.). Declare the
callee as `s32` and cast to `s16` only where the body needs it:

```c
void Prim_DrawTPage(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    /* … */
    GetTPage(0, (s16)arg0, (s16)arg1, (s16)arg2);
}
```

That keeps the callee's leading `sll`/`sra` chain while call sites can emit
plain `move aN, s0`. `Prim_DrawFadeTile` is the pure example.

## `(s8)u8_field` at call sites forces `lb` with `s32` formals

When a callee must take `s32` args so its *body* does not re-sign-extend the
incoming registers (a plain `s8` formal emits early `sll`/`sra` on `$a1` and
breaks an otherwise perfect match), call sites that pass `u8` struct fields
would emit `lbu`. The original often wants `lb`.

Cast the field through `(s8)` at the call:

```c
/* callee: void SndVoice_SetPanRamp(s32, s32, s32); — body keeps $a1 as-is */
SndVoice_SetPanRamp(idx, (s8)p->field_4, (s8)mid->field_1); /* lb, not lbu */
```

Bare `p->field_4` with an `s8` formal also yields `lb`, but then the callee
mismatches. Prefer `s32` formals + `(s8)` at the few call sites. `SndVoice_SetPanRamp`
/ `SndEvt_HandlePanRamp` are the pure example (sibling `SndVoice_SetVolumeRamp` already takes
`s32` and its caller correctly uses `lbu`).

## Early load into a temp forces prior store before zero-fills

When the target does:

```
lbu  v0, field_a(src)
nop
sh   v0, field_x(dst)   /* must store first to free v0 */
lbu  v0, field_b(src)
sh   zero, field_c(dst)
sw   zero, field_d(dst)
...
sh   v0, field_y(dst)   /* field_b value stored last */
```

writing the C in source order:

```c
p->field_x = src->field_a;
p->field_c = 0;
p->field_d = 0;
p->field_y = src->field_b;
```

lets GCC keep `field_a` in `$v0` across the zero stores and emit
`sh field_x` *after* them. Force the free by loading `field_b` into a temp
before the zeros:

```c
p->field_x = src->field_a;
temp = src->field_b;   /* lbu reclaims v0 → prior sh must emit first */
p->field_c = 0;
p->field_d = 0;
p->field_y = temp;
```

`SndVoice_SetupEnvelope` is the pure example (SndVoiceFx init from SndNote bytes).

## Overlay pointer at a fixed offset for multi-field base `$tN`

When the target does `addiu t0, a0, 0x10` once and then addresses many fields
as `N(t0)`, take the address of the first field (or cast it to a nested
struct type) and store through that pointer:

```c
SndVoiceFx* p = (SndVoiceFx*)&arg0->field_10;
p->field_20 = chunk;
p->field_1 = 0;
/* ... */
```

Keep one or two stores as `arg0->field_10 = …` (parent-relative) when the
target uses `sb …, 0x10(a0)` rather than `sb …, 0(t0)`.

## Prefer separate stores over `next` + goto for shared `field_30`

When the target joins two arms on a shared `sw v0, 0x30(s0)` after loading
different constants into `$v0`, a C `next` temp plus goto often matches the
control flow but **swaps** `$s0`/`$s2` for `arg0`/`arg1` (or otherwise shifts
prologue allocation).

Writing the store in each arm separately still CSE's into one shared `sw`,
and keeps the natural `s0 = arg0; s2 = arg1` prologue:

```c
/* Matches: j store; li v0,8  /  …; li v0,0x27; store: sw v0,0x30(s0) */
if (ret != -1) {
    if (ret == 1) {
        arg0->field_30 = 8;
    }
} else {
    /* … side effects … */
    arg0->field_2a = 0xC;
    arg0->field_30 = 0x27;
}
```

Avoid:

```c
/* Same CFG shape, wrong s0/s2 assignment */
if (ret != -1) {
    if (ret == 1) {
        next = 8;
        goto store;
    }
} else {
    next = 0x27;
store:
    arg0->field_30 = next;
}
```

`Mc_StatePromptChoiceB` is the pure example (memcard state handler next to
`Mc_StatePromptChoice9`).

## Reload pointer into a0 with halfword temp for dual end stores

When the target ends like Mc_StateCloseReturn but also copies a halfword from
a second arg before setting `field_2E = -1`:

```
lw    a0, 0x20(s3)       /* reload Task::field_20 */
li    v0, K
beqz  a0, end
 sw   v0, 0x30(s3)
lhu   v1, 0xA18(s5)      /* halfword from McWork */
li    v0, -1
sh    v0, 0x2e(a0)
sh    v1, 0x2c(a0)
```

do **not** reuse the earlier `obj` for the null check (extends live range,
scrambles s-regs). Declare a fresh pointer plus an `s16` temp, load the
halfword first inside the `if`, then store:

```c
UiObject* flag;
s16       val;

arg0->field_30 = K;
flag = arg0->field_20;
if (flag != NULL) {
    val            = arg1->field_A18; /* s32→s16 emits lhu */
    flag->field_2E = -1;
    flag->field_2C = val;
}
```

Cast-only double loads of `arg0->field_20` (as in Mc_StateCloseReturn) reload
twice and miss the `lhu`/`sh` pairing. `Mc_StateClosePrompt` is the pure example.

## `s16` first arg forces `$a2`/`$a3` dual copies of the promoted value

When the target saves arg0 twice before the early-out check:

```
move  a2, a0
andi  a1, a1, 0xff
beqz  a1, ret0
 move a3, a2          /* delay: second full-width copy */
andi  a1, a3, 0xffff  /* u16 id from a3 */
...
andi  v0, a2, 0xf000  /* switch key from a2 */
...
andi  v0, a3, 0xffff  /* default path re-masks from a3 */
```

declaring the first parameter as `s32` and writing `u16 x = arg0` only produces
`move a2, a0` plus a short-lived `move v0, a2` for the truncation — `$a3` never
appears, and the default re-mask uses `$a2` (~99.7%).

Fix: type the first parameter as `s16` (or `short`). The ABI still passes it in
`$a0`, but the HImode formal forces a second full-width copy into `$a3` for the
`u16` path while `$a2` holds the value used for wider masks (`& 0xF000`). Keep
the rest of the `SndLoad_AllocBuffer` pattern:

```c
s32 func_...(s16 arg0, s32 arg1)
{
    u16 x;

    x = arg0;
    if ((arg1 & 0xFF) == 0) {
        return 0;
    }
    /* table[x >> 12], switch ((u32)(arg0 & 0xF000) >> 12), ... */
}
```

`SndBank_FreeById` is the pure example. Hard-register pins on the copies swap
`$a2`/`$a3` or destroy `$a2` in-place for the switch key.

## `if (p != NULL) { if (x == V) return; } else { ... } /* fallthrough */`

When the target lays out blocks as:

```
beqz  p, L_else        # null → else
...
bne   x, V, L_cont     # non-V → continuation (past else)
j     epilogue
 move v0, zero         # early return for x == V
L_else:
 ...                   # null path
 j    epilogue
 ...
L_cont:
 ...                   # non-null && x != V path
```

write the early-return as a nested `if` inside the non-null arm, put the null
path in the `else`, and leave the non-null non-V work as a shared continuation
after the whole if/else:

```c
if (arg0 != NULL) {
    if (arg0->field_8 == 5) {
        return 0;
    }
} else {
    /* null path — absolute draw */
    ...
    return arg1;
}
/* non-null && field_8 != 5 — relative draw */
...
return diff;
```

`if (arg0 == NULL) { null; return; } if (field_8 == 5) return 0; /* draw */`
inverts the first branch (`bnez`) and places return-0 after the draw block.
`if (arg0 != NULL) { if (field_8 != 5) { draw; return; } return 0; } /* null */`
keeps `beqz` but falls through into the draw and branches on `beq` for the
early return — wrong block order. Only the if/else + continuation shape emits
`bne` over both the early return and the else into the shared tail.

`Text_DrawPrompt` is the pure example (two `TextDrawReq` stack slots at `sp+0x10`
and `sp+0x20` for the two draw paths).

## Interleaved jump tables: pad + absolute copy for still-asm neighbors

When matching a switch function whose `jtbl` sits *between* another still-asm
function's table and an already-matched C table, GCC emits the matched tables
contiguously in `.rodata` and squeezes out the middle slot. Link then fails
(undefined `.L` labels from the asm table) or the later C table lands at the
wrong VMA.

Fix without matching the middle function yet:

1. Expand the C unit's `.rodata` subsegment in `configs/USA/main.yaml` to cover
   from the newly matched jtbl through the last C-owned jtbl (including the
   middle slot's bytes).
2. Emit a 4-byte alignment pad (GCC's 9-entry tables are 0x24; original layout
   pads to 0x28) plus a global `const s32 jtbl_XXXXXXXX[N]` filled with the
   **absolute** case addresses from the original table. The still-asm function
   keeps referencing that symbol; the pad keeps subsequent tables at the right
   offsets.

```c
static const s32 s_jtbl_pad = 0;
const s32 jtbl_80012FCC[9] = {
    0x8001D29C, /* absolute targets of still-asm CdCmd_EnqueueFollowUp */
    /* ... */
};
```

Remove the pad and absolute table when the middle function is matched (its
compiler-generated jtbl will occupy the slot naturally). `CdCmd_ProcessPhase2` is the
example (pad for `jtbl_80012FCC` / `CdCmd_EnqueueFollowUp`).

## Hex digit loop: `asm("")` after the raw-digit store

Unsigned hex itoa (`Text_ItoaHex`) stores the raw nibble then overwrites it
with ASCII. With `-fschedule-insns2`, GCC freely moves the first `sb` after
`andi`/`mult` (starting the mult early). The target keeps:

```
mflo  v0
nop
sb    v0, 0(a3)     /* raw digit */
andi  v1, v0, 0xff
mult  v1, a2
srl   a2, a2, 4
sltiu v0, v1, 0xa
mflo  t4
```

An empty `asm("");` between the raw store and the mask restores that order
without changing semantics. Reusing the digit variable for the mask
(`digit &= 0xFF`) also forces store-first but collapses `andi` into the same
register and breaks the later `sltiu`/`addiu` schedule.

## Split-address formation vs `0(reg)` loads of a string

For the zero-path copy of `D_800138C8` ("0"), the target wants:

```
lui   v0, %hi(D_800138C8)
addiu t3, v0, %lo(D_800138C8)
lb    t0, 0(t3)
lb    t1, 1(t3)
```

With `-msplit-addresses`, `src = D_800138C8; c0 = src[0]` emits the lui/addiu
pair correctly but rewrites the first load to `lb t0, %lo(D_800138C8)(v0)`.
`asm("" : "+r"(src))` kills the REG_EQUIV and yields `lb 0(t3)`, but also
rematerializes as `lui t3` / `addiu t3,t3`. When both the intermediate `$v0`
form *and* `0(t3)` loads are required, pin `src` to `$t3` and emit the
address with inline asm:

```c
register s8* src asm("t3");
register s32 hi asm("v0");
asm volatile(
    "lui %1, %%hi(D_800138C8)\n\t"
    "addiu %0, %1, %%lo(D_800138C8)"
    : "=r"(src), "=r"(hi));
```

`Text_ItoaHex` is the pure example. Power-of-two / no-div TUs do not need
`--expand-div`; this one does (`textdraw.c`).

Signed hex sibling `Text_ItoaHexSigned` reuses the same digit-loop tricks (`asm("")`
after the raw store, inline-asm zero-path with `D_800138C8`) but:

- Prefixes `'-'` and recurses on `-arg1` when `arg1 < 0` (return value is the
  original buffer, held in `$s0`).
- Uses **signed** place math: `sra` / `slt` / `blez`/`bgtz` instead of
  `srl` / `sltu` / `beqz`/`bnez`, and `div` instead of `divu`.
- Saves `arg0` in `$s0`, so `place` can live in `$a0` and `dest` in `$a2`
  (unsigned keeps `arg0` in `$a0`, so `place`/`dest` are `$a2`/`$a3`).
- Zero-path pins differ: `src` in `$t2`, loads into `$a3`/`$t0`.

## Unsigned decimal itoa: keep raw digit store via reload-style mask

Unsigned decimal itoa (`Text_ItoaUnsigned`) stores the raw quotient then overwrites
it with ASCII, interleaved with `place /= 10` (magic `0xCCCCCCCD` multu):

```
mflo  v1            /* digit */
nop
nop
multu a2, t0        /* start place/10 */
mfhi  t5
sb    v1, 0(a3)     /* raw digit — must stay */
andi  v0, v1, 0xff
mult  v0, a2
srl   a2, t5, 3
addiu v0, v0, 0x30
sb    v0, 0(a3)     /* ASCII */
```

Unlike the hex siblings, there is no `if (temp >= 10)` branch to keep the first
store alive. Plain `*dest = digit; temp = digit & 0xFF; *dest = temp + '0'`
DSE-eliminates the raw store. `volatile` keeps it but schedules it too late
(after `addiu +0x30`). Dummy if/else with identical arms keeps the store but
pulls `mflo` of the product too early.

Match with a memory-style mask that still CSEs to the register form:

```c
*dest = digit;
temp  = *dest & 0xFF;   /* keeps sb; compiles as andi, not lbu */
digit = temp * place;
place /= 10;
*dest = temp + 0x30;
```

Also hoist `cmp = arg1 < place` *before* the zero check so `sltu` lands in the
delay slot of `bnez arg1` (result discarded on the zero path, reused after).
Clamp overflow with a 10-byte `u8[10]` struct assign of `"999999999"`; zero path
is a 2-byte `u8[2]` assign of `"0"` — both emit the unaligned lwl/lwr/lb
sequence without register pins. Needs `--expand-div` (`textdraw.c`).

## Signed decimal itoa: do **not** pin `place`/`dest`/`digit`

Signed sibling `Text_ItoaSigned` (same shape as unsigned `Text_ItoaUnsigned`, plus a
`'-'` / recurse prefix and signed `slt`/`blez`/`div`) needs natural regalloc:

- Start `place` at `0x989680` (10^7); clamp with a 9-byte copy of `"99999999"`.
- Same digit-loop pattern as unsigned (`temp = *dest & 0xFF`, hoist `cmp` before
  the zero check, `place > 0` for the digit loop).
- **Do not** `register ... asm("a2"/"a3"/"v1")`. Pins push the signed `/10`
  magic (`0x66666667`) into `$v1` and force `sra place, place, 31` instead of
  the target's `lui a0, magic` / `sra v0, place, 31` / `sra v1, hi, 2` form.
  Unpinned, GCC picks `$a0` for magic and `$v1` for digit exactly as in the ROM.
- The empty `if (!arg0) {}` delay-slot trick is **not** needed once pins are
  gone — the overflow `beqz` fills with `lui %hi(D_800138BC)` on its own.

`Text_ItoaSigned` is the pure example. Needs `--expand-div` (`textdraw.c`).

## Loop-invariant QImode constants: `s8` temp + widen via `s32`

When a loop repeatedly stores a small constant into a byte field
(`p->field_C = 4`), LICM often pins the constant in a callee-saved register
(`li s5,4` in the prologue), which grows the stack frame and mismatches a
target that reloads with `li v1,4` each iteration.

Routing the store through an `s8` (or `char`) temporary *and then* an `s32`
temporary blocks both the hoist and the QImode CSE that would otherwise keep
the value in an s-reg:

```c
s8  c;
s32 tmp;

c = 4;
/* other stores */
tmp = c;
p->field_C = tmp;
c = 2;
p->field_D = c;
```

A plain `s32 v = 4; p->field_C = v; v = 2; p->field_D = v;` also avoids the
s-reg hoist but schedules `li v1,4` *before* the `move a0/a1` setup for the
following call. The `s8`+`s32` widen form keeps `move a0,p; move a1,buf;
li v1,4` order.

An empty `do {} while (0);` inside a nearby `if` can also be required to lock
that schedule (same role as other `do {} while (0)` notes in this file).

`Text_MeasureMultiLine` is the pure example (multi-line text measure).

## Pointer init order swaps callee-saved assignment (`s3`/`s4`)

When two long-lived pointers are captured once in the prologue
(`p = &stack_struct; buf = stack_array`), the order of those assignments
controls which gets `$s3` vs `$s4`. Target:

```
move  s4, zero        /* maxWidth first */
move  s2, s4          /* height = maxWidth */
addiu s1, sp, 0x50    /* p */
addiu s3, sp, 0x10    /* buf */
```

With `buf = sp10; cur = arg0` the allocator often puts `buf` in `$s4` and the
accumulator in `$s3`. Swapping to:

```c
cur = arg0;
buf = sp10;
```

(after `p = &sp50` and the zero init of the accumulators) flips `buf` into
`$s3` and the max-width accumulator into `$s4` without changing semantics.
`Text_MeasureMultiLine` needs this together with the `s8`/`s32` constant trick above.

## Force `Mem_Set` arg order: `move a1,zero` then `lui a2` then dest load

When the target schedules a large-size `Mem_Set(ptr, 0, 0xNNNNN)` after a
branch as:

```
bnez  cond, other
 move  a1, zero       /* delay: fill byte */
lui   a2, HI(size)
lui   v0, %hi(ptr)
lw    a0, %lo(ptr)(v0)
jal   Mem_Set
 ori   a2, a2, LO(size)
```

plain `Mem_Set(D4CB64_ImgBuffers, 0, 0x25800)` usually loads the global first
(`lui v0` in the delay slot), then sets `a1`/`a2`. Score sticks at ~99% with
only that reorder left.

Two pieces together fix it:

1. **Pin + kill REG_EQUAL on the fill and the size high half**, then complete
   the low half with a bitwise OR so the `ori` can still fill the `jal` delay
   slot:
   ```c
   register s32 ch asm("a1");
   register u32 size asm("a2");

   ch = 0;
   size = 0x20000; /* high half only — do not write 0x25800 here */
   asm("" : "+r"(ch), "+r"(size));
   Mem_Set(D4CB64_ImgBuffers, ch, size | 0x5800);
   ```
2. Without the empty `asm`, CSE folds `ch` back to `$zero` / reorders past the
   load. Without splitting `0x25800` into `0x20000 | 0x5800`, the full constant
   is materialised before the call and the `jal` delay becomes `nop`.

`Boot_LoadInitialFile` is the pure example (fade-out complete → clear image
buffers). Same shape as the empty-asm REG_EQUAL kill under “Force `move aN, s0`
for a known-zero live return value”.

## Two-step address through the same pointer keeps `addu`/`addiu` in one reg

When the target builds a slot pointer as:

```
addu  a1, a3, s0      /* tmp = offset + base */
addiu a1, a1, 0x504   /* slot = tmp + voiceSlots */
```

a single expression `(offset + (s32)base) + 0x504` (or `base->voiceSlots` with
an index) often lands the intermediate in `$v0`:

```
addu  v0, a3, s0
addiu a1, v0, 0x504
```

Force the intermediate into the final register by assigning through the *same*
pointer variable twice — first the integer `offset + base`, then the struct
field that adds the fixed mid-struct offset:

```c
slot = (MidiNoteSlot*)(offset + (s32)obj);
slot = ((MidiSong*)slot)->voiceSlots; /* addiu a1, a1, 0x504 */
```

`Midi_InitSlot` is the pure example (voice-slot clear loop). Pair with the
`offset + (s32)base` integer cast (see “Force `addu rd, offset, base`”) so the
`addu` operands stay offset-first.

## Separate `ptr += N` for switch case vs default (delay-slot fill)

When a binary switch decision tree matches the target but every arm ends with
the same `stream += 2` before shared work, a single post-switch advance (or
identical advances that rejoin via one label) collapses to one `addiu` that
empty/default arms branch *to*:

```
bne  id, K, shared      /* wrong — target wants j after; addiu in delay */
nop
j    shared
 sw   val, 0(stream)
shared:
addiu stream, stream, 8
after:
lw   ...
```

The target keeps **two** advances: case arms fall through a shared `addiu`,
while default is `j after` with `addiu` in the delay slot:

```
beq  id, K, case_body
nop
j    after
 addiu stream, stream, 8   /* default */
case_body:
j    case_advance
 sw   val, 0(stream)
case_advance:
addiu stream, stream, 8
after:
lw   ...
```

Force that shape with an explicit decision tree and **two** advance labels
that both do `ptr += N` but are not merged before the shared load:

```c
if (id == 0x3B) {
    goto case_advance;
}
if (id < 0x3C) {
    if (id == 0x38) {
        goto case_38;
    }
    goto default_advance;
}
/* ... more positive branches to late bodies ... */
goto default_advance;

case_38:
    *stream = 0x4038;
    goto case_advance;
case_78:
    *stream = 0x4078;
case_advance:
    stream += 2;
    goto after;
default_advance:
    stream += 2;
after:
    dims = *stream;
    /* ... */
```

A plain `switch` with empty cases for the non-writing IDs folds those cases
into default and loses the `sltiu` split. Putting `stream += 2` only after the
switch (or on every arm with one join) also merges the two `addiu`s.

`Tmd_RewriteOpcodes` is the pure example (rewrite opcodes `0x38`→`0x4038` /
`0x78`→`0x4078` while walking a `[id, handler, dims, data…]` stream).

## Dual terminator constants for outer/inner loops

When the target preloads `-2` into one register for the outer "skip group"
check and reloads `-2` into another for the inner loop exit (`bne …, a1`),
hold the outer value in a local and keep the inner compare as a literal:

```c
u32 stop = -2;
do {
    if (*stream != stop) {
        do {
            /* ... */
        } while (*stream != -2U);
    }
    stream++;
} while (*stream != -1U);
```

Using the same `-2U` (or one local) for both compares coalesces them into a
single register and shifts the rest of the allocation.

## `u16` formal + `s32 key = arg0` for `move v0,a0` in `beqz` delay

When a `switch (arg1)` target opens with:

```
beqz  a1, case0
 move  v0, a0          /* delay: full-width copy of the first arg */
li    v0, 1
beq   a1, v0, case1
 move  v0, zero
...
case0:
move  a1, zero
andi  a0, v0, 0xffff   /* mask the copy, not a0 itself */
```

an `s32 arg0` formal with `arg0 &= 0xFFFF` collapses to `andi a0, a0, 0xffff`
and fills the `beqz` delay with `li v0, 1` instead (~97.5%).

Fix: type the first parameter as `u16` and widen into an `s32` local inside
the case that needs the zero-extend. The HImode formal forces a full-width
copy that delay-slot filling hoists into `$v0`:

```c
SndBankSlot* func_...(u16 arg0, s32 arg1)
{
    s32 key;
    /* ... */

    switch (arg1) {
    case 0:
        key = arg0;          /* move v0,a0  then  andi a0,v0,0xffff */
        /* walk with key */
        return NULL;
    case 1:
        key = arg0 & 0xF000; /* andi a0,a0,0xf000 — no extra copy */
        /* walk with key */
        break;
    }
    return NULL;
}
```

`SndBankSlot_Find` is the pure example. A plain `s32` formal never emits the
leading copy; `u16 key = arg0` alone reorders the `andi` after the table base
load.

## s16 field temps + reuse long-lived locals for post-loop `>> 8`

When a loop body uses two globals via live `%hi` bases in `$a1`/`$a2`, the
fixed-point targets must live in `$t*` so the arg registers stay free. After
the loop the target often reuses those same `$t*` for `(global >> 8) - field`
before a call.

Three pieces have to land together:

1. **Load both halfword fields into `s16` locals first**, then add the args.
   That forces `lh v0` / `lh v1` before both `addu`s (instead of folding each
   field into an in-place `addu a1, a1, v0`).
2. **Operand order `arg + base`** (not `base + arg`) so the `addu` is
   `addu tN, aM, vK` matching the target.
3. **Reuse the same `s32` locals after the loop** for `local = global >> 8`
   then `func(..., local - field, ...)`. Fresh temps for the epilogue put the
   loads straight into `$a1`/`$a2` and lose the `sra tN, v0, 8` form.

```c
s16 baseX, baseY;
s32 targetX, targetY;
u8 count;

baseX = obj->field_20;
baseY = obj->field_22;
targetX = arg1 + baseX;
targetY = arg2 + baseY;
targetX <<= 8;
targetY <<= 8;
count = Display_State.field_10a;
if (count != 0) {
    do {
        i += 1;
        gX += (targetX - gX) >> 2;
        gY += (targetY - gY) >> 2;
    } while (i < count);
}
targetX = gX >> 8;          /* reuses the loop temps → sra t1/t0 */
targetY = gY >> 8;
func(obj, targetX - obj->field_20, targetY - obj->field_22);
```

`Ui_SmoothCursor` is the pure example (smooth cursor toward a UI object over
`Display_State.field_10a` frames, then call `Ui_DrawCursor`).

## `u8` index + `arr[i]` for large-offset slot walks

When the target does:

```
andi  v1, a0, 0xf
...
andi  a0, v1, 0xff          /* delay of a later branch */
sll   v0, a0, 3
...
move  s0, a2
lb    v0, 0x506(s0)         /* voiceSlots[i].field_2 */
...
addiu s0, s0, 0xc
```

two codegen traps show up:

1. **`s32 idx = arg0 & 0xF` kills the zero-extend.** GCC proves the value is
   already 0..15 and drops `andi a0, v1, 0xff`. Hold the nibble in a `u8` so
   promoting it to an array index / shift emits the zero-extend:

```c
u8 t = arg0 & 0xF;
/* later: field_484[t] / voiceSlots[i].field_1 == t  →  andi a0, v1, 0xff */
```

2. **A mid-struct pointer rebases the walk.** `slot = arg2->voiceSlots; slot++`
   becomes `addiu s0, a2, 0x504` + relative `lb 2(s0)`. Indexing from the parent
   keeps the full base and the large offsets:

```c
for (i = 0; i < 0x12; i++) {
    if (arg2->voiceSlots[i].field_2 == param &&
        arg2->voiceSlots[i].field_1 == t) {
        Spu_KeyOff(arg2->voiceSlots[i].field_0);
    }
}
```

3. **Do not copy `t` into a separate `s32 idx` for the loop.** That extra live
   range often swaps the `s3`/`s4` assignment order (`move s4, a0` before
   `andi s4, a1, 0xff`). Use the `u8` directly in both the table index and the
   loop compare.

`Midi_KeyOffChannel` is the pure example (opcode nibble + optional `0x90` skip,
then key-off matching SPU voice slots).

## Force late independent store with a reloaded temp

When the target stores field A, then an unrelated constant to field B, then
field C derived from a second load of a source already used earlier, GCC may
hoist the constant store into the load/add/store window of A:

```
# wanted
lbu  v0, w(src)
nop
addiu v0, v0, 1
sh   v0, w(dst)
lbu  v0, h(src)
li   v1, K
sh   v1, clut(dst)
addiu v0, v0, 1
sh   v0, h(dst)

# got (same score almost, wrong schedule)
lbu  v0, w(src)
li   v1, K
sh   v1, clut(dst)
addiu v0, v0, 1
sh   v0, w(dst)
...
```

Fix: load the next source into a temporary *before* the independent store so
the constant has nowhere to sit between A's load and store:

```c
p->w = src->w + 1;
temp = src->h;       /* pins the next load here */
p->clut = 0x7FFD;    /* independent store lands after w, before h+1 */
p->h = temp + 1;
```

`Text_DrawGlyphImmediate` is the pure example (SPRT setup: w, then clut 0x7FFD, then h).

## `do {} while (0)` + address-of global for store/`subu` interleave

When a tail assigns several BSS size/heap globals and the target wants:

```
lui  v0, %hi(D_sizeA)
lui  v1, 0x1
sw   v1, %lo(D_sizeA)(v0)
lui  v0, %hi(D_sizeB)
subu a0, v1, s1
sw   a0, %lo(D_sizeB)(v0)
```

a natural `D_sizeA = 0x10000; size = 0x10000 - arg; D_sizeB = size;` schedules
`subu` *before* the first `sw` (into the free slot after `lui v1`). Wrapping
only the first store in `do {} while (0)` forces that store first but can put
`subu` before the second `lui %hi`. Taking the address of the second global
before the subtraction restores the full sequence:

```c
size_t size;
size_t* pSize;

imgBufSize = 0x25800; /* keep late pointer math constant live early in $a1 */
do {
    D_80068F90 = 0x10000;
} while (0);
pSize  = &GActiveAuxHeapSize; /* lui %hi before subu */
size   = 0x10000 - arg3;
*pSize = size;
D_800691F8   = 0x10000;
GAuxHeapSize = size;
```

`Gfx_StoreImageSlot` is the pure example (VRAM `StoreImage` then aux-heap base/size
setup). Sibling `Gfx_LoadImageSlot` is the matching `LoadImage` without heap work;
note its `arg2` → `rect.y` polarity is the opposite of `Gfx_StoreImageSlot`.

## Goto-loop vs while: LICM of loop-invariant masks

GCC 2.8.1's loop-invariant code motion hoists `arg & 0xFFFF` out of a
`while (1)` / `for (;;)` / `do {} while`, rewriting it as an early
`andi aN,aN,0xffff` and breaking delay-slot fills. The same body written as a
goto-based loop does **not** get that hoist, so the mask stays at the use site
and can fill the previous branch's delay slot:

```
bne  v1, v0, cont        /* e.g. field_E != arg0[0] */
 andi v0, a1, 0xffff     /* delay: arg1 & 0xFFFF — only with goto-loop */
lhu  v1, 0x10(a3)
bne  v1, v0, cont
```

```c
/* Hoists andi to prologue */
while (1) {
    if (entry->field_E == arg0[0]) {
        if (entry->field_10 == (arg1 & 0xFFFF)) { /* ... */ }
    }
    /* ... */
}

/* Keeps andi at the compare (delay-slot fillable) */
loop:
    if (entry->field_E == arg0[0]) {
        if (entry->field_10 == (arg1 & 0xFFFF)) { /* ... */ }
    }
    i++;
    if ((u32)(i & 0xFFFF) < N) {
        goto loop;
    }
```

`Stream_FindSlot` is the pure example (search of `Stream_Slots`, mask of `arg1`
against `field_10`).

## Place `return -1` after shared match labels

When a search function has a shared "record match then re-check found" tail
(`matched:` / `matched_result:` after the main done-check), put the negative
return **after** those labels, not as the then-branch of the found test:

```c
done:
    if ((found & 0xFFFF) == 0) {
        goto ret_neg;
    }
    ret = result << 0x10;
    return ret >> 0x10;

matched:
    found = 1;
matched_result:
    result = i;
    goto done;

ret_neg:
    return -1;
```

Putting `return -1` directly under `if (found == 0)` with `matched` after
`done` often inverts the branch (`bnez` + early `-1`) and drops the
`li t1,1` / `j done` / `move t2,t0` block past the success `jr`. Target layout:

```
andi  v0, t1, 0xffff
beqz  v0, ret_neg
sll   v0, t2, 16
jr    ra
 sra  v0, v0, 16
li    t1, 1          /* matched */
j     done
 move t2, t0         /* matched_result */
jr    ra             /* ret_neg */
 li   v0, -1
```

Also: declare locals in order `i`, `found`, `result` (init `result = 0; i =
result; found = result`) so `$t0`/`$t1`/`$t2` match that assignment chain.

## `s16` temp for constant + `(s8)(u8)` halfword pair

When the target loads a constant into `$v1` in a branch delay slot, then
`lbu`/`sll`/`sra` a signed-byte field into `$v0`, then `sh` both halfwords
(`field_2E = 6` then `field_2C = field_8 + 1`), writing the constant store
first forces `li $v0, 6; sh $v0` and leaves a nop after the later `lbu`.

Hold the constant in an `s16` temporary, write the `(s8)(u8)` halfword first,
then store the temp:

```c
s16 temp;

if (Pad_CheckButtons(0, 1, mask) != 0) {
    temp           = 6;
    arg1->field_2C = (s8)(u8)arg0->field_8 + 1;
    arg1->field_2E = temp;
    return;
}
```

`li $v1, 6` fills the `beqz` delay slot; `lbu` reuses `$v0`; `sh $v1, field_2E`
sits between the load and the sign-extend. `Ui_DrawDialogLine` is the example.

## Array re-index each iteration to keep struct base as IV

When a loop walks a fixed-size struct array and both (a) loads fields near the
start and (b) takes the address of a mid/high field for a call, writing:

```c
SndScript* p = SndScript_Slots;
for (i = 0; i < 8; i++, p++) {
    if (p->field_0 == arg0) {
        LinInterp_Setup(&p->field_50, ...);
    }
}
```

lets GCC 2.8.1 keep *two* induction pointers — one at the struct base (for
`field_0`) and one at `&field_50` (for the call / nearby bytes via negative
offsets). That spills an extra callee-saved reg (`$s6`), shifts the stack frame,
and mismatches even when the logic is identical.

Re-deriving the element from the index each time keeps a single base IV:

```c
for (i = 0; i < 8; i++) {
    p = &SndScript_Slots[i];
    if ((arg0 == p->field_0) || ((p->field_0 & 0xF0000000) == arg0)) {
        if (p->field_16 == 8) {
            p->field_16 = 0x10;
            LinInterp_Setup(&p->field_50, 0, (u8)D_80082748, 8);
        }
    }
}
```

The compiler still emits `addiu $s0, $s0, 0x60` for the walk, but no longer
CSEs `&p->field_50` into a second live pointer. Operand order `arg0 == p->field_0`
also matters for `beq $s4, $v1` vs the swapped form.

`SndVoice_FadeMatching` is the pure example. Closely related: `Midi_FadeVolume` avoids the
trap because its status byte sits at offset 0 (free relative to the base) and
the interpolator is only `+0x14`.

## Post-decrement for `move` + `bgtz` on the pre-decrement value

A countdown that the target implements as:

```
lw    v0, field
move  v1, v0
addiu v0, v0, -1
bgtz  v1, skip
sw    v0, field
```

compares the *original* value (`bgtz v1`) while still storing `value - 1`.
That is not the same codegen as either:

```c
x -= 1;
if (x < 0) { ... }   /* addiu; bgez v0  — compare new value */
```

```c
x -= 1;
if (x <= 0) { ... }  /* addiu; bgtz v0  — compare new value */
```

Use a post-decrement in the condition:

```c
if (arg1->field_4-- <= 0) {
    /* timeout / transition */
}
```

GCC 2.8.1 keeps the pre-decrement copy in `$v1`, decrements `$v0`, and emits
`bgtz $v1`. Semantically this enters when the original value was `<= 0` (i.e.
after the store, when the new value is `< 0`), which is one-off from a plain
`-= 1; if (x <= 0)` check that fires when the counter hits zero.

`Mc_StateCountdownPrompt4` is the pure example. Nearby `Mc_StateUiCountdownF` uses the early
`x -= 1; if (x <= 0)` form and correctly gets `bgtz` on the decremented value
because the check is at the top of the function with lower register pressure —
same logical intent, different placement, different instruction shape.

## `flag = field & 1; if (flag)` vs `if (field & 1)` for `andi rd,rd,1`

When the target has:

```
lbu  v1, field(s0)
...
andi v1, v1, 0x1
beqz v1, skip
```

writing `if (obj->field & 1)` (or `flag = obj->field; if (flag & 1)`) often
emits `andi v0, v1, 0x1` / `beqz v0` — correct form, wrong dest. Force the
mask into the same register as the load by assigning the masked result first:

```c
s32 flag;

flag = obj->field_984 & 1;
if (flag) {
    /* ... */
}
```

That yields `andi v1, v1, 0x1; beqz v1, ...`. `Display_SpawnFromMode` needs this on
both copies of the slot-3 object setup. A `register u32 flag asm("v1")` pin
also works but is unnecessary once the assign-then-test form is used.

## Irregular switch: branch-to-case layout for sparse first case

When the target checks a sparse case first with `beq` *into* the case body
(body is not the fallthrough of the compare), a C `switch` reorders cases by
density and an `if (mode == K) { body }` puts the body as fallthrough with
`bne`. Match the branch-to-case layout with gotos:

```c
if (mode == 4) {
    goto block_case4;
}
if (mode >= 5U) {
    goto block_default;
}
if (mode == 1) {
    goto block_case13;
}
if (mode == 3) {
    goto block_case13;
}
goto block_default;

block_case4:
    /* case 4 work */
    /* fallthrough */
block_case13:
    /* cases 1, 3, and fallthrough from 4 */
    ...
    goto end;
block_default:
    ...
end:
```

`Display_SpawnFromMode` is the pure example (mode 4 first, then range `< 5`, then 1/3,
with 4 falling into the 1/3 block).

## `getClut` between SPRT `u0`/`v0` stores needs a `u8` temp for `v`

When filling a `SPRT` with texture coords and a CLUT, the target often does:

```
lbu  v0, u(src)      # load u
sll  v1, y, 6        # start getClut
sb   v0, u0(p)       # store u
srl  v0, x, 4
andi v0, v0, 0x3f
lbu  a0, v(src)      # load v into $a0
or   v1, v1, v0
sh   v1, clut(p)     # store clut
sb   a0, v0(p)       # store v
```

Writing `p->u0 = ...; p->clut = getClut(...); p->v0 = ...;` reorders: the
compiler computes the whole CLUT first, stores it, then does `u0`/`v0`, and
may also fill the preceding `y0` load-delay with `sll` (target wants a `nop`
there). Score stalls around ~92%.

Fix: load `v` into a `u8` temporary *before* the `getClut` assignment, then
store it after:

```c
u8 v;

p->u0   = arg0->field_4;
v       = arg0->field_6;
p->clut = getClut(arg1, arg2); /* arg1 must be unsigned for srl, not sra */
p->v0   = v;
```

Also type the CLUT X argument as `u32` (or cast) so `((x) >> 4)` emits `srl`
rather than `sra`. `Prim_DrawSprt` is the pure example (SPRT twin of TILE
helper `Prim_DrawTile`).

## Force `move v0, tN; sw v0` when CSE wants `sw tN`

When a live loop counter (or other long-lived constant) sits in `$tN` and is
also stored to a struct field, GCC 2.8.1 often emits `sw tN, off(reg)` while
the target has:

```
move  v0, tN
sw    v0, off(reg)
```

Assigning through a register pinned to `$v0` forces the extra move:

```c
register s32 val asm("v0");
/* ... */
val            = i;   /* move v0, t2 */
arg1->field_2C = val; /* sw v0, 0x2c(a1) */
```

Plain `arg1->field_2C = i` (or `= 1` CSEd with `i`) collapses to `sw tN`.
`Mc_WriteSlotChecksumsEx` is the pure example.

Same function also shows that a void-arg checksum sibling using
`register u32 j asm("a0")` must switch to `asm("a1")` when `$a0` holds a live
`Task*` argument through the end of the function.

## Assign walk pointer after null early-return for `lw v0` + delay-slot `move v1,v0`

When attaching into a circular sibling list, the target often loads the head once
into `$v0`, tests it, and copies to the walk register in the branch delay slot:

```
lw    v0, 0xc(a0)
nop
bnez  v0, use
 move  v1, v0
jr    ra
 sw    a1, 0xc(a0)   /* null path */
/* use path walks from v1; a0 is then reused as the first-child sentinel */
```

Writing `cur = temp` *before* the null test CSE's both into one register and you
get a plain `lw v1` with a `nop` delay:

```c
/* BAD — CSE merges temp and cur into v1 */
temp = arg0->field_c;
cur  = temp;
if (temp == NULL) { arg0->field_c = arg1; return; }
```

Assign the walk pointer only on the non-null path, and rebind the dead parent
argument as the first-child sentinel (so the loop compare uses `$a0`):

```c
temp = arg0->field_c;
if (temp == NULL) {
    arg0->field_c = arg1;
    return;
}
cur  = temp;  /* delay-slot: move v1, v0 */
arg0 = temp;  /* later: move a0, v1 before the walk */
if (cur->field_10 != cur) {
    do {
        cur = cur->field_10;
    } while (cur->field_10 != arg0);
}
arg1->field_10 = arg0;
cur->field_10  = arg1;
```

`Task_Reparent` is the pure example (reparent: detach then insert into parent's
circular child list — same unlink shape as `Task_DetachFromParent`).

## Force a second `(s8)` cast into the same reg (`sll v1; sra v1,v1`)

When the target re-sign-extends `arg1` in an else/default arm even though a
prior `(s8)arg1` is still live (e.g. switch dispatch left the value in `$v1`),
plain `temp = (s8)arg1` is CSE'd away and the next use of the value is just
`andi` / `slt` against the old reg — often stuffing the wrong instruction into
the preceding `bne` delay slot.

Symptom: case body is otherwise identical, but the `bne …, else` delay is
`andi` (or similar) instead of `sll v1,a1,0x18`, and the else starts without
`sra v1,v1,0x18`. Score stuck ~96–99% on an otherwise matching function.

Two ingredients:

1. **Dispatch with `switch ((s8)arg1)` / `switch (temp)`** so the default arm
   is a real basic block that can hold a fresh cast (if/else chains often share
   the first cast and never re-emit it).

2. **Write the second cast as in-place shifts into the same temp** so regalloc
   produces `sll v1,a1,24; sra v1,v1,24` rather than `sll v0; sra v1,v0`:

```c
temp = (s8)arg1;
switch (temp) {
case 0x14:
    arg1 = 0;
    break;
case 0x1D:
    arg1 = 1;
    break;
default:
    temp = arg1 << 24;
    temp = temp >> 24; /* must be two stmts — `(arg1<<24)>>24` regallocs via $v0 */
    arg1 = arg1 - arg2;
    temp = temp < ((arg2 & 0xFF) + 1); /* slt v1,v1,v0 then beqz v1 */
    if (temp != 0) {
        arg1 = 0;
    }
    break;
}
```

`temp = temp < …; if (temp != 0)` is what turns the compare into `slt v1,v1,v0`
(reuse the cast reg) instead of `slt v0,v1,v0`.

`TaskIdMap_RemapIndex` case 5 is the pure example.

## Force multiply-before-base for `base + index * size`

When the target computes a scaled index into `$sN` *before* materialising the
array base (`addiu a0, s1, off` then `addu a0, sN, a0`), writing
`&ptr->arr[i]` alone often yields the opposite order (`addiu a0, sN, off` /
`addu a0, a0, s1`) or schedules the base addiu too early (before the multiply).

Fix: assign the scaled index to a local *before* materialising the base pointer,
even if the local is not read afterward. GCC still emits the multiply early and
CSEs it into the later array access:

```c
drawBase = ds->field_48;
PutDrawEnv(&drawBase[buf]);
stride = buf * 0x14;          /* emit s0*0x14 into $s2 first */
dispBase = ds->field_20;      /* then addiu a0, s1, 0x20 */
PutDispEnv(&dispBase[buf]);   /* then addu a0, s2, a0 */
/* later DrawOTag(Gpu_OtBuffers[buf].field_10) reuses $s2 */
```

`Display_VSyncCallback` needs this for `PutDispEnv(&Display_State.field_20[buf])`
(DISPENV and GpuOtBuf are both 0x14). Without the dead `stride` store the
`addiu a0,s1,0x20` lands either too early (right after `PutDrawEnv`) or as
`addiu a0,s2,0x20` / `addu a0,a0,s1`.

## `s8` field loads as `lb`; `volatile u8` forces re-load across arms

`Display_State.field_1e` is compared with `bnez` after an `lb`. Declaring it
`u8` emits `lbu` and can also let a nearby volatile store fill the branch delay
slot. Use `s8` when the target has plain `lb`.

`Display_State.field_108` is written by main-line code and read by the VSync
callback `Display_VSyncCallback`. Marking it `volatile u8` forces a second load for
`if (f == 0) … else if (f == 1)` (target reloads into `$v1` rather than CSE'ing
the first `lbu`). Same idea as `D_8006EC30` / `D_80070E38`.

`D_80070F64` (countdown next to `Display_State`) is also VSync-shared: without
`volatile`, `D_80070F64 -= 1` fills the following `bnez` delay slot; the target
has `sw` then `nop`.

## Early source assignment of a stack arg schedules late store

When the target loads a stack parameter early (`lw v0, 0x40(sp)` right after an
unrelated store frees `$v0`) but stores it later after several independent field
writes, writing the assignment in late source order keeps the load late too
(with a load-delay `nop`):

```
/* Late load — mismatches */
p->field_0 = arg3;
p->field_4 = 0;
p->field_10 = arg1;
p->field_13 = arg2;
p->field_17 = 0;
p->field_44 = arg4;   /* lw then nop then sw */
```

Assign the stack-arg field *first* among that block. GCC 2.8.1 still emits the
`sw` last (after the independent stores fill the load delay) but hoists the
`lw` to right after the preceding free of `$v0`:

```c
p->field_16 = 1;
p->field_40 = NULL;
p->field_44 = arg4;   /* load early; store after the next few sw/sb */
p->field_0 = arg3;
p->field_4 = 0;
p->field_10 = arg1;
p->field_13 = arg2;
p->field_17 = 0;
```

`SndScript_Play` is the pure example. Pair with a second live copy of a later
pointer arg (`desc = arg5; … p->field_48 = arg5; flags = desc->field_E`) when
the target holds the same pointer in two callee-saved regs for interleaved
`lhu` / `sw`.

## Adjacent BSS via typed pointer subtraction

When two BSS symbols are laid out back-to-back (e.g. `CdStream_State` size `0x58`,
then `CdStream_Channels`) and the target forms the earlier address as
`addiu a1, a2, -0x58` off the later base (`lbu v0, -0x58(a2)`), do not name both
globals independently — that reloads `%hi/%lo(CdStream_State)`. Anchor on the later
symbol and step back one typed element:

```c
CdStreamChannels* p = &CdStream_Channels;
volatile CdStreamState* q = (volatile CdStreamState*)p - 1; /* sizeof == gap */
```

`sizeof(*q)` must equal the BSS gap. Same pattern as `parent = (CdAudioLocEx*)interp - 1`.
`CdStream_SetPitch` needs this (with `volatile` on `q` so the else path reloads
`unknown_0[1]` instead of CSEing the bit test).

## Else-only `register … asm("v0")` for dual channel pointers

When the target does:

```
beqz  v0, else
 addiu v0, a2, 0x40   /* delay: else path channel ptr */
addiu a1, a2, 0x40    /* then path recomputes into a1 */
…
else:
sh    a0, 0xA(v0)
```

a single shared `ch1 = &p->ch[1]` before the `if` coalesces both paths into
`$a1` and drops the delay-slot `$v0` copy. Keep two locals: one for the then
path, and pin the else-only pointer to `$v0`, assigning it only on the fall-
through path:

```c
if (flag) {
    ch1b = &p->ch[1];
    /* use ch1b → $a1 */
    return;
}
ch1 = &p->ch[1];   /* register CdStreamChannel* ch1 asm("v0"); */
ch1->field_A = arg0;
```

`CdStream_SetPitch` is the pure example (`CdStream_Channels.ch[0]` / `ch[1]`, stride `0x40`).

## Dual `if (size != 0)` fill loops need a reloaded `cond`

Two consecutive zero-test loops over the same size:

```c
if (size != 0) { /* zero-fill */ }
if (size != 0) { /* 0xFF-fill */ }
```

are often merged by GCC 2.8.1 so the first `beqz` jumps past *both* loops. The
target wants two separate branches (first `beqz` → second `beqz` → continue).
Copy the size into a fresh local for each test:

```c
cond = size;
if (cond != 0) { /* zero-fill */ }
cond = size;
if (cond != 0) { /* 0xFF-fill */ }
```

`Mc_InitBufferSlots` is the pure example (`Mc_BufferSlots[1..8]` dual fill + checksum).

## `asm("")` pins independent zero-init after a load

When the target needs:

```
move  a2, zero     /* sum = 0 */
lw    t0, 0(t1)    /* block = slot->field_0 */
move  a1, zero     /* i = 0 */
```

writing `sum = 0; block = slot->field_0; i = 0;` freely reorders the two
`move …, zero` together before the `lw`. An empty `asm("");` after the load
(same barrier used for hex-digit store order) keeps the load between them:

```c
sum   = 0;
block = slot->field_0;
asm("");
i     = 0;
```

## Checksum add: `volatile u8*` + `sum = sum + tmp` together

For `lbu` / `sll 24` / `sra 24` **and** `addu sum, sum, v0` (not
`addu sum, v0, sum`):

```c
register volatile u8* cptr asm("v1");
s32 tmp;
…
tmp = (s8)*cptr;
sum = sum + tmp;   /* not sum += (s8)*cptr */
```

`sum += (s8)*cptr` alone gives the shift pair but the wrong `addu` operand
order; a non-volatile `tmp = (s8)*ptr; sum = sum + tmp` gives the right `addu`
but collapses to `lb`. Both together match. Used in `Mc_InitBufferSlots` over
`McChecksumBlock` payloads (same loop shape as `Mc_WriteBlockChecksum`).

## Volatile byte cast forces reload of a just-tested field

When the target tests a `u8` field then later stores that same field into
another object, GCC 2.8.1 will CSE the load into a free register (`$a3`) and
schedule the entry-pointer `addu` early. The target instead reuses `$v0` for
the first load, spills other bytes to the stack, **reloads** the field, then
`addu`s:

```
lbu  v0, field(a2)
beqz v0, fail
…
sb   …, 0(sp)          # spill other bytes
lbu  v0, field(a2)     # reload — not CSE'd
addu v1, v1, a2
sb   v0, 4(v1)
```

A plain second read CSEs (~96%). Route **one** of the two accesses through a
volatile byte cast so the value cannot stay live:

```c
if (*(volatile u8*)&p->field_50.cmd == 0) {
    return -1;
}
…
entry->cmd = p->field_50.cmd;   /* second load now materializes */
```

(The cast can sit on either access.) Same shape already used in `cdcmd.c` for
`entry->param0`. `CdCmd_CommitReplace` (commit `field_50` into the ring) is the pure
example; pair with `(s16)writeIdx` when the return needs `sll`/`sra 16`
sign-extend rather than Enqueue's `andi …, 0xffff` zero-extend.

## Empty `asm volatile("")` blocks delay-slot fill of independent ops after abs

After an abs-style `bgez`/`negu` sequence, GCC 2.8.1 with `-fdelayed-branch`
happily pulls the next independent instruction into the branch delay slot:

```
bgez  v0, join
 li    v1, 0x7F     /* filled from after the join */
negu  v0, v0
join:
subu  v1, v1, v0
```

The original often leaves a `nop` instead and keeps the `li` after the join.
The same thing happens for a second abs when a load of one multiplicand sits
just before the branch and the `mult` is available after the join — the
compiler fills the delay with `mult` (and emits a second `mult` after `negu`
for the negative path) while the target wants `nop` + a single shared `mult`.

An empty statement-asm is a scheduling barrier that stops both fills:

```c
register s32 temp_v0 asm("v0");
s32 temp_v1;

temp_v0 = arg1;
if (temp_v0 < 0) {
    temp_v0 = -temp_v0;
}
asm volatile("");
temp_v1 = 0x7F - temp_v0;
temp_v0 = ptr->field_2;
if (temp_v1 < 0) {
    temp_v1 = -temp_v1;
}
asm volatile("");
temp_v0 *= temp_v1;
temp_v1 = temp_v0 / 127;
```

Pinning the abs/product temporary to `$v0` (as above) is usually also required
so the following signed-division magic and clamp keep `$v0`/`$v1` the way the
target does; without the pin, scale and product drift into `$a0` and the
`mfhi`/`slti` register choices diverge. `SndVoice_ScaleVolume` is the pure example.

## `u8` ring index: store then compare (not check-then-store)

For a wrap-around counter written as:

```c
idx = idx + 1;
p->field_2 = idx;
if (idx >= 8) {
    p->field_2 = 0;
}
```

`idx` must be `u8` (same width as the field). That emits:

```
addiu  v0, v1, 1
sb     v0, field(t0)
andi   v0, v0, 0xff
sltiu  v0, v0, 8
bnez   v0, no_wrap
 nop
sb     zero, field(t0)
```

An `s32` index with `(u8)idx >= 8` rewrites to check-first / store-in-delay-slot
and mismatches. `Pad_PostEvent` is the pure example (pad-event ring at
`PadState.field_2`).

## Force `prev = curr` before the next-pointer load in list walks

When walking a singly-/doubly-linked list:

```c
do {
    curr = (Node*)prev->next;
    if (/* match */) { /* ... */ return; }
    prev = curr;
} while (curr->next != 0);   /* BAD for scheduling */
```

GCC puts `move prev, curr` in the `bnez` delay slot. The target often wants:

```
move  a1, a0        /* prev = curr */
lw    v0, next(a0)
nop
bnez  v0, loop
 lui   ...          /* fail-path setup in delay */
```

Write the condition through `prev` so the assignment is a true data dependency
of the load:

```c
prev = curr;
} while (prev->next != 0);   /* GOOD */
```

`AudioTick_Remove` is the pure example.

## Doubly-linked unlink: re-read `arg->next` after storing it (aliasing)

Target:

```
lw  v0, next(s0)
sw  v0, next(a1)     /* prev->next = arg->next */
lw  a0, next(s0)     /* reload — not CSE'd */
beqz a0, skip
 nop
sw  a1, prev(a0)     /* arg->next->prev = prev */
lw  v0, next(a1)     /* return prev->next */
```

Do **not** stash `next` in a temporary before the store:

```c
/* BAD — CSE folds the two loads; delay-slot-fills the store */
next = arg->next;
prev->next = next;
if (next != NULL) next->prev = prev;
```

Write the store first, then re-read. Same-type pointers may alias, so GCC 2.8.1
reloads:

```c
prev->field_14 = arg0->field_14;
if (arg0->field_14 != 0) {
    ((AudioTickNode*)arg0->field_14)->field_10 = (s32)prev;
}
return (AudioTickNode*)prev->field_14;
```

Also assign `head = &sentinel` *before* any callback that may clobber
caller-saved regs — that keeps `&sentinel` in a callee-saved register across
the call (matches early `addiu s1, ..., %lo(head)`). `AudioTick_Remove`.

## State-machine dispatch: `goto case0` after the `>= N` arm

When the target dispatches `state == 1` first (`beq …, case1`), then
`state < 2` with:

```
beqz  v0, ge2        /* state >= 2 */
li    v0, 2
beqz  v1, case0      /* state == 0 → jump over ge2 */
move  v0, zero
j     ret_zero
nop
ge2:
beq   v1, v0, ret_one
…
case0:
/* large body */
```

an inlined `if (state == 0) { /* large */ }` puts the body *before* `ge2` and
often fills the `beqz` delay slot with a `lui` from that body. Force the layout
with an explicit forward goto and shared exit labels:

```c
if (state != 1) {
    if (state < 2) {
        if (state == 0) {
            goto case0;
        }
        goto ret_zero;
    }
    if (state == 2) {
        goto ret_one;
    }
    goto ret_zero;
case0:
    /* large body — lives after ge2 in the object */
    …
    goto ret_one;
}
/* case 1 */
…
ret_one:
    return 1;
ret_zero:
    return 0;
```

`goto ret_zero` / `goto ret_one` also keeps a dedicated `move v0, zero` /
`li v0, 1` before the epilogue instead of reusing a delay-slot instruction as a
branch target. `Stream_RestoreAfterLoad`.

When setting several `u8` stack slots from one struct (e.g. `CdCmd_Enqueue`
params), load each field into a local before the corresponding `sb` so the
compiler emits `lbu` → `sb` → `lbu` → … rather than reordering independent
zero-stores ahead of dependent field stores.

## `(s16)` cast on a `u16` field forces `lh` without changing the struct

When the target loads a halfword field with `lh` but the struct types it as
`u16` (because other matched functions need `lhu` on the same field), a bare
read emits `lhu`. Cast at the use site:

```c
/* target: lh v0, 0x20(s0) */
func(x - (s16)arg0->field_20, y - (s16)arg0->field_22);
```

Changing the struct field to `s16` would break other matches that expect `lhu`
(e.g. `Ui_DrawTextAtLayout` on `UiPanel::field_20`). `Ui_DrawTitle`.

## `(u16)` cast on an `s16` field forces `lhu` without changing the struct

The inverse of the previous entry: when the target uses `lhu` for a zero-check
(or other unsigned halfword use) but the field is `s16` (other matches need
`lh` / signed stores), cast at the use site:

```c
/* target: lhu v0, 0x20E(a1); bnez v0, ... */
if ((u16)p->field_20E != 0) {
    p->field_4c = 1;
    return 1;
}
return 0;
```

A bare `if (p->field_20E != 0)` emits `lh` and often inverts branch polarity
(`beqz` with the non-zero body as fall-through). Prefer the positive `!= 0`
test first so GCC emits `bnez` with the zero-return as the fall-through
epilogue. `CdCmd_ActivatePhase1` (`CdCmd_Queue.field_20E`).

## Short-lived stack `RECT*` stays in `$a1` for switch stores + callee arg

When a function takes a string in `$a1` (saved to `$s2`), then builds a stack
`RECT` used both as `Ui_ScaleRect(..., r, ...)` and for field stores in the
default arm, a local `RECT *r = &sp18` that dies before the post-switch draw
call is allocated to `$a1`. GCC then:

- fills the first switch `beq` delay slot with `addiu a1, sp, 0x18`;
- stores via `sh v0, off(a1)` instead of `sh v0, off(sp)`;
- reuses `$a1` as the callee's RECT arg without a second materialization.

If the same pointer is also read after a later `jal` (draw uses `r->x`), it
spills to a callee-saved and the frame grows. Keep draw coords as
`sp18.x` / `sp18.y` so `r` is switch-only. `Ui_DrawTitle`.

## Split `x = x + 1` to stop `(x+1)-base` reassociating to `x-(base-1)`

`(sp18.x + 1) - field_20` is algebraically equal to `sp18.x - (field_20 - 1)`,
and GCC often emits the latter (`addiu a1, field, -1` then `subu`). The target
wants the former (`lh` both operands, `addiu ..., 1`, then `subu`). Force it
with temps and a separate increment:

```c
x = sp18.x;
y = sp18.y;
x = x + 1;
y = y + 1;
func(..., x - base_x, y - base_y, ...);
```

Inlining the `+ 1` into the call expression is enough to invite the rewrite.
`Ui_DrawTitle`.

## Materialize long-lived pointers before early-return guards

When a function early-outs on a flag but still needs a global address for the
whole body (e.g. `&Fs_CdSector` used after several `jal`s), assign that pointer
*before* the early-return checks. The target prologue then does:

```
sw    s4, 0x20(sp)
lui   s4, %hi(Fs_CdSector)
... setup other s-regs ...
lbu   v0, flag(s3)
bnez  v0, early_out
 addiu s1, s4, %lo(Fs_CdSector)   /* delay */
```

Assigning the pointer only after the guards leaves `%hi` later, parks the raw
`arg0` in `$s2`, and puts other bases in `$s0` — a register shuffle that looks
like a large diff even when the body is otherwise identical.

`CdAudio_FeedSector` needs `sector = &Fs_CdSector` first, then the
`CdAudio_Ctl.field_B` / `CdAudio_Tbl.field_1` guards, then `arg = arg0 & 0xFF`.

## `volatile` struct pointer preserves independent field load order

When the target loads two independent fields from the same struct in a fixed
order (e.g. `lw a2, 0x18(s1)` then `lw v1, 4(s1)`) but GCC reorders them no
matter how the C assignments are written, cast the base to `volatile` for those
reads only:

```c
base = ((volatile SndBank*)bank)->field_18;
ptr  = (s32*)((volatile SndBank*)bank)->field_4;
```

Plain `base = bank->field_18; ptr = bank->field_4;` is free to swap the loads
by schedule/urgency (the pointer used sooner after a following branch often
loads first). The `volatile` cast forces source order without changing the rest
of the function. `SndBank_FinalizeLoad` is the pure example (relocate loop setup).

## Shared `var_v0` + epilogue flips global pointer store register order

For multi-way dispatch that ends every arm with `D5B498_8006ACB0 = some_table;
*arg0 = K`, collecting `K` into a shared `var_v0` and one trailing `*arg0 =
var_v0` looks like the target's shared `sb v0,0(s0)` epilogue — but it often
compiles the stores as `lui v1,dest; lui v0,src; sw v0,(v1)` with the wrong
symbol in the branch delay slot (`lui v0,src` instead of `lui v0,dest`).

Writing each arm as an early return matches both the shared epilogue *and*
the dest-first store pattern:

```c
/* BAD ~88%: shared phi for *arg0 flips store regs / delay-slot lui */
case_arm:
    D5B498_8006ACB0 = D_80062E50;
    var_v0 = 0xC;
    goto store;
...
store:
    *arg0 = var_v0;

/* GOOD 100%: early return; GCC still emits j + li v0,K into the shared sb */
case_arm:
    D5B498_8006ACB0 = D_80062E50;
    *arg0 = 0xC;
    return;
```

Also: sparse outer + dense inner irregular trees (`beq` / `slti` / `bnez`) need
explicit if/goto decision trees (not `switch`) so case *body* order is
`[fallthrough default][case4][case5]` after the dispatch, and the outer test
order is `== 0x1B`, then `< 0x1C`, then `!= 0x11`. Keep the switch key in an
`s32` (not `u8`) so the load is plain `lbu` without `andi`/`sltiu`.

`Fs_SelectLoadHandlers2` is the pure example (FS load-table select by
`D5B498_8006ACB8.field_2` × `GameFlag_GetNibble(0x7A)`).

## Independent `entry++` + mid-loop `i++`: prefer `goto` over re-index / `do`

When a loop both (a) bumps a counter mid-body (often into a `jal` delay) and
(b) advances a struct pointer only at the bottom (branch delay of `i < N`),
with fields of `entry` still read *after* `i++`:

```c
/* BAD ~92%: re-index snapshots entry into $s0 and early-increments $s2 */
do {
    entry = &table[i];
    /* ... uses of entry ... */
    i++;
    /* more uses of entry */
} while (i < 2);

/* BAD ~88%: do/while entry++ often strength-reduces a second IV at +8 */
do {
    /* ... */
    entry++;
} while (i < 2);

/* GOOD 100%: independent IVs; entry++ lands in the bnez delay */
entry = table;
i = 0;
loop:
    /* ... uses of entry; i++ mid-body fills first jal delay ... */
    entry++;
    if (i < 2) {
        goto loop;
    }
```

Pair with `idx + (s32)base` / `(slot << 5) + (s32)banks` for offset-first
`addu`, and `*(volatile s32*)&flag = …` before a call so the last arg (`li a2`)
fills the `jal` delay instead of the store (see “volatile blocks delay-slot
filling”). `Snd_InitBanks` is the pure example.

## Late `li a0, K` before malloc: wrap setup in `do {} while (0)`

When the target prepares a constant call argument only after unrelated
address setup (e.g. `sw field_10` then `lui %hi(slot); li a0, 0x582; lb …`),
writing `F3D458_Malloc(0x582)` in straight-line code often hoists
`li a0, 0x582` immediately after the previous `jal` returns — free `$a0` and
an independent constant. That early `li` also steals the `lui` of the next
symbol into `$v1` instead of `$v0`.

Wrap the pointer math + flag store + malloc in a `do { … } while (0);` so the
constant stays with the call:

```c
state->field_10 = buf;
do {
    bank            = &Snd_Banks[D_800680BB];
    state->field_40 = bank;
    bank->field_8   = 0xF0FF;
    state->field_40->field_1C = F3D458_Malloc(0x582);
} while (0);
state->field_40->field_0 = state->field_40->field_1C;
/* … */
```

Also watch for a live `li v0, -1` through the epilogue with no store: the
function returns `-1` even if call sites ignore it (`s32` not `void`).
`Midi_InitSystem` is the pure example.

## Identity MATRIX: global `= ONE` first, then local `one = ONE`

When the target opens with:

```
lui  v0, %hi(D_xxx)
li   a1, 0x1000
sw   a1, %lo(D_xxx)(v0)
addiu v0, v0, %lo(D_xxx)
```

assigning a local first (`one = ONE; *(s32*)&D_xxx = one;`) schedules `li`
before `lui`. Write the bare constant into the global, then load the local
for the remaining word/halfword stores (CSE keeps `0x1000` in `$a1`):

```c
*(s32*)&D_80070E94 = ONE;
one = ONE;
m = &D_80070E94;
*(s32*)&m->m[0][2] = 0;
*(s32*)&m->m[1][1] = one;
*(s32*)&m->m[2][0] = 0;
m->m[2][2] = one;
```

The word stores through `*(s32*)&m->m[r][c]` match the target's packed
halfword pairs; individual halfword assigns usually do not.

`Gfx_InitCoordinateTrees` also derives the parent `GsCOORDINATE2*` as
`(GsCOORDINATE2*)((u8*)m - OFFSET_OF(GsCOORDINATE2, coord))` so `sub` /
`coord.t[]` / `flg` use `off($a2)` while the matrix body stays on `$v0`
(including `sw zero, -4($v0)` for `flg`). Naming a separate base symbol
(`&D_80070E90`) reloads with a fresh `lui` and breaks the match.

## Store stack-derived field before unrelated prim color word

When a TILE (or similar) sets both `p->h = argN - 1` (5th+ stack arg) and
`*(u32*)&p->r0 = color` (6th stack arg), writing color first often schedules
`sw color` *before* `lw argN, 0x10(sp)`. The target typically wants:

```
lw    v0, 0x10(sp)     /* argN */
lui   a1, 0xff00       /* addPrim mask fills load delay */
sw    t3, 4(a0)        /* color */
addiu v0, v0, -1
sh    v0, 0xe(a0)      /* h */
```

Write `h` *before* color in the C source — GCC still emits color before the
`sh` of `h`, but the stack load of argN moves earlier into the load-delay
slot of the mask:

```c
p->w = arg3 - 1;
p->h = arg4 - 1;           /* forces lw of stack arg early */
*(u32*)&p->r0 = color;     /* scheduled after that lw */
setlen(p, 3);
```

`Ui_AllocTile` also needs `register u32 color asm("t3")` so the 6th arg
lands in `$t3` (without the pin, `$t2`/`$t3` for arg1/arg5 swap). Pair with
Psy-Q `addPrim` / `setlen` / `setcode` (see above) and
`addPrim(D_800710A0 + (s16)obj->field_14 + 1, p)` when the target uses `lh`
on a `u16` OT index and OT slot `field_14 + 1`.

## Empty `asm volatile` a0 clobber for branch-delay restore

When a function sets `a0` in the delay of an earlier branch (e.g.
`move a0, s0` after `bnez rcnt_ok`), then has:

```
bnez  s1, restore     /* if retry != 0 */
 move  a0, zero       /* delay: for ContinueDraw(NULL, ...) */
jal   ContinueDraw
...
restore:
 move  a0, s0         /* undo the delay clobber before shared tail */
lui   ...             /* shared finish */
```

GCC 2.8.1 often prefers to put the *finish* block's first `lui` in the
`bnez s1` delay instead, and shares that `lui` with the taken path (so no
`move a0, s0` restore is needed). That is functionally fine but mismatches.

Fix: on the `retry != 0` path only, empty-clobber `$4`/`a0` so the taken
path must re-materialize `ot` into `a0`. Fallthrough then keeps
`move a0, zero` in the delay, and the taken path emits the restore:

```c
if (GetRCnt(timer) >= TIMEOUT) {
    if (retry == 0) {
        ContinueDraw(z, ot);  /* z = NULL forces move a0, zero not move a0, s1 */
        return 0x7F;
    }
    asm volatile("" : : : "$4");
}
/* shared finish uses ot in a0 */
```

Also: assign `z = NULL` *before* the idle wait so CSE does not replace
`move a0, zero` with `move a0, s1` when `retry == 0` is proven.

`Fs_LoadImageChunk` is the pure example. Pair with `register ... asm("s0")` /
`asm("s1")` when ot/retry would otherwise swap.

## Split range-check condition into a temp for `move a1` delay fill

When a later region needs `a1 = arg0` for field access (and the range
compare must still load `y` from `s3`), compute the condition into a
temporary *before* copying the pointer:

```c
inRange = (u32)(arg0->y - 0xF5) < 0xBU;
img = arg0;           /* lands in beqz delay as move a1, s3 */
if (inRange) {
    yAdj = flag;
} else {
    yAdj = 0;
}
/* subsequent y/w/h loads use img/a1; j delay gets lui of next global */
```

Assigning `img = arg0` only after the if/else often puts `move a1` in the
*true-path* `j` delay instead, and duplicates it on the else path.

## Do not include `libgs.h` for `GsF_LIGHT`

`include/main/game.h` redeclares `GsClearOt` with `GameOt*` so callers avoid
libgs. Including `<psyq/libgs.h>` redeclares `GsClearOt` with `GsOT*` and fails
with `conflicting types for GsClearOt`.

For flat-light structs (vx/vy/vz + r/g/b at 0xC/0xD/0xE), define a local
layout-matching type (e.g. `FlatLight`) instead of including libgs.

## Scratch-head light direction: 0x18 block, SVECTOR at +0x10

`G_SCRATCH_HEAD` (`PSX_SCRATCH_ADDR(0x3FC)`) is a downward-growing arena pointer.
Helpers that call `Gfx_NormalizeLightDir` to normalize a light direction use:

```c
head = *scratch;
block = (ScratchLightBlock*)((u8*)head - 0x18); /* pad[0x10] + SVECTOR */
*scratch = block;
Gfx_NormalizeLightDir(light, (SVECTOR*)((u8*)head - 8)); /* == &block->dir */
/* read -block->dir.{vx,vy,vz} into MATRIX row id */
*scratch = (u8*)*scratch + 0x18;                 /* free */
```

Keeping a single `block` base (access dir as `block->dir` / `+0x10`) avoids an
extra callee-saved for a separate SVECTOR pointer. Computing the out-arg as
`head - 8` (not `block + 0x10`) matches the target's `addiu a1, a1, -8` from the
loaded head. Colors go in MATRIX **columns** (`m[0/1/2][id] = component << 4`);
directions go in MATRIX **rows** (`m[id][0/1/2] = -dir`).

## `static __inline__` forces scratch-head rematerialisation (not s-reg CSE)

`Gfx_SetFlatLight` takes `MATRIX* dirMtx/colorMtx` and keeps
`scratch = (void**)G_SCRATCH_HEAD` in a callee-saved reg (`lui`/`ori` + `$sN`).
`Gfx_SetDefaultFlatLight` is the same body writing to globals `&GsLIGHTWSMATRIX` /
`&D_80074080`, but the ROM rematerialises `0x1F8003FC` on every access
(`lui $r,0x1f80` / `lw|sw 0x3fc($r)`) and only uses five s-regs (frame `0x28`).

Writing the body with local matrix pointers plus direct
`*(void**)0x1F8003FC` still CSE's the address into `$s5` (frame `0x30`).
The match is a `static __inline__` helper that takes the matrix pointers,
called as:

```c
static __inline__ void setLightToMatrices(s32 id, FlatLight* light,
                                          MATRIX* dirMtx, MATRIX* colorMtx)
{
    /* same body as Gfx_SetFlatLight */
}

void Gfx_SetDefaultFlatLight(s32 id, FlatLight* light)
{
    setLightToMatrices(id, light, &GsLIGHTWSMATRIX, &D_80074080);
}
```

Inlining that form rematerialises the scratch address and matches. Do **not**
also route `Gfx_SetFlatLight` through the same inline — that changes its
scratch-pointer codegen and breaks the existing match. Keep `Gfx_SetFlatLight`'s
body out-of-line as written.

## Nested blocks force pointer reloads between store groups

When the target reloads a stack-resident pointer between *pairs* of stores
(e.g. `lw v0, 0x14(sp)` then two `sw`/`sh`, then another `lw`), a single
function-scope temp produces one load for the whole sequence, and writing
through the field every time reloads *before every store*.

Use a nested block per group so the local dies after the pair:

```c
{
    SpuVoiceAttr* attr = sp10.field_4;
    attr->loop_addr = spuAddr;
    attr->addr      = spuAddr;
}
{
    SpuVoiceAttr* attr = sp10.field_4;
    attr->volume.right = 0;
    attr->volume.left  = 0;
}
/* …one block per target load group… */
```

Keep large constants (e.g. `0x7008FU` for a `mask` field) as *literals* at the
store site, not loop-invariant locals — otherwise GCC pins them in a callee-
saved reg (`s3`) instead of interleaving `lui`/`ori` into `$a1` delay slots.

`Spu_InitVoices` is the pure example (voice-attr init after `Spu_GetVoiceRef`).

## Empty asm after pinned arg copies for prologue `li sN` order

When `register … asm("s0")` / `asm("s1")` pin the formals and an early
`flag = 1` lives in `$s3`, GCC often schedules:

```
sw   s3, …(sp)
li   s3, 1
sw   s1, …(sp)
move s1, a0
…
```

while the target wants the arg copies first, then `sw s3` / `li s3, 1`.

An empty constraint after the copies blocks that hoist:

```c
register DialogPrompt* s1 asm("s1");
register UiObject*  s0 asm("s0");
register s32        s2 asm("s2");

s1 = arg0;
s0 = arg1;
asm("" : "+r"(s0), "+r"(s1));
var_s3 = 1; /* now after move s1,a0 / move s0,a1 */
```

Same empty-asm family as boot.c / the `ret` CSE notes; the `+r` operands on
the pinned arg regs are what force the `move`s to complete first.

Also pair with `register s32 temp asm("s2")` when a long-lived work pointer
must occupy `$s2` (otherwise it steals `$s1` and flips the arg colors).

`Mc_StateSaveSlotUi` is the pure example (checksum gate + confirm/cancel pad path
over `McWork::field_294[slot]`).

## Force `(idx << k) + C` before base add for `addiu`/`addu` order

`base + ((idx << 7) + 0x294)` often reassociates to
`addiu v1, base, 0x294` then `addu a2, v1, shifted`. When the target does:

```
sll    v0, v0, 7
addiu  v0, v0, 0x294
addu   a2, s2, v0
```

compute the scaled offset first:

```c
off = (arg0->field_8 << 7) + 0x294;
save = (McSaveData*)(work + off);
```

## `if (x >= K)` for `slti`/`bnez` fall-through compute + `j` join

When one arm is a constant and the other is a multi-instruction expression,
`if (x < K) { v = K; } else { v = expr; }` often peepholes to `bnez` with
`li v0, K` in the delay slot and no `j`. The target that keeps the classic
shape:

```
slti  v0, x, K
bnez  v0, li_path     /* delay: first insn of expr */
…expr…
j     join
addiu …
li_path:
li    v0, K
join:
sh    v0, …
```

needs the compute arm as fall-through:

```c
if (temp >= 0xC) {
    temp = (((temp - 0xC) * arg2) >> 3) + 0xC;
} else {
    temp = 0xC;
}
arg1->h = temp;
```

Also cast `byte` (signed char) through `(u8)` before `>>` so the nibble test
is `srl`, not `sll`/`sra`. `Ui_ScaleRect`.

## Irregular status switch: if/goto + pinned s-regs + `while (++j < limit)`

Sparse status dispatch that targets `beq $s7,$v1` / `beq $s6,$v1` with
`slti` range pivots (not a jump table) will not match as a C `switch` — case
bodies get reordered and case constants reload into `$v0` instead of the
pinned s-regs. Match the binary-search tree with gotos, and pin the constants
that are both compared and stored:

```c
register s32 two asm("s6");
register s32 eight asm("s7");
register s32 ffff asm("s5");

two = 2;
eight = 8;
ffff = 0xFFFF;
/* status == eight / status == two in the tree; field = two / field = ffff later */
```

Without the `asm("sN")` pins, GCC often swaps `$s5`/`$s6` when the store-only
sentinel (`0xFFFF`) and the compare+store value (`2`) have different use
patterns.

For the per-entry walk that reloads `obj->field_3` every iteration and keeps
a base cursor + byte offset (target: `addiu s3,0x3c; lbu field_3; addiu s2,1;
slt; bnez` with `addiu s1,0x3c` in the branch delay):

```c
/* BAD: j++ before the limit load leaves a nop in the load delay */
off += 0x3C;
j += 1;
cursor += 0x3C;
} while (j < (s32)obj->field_3);

/* GOOD: ++j in the condition; cursor+= before it fills the branch delay */
off += 0x3C;
cursor += 0x3C;
} while (++j < (s32)obj->field_3);
```

`Midi_Tick` is the pure example (MidiSong status driver over Midi_Song).


## Reuse a temp through field copy and `&= ~const` masks

When the target interleaves a field store with a multi-step flag update:

```
lw  flags, 0(entry)
lw  temp,  field_C(arg)
ori flags, 1
sw  temp,  field_C(entry)   /* store between ori and first and */
li  temp, -5
and flags, temp
li  temp, -9
and ...
lw  temp,  field_10(arg)
ori flags, 2
sw  flags, 0(entry)
sw  temp,  field_10(entry)
```

writing `flags &= ~4` (or a single combined mask) lets CSE schedule the
`li -5` early and delay the field_C store until after all the ands. Force the
interleaving by routing the first mask through the same temp used for the
field_C value:

```c
temp = arg0->field_C;
flags = entry->field_0;
flags = flags | 1;
entry->field_C = temp;
temp = ~4;                 /* reuses temp; emits li + and, not a combined mask */
flags = flags & temp;
flags = flags & ~8;
flags = flags & ~0x1FE0;
temp = arg0->field_10;
flags = flags | 2;
entry->field_0 = flags;
entry->field_10 = temp;
```

`temp = ~4` (or `temp = -5`) is required for the first mask only — later
`& ~8` / `& ~0x1FE0` can be written directly once temp is free for field_10.

Also index the ring buffer via the global name, not a local pointer, when the
target builds `idx*stride` first then `addiu base, p, offsetof(entries)`:

```c
/* Right: multiply first, then base = p+8 */
entry = &CdReady_Queue.entries[(s8)p->field_3];

/* Wrong fold: (idx*20 + 8) + p */
entry = &p->entries[(s8)p->field_3];
```

`CdReady_Enqueue` is the pure example (CdReady_Queue.entries queue push).

## Dual `goto` return labels for `beqz` + `j`/`li` fallthrough layout

When the target ends with:

```
beqz  v0, fail        # null → li -1
 move a0, v0
/* success body */
j     epilogue
 move v0, s3          # return saved orig
fail:
li    v0, -1
epilogue:
```

and early exits also jump to the `j epilogue; move v0,s3` pair, a plain
`if (p != NULL) { work; } else { return -1; } return orig;` often inverts to
`bnez` (large success block becomes the branch target). Assigning
`orig = -1` instead of `return -1` restores `beqz` but then emits
`li s3,-1; move v0,s3` instead of `li v0,-1`.

Fix: two trailing labels, with success and the early-exit fallthrough sharing
the orig path, and the null path an explicit goto to the -1 path:

```c
temp = SndEvt_Alloc();
if (temp != NULL) {
    /* setup … */
    goto ret_orig;
}
goto ret_neg1;
ret_orig:
return orig;
ret_neg1:
return -1;
```

GCC 2.8.1 lays this out as fallthrough setup → `j`/`move v0,s3` → `li v0,-1`
→ epilogue, with `beqz` to the `li`. Early `return orig` from the outer
`if (arg0 == 0)` path merges into `ret_orig`.

`SndEvt_EnqueueType6` is the pure example.

## Switch case stores of `field_30` vs a shared `next` temp

When a switch assigns the same struct field (`arg0->field_30 = K`) on every
arm and then falls into shared post-switch code, write the store **inside each
case** rather than:

```c
s32 next;
switch (...) {
case 0: ...; next = 0xF; break;
...
}
arg0->field_30 = next;
```

Symptom with the `next` form: case bodies that free `$a0` (e.g. after
`lui/addiu a0, %hi/%lo(Mc_BufferSlots)`) allocate `next` to `$a0`. The target
wants the phi value in `$v0` (`li v0,K` / `sw v0, field_30`), and the early
`li a0,K` also steals the load-delay slot that the target fills with `nop`
between `lw v0,8(v0)` and `addu`.

Writing `arg0->field_30 = K` in each case still compiles to one shared
`sw v0, field_30` (GCC 2.8.1 phi-merges the constants), but the merged value
lands in `$v0` and case 0 keeps the natural:

```
lw  v1, field_1C
lw  v0, 8(v0)
nop
addu v1, v1, v0
li  v0, 0xF
j   common
 sw v1, field_1C
```

`Mc_StateFreeBuffer` is the pure example — ~98% with `next`, 100% with per-case stores.

## Capture TaskDesc tail field before assigning the callback

When building a stack `TaskDesc` whose first two halfwords come from a source
struct and whose `callback` is a function address, assign the trailing word
(`field_8`) into a local **before** writing `callback`:

```c
TaskDesc desc;
s32      field_8;

desc.flags   = src->field_10;
desc.field_2 = src->field_12;
field_8      = src->field_18;       /* load first */
desc.callback = SomeFunc;
desc.field_8  = field_8;
task = Task_SpawnFromTable(&desc, ...);
```

Without the temp, GCC 2.8.1 hoists `lui %hi(SomeFunc)` ahead of the second
halfword load/store, uses `$v1` for that halfword, and puts the `field_8` load
in `$v0` after the callback `sw`. The target wants both halfword copies to
finish (reusing `$v0`), then `lui`/`lw $v1,field_8`/`addiu`/`sw callback` with
`sw field_8` in the `jal` delay slot.

`volatile TaskDesc` restores halfword order but blocks the delay-slot store.
The local-temp form matches both.

`Ui_SpawnFromDesc` is the pure example.

## Reuse `obj = NULL` as the zero argument source

When the target seeds `$s0` with `move s0,zero` and reuses it for several
call args (`move a1,s0` / `move a3,s0`) before overwriting `$s0` with an
allocation result, hold the eventual return pointer at NULL and cast it:

```c
UiObject* obj;

obj = NULL;
task = Task_SpawnFromTable(&desc, (s32)obj, arg1, (s32)obj);
if (task != NULL) {
    obj = (UiObject*)Mem_Calloc(0x30, (s32)obj);
    ...
}
return obj;
```

Literal `0` often becomes `move aN,zero` instead of `move aN,s0`, which also
shifts later register assignment. Early `return` paths that need `v0 = 0`
then reuse `move v0,s0` for free.

`Ui_SpawnFromDesc` is the pure example.

## Ternary second arg schedules `arr[idx]` base-before-index

When `DecDCTin`/`similar` needs the target schedule

```
lui    v1,%hi(arr)
lui    v0,%hi(idx)
addiu  v1,v1,%lo(arr)
lhu    v0,%lo(idx)(v0)
...
addu   a0,v0,v1
lhu    v1,%lo(mode_global)
li     v0,2
beq    v1,v0,...
 move   a1,zero
move   a1,v1
lw     a0,0(a0)
jal    DecDCTin
```

writing a local `mode` first, then `DecDCTin(arr[idx], mode)`, materialises
the mode compare *before* the array address and often evaluates the index
before the base. Passing the conditional as the second call argument keeps
address setup first and emits the branchless-looking `mode = 0; if != 2
mode = val` pattern from a ternary:

```c
DecDCTin(D_8006AC50[D_8005EAEC], D_8006AC14 == 2 ? 0 : D_8006AC14);
```

Pair with `register u_long **base asm("v1")` when a later double-buffer base
(`D_8006AC48`) must also be `lui v1; addiu v1,v1,%lo` rather than
`lui v0; addiu v1,v0`.

`Mdec_KickStrip` is the pure example.

## Dual 7-bit volume product divides by 16129 (127×127)

SPU voice volume scaling multiplies a master level (`s8`, often 0..0x7F) by two
`u8` scales (bank/voice params) and divides by `127 * 127` (= `16129` =
`0x3F01`). GCC 2.8.1 emits the signed magic multiply `0x82061029` with
`mfhi` / `addu` / `sra 13` / sign correction — write the natural division:

```c
node->field_2 = (master * params->field_5 * node->field_A) / 16129;
```

Do not hand-write the magic constant. `SndVoice_ApplyMasterVolume` (and the same sequence in
`SndScript_Exec`) is the reference. Related layout notes:

- `SndScript::field_4C` is a `SndVoiceParams*` voice-param block (`field_5` scale).
- `SndVoice::field_A` is the per-voice `u8` scale; `field_2` stores the result.
- Null-check `field_40` via a temp then assign the walk pointer so the target
  keeps `lw v0,0x40; beqz v0; move a1,v0` (see earlier "temp then cur" entry).

## Range-check + shared non-zero body needs `if`/`goto`, not `switch`

When the target opens with

```
lw     v1, status
sltiu  v0, v1, N
beqz   v0, default_entry
...
bnez   v1, shared_body
 # delay: load shared address
# case 0 body
j      join
 # delay: last case-0 store
default_entry:
 # load same shared address
shared_body:
 # single copy of pad/work
join:
 sw     v0, field_30(a0)
```

a C `switch` whose case 0 is unique and cases `1..N-1` plus `default` all share
one body collapses to a plain `bnez status` (no `sltiu`). Duplicate the shared
body under `if (status < NU) { ... } else { ... }` and the compiler emits two
copies.

Match by writing the m2c shape with a single shared tail:

```c
status = work->field_14;
if (status < 4U) {
    ptr = Mc_FileName; /* also fills the bnez delay slot on the case-0 path */
    if (status == 0) {
        /* case 0 */
        work->field_24 = 9;
        work->field_28 = -1;
        work->field_2C = 0;
        task->field_30 = 5; /* leave value in $v0 for the join store */
    } else {
        goto shared;
    }
} else {
    ptr = Mc_FileName;
shared:
    /* one pad / work block; ends with task->field_30 = 0x2A */
}
work->field_18 = 0;
/* common prompt draw */
```

Assign `field_30` in each branch (do **not** funnel through a `next` temp). A
`next` temporary often lands in `$v1` (colliding with the loop index) and
reorders `sw field_30` past `li a1,1`. Direct stores keep the constant in `$v0`
and schedule the join `sw` before the prompt setup.

`Mc_StatePadFileName` is the pure example (status 0 vs pad-filename path for
1..3/default).

## Dual `lb`/`lbu` + `bltz` clamp to 0x7F

When the target both sign-loads and zero-loads the same byte, then clamps a
negative value to `0x7F`:

```
lb   v0, 2(s3)
lbu  v1, 2(s3)
bltz v0, neg
li   v0, 0x7F
j    join
 sb  v1, 4(s0)   /* >= 0: store raw byte */
neg:
sb   v0, 4(s0)   /* < 0: store 0x7F */
```

Write the positive arm first (`>= 0`) so the branch is `bltz` to the clamp:

```c
if ((s8)arg1[2] >= 0) {
    p->field_4 = arg1[2];
} else {
    p->field_4 = 0x7F;
}
```

`if ((s8)x < 0)` inverts the polarity to `bgez` and swaps the store order.
`func_80052488` (MIDI CC 6 data-entry path) is the pure example; same shape as
`SndEvt_EnqueueType5` without the dual load.

## `s32 value = u8; if ((value & 0xFF) == K)` keeps load-delay `andi`

Assigning a byte load into `s32` and comparing with `(value & 0xFF)` forces:

```
lbu  v0, ...
nop
andi v1, v0, 0xFF
sb   v0, ...
li   v0, K
beq  v1, v0, ...
```

A plain `u8 value` drops the `andi`/`nop` and shortens the function by 8 bytes,
shifting every later label. Use this when the target has an otherwise-redundant
`andi 0xFF` of a just-loaded byte. `func_80052488` case `0x63` needs it.

## Fade color global before prim cursor for `lui` order

When a function both writes a halfword fade/clear color and allocates a TILE from
`D_80070EE0`, source order of the *first mention* of each global controls the
early `lui %hi` order, while the actual `lw`/`sh` can still schedule as
`lw cursor` then `sh color`:

```c
extern volatile s16 D_8006ACB4;

D_8006ACB4 = 0xFF;                      /* mention color first → lui v1 first */
color      = *(volatile u8*)&D_8006ACB4; /* forces lbu reload, not reg reuse */
p          = (TILE*)D_80070EE0;         /* lui t0 second; lw may still lead */
D_80070EE0 = (u8*)(p + 1);
setlen(p, 3);
setcode(p, 0x62);                       /* 0x62 = TILE | semi-trans */
p->r0 = color;
p->g0 = color;
p->b0 = color;
/* x/y/w/h then addPrim(D_800710A0 - 0x10, p); DR_TPAGE with setDrawTPage */
```

Writing `p = D_80070EE0` first swaps the two `lui`s (~99.7% near-match). The
`*(volatile u8*)&` cast is required for the post-`sh` `lbu`; a plain
`(u8)D_8006ACB4` may keep the value in a register. `Fade_StartWhite` is the pure
example (fullscreen white TILE at OT slot `-0x10` plus `setDrawTPage(..., 0, 1, 0x40)`).

## Scoped `register asm` pin for delay-slot `move a0,v1` after `lb`

When the target loads a signed byte then copies it into `$a0` in the compare's
delay slot:

```
lb    v1, 0(v0)
li    v0, -1
beq   v1, v0, fail
 move  a0, v1
```

a plain `s8 temp; s32 slot = temp; if (temp == -1)` often coalesces `slot` into
`$v1` (no move). An `s8 slot` used both for the `-1` compare and later as an
index tends to emit **both** `lbu a0` and `lb v1`.

A hard pin on `slot` for the whole function fixes the prologue but then forces
later loads that *should* land in `$v0` (e.g. `lbu v0, D_xxx; addiu a0, v0, 4`)
to write `$a0` directly.

Fix: pin only long enough to force the delay-slot move, then copy out to an
unpinned local for the rest of the function:

```c
s8  temp;
s32 slot;

temp = table[idx];
{
    register s32 p asm("a0");
    p = temp;
    if (temp == -1) {
        return NULL;
    }
    slot = p;
}
/* later: slot = D_xxx + 4; uses lbu v0 / addiu a0,v0,4 as in the target */
bank = &banks[(s8)slot];
```

`Snd_AllocBank` is the pure example.

## Kill parameter `$a1` liveness after pin-copy so CSE can reuse it for call args

When a formal is copied into a pinned callee-saved reg (`register McWork* work
asm("s2"); work = arg1;`), GCC 2.8.1 often keeps the original `$a1` live as a
second home for the same pointer. That breaks the classic delay-slot CSE for:

```
li    a1, K
lw    a2, 0(s2)
move  v0, a1
jal   func
 sw   v0, 8(s2)   /* field = K in delay slot */
```

and instead emits:

```
li    v0, K
sw    v0, 8(s2)
lw    a2, 0(a1)   /* still using original a1 as the base */
jal   func
 move a1, v0
```

An empty `asm("" : "+r"(work))` barrier can force the right call sequence, but
it also reorders the prologue saves (`sw s2` / `move s2,a1` before `sw s3`).

Fix: after the pin-copy, assign over the formal so `$a1` is dead:

```c
register McWork* work asm("s2");

work = arg1;
arg1 = 0; /* kills a1; CSE can put K in a1 and load field_0 from s2 */
if (work->field_2C == 1) {
    work->field_8 = 0x11;
    status = Mc_PromptDialogSpawn(arg0, 0x11, work->field_0);
    ...
}
```

`Mc_StateNameEntry` is the pure example. Pair with `register Task* task asm("s3")`
(and other pins for later reuse of `$s0`/`$s1`) so the prologue is
`sw s3; sw s2; move s2,a1` with `move s3,a0` in the first `bne` delay slot.

## `field_222 = 1` in the delay of `bnez busy` (dual base pointers)

When the target does:

```
lh   v0, busy(a0)     /* p = &Queue reloaded into a0 */
li   v1, 1
bnez v0, skip
 sh  v1, field_222(s0) /* always; state lives in s0 */
sh   v1, busy(a0)
```

writing `state->field_222 = 1; if (p->busy == 0) p->busy = 1;` stores
`field_222` *before* the load. Force the load first with a temporary, then
assign `field_222` (still before the if body so the store can fill the
branch delay):

```c
p = &CdCmd_Queue;
{
    s32 busy = p->busy;
    state->field_222 = 1;
    if (busy == 0) {
        p->busy = 1;
        Display_State.field_130 = 0xFF;
    }
}
```

`CdCmd_HandleFileLoad` is the pure example. The two bases (`s0` for `state`, `a0` for
the reloaded `p`) are required so `field_222` and `busy` use different
addressing.

## Rematerialize `CdlDiskError` so it is not pinned in `$sN`

A function that only saves `$s0` but compares `CdSync(...) == CdlDiskError`
twice (outer check + 0x80 path) will CSE `5` into `$s1` and grow the frame.
Block the CSE by assigning through a throwaway and killing its REG_EQUAL:

```c
s32 sync;
s32 diskErr;

sync = CdSync(1, NULL);
diskErr = CdlDiskError;
asm("" : "+r"(diskErr));
if (sync == diskErr) {
    ...
}
```

Each site reloads `li v1, 5` after the `jal`, matching the ROM. Same idea as
other `asm("" : "+r"(...))` REG_EQUAL kills; `CdCmd_HandleFileLoad` is the pure
CdSync example.

## Two near-identical status switches: fully inline the shared tails

Two `switch (Fs_CdOpStatus)` blocks in one function (e.g. step 2 and step 5)
will cross-merge their `0x10/0x20/0x40` retry bodies when both go through a
shared `handle_ret` / `flush_or_retry` label. The dispatch then jumps from
case 5 into case 2's retry and the `slti` tree flips (`bnez` vs `beqz`).

Fix: inline the `ret < 2` / `ret == 2` / `F12D18_80024EC0` tails at every
site (duplicate the small blocks). GCC still cross-jumps the *identical*
`ret < 2 → (ret==0 ? return : end)` sequences into one shared block, so you
keep a single handle without the bad merge — and the sites that need a
*different* shape (case 5 retry uses an inverted `ret >= 2` tree) stay
separate. Also avoids `move a0, v0` after `CdCmd_PollStatus` that appears when
`ret` is live into a multi-predecessor shared label across calls.

`CdCmd_HandleFileLoad` is the pure example (paired with the busy-temp tip above).

## Reuse one pointer across Task* → field_20 → UiObject* for `$a0`/`lw a0,0x20(a0)`

When the target does:

```
lw    a0, 0xc(s2)      # child = task->field_c
nop
bnez  a0, else
 li    v0, 6           # else-only constant in delay slot
# null fall-through: spawn UI into a0, ...
else:
lw    a0, 0x20(a0)     # childObj = child->field_20 (same reg)
lh    v1, 0x2e(a0)
nop
bne   v1, v0, skip
```

Separate `Task* child` and `UiObject* childObj` variables put the child in
`$v0`, so `li v0, 6` cannot ride in the outer delay slot and the else path
becomes `lw a0, 0x20(v0)`.

Fix: one pointer reused for both roles (cast through `Task*` for `field_20`):

```c
UiObject* p;

p = (UiObject*)arg0->field_c;
if (p == NULL) {
    p = Ui_SpawnFromDesc(...);
    /* ... */
    return 0;
}
p = ((Task*)p)->field_20;
if (p->field_2E == 6) {
    /* ...; Ui_TeardownTree(p, p->field_28) keeps p in $a0 */
}
return obj->field_2C;
```

`Mc_PromptDialogChoice` is the pure example (~97.5% → 100% with only this change).

## Dense 0..N switch → equality-chain if/gotos with duplicated tails

A target that dispatches with a pure equality chain:

```
li   s4, 1
beq  v1, s4, case1
nop
beqz v1, case0
li   a1, 2
beq  v1, a1, case2
li   v0, 3
beq  v1, v0, case3
...
j    default
```

is *not* produced by `switch (x) { case 0: ... case 1: ... case 2: ... case 3: ... }`.
GCC 2.8.1 emits a binary tree with `slti` / range checks for consecutive cases
(score ~96% with otherwise identical bodies and correct `$s2` for a global).

`if (x == 1) goto case1; if (x == 0) goto case0; ...` fixes the dispatch, but
an explicit shared tail label for a common `p->field++` often rematerialises
`%hi(global)` into `$v0` instead of reusing the callee-saved address reg, and
can swap `$s2`/`$s3` between that global and a competing mid-function
`&Display_State` (~97%).

Match both: keep the if/goto equality dispatch, **and write the shared tail
inline in every case** (duplicate `Stage_Ctx->field_28++`). GCC CSEs those
copies into the dual-entry shared block the target wants (`lw` via `$s2` at the
head, case0 preloads and jumps mid-block with `sb` in the delay slot):

```c
temp = p->field_28;
if (temp == 1) {
    goto case1;
}
if (temp == 0) {
    goto case0;
}
if (temp == 2) {
    goto case2;
}
if (temp == 3) {
    goto case3;
}
goto default_case;

case0:
    /* ... */
    p->field_28 = p->field_28 + 1;
    goto end;
case1:
    if (ready()) {
        /* ... */
        p->field_28 = p->field_28 + 1;
    }
    goto end;
/* case2 likewise ends with the same increment */
case3:
    /* ... */
    p->field_28 = p->field_28 + 1;
default_case:
    /* ... */
end:
    return 1;
```

`Display_TransitionLoad` is the pure example (switch ~96.7%, shared-goto ~97.2%,
duplicated tails + if/gotos → 100%).

## Duplicate shared RECT/field updates into both `if`/`else` arms

When both arms of a branch update some fields differently and then apply the
*same* further updates to other fields, factoring the common updates after the
`if`/`else` often scores high but misses the target schedule:

```c
if (cond) {
    r.y += 9;
    r.h -= 0xB;
} else {
    r.y += 2;
    r.h -= 4;
}
r.x += 2;   /* common — looks clean */
r.w -= 4;
```

Symptom: join keeps `y` live in `$v0` and stores it after the branch; `$x` is
loaded only after the join; later half-width math interleaves with the `x`/`w`
stores (~73–81%). Target stores `y` and loads `x` in *both* arms, then joins
with `x` in `$v0` / adjusted `h` in `$v1`.

Duplicate the common updates into each arm so the compiler treats them as part
of the branch bodies and sinks only the post-store half math:

```c
if (cond) {
    r.y += 9;
    r.h -= 0xB;
    r.x += 2;
    r.w -= 4;
} else {
    r.y += 2;
    r.h -= 4;
    r.x += 2;
    r.w -= 4;
}
```

Also for signed half of a `short` width stored then reloaded as `lhu; sll 16;
sra 17; negu`: write `-(w >> 1)` (not `/ 2`, which emits the signed-div bias)
and chain the pair through the field itself (`p->lo = -(w >> 1); p->hi = p->lo
+ w`) so `negu` stays in `$v0` rather than a separate temp in `$v1`.

`Ui_InsetLayout` is the pure example (factored tail ~81%, duplicated arms → 100%).

## Force early `lui`/`ori` of a late-used constant

When the target completes a multi-instruction constant (`lui tN,hi` /
`ori tN,tN,lo`) before any field loads, but your build leaves the `ori` as the
branch delay-slot filler (and puts a later body instruction after the branch),
the constant is too free to schedule. Pin it and emit an empty asm that takes
it as an I/O operand so the full materialization finishes before subsequent
loads (`Ui_DrawListHighlight`):

```c
register u32 color asm("t4");
register Arg* a1 asm("t0");

a1    = arg1;
color = 0x1741F;
asm("" : "+r"(color), "+r"(a1));
/* only now may field loads begin */
f14 = a1->field_14;
```

Pinning the object pointer in the same asm also locks the target's early
`move t0,a1` register choice when that is the only remaining mismatch.

## Memory clobber to stop a later field load hoisting past a store

Two distinct fields of the same object have no alias conflict, so GCC will
happily load `field_1E` *before* storing `field_14 + 1` even when the target
does the store first and reuses `$v0` for the subsequent `lh field_1E` (with a
load-delay `nop`). A memory clobber between the store and the next load
restores the target order:

```c
a1->field_14 = f14 + 1;
asm("" ::: "memory");
width = a1->field_1E - x1; /* lh into $v0, nop, subu */
```

## Signed layout overlay when the canonical struct is `u16` for other matches

`UiPanel.field_1C` / `field_1E` are `u16` so functions like `Ui_InsetLayout`
emit `lhu`. A sibling draw helper that needs `lh` for the same offsets must not
flip the canonical type (that breaks the other matches). Use a local overlay
with `s16` at those offsets and cast once:

```c
typedef struct {
    u8  pad0[0x14];
    u16 field_14;
    u8  pad16[6];
    s16 field_1C;
    s16 field_1E;
    u16 field_20;
    u16 field_22;
} GStruct30SignedLayout;

a1 = (GStruct30SignedLayout*)arg1;
x1 = a1->field_1C; /* lh */
```

## Preload `y` and subtract height into `arg2` for TILE y0 delay-slot form

For a TILE whose `y0 = base_y + arg2 - h + 1`, the target often does
`subu a2,a2,t3` in the early-out branch delay slot, then later
`addu v1,v1,a2; addiu v1,v1,1`. Mirror `Ui_AllocTile`'s `y = field_22` preload
and write the adjust as an assignment on `arg2` inside the body:

```c
y = a1->field_22;
...
arg2  = arg2 - h;       /* fills bnez delay slot */
p->y0 = y + arg2 + 1;   /* addu + addiu, no separate subu of h */
```

`Ui_DrawListHighlight` is the pure example (together with the color-pin / memory-clobber
tips above).

## Force halfword reload after a live register holds the same value

When the target reloads a field (`lhu v1, off(p)`) that is still live in another
register from an earlier store of the same value, plain field access CSE's the
reload away (`nop` instead of `lhu`). A volatile-qualified read forces the
memory load without changing surrounding non-volatile stores:

```c
tv0 = p->x1;                          /* kills live y in $v0 */
tv1 = ((volatile POLY_F3*)p)->y0;     /* must lhu, not reuse live y */
```

`Ui_DrawFlatCaret` needs this so the if/else arms start with `lhu x1; lhu y0`
exactly as the target (register-asm on `$v0`/`$v1` alone is not enough when y
is still live in `$v1` from the prologue).

## Pin OT base and `0xFF000000` for dual-use addPrim codegen

Manual OT linking that reloads `field_14` twice (equivalent to
`addPrim(ot + (s16)field_14 + 1, p)`) wants:

```
lui  v1, %hi(D_800710A0)
lui  a1, 0xFF000000
lw   a2, %lo(D_800710A0)(v1)
```

Without pins, GCC often swaps `$a1`/`$a2` (mask in `$a2`, OT in `$a1`) or
reloads the OT base. Pin both:

```c
register u32  mask_hi asm("a1");
register u32* ot asm("a2");

mask     = 0xFFFFFF;
/* set color / setlen / setcode first so $a0 holds 0xFFFFFF */
ot       = D_800710A0;
mask_hi  = 0xFF000000;
p->tag   = (p->tag & mask_hi) | (ot[(s16)idx + 1] & mask);
ot[(s16)idx + 1] = (ot[(s16)idx + 1] & mask_hi) | ((u32)p & mask);
```

Assign `ot` before `mask_hi` so the `lui %hi(D_800710A0)` precedes
`lui a1,0xFF00`. `Ui_DrawFlatCaret` is the pure example.

Do **not** also pin an earlier mid-function temporary to `asm("a1")` (e.g. a
live `y = field_22` that the target keeps in `$a1` across a branch). Pinning
both forces the early value elsewhere and emits `lui a1,0xFF00` too early.
Leave the early temp unpinned: with `mask_hi` reserved for the epilogue, GCC
still naturally places the live-across-branch value in free `$a1`, and the late
`mask_hi` assignment keeps the correct `lui` schedule. `Ui_DrawCaret` is the
example (shares the dual-use OT pattern with `Ui_DrawFlatCaret`).

## Oversize prim advance via a larger sibling type

When the buffer cursor advances by more bytes than `sizeof` the prim being
written (e.g. POLY_F3 is `0x14` but the target does `addiu …, 0x1C`), cast
through a same-header type of the right size for the `+ 1` step:

```c
p          = (POLY_F3*)D_80071190;
D_80071190 = (DR_TPAGE*)((POLY_G3*)p + 1); /* +0x1C */
```

Avoid raw `(u8*)p + 0x1C`.

## Second arg as `s32` to reuse `$a1` after pointer loads

When the target loads several fields through `$a1`, then sign-extends a
halfword into `$a1` itself and reuses that register for the rest of the
function, declare the second parameter as `s32` and cast once to the real
pointer type:

```c
void func(UiList* arg0, s32 arg1)
{
    UiPanel* a1 = (UiPanel*)arg1;
    s16 temp;

    /* all loads from a1… */
    temp = a1->field_1A - a1->field_18;
    arg1 = temp;           /* sra a1, … — overwrites the pointer reg */
    arg1 = arg1 - arg0->field_17;
    /* further uses of arg1 as height */
}
```

A typed pointer parameter forces the compiler to spill `$a0` into `$a2` and
put temps in `$a0`/`$a1`, scrambling the whole body. `Ui_ComputeVisibleRows`.

## Dead stack `RECT` kept with an `"m"` constraint

When the target builds a full `RECT` on the stack (`sh`×4) but never reads
it, plain locals get DCE'd under `-O2`. Keep the stores without reloads:

```c
RECT sp;
sp.x = …;
sp.y = …;
sp.w = …;
sp.h = temp;
/* … rest of function … */
asm("" : : "m"(sp));
```

`volatile RECT` also keeps the stores but can reorder the final `sh` ahead of
the sign-extend of `h`. The trailing `"m"` constraint matches the target
schedule. `Ui_ComputeVisibleRows`.

## Memory clobber to force a same-field reload after store

When the target does `sw field_10; lbu field_4` (reload into the same reg
used for the prior compare) even though `field_4` was not written, a plain
`temp = arg0->field_4` after the store is CSE'd away. Defeat it with a
memory clobber between store and reload (`Ui_ComputeVisibleRows`):

```c
temp = arg0->field_4;
if (arg0->field_10 >= temp) {
    arg0->field_10 = temp - 1;
    asm("" ::: "memory");
    temp = arg0->field_4; /* lbu only on this path, before the join */
}
if ((s8)arg0->field_5 >= temp) {
    …
}
```

A `u8` temporary for `temp` often inserts a redundant `andi …, 0xff` after
the first `lbu`; prefer `s32 temp = arg0->field_4`.

## `--expand-div` for TUs with signed division traps

ASPSX expands `div` into the `bnez`/`break 7`/`break 6` trap sequence. Enable
`--expand-div` in `ninja_config.py` for any TU whose target contains those
traps (now includes `ui.c` alongside `tmd` / `sndbank` / `textdraw`).
Scratch-env `build.sh` does not pass this flag by default — use a local
wrapper or expect a score gap of only the missing trap block.

## Dual `TextDrawReq` stack slots via array + mixed `sp[i]` / `p[i]` access

When the target draws through two adjacent `TextDrawReq` slots (e.g. `sp+0x50`
relative path, `sp+0x60` absolute path) and mixes absolute stack stores
(`sh …, 0x50(sp)`) with s0-relative ones (`sw s6, 0x14(s0)` / `sb s6, 0x1c(s0)`
into the second slot), declare them as one array and take a pointer to the
base:

```c
TextDrawReq sp50[2];
register TextDrawReq* p asm("s0");

p = sp50;
/* relative path: absolute addressing via sp50[0].field_…, field_C via p */
sp50[0].field_0 = …;
p->field_C = 4;
func_8002E53C(p, buf);

/* absolute path: most fields via sp50[1], field_4/field_C via p[1] */
sp50[1].field_0 = x;
p[1].field_4 = four;   /* sw four, 0x14(s0) */
sp50[1].field_8 = arg4;
p[1].field_C = four;   /* sb four, 0x1c(s0) */
func_8002E53C(&sp50[1], buf);
```

Two separate `TextDrawReq sp50, sp60` locals usually emit only absolute
addressing for the second slot. Writing everything through `p` alone emits
only relative `off(s0)`. The split is required.

Also for multi-line loops over text (`Text_ParseLine` + `func_8002E53C`):

- Pin long-lived args/temps into `$s0`–`$s7` so the remaining stack arg
  (`arg4`) stays on the stack and is reloaded with `lw t0, 0xB0(sp)` each
  use. Early-copy `arg5`/`arg6` into locals (one of them naturally lands in
  `$fp`/`$s8`) *before* pinning `four = 4` into `$s6` for the dual-width
  `field_4`/`field_C` stores.
- Pass the stack array name (`sp10`) to the first call so the `jal` delay
  slot rematerializes `addiu a1, sp, 0x10` instead of `move a1, s4`; keep
  `buf` in `$s4` for later calls that need `move a1, s4`.
- Reload the x formal from its home at the end of each iteration
  (`x = arg1`) so the target's `lw s2, 0xA4(sp)` matches.

`Text_DrawMultiLine` is the pure example (multi-line sibling of single-line
`Text_DrawPrompt`).

## Scratch-head 8-byte alloc: pin `v1`/`a3`/`v0` for `move s1,v0`

Downward 8-byte scratch allocations that keep the buffer in `$s1` across
calls often need:

```
lui  v1, 0x1f80
ori  v1, v1, 0x3fc
lw   a3, 0(v1)        /* head */
addiu v0, a3, -8
move s1, v0           /* param1 */
sw   v0, 0(v1)
li   v0, 2
andi v1, a2, 0xff     /* reuse dead scratch ptr */
sb   v0, 2(s1)
sltiu v0, v1, 5
sb   zero, 3(s1)
sb   a1, -8(a3)       /* store via original head, not s1 */
```

Without register pins, GCC puts the scratch address in `$v0` and the head in
`$v1`, which forces `addiu s1, v1, -8` / `sw s1, ...` (no `move`) and delays
the `andi` until after the head-based store. Force the target allocation:

```c
register void** scratch asm("v1");
register void*  head asm("a3");
register void*  temp asm("v0");

scratch  = (void**)G_SCRATCH_HEAD;
head     = *scratch;
temp     = (u8*)head - 8;
param1   = temp;
*scratch = temp;
param1[2] = 2;
param1[3] = 0;
((u8*)head)[-8] = arg1; /* not param1[0] */
```

Free paths after a `jal` must rematerialize with a bare
`*(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8` (do not keep the
`scratch` local live across the call). `CdCmd_EnqueueLoadFile` is the pure example.

## `s8` stack slots for signed `li` of negative byte constants

Under `-funsigned-char`, `u8 param2[4]; param2[i] = -8` emits `li v0, 0xf8`.
The target often wants `addiu v0, zero, -8` / `sb`. Declare the stack array
as `s8` (and cast to `u8*` at the `CdCmd_Enqueue` call):

```c
s8 param2[4];
param2[2] = -8; /* li v0, -8 */
param2[3] = -3;
CdCmd_Enqueue(0x21, param1, (u8*)param2);
```

## Jump-table slot with trailing zero pad

When the original `jtbl` is N case words plus a trailing `.word 0` (for 8-byte
alignment of the next table) and GCC emits only the N case words, split the
rodata so the C `.rodata` covers just the N words and the next asm segment
starts at the pad word:

```yaml
- [0x46E8, .rodata, loadui]   # GCC 5-entry jtbl (0x14 bytes)
- [0x46FC, rodata, 32AF8_1]  # .word 0 pad + still-asm jtbls
```

Do not expand the C range to include the zero — GCC will not emit it and the
layout shifts. `CdCmd_EnqueueLoadFile` / `jtbl_80013EE8`.

## `goto` body forces `bnez` over a mid-function shared `return 0`

When the target has:

```
beq  cond_fail, ret0
nop
bnez cond_ok, body
nop
jr   ra
 move v0, zero     /* ret0 sits BETWEEN the checks and the body */
body:
 ...
```

nesting `if (outer) { if (inner) { /* body */; return 1; } } return 0;` (or a
combined `&&`) produces `beqz` jumps to a trailing `return 0` epilogue — same
semantics, wrong layout. Bare early `return 0` on each fail path also lands
the epilogue at the end.

Fix: keep the positive outer/inner tests, but jump to the work with `goto`,
so the shared `return 0` is the fall-through between the branch and the label:

```c
if ((cmd >> 4) != 8) {
    if (cmd != 0) {
        goto do_work;
    }
}
return 0;
do_work:
    /* long copy / setup */
    return 1;
}
```

Pairs with the early busy-path `if (flag != 0) return 1;` (which still puts
`li v0, 1` in the `bnez` delay slot). `CdCmd_ActivatePhase2` is the pure example.

## Keep signed-div dividend live for early `sra` before `mfhi`

Signed division by a constant (e.g. `/ 8191` → magic `0x80040021`) expands to
either:

```
# early sign (target for Midi_PitchBend)
sra  v0, a1, 31
mfhi v1
addu v1, v1, a1
sra  v1, v1, 12
subu v1, v1, v0
```

or

```
# late sign (reuses dividend reg after last use)
mfhi v1
addu v0, v1, a1
sra  v0, v0, 12
sra  a1, a1, 31
subu v0, v0, a1
```

The late form appears when the dividend dies at the `addu` (sign can clobber
it). Force the early form by keeping the product live past the division:

```c
prod  = scale * pitchBend;
key   = (s8)*(volatile u8*)&slot->field_2 & 0xFFFF; /* may fill mult delay */
pitch = prod / 8191;
asm volatile("" :: "r"(prod)); /* dividend still live → separate sign reg */
slot->field_8 = pitch;
```

Also pin the scale/`<< 8` temporary with `register s32 scale asm("a1")` when
the target loads the scale with `lbu a1, …` and keeps the product in `$a1`
through `mflo a1`. Pair with `register GStruct* note asm("a3")` when the
target does `move a3, v0` in a `bltz` delay and later `lbu a2,4(a3); lbu a3,5(a3)`.

`Midi_PitchBend` is the pure example.

## Force `andi` after `lhu` with a non-constant mask temp

When the target does `lhu` then later `andi rd, rs, 0xFFFF` (e.g. array index
from a `u16` that is also used for `xori` in between), a literal `t & 0xFFFF`
is often deleted as redundant after the zero-extending load. Assign the mask
to a local first and reuse it:

```c
s32 mask;

mask = 0xFFFF;
index = (new_val & mask) - 1;
/* ... */
p = &base[t & mask]; /* keeps andi after lhu / xori */
```

`Mdec_UploadSlice` needs this so `xori` (flip) and `andi`/`sll`/`addu` (pointer)
share the same `lhu` of `D_8005EAEE`.

## Pin callee-saved reg + empty asm for store-in-delay-slot schedules

When the target materializes a pointer with `addu a0, …` *before* loading a
compare operand, then puts a halfword store in the `bne` delay slot, the
scheduler often does the store first (freeing a reg for the compare) and
parks the `addu` in the delay slot instead.

Force the pointer to complete first with a multi-output empty asm (already
documented above), and if that swaps which globals land in `$s0`/`$s1`, pin
the one that should be in a specific callee-saved reg:

```c
register u_long** base asm("s1");

base = D_8006AC48;
/* ... */
p = &base[t & mask];
asm("" : "+r"(p), "+r"(flipped)); /* p fully in $a0 before store */
D_8005EAEE = flipped;             /* lands in bne delay slot */
if (D_8006AC14 == 1) {
    size *= 0xC;
} else {
    size *= 8;
}
DecDCTout(*p, size);
```

Without `asm("s1")` on `base`, the empty asm alone matched the body but
swapped `$s0`/`$s1` between `D_8005EAEE` (`%hi` only) and `D_8006AC48`
(full address). `Mdec_UploadSlice` is the pure example.

## Snapshot compare operand so a post-load `arg++` fills the `bne` delay

When the target does:

```
lbu   v0, 1(p)
nop
bne   v0, K, merge
 addiu a1, a1, 1   /* delay: always */
addiu a1, a1, 1    /* taken only */
/* taken-only work */
merge:
```

writing the always-increment *after* the `if` often lets GCC speculate the
taken path's address math into the delay slot instead (`addiu v0, p, N` /
`sw`), leaving both `addiu a1` after the branch (~99%):

```c
/* BAD — addiu v0,p,2 lands in bne delay */
if (p[1] == K) {
    arg1 += 1;
    *arg0 = p + 2;
}
arg1 += 1;
```

Snapshot the loaded byte, then increment *before* the `if`. The compare uses
the temp so the first `arg1++` is independent of the branch and becomes the
delay-slot fill; the second stays on the taken path:

```c
/* GOOD */
next = p[1];
arg1 += 1;
if (next == K) {
    arg1 += 1;
    *arg0 = p + 2;
}
```

`Text_ParseLine` needs this for the `\r` / `\r\n` line-break arm.

## Separate mask temp vs. in-place `&=` for flag bit clears

When the target clears flag bits with a fresh load into `$v0` and masks in `$v1`:

```
lw  v0, 0(s0)
li  v1, -2
and v0, v0, v1
li  v1, -5
and v0, v0, v1
sw  v0, 0(s0)
```

writing `flags = entry->field_0; flags = flags & ~1; flags = flags & ~4` reuses the
early `flags` register (often `$v1` from the bit tests) and swaps the pair. Use
in-place RMW instead so the load is a temporary in `$v0`:

```c
entry->field_0 &= ~1;
entry->field_0 &= ~4;
```

Do **not** fold to `entry->field_0 &= ~1 & ~4` (or `&= ~6`) — the constant folder
emits a single `li v1, -6`.

When a later path needs `li a0, -9` *before* reloading flags (so `$a0` holds the
mask and `$v1` the flags), assign the mask to its **own** temporary first — do
not reuse the early `flags` name, which can pull the initial `flags` load into
`$a0` (~99.8% with only that reg wrong):

```c
/* GOOD — mask is a separate local; early flags stays in $v1 */
ret  = entry->field_10(entry);
mask = ~8;
entry->field_0 = (entry->field_0 & mask) | ((ret & 1) * 8);

/* BAD — reusing flags for ~8 reallocates the early load into $a0 */
flags = ~8;
entry->field_0 = (entry->field_0 & flags) | ((ret & 1) * 8);
```

`AsyncCb_Poll` is the pure example.

## Reuse arg regs for fixed UV constants; pin f20/next for prim cursor order

When a POLY_FT4 setup reuses `$a1`/`$a2` for fixed U coordinates after their last
use as real arguments (target: `li a1,0x6F` right after the `D_80071190` update,
`li a2,0x68` right after `field_20 + arg2`), separate locals for those constants
usually rematerialize as late `li v0,K`. Two tricks together match:

1. **Reassign the argument** after its last real use (`arg2 = 0x68`) so the
   constant stays in `$a2` through the U stores.
2. **Pin the mid-section temps** so `field_20` reloads into `$v0` *before* the
   prim-cursor advance, and the advance uses `$v1` — the same interleaving as
   the target between the first X pair and `li a1,0x6F`:

```c
p->x2 = temp;
p->x0 = temp;
{
    register u16 f20 asm("v0");
    register s32 next asm("v1");
    register s32 ur asm("a1");

    f20  = arg0->field_20;          /* lhu into $v0 before cursor update */
    next = (s32)(p + 1);
    D_80071190 = (DR_TPAGE*)next; /* addiu/sw via $v1; frees $a1 */
    ur   = 0x6F;                    /* li a1,0x6F */
    temp = f20 + arg2;
    arg2 = 0x68;                    /* li a2,0x68 — reuses arg reg */
    p->x3 = temp;
    p->x1 = temp;
    /* … UV setup … */
    p->u1 = ur;
    p->u3 = ur;
    p->u0 = arg2;
    p->u2 = arg2;
}
```

Without the `f20`/`next` pins, GCC advances the cursor in `$v0` first and
hoists `li v0,0x50`, losing the `lhu`/`addiu` interleave. Without `arg2 = 0x68`,
`$a2` is rematerialized at the U stores. `Ui_DrawHBar` is the pure example.

`Ui_DrawVBar` is the vertical sibling (Y from arg1..arg2, X from
`field_20+arg3±offsets`) and cannot reassign `arg2` — both arg1 and arg2 are
still live for the Y edges, so the compiler saves them to `$t1`/`$t2` and the
U constant must land in the now-free `$a2` via an explicit pin. Also pin the
X offset through `$v1` so `$a1` stays free for the `D_80071190` hi/lo pair
(natural allocation otherwise puts arg3 in `$a1` and the cursor hi in `$a2`,
pushing `0x70` to a late `li v0`):

```c
if (arg1 < arg2) {
    register s32 xoff asm("v1");
    register s32 left asm("v1");
    s32          base;   /* s32 avoids sll/sra sign-extend on left = base-3 */

    xoff = arg3;         /* delay-slot move v1,a3 */
    p    = (POLY_FT4*)D_80071190;
    base = arg0->field_20 + xoff;
    left = base - 3;
    temp = base + 5;
    /* … */
    {
        register u16 f22 asm("v0");
        register s32 next asm("v1");
        register s32 ur asm("v1");
        register s32 ul asm("a2");

        f22 = arg0->field_22;
        next = (s32)(p + 1);
        D_80071190 = (DR_TPAGE*)next;
        ur = 0x77;
        ul = 0x70;       /* early li a2,0x70 (reg free after t2 save) */
        /* … p->u0 = ul; p->u2 = ul; … */
    }
    y = y + arg2;        /* original arg2 still in $t2 */
}
```

Pinning `ul asm("a2")` at function scope (or before the `t0/t1/t2` saves are
decided) shifts the t-regs and steals `$a3` from the addPrim mask — keep the
`a2` pin inside the block that also pins `f22`/`next`/`ur`.

## `lhu` / `li 0xFFFF` / `andi …,0xFFFF` with dual empty-asm barrier

When the target does:

```
lhu   a1, 8(s0)
li    v0, 0xffff
andi  v1, a1, 0xffff
bne   v1, v0, success
 srl  v1, v1, 0xc
```

a plain `temp = index & 0xFFFF` after `lhu` is deleted (zero-extend is already
proven), leaving `move v1,a1`. An `asm("" : "+r"(index))` alone restores the
`andi` but freezes scheduling so you get `lhu; nop; andi; li` instead of
`lhu; li; andi`.

Fix: keep both the index and the compare constant live across one empty asm,
with the `li` written *after* the load in source order:

```c
register u32 index asm("a1");
register u32 temp asm("v1");
s32 mask;

index = bank->field_8;
mask  = 0xFFFF;
asm("" : "+r"(index), "+r"(mask));
temp = index & 0xFFFF;
if (temp != mask) {
    goto success;
}
success:
temp >>= 12; /* fills the bne delay as srl v1,v1,0xc */
```

Pinning `index` to `$a1` and `temp` to `$v1` keeps the `andi v1,a1,0xffff` /
`srl v1` chain; `mask` in a GPR supplies the `li v0,0xffff` for the compare.
`SndBank_SetupFromLoad` is the pure example (bank id check before `D_800680AC` lookup).

## Signed `/ 64` map store: pin dividend adj to `$v1`, shift result to `$v0`

Target for `map[x / 64] = i` (signed):

```
lw    v0, 0x50(a1)
nop
bgez  v0, pos
 move v1, v0
addiu v1, v0, 0x3f
sra   v0, v1, 0x6
addu  v0, v0, base
sb    a0, 0xa23(v0)
```

A plain `x / 64` or unpinned temps often folds the adjust into `$v0` (`nop` in
the `bgez` delay, `addiu v0,v0,0x3f; sra v0,v0,0x6`) or keeps the shift in
`$v1` (`sra v1,v1,0x6`). Fix with two pins inside the loop scope only:

```c
{
    register s32 adj asm("v1");
    register s32 sh asm("v0");
    s32 raw;

    raw = entry->head; /* or equivalent load */
    adj = raw;
    if (raw < 0) {
        adj = raw + 0x3F;
    }
    sh = adj >> 6;
    ((u8*)work)[sh + 0xA23] = (u8)i;
}
```

`Mc_StateListDirectory` is the pure example (DIRENTRY.head / 64 → block map at 0xA23).

## Indexed multiply form can SR and still leave `$a1` free for a later `li a1,1`

A post-loop `func(..., 1)` often wants:

```
blez  n, after
 li   a1, 1          /* delay: 1 ready if the loop is skipped */
blez  n, after
 move a0, zero       /* i = 0 */
move  a1, base       /* walker */
/* ... loop clobbers a1 ... */
li    a1, 1          /* restore for the call */
```

An explicit walker in the `for` init (`for (i = 0, p = base; ...)`) tends to
emit `move a1, base` *between* the two `blez`s, killing the delay-slot `li a1,1`.
Writing the body as an index multiply instead:

```c
if (n > 0) {
    for (i = 0; i < n; i++) {
        raw = *(s32*)((u8*)base + i * 0x28 + 0x50);
        /* ... */
    }
}
func(obj, 1);
```

still strength-reduces to `move a1, base` / `addiu a1, a1, 0x28` under `-O2`,
but schedules the walker init *after* both `blez`s so the hoisted `li a1,1`
survives. Do not pin `i` to `$a0` here — that blocks the SR into `$a1`.
`Mc_StateListDirectory` is the pure example.

## Goto dispatch for `beq`-to-handler type switches

When the target does positive equality tests that jump *to* handlers laid out
as case1 then case2 then merge (`beq type,1,case1` / `beq type,2,case2` /
`j merge`), an `if (type == 1) {…} else if (type == 2) {…}` chain emits the
inverse (`bne` past an inlined body) and puts case2 before case1.

Write explicit gotos so the case bodies appear in target order:

```c
if (type == 1) {
    goto case1;
}
extra = NULL;
if (type < 2) {
    goto merge;
}
if (type == 2) {
    goto case2;
}
goto merge;

case1:
    /* … */
    goto merge;
case2:
    /* … */
merge:
```

`Task_SpawnFromDesc` is the pure example (flags low byte 0 / 1 / 2).

## Booleanize `(x & mask)` with `(u32)temp > 0` for `andi` + `sltu`

`(flags & 0x100) != 0` often becomes `srl` / `andi 1`. To get:

```
andi  v0, a0, 0x100
sltu  a2, zero, v0
```

capture the mask result and compare unsigned against zero:

```c
temp = flags & 0x100;
flags_a2 = (u32)temp > 0;
```

## Keep `u16` fields that other TUs store with `sh`

Narrowing `TaskDesc::field_2` from `u16` to `u8` made `Task_SpawnFromDesc`
emit `lbu`, but broke already-matched `Ui_SpawnFromDesc` (`desc.field_2 =
arg0->field_12` became `lbu`/`sb` instead of `lhu`/`sh`). Keep the wider type
and force the byte load where needed:

```c
priority = *(u8*)&desc->field_2; /* lbu, not lhu */
```

## Reuse walk pointer as insert slot via `&node.prev`

Target list insertion reuses one register: after walking to the insert point
it does `bnez curr, join` / `addiu curr, curr, 4` / `addiu curr, list, 4`, then
treats that register as `TaskNode**`. Pin the walker and convert in place:

```c
register Task* curr asm("a3");
/* … walk by field_29 … */
if (curr != NULL) {
    curr = (Task*)&curr->node.prev;
} else {
    curr = (Task*)&list->prev;
}
task->node.next = (*(TaskNode**)curr)->next;
(*(TaskNode**)curr)->next = task;
task->node.prev = *(TaskNode**)curr;
*(TaskNode**)curr = &task->node;
```

A separate `TaskNode** insert` usually allocates a second register and drops
the delay-slot `+4` form. `Task_SpawnFromDesc` is the example.

## Nested `register Task* ch asm("v1")` to stop `a0` coalesce on child→obj

When the target loads a child task into `$v1` then its `field_20` into `$a0`:

```
lw   v1, 0xc(s6)
beqz v1, end
 sw  v0, 0x30(s6)
lw   a0, 0x20(v1)
```

a single function-scope `Task* child` is often coalesced into `$a0`
(`lw a0,0xc; lw a0,0x20(a0)`). Pinning `child` with `asm("v1")` for the whole
function then forces the *other* child load (later in the same function) into
`$v1` as well.

Fix: scope the pin to only the early path via a nested block so the later
reload can still use `$a0`:

```c
if (work->field_14 != 0) {
    register Task* ch asm("v1");
    ch = task->field_c;
    task->field_30 = 7;
    if (ch != NULL) {
        childObj = ch->field_20; /* lw a0, 0x20(v1) */
        ...
    }
}
/* later: */
child = task->field_c; /* lw a0, 0xc(s6) again */
```

`Mc_StateSyncFileSelect` is the pure example. Pair with `register s32 syncResult
asm("s1")` so `flag->field_0 = syncResult` emits `sw s1,0(s0)` instead of
substituting a live `li`/constant register after `syncResult == one`.

## `volatile u16*` blocks strength-reduction of mid-struct halfwords

When a loop walks with two induction pointers at `field_0` and `field_1` of a
4-byte record, and the target loads the following halfword as `lhu 1(p1)` /
`sh 1(p1)`:

```
addiu a1, base, 0x10   /* field_0 */
addiu a2, base, 0x11   /* field_1 */
lhu   v0, 1(a2)        /* field_2 */
sh    v0, 1(a2)
```

plain `*(u16*)(p1 + 1)` is strength-reduced into a *third* IV at `base+2`
(`addiu v1, base, 0x12` / `lhu 0(v1)` / `addiu v1, v1, 4`). That also scrambles
register assignment for the rest of the function.

Fix: mark the halfword access volatile so GCC cannot SR the address:

```c
half = *(volatile u16*)(p1 + 1) - 1;
*(volatile u16*)(p1 + 1) = half;
if ((half << 16) == 0) { /* sll; bnez zero-check on the low half */
    *p0 = 0;
}
```

Do **not** use a packed `{u8; u16}` view of `field_1`/`field_2` — GCC 2.8.1
emits byte-wise loads/stores for the "unaligned" u16. Pair with
`pad = arg0` plus a short-lived `register void** scratch asm("a0")` block so
`$a0` holds `G_SCRATCH_HEAD` for the alloc and is free to reuse as the second
bank's `field_1` pointer. `Pad_TickEventBanks` is the pure example.

## Separate cleanup tails: early `return` + distinct base pointers

When two arms of a function both clear `busy` / dequeue a queue slot but the
target keeps them as *separate* instruction sequences (one base in `$a0`, the
other reloads `$s0`), a shared local pointer plus `break` lets GCC cross-jump
the identical tails into one block. That collapses the first cleanup onto the
second's register and drops a chunk of code.

Force both copies to stay:

1. End the first cleanup with `return` (not `break` into a shared epilogue
   path that the second arm also falls into).
2. Use a *fresh* local for the first cleanup's base (`q = &CdCmd_Queue` after
   `Mem_Set`) while the second arm reassigns the original `p` /
   `$s0` (`p = &CdCmd_Queue` at the cleanup label).

`CdCmd_ProcessPhase1` is the example — cases 3/4/6/7 clean up via `$a0`, case 8 via
`$s0`.

## `s32` temp for halfword so the pointer stays in `$v1`

After a compare that leaves both `$v0` and `$v1` free, a `u16 temp =
ptr->field` assignment tends to put the pointer in `$v0` and the halfword in
`$v1`. The target often wants the opposite (`lw v1, ptr` / `lhu v0, field`).

Hold the halfword in an `s32` temporary (and keep an explicit pointer local):

```c
CdCmd190* info;
s32       temp;

info = p->field_190;
temp = info->field_14;
if (temp) {
    func(info->field_4 + temp);
}
```

The wider temp prefers `$v0` and leaves `$v1` for the pointer. Same family as
the `s16 ret` tip (narrow vs wide forcing different REG_EQUAL modes), just the
other direction. `CdCmd_ProcessPhase1` case 8 / `field_190` is the pure example.

## Empty memory clobber forces `sw ra` before the first delayed branch

When the target opens with a complete prologue (`sw s0` / `move s0,a0` /
`sw ra`) and then a value-select branch:

```
sw    ra,0x14(sp)
bltz  a1, label
 li   v0,0x2d        /* delay: default */
li    v0,0x2b        /* fall-through overwrite */
label:
sb    v0,0(s0)
```

writing the default into a local first often lets `-fdelayed-branch` park
`sw ra` in the `bltz` delay slot and leave `li v0,0x2d` *before* the branch
(~98.75% with otherwise identical body).

An empty memory clobber at the top of the body freezes the prologue stores
before any delayed-branch fill can steal them:

```c
s32 sign;

asm("" ::: "memory");
sign = 0x2D;
if (arg1 >= 0) {
    sign = 0x2B;
}
*arg0 = sign;
```

The body is otherwise the signed counterpart of `Text_ItoaSigned` (leading
`+`/`-`, digits written at `arg0 + 1`, negatives hand off to
`Text_ItoaSigned(arg0 + 2, -arg1)`). `Text_ItoaSignedPlus` is the pure example.

Note: TUs that need `--expand-div` (e.g. `textdraw.c`) must pass that flag in the
scratch `build.sh` as well, or local scores omit the `break` checks and look
worse than the real project match.

## POLY color-before-y and `fourth = f22 - 7` for call args

UI text-draw helpers that emit a `POLY_F4` then call a bar/underline routine
(e.g. `Ui_DrawTextUnderline` → `Ui_DrawHBar`) need two scheduling tricks:

**1. Store the solid color before the y-coords.** Target after the text call:

```
lui  a3, 0x2 / ori a3, 0x1002   /* color in $a3 */
lui  a2, 0xff / lui a1, %hi(cursor)
...
sw   a3, 4(p)                   /* *(s32*)&p->r0 = 0x21002 */
```

Writing `*(s32*)&p->r0 = 0x21002` *after* `p->y3 = y + 7` puts the constant
in `$a1` and forces `%hi(D_80071190)` into `$v1`, which then reorders the
cursor advance and the final call's field loads. Store color first:

```c
*(s32*)&p->r0 = 0x21002;
p->y3 = y + 7;
p->y2 = y + 7;
setcode(p, 0x28);
setlen(p, 5);
```

**2. Split `y - (f22 - 7)` so CSE cannot fold it with poly `y + 7`.** The
inline form `y - ((s16)p->field_22 - 7)` rewrites to `(y + 7) - field_22`,
keeps `y+7` live in `$t0`, and breaks the poly block. Load both fields, then
a dedicated temp:

```c
t20    = (s16)self->field_20;
f22    = (s16)self->field_22;
fourth = f22 - 7;
func_bar(self, x - t20, endX - t20, y - fourth);
```

That yields `lh` field_20 / field_22, `addiu a3, a3, -7`, `subu a3, s1, a3`
with the poly's `y+7` dying in `$v0` after the two `sh`s.

Pinning `self` in `$s3` and the OT index in `$s0` via `register ... asm("s3")`
/ `asm("s0")` may still be required so one-step `x = arg1 + field_20` does
not swap `$s2`/`$s3` with the saved `arg0`.

## GTE outer product (SV) + destroy head base across a call

`Gfx_OrthonormalBasis` builds a normal matrix from two SVECTORs via GTE outer product
on the scratch arena, then transposes into the output. Matching pieces:

**1. Real `op12` opcode.** `gte_op12()` from `inline_c.h` is a DMPSX placeholder.
Use the real COP2 word (same pattern as `gte_rtv0sf0`):

```c
#define gte_op12_real() __asm__ volatile("nop; nop; .word 0x4B78000C")
/* then: gte_ldopv1SV(v0); gte_ldopv2SV(v1); gte_op12_real(); gte_stsv(out); */
```

Load/store helpers `gte_ldopv1SV` / `gte_ldopv2SV` / `gte_stsv` match as-is.

**2. Overwrite the head pointer so `mat` cannot be recomputed after a call.**
Allocate `mat = head - 0x20`, then later `head = head - 0x1A` for the second
temp SVECTOR. If the original `head` stays live, GCC rebuilds `mat` as
`head - 0x20` after `MatrixNormal_2` (`lhu t4, -0x20(sN)` plus an extra s-reg
for the head). Updating `head` in place destroys that base and keeps `mat` in
`$s0` across the call:

```c
mat      = (MATRIX*)(head - 0x20);
/* … */
head     = head - 0x1A;   /* reuses v1; original head is gone */
*scratch = mat;
gte_ldopv1SV(head);
/* … */
MatrixNormal_2(mat, mat);
t4 = mat->m[0][0];        /* lhu t4, 0(s0) — not -0x20(head) */
```

**3. Unaligned 8-byte arg copy at `head - 0x1A`.** That offset is only
halfword-aligned, so assign through `GBytes8` (already in `game.h`) for
`lwl`/`lwr`/`swl`/`swr`. Halfword fields of the other SVECTOR use a `u16 tmp`
so loads stay `lhu`.

**4. Pins + `volatile` dest for prologue and free.** `register void** scratch
asm("s1")`, `register u8* head asm("v1")`, `register SVECTOR* sv1 asm("a0")`
match the target's three live pointers. Writing the transpose into
`volatile MATRIX* dest` forces all stores before `*scratch += 0x20`, which
needs the load-delay `nop` after `lw` of the head.

## Volatile field stores keep zero-init order before a global load

When the target zeros several struct fields then loads a BSS flag:

```
sb   zero, field_A(a0)
sh   zero, field_14(a0)
sb   zero, field_16(a0)
sw   zero, field_C(a0)
lb   v0, %lo(D_flag)(v0)   /* lui %hi was in prior bnez delay */
nop
beqz v0, ...
```

plain stores get reordered around the load — halfword/word zeros slip into the
`lb` load-delay and the `beqz` delay slot (~96%). A full `asm("" ::: "memory")`
barrier before the load restores store order but also delays the `lui %hi`,
leaving a `nop` in the `bnez` delay (~98%).

Fix: mark only the stores that were being delayed as volatile:

```c
arg0->field_A = 0;
*(volatile s16*)&arg0->field_14 = 0;
arg0->field_16 = 0;
*(volatile s32*)&arg0->field_C = 0;
if (D_flag != 0) {
    /* ... */
}
```

Volatile stores cannot move past the subsequent non-volatile load, so order
matches, while the `lui %hi(D_flag)` still fills the earlier branch delay.
`Ui_InitList` is the pure example (UiList tail zero-init + `D_80072313`).

Same idea when the target wants `lui %hi(flag)` *between* two groups of
struct stores (not just in a branch delay):

```
sw   v0, field_4(s0)     /* li 2; sw */
lui  v0, %hi(D_flag)     /* scheduled here */
sw   s2, field_8(s0)
sw   zero, field_0(s0)
lb   v0, %lo(D_flag)(v0)
nop
beqz v0, ...
```

A full `asm("" ::: "memory")` before the load restores store order but parks
`lui` with the `lb` (~99%). Marking *every* store that must precede the load
as volatile (including earlier ones like `field_14` / `field_4`) lets the
scheduler place `lui` after the last store that still uses `$v0` for a
constant, while the later zero/flag stores fill the gap before `lb`. Leaving
any of those stores non-volatile lets it slip into a delay slot and reorders
the rest. `func_8009389C` (title init) is the pure example.

## RECT field store order changes LoadImage arg scheduling

When preparing a stack `RECT` then calling `LoadImage(&rect, global_ptr)`, the
order of the `rect.x` / `rect.w` stores after `rect.y` changes whether cc1 emits:

```
lui  v0, %hi(global_ptr)
addiu a0, sp, rect
lw   a1, %lo(global_ptr)(v0)
```

or the swapped `addiu a0` / `lui` order (99.4% near-miss that never matches).

```c
rect.y = y;
rect.x = 0;       /* before w */
rect.w = 0x140;
rect.h = 0xF0 - field;
LoadImage(&rect, D4CB64_ImgBuffers);
```

`rect.w` then `rect.x` produces `addiu` first. `Display_LoadImageStrips` is the pure
example — only that swap separated 99.4% from 100%.

## GTE LZC: compute store address after the latency nops

`gte_Lzc(val, ptr)` expands to `mtc2; nop; nop; swc2`. The ROM often folds the
destination address math into the GTE latency window instead:

```
lw    t1, 0(a0)
nop
mtc2  t1, $30
nop
nop
addiu v0, a1, -8   /* address formed here */
swc2  $31, 0(v0)
```

Pre-computing `p = head - 8` before `gte_ldlzc` schedules the `addiu` too early.
Split the macro and form the pointer between the nops and the store:

```c
gte_ldlzc(vec->vx);
gte_nop();
gte_nop();
p_min = (s32*)(head - 8);
gte_stlzc(p_min);
```

`Gfx_NormalizeLightDir` is the pure example (three LZC passes over a scratch VECTOR).

## Empty asm barriers: load order + dual shift registers

When the target does:

```
lw   v0, 4(a2)      /* load A first */
lw   a0, 0x10(a2)   /* then shift amount */
lw   v1, 8(a2)
move a1, a0         /* copy shift for second srav */
srav v0, v0, a0
srav v1, v1, a1
```

two GCC 2.8.1 habits fight the match:

1. It reorders independent loads (shift before A).
2. CSE collapses the second shift amount into the same register, dropping `move`.

Pin the temps with `register … asm("…")`, then insert empty volatile asm to
fix load order and keep the copy live:

```c
register s32 t_vy asm("v0");
register s32 t_sh asm("a0");
register s32 t_vz asm("v1");
register s32 t_sh2 asm("a1");

t_vy = block->vy;
__asm__ volatile("" :: "r"(t_vy));   /* force vy load first */
t_sh = block->lzc_min;
t_vz = block->vz;
t_sh2 = t_sh;
__asm__ volatile("" : "+r"(t_sh2));  /* keep move a1,a0; block CSE */
block->vy = t_vy >> t_sh;
block->vz = t_vz >> t_sh2;
```

Without the first barrier, `lw a0,0x10` wins the schedule. Without the second,
both `srav` reuse `a0`. `Gfx_NormalizeLightDir` is the pure example.

## Keep `%hi(global)` live for post-loop `lhu %lo` loads

When a function forms `&global` before a loop, uses a walking pointer inside the
loop, then loads `global.field` after the loop, the target often keeps
`%hi(global)` in a register (`$a3`) across the loop:

```
lui   a3, %hi(G)
addiu t0, a3, %lo(G)   /* base pointer; a3 still holds hi */
move  a0, t0           /* walking copy */
/* loop uses a0 only */
lhu   v1, %lo(G)(a3)   /* post-loop field load via kept hi */
```

GCC 2.8.1 with `-msplit-addresses` will *not* CSE the hi across a multi-iteration
loop when the full address is assigned to a hard-pinned register
(`register … asm("t0")` forces `lui t0; addiu t0,t0`). Without the pin, the full
address lands in `$a3` and later field loads re-`lui`.

Fix: form the address with non-volatile asm so `$a3` holds hi and `$t0` holds
the full pointer, then load fields via the same hi:

```c
register FsWorkEntry* base asm("t0");
register u32 ace_hi asm("a3");

__asm__(
    "lui %0, %%hi(D5B498_8006ACE8)\n\t"
    "addiu %1, %0, %%lo(D5B498_8006ACE8)"
    : "=&r"(ace_hi), "=r"(base));
/* … loop on a walking copy of base … */
__asm__("lhu %0, %%lo(D5B498_8006ACE8)(%1)" : "=r"(t) : "r"(ace_hi));
```

Also: `s8_global * 64` (or `(s8)u8_global * 64`) emits `lb; sll 6`, while
`s8_global << 6` emits `lbu; sll 24; sra 18`. Prefer multiply for signed-byte
scale factors. `Fs_CopyWorkEntries` is the pure example.

## Fade step: non-volatile `lh` + volatile `lhu` + dual temps

`D_8006ACB4` is `volatile s16`. Plain `D_8006ACB4 < 0x101` emits
`lhu` + `sll 16` / `sra 16` / `slti`, not `lh`. To get target `lh` for the
compare and `lhu` for the add:

```c
s16 cur  = *(s16*)&D_8006ACB4; /* lh */
u16 next = D_8006ACB4;         /* lhu (volatile) */
if (cur < 0x101) {
    D_8006ACB4 = next + arg0;
    ret = 0;
} else {
    /* ClearImage both buffers; field_100 = 0 */
    ret = 1;
}
return ret;
```

Preloading both temps before the `if` schedules `sw` (finish `addPrim`) /
`lhu` / `beqz` / delay-slot `addu` correctly. Early returns on both arms invert
to `bnez` with the long path as fall-through.

## Shared `ret` + `register … asm("v0")` for dual-return fade

With the preload pattern above, a shared `s32 ret` keeps `beqz` polarity but
parks the return in `$v1` (`move v1,zero` in the `j` delay slot;
`li v1,1` + `move v0,v1` after `lw ra`). Pin the result:

```c
register s32 ret asm("v0");
```

so the delay slot is `move v0,zero` and the done path is `li v0,1` /
`lui v1,%hi(Display_State)` / `sb zero,field_100`.

## Pin width constant to `$t7` when `arg0` should land in `$t6`

Fullscreen fade helpers hoist `0x140` / `0xF0` into temps used for both the
TILE and the stack `RECT`. If GCC assigns `arg0` → `$t7` and `0x140` → `$t6`
(only mismatch left at ~99.7%), pin the width:

```c
register s32 w asm("t7");
w = 0x140;
p->w = w;
/* … */
rect.w = w;
```

That frees `$t6` for the early `move t6,a0`. `Fade_StepIn` is the pure
example (fade-up sibling of `Fade_StepOut`).

## `do { x = (s32)SomeFunc; } while (0)` delays the following store

When materializing a function pointer into `$v0` (`lui`/`addiu`) and then
storing it to a stack struct field, GCC often fuses the store immediately after
`addiu`, leaving no room for independent loads that the target schedules into
that gap (`lbu` of a flag, `lui` of another global's hi half).

Wrapping only the address materialization in a no-op loop is a scheduling
barrier:

```c
do {
    temp = (s32)func_80059EE0;
} while (0);
rem = p->unknown_0[2];   /* lbu can now sit between addiu and sw */
entry.field_8 = temp;
```

Without the wrapper the store wins the schedule; with it, the compiler emits
the independent loads first. `func_80058748` is the pure example (else-arm
callback install next to `CdStream_Continue`).

## Reserve `$a0` with `register … asm("a0")` so address-hi keeps `$a2`

When `$a1` is pinned (e.g. a live dividend across two `div`s), GCC often
recolors the split-address hi of a global from `$a2` to `$a0`. That breaks
later `%lo(sym)(a2)` accesses and steals `$a0` from the rem path's unsigned
halfword.

Pin a rem temporary to `$a0` for the whole function even if it is only written
on one arm:

```c
register s32 rem_tmp asm("a0");
register s32 field18 asm("a1");
/* … */
rem_tmp = (u16)p->field_40 - (field18 % p->field_40) + 1;
```

The hard pin keeps `$a0` reserved so the global's hi stays in `$a2`. On the
else arm, reassign `rem_tmp = (s32)&entry` just before the call so `&entry`
does not hoist to the top of the block. Needs `--expand-div` when the target
has the full trap sequence (`cdaudio.c` / `func_80058748`).

## Late `register … asm("sN")` assignment for prologue save order

When several callee-saved registers must be initialized from arguments early,
GCC 2.8.1 often saves/moves them in register-number order (e.g. `$s3` before
`$s4`) even if the C assignments are written the other way. Assigning the
higher-numbered register *late* (after the arg8 / a0–a1 setup that clobbers
argument registers) can force its prologue `sw`/`move` pair to the *front* of
the function, matching a target that leads with that register:

```c
register UiObject* obj asm("s4");
register s32 x asm("s3");
/* … */
x = arg1;          /* and y, result, rem, a1=arg8, a0=arg3 … */
/* bit-ops / optional call that use a0/a1 */
obj = arg0;        /* late assign → prologue still does sw s4; move s4,a0 first */
p = sp50;
```

`Text_DrawMultiLineScroll` needed `$s4` (obj) saved before `$s3` (x); early `obj = arg0`
kept putting `$s3` first, while the late assign matched the target prologue.

## Volatile flags: force reload + delay-slot `lui`

Shared flag words (e.g. `D_8005EC80`) that the target reloads from a kept
`%hi` base in `$a0` will CSE into a single load if the global is non-volatile:
the value stays in a register, the second access becomes `andi` of that reg,
and the address never lands in `$a0`. Mark the flag `volatile` so each access
reloads; GCC then keeps `%hi(flag)` in `$a0` across the early checks.

Related scheduling: when the same constant `1` is needed both as a shift
amount for `flags |= 1 << arg` and as a later live value (`setlen`, compares,
stores), write the shift with a literal first and assign the named temp after:

```c
D_8005EC80 |= 1 << arg0; /* volatile load address can fill prior bnez delay */
one = 1;                 /* li s2,1 then CSE into the shift as sllv …,s2 */
tile = &D_8006EC18;
/* … setlen(dr, one); … if (arg0 == one) … */
```

`one = 1; flags |= one << arg0` puts `li s2,1` in the branch delay instead of
the `lui` the target wants. `GameMain_ShowLoading` is the pure example.

## `(s16)` cast on `u16` fields that the target loads with `lh`

A field typed `u16` in the header may still be loaded with `lh` (signed) in
the original code. Writing `field != 0` emits `lhu`; cast to force the signed
load:

```c
if ((s16)CdCmd_Queue.field_244 != 0 && !(flags & 8)) { … }
```

## OT addPrim offsets: elements, not bytes

`D_800710A0` is `u_long*`. Asm immediates on loads/stores are *bytes*, so a
target `lw v0, -0x40(a2)` is one OT entry stride of `0x10` words:

```c
/* WRONG — scales by sizeof(u_long) → -0x100 bytes */
addPrim(D_800710A0 - 0x40, p);

/* RIGHT — matches lw/sw -0x40(reg) */
addPrim(D_800710A0 - 0x10, p);
```

Scratch-object matching can still report 100% when only the I-type immediate
differs if the scoreer is too loose; always confirm with
`./tools/build-and-verify.sh` (`build/USA/out/SLUS_010.42: OK`).
`Prim_DrawLoadingSprt` is the pure example (also `1C034.c` / `bootload.c` use `-0x10`).

## Dual-scope `RECT*` for early `$a1` vs late `$s1` (same address)

When one stack RECT is both (1) the destination of a switch that must put
`&sp10` in `$a1` early (delay-slot of the first branch, copy via `sh …,0(a1)`,
and the `jal` second arg with no extra `move`) and (2) a nullable pointer that
must live in `$s1` for a later `if (p != NULL)` after more calls, a **single**
live `RECT* r = &sp10` across the whole function pins the address in `$s1` from
the start (extra `move a1,s1`, stores to `0(s1)`, wrong prologue order).

Split into two scopes so the early binding dies before the late one is born:

```c
{
    RECT* arg1 = &sp10;
    switch (mode) {
    case 1:  func_A(obj, arg1, …); goto after;
    case 3:
    case 4:  func_A(obj, arg1, …); goto after;
    }
    arg1->x = …; /* default copy — uses a1, not s1 */
}
after:
{
    RECT* arg1 = &sp10; /* separate lifetime → s1, scheduled into bne delay */
    func_B(obj, &obj->rect, &sp20);
    /* layout math … */
    if (arg1 != NULL) {
        func_B(obj, arg1, &sp18);
    }
    func_C(obj, &sp10, &sp18, 0);
}
```

Do **not** also name a long-lived `RECT* arg2 = &sp18`: that steals `$s2` and
changes the `bne` delay from `addiu s1,sp,0x10` to `addiu s2,sp,0x18`. Pass
`&sp18` directly so each use is a fresh `addiu a2,sp,0x18`.

`Ui_LayoutAndDraw` is the pure example (same shape as inlined `Ui_ComputeAnimRect` +
`Ui_InsetLayout` + `Ui_DrawPanel`).

## Ring-buffer queue drain: non-volatile entry + split index advances

`CdReady_Poll` (and the sibling `AsyncCb_Poll`) process one slot of a 4-entry
callback ring. Two matching details that look like style nits but are required:

1. **Non-volatile entry pointer.** The queue object itself is `volatile`
   (`CdReady_Queue`), but the current slot must be taken as a plain
   `CdReadyEntry*`:

```c
entry = (CdReadyEntry*)&CdReady_Queue.entries[(s8)p->field_2];
entry->field_0 &= ~1;
entry->field_0 &= ~4;
```

   With a `volatile CdReadyEntry*`, each `&=` becomes its own load/store (or
   a temp lands in the wrong register). Non-volatile gives the target's
   `lw` / `li -2` / `and` / `li -5` / `and` / `sw` chain in `$v0`/`$v1`.

2. **Do not share the "advance index" block across arms.** The bit-0 path
   advances through the base already in `$s1` (`p->field_2 = …`). The bit-2
   path reloads the global (`lui`/`addiu` of `CdReady_Queue`) and writes
   `CdReady_Queue.field_2`. Sharing one advance via `goto` from both arms merges
   them onto `$s1` and shrinks the function. Duplicate the increment/wrap
   literally, once via `p` and once via the global name.

`AsyncCb_Poll` is the pure template for control flow; `CdReady_Poll` adds the
`field_0` lock check and the no-arg `field_C` callback.

## Sign-extend loop counter via `next` in `$v0` + empty asm barrier

When the target does:

```
addiu  v0, s2, 1
move   s2, v0
sll    v0, v0, 24
sra    v0, v0, 24
slti   v0, v0, N
bnez   v0, loop
 sll   v0, s2, 24   /* delay: prep next (s8)i */
```

a plain `do { …; i++; } while ((s8)i < N)` either strength-reduces the counter
to `lui sN, 0xXX00` form or emits `sll v0, s2` after the move (CSE substitutes
`i` for the equal `next`).

Match with a named next temp pinned to `$v0`, `i` pinned to `$s2`, and an
empty asm barrier so CSE cannot fold `next` into `i` for the cast:

```c
register s32 i asm("s2");
register s32 next asm("v0");

i = 0x16;
do {
    voice = (s8)i;
    /* … */
    next = i + 1;
    i = next;
    asm("" : "+r"(next));
} while ((s8)next < 0x18);
```

The same barrier after `acc = temp` forces `sll v0, v0` for `(s8)temp` when
`temp` still shares `$v0` with the `addu` result:

```c
temp = acc + func(voice);
acc = temp;
asm("" : "+r"(temp));
status = (s8)temp;
```

`func_800567E4` is the pure example (voice poll over slots 0x16..0x17).

## Cast a `u8`-returning helper through an `s32` function pointer

When the target uses a `u8` return with a bare `addu`/`move` and no `andi 0xff`,
but the real prototype is `u8 foo(...)`, a direct call inserts the zero-extend
`andi`. Call through an `s32` function-pointer cast so the front end treats the
return as SImode:

```c
temp = acc + ((s32 (*)(s32))Spu_GetVoiceStatus)(voice);
```

Safe only when the callee already returns a clean low byte (e.g. via `lbu`).
Do not change the shared `u8` declaration — other matched callers may depend on
the `andi`. `func_800567E4` needs this for `Spu_GetVoiceStatus`.

## Jump-table mult: load order vs `mult` operand order

When a `u16 * u16` product shares one operand with a later scaled use
(e.g. `h * 0x30` after `w * h`), the plain expression forms are coupled:

- `D_w * D_h` → correct `mult a2, a1` (w in `$a2`, h in `$a1`) but loads h first
- `D_h * D_w` → perfect load interleave with `D_8006AC48` setup, but `mult a1, a2`

Target often wants **both** the load interleave of the second form **and**
`mult a2, a1`. Force h live early without putting it on the left of the mult:

```c
u16 h = D_h;
s32 stride = h * 0x30;   /* materialises hi(h) first, keeps h in $a1 */
temp = D_w * h;          /* mult a2, a1; lhu w before lhu h */
/* … buffer setup using mult delay … */
ptr = base + stride;     /* reuses $a1 for the * 0x30 shift pattern */
```

`Mdec_SetupBuffers` case 0 is the pure example (MDEC buffer layout).

## Stack pad between packed s32 and RECT (0x10 / 0x18 locals)

When the target has an s32 at `sp+0x10` (often a packed return reloaded with
`lhu` of both halves) and a `RECT` at `sp+0x18`, separate locals reverse the
order or pack tightly. Force the gap with an explicit stack struct:

```c
struct {
    union {
        s32 as32;
        struct { u16 w; u16 h; } hw;
    } dims;
    s32  pad;   /* unused; keeps RECT at +8 */
    RECT rect;
} sp;

sp.dims.as32 = func_that_returns_wh();
/* … */
func(..., sp.dims.hw.w + t, sp.dims.hw.h + u);
```

The union gives `lhu` of each half; a plain `s32` with `((u16*)&dims)[i]` often
changes call-arg scheduling. `Ui_SizeFromText` is the pure example.

## Temps for `arg + K` before `mem + (arg + K)` call args

Target sequence for call setup:

```
lhu  v0, mem
addiu a1, s1, K
addu  a1, v0, a1
```

A direct `mem + (arg + K)` reassociates to `(mem + K) + arg` (`addiu` on the
loaded halfword). Assign the s-reg addend first:

```c
t = arg2 + 5;
u = arg3 + 1;
func(arg0, dims.hw.w + t, dims.hw.h + u);
```

`Ui_SizeFromText` is the pure example (text size padding into `Ui_UpdateLayoutSize`).

## Force `bne` (not `xor`/`sltiu`) for u16 equality into a u16 temp

When materializing queue-idle style checks:

```c
if (field_4c != 0) {
    ret = 0;
} else if (writeIdx != readIdx) {
    ret = 0;
} else {
    ret = 1;
}
if (ret == 0) return 0;
```

`-O2` often collapses the equality arm into `xor`/`sltiu`. The target wants:

```
bnez field_4c, check
 move v0, zero
lhu  v1, writeIdx
lhu  v0, readIdx
bne  v1, v0, check
 move v0, zero
li   v0, 1
check:
andi v0, v0, 0xffff
beqz v0, ret0
```

Fix: assign the success constant through a wider temporary that is also the
function's default return:

```c
unsigned int new_var;
...
new_var = 1;
ret = new_var;
...
return new_var; /* default return-1 paths */
```

`CdCmd_EnqueueFollowUp` is the pure example. Plain `ret = 1` stuck at xor/sltiu.

## `do{}while(0)` + `register … asm` for shared enqueue register map

A shared enqueue body that needs:

```
lui  a1, %hi(Q) / addiu a1, a1, %lo(Q)   /* not lui v0; addiu a1, v0 */
li   v0, cmd
lhu  v1, writeIdx(a1)
...
addu v1, v1, a1   /* index + base, not base + index */
```

often needs both:

1. Wrap the whole case-0 path (including the shared label) in `do { ... } while (0);`
   so the delay-slot `li v0, cmd` and shared body pick the right hard regs.
2. Pin the key locals when the natural coloring still flips:
   `register CdCmdQueue* q asm("a1");`
   `register s32 cmd asm("v0");`
   `register CdCmdEntry* entry asm("v1");`
   `register u8* paramA asm("a0");`
3. For `addu dst, index, base` (index-first): compute
   `t = idx; t = t * 8; t = t + (u32)base; entry = (CdCmdEntry*)t;`
   rather than `&base->entries[idx]`.

Also: use `(&Global)->field` (not a local `q = &Global`) on a later path when
the target reloads the global into `$a0` while `$a0` already holds another
address in a delay slot.

`CdCmd_EnqueueFollowUp` needs all of the above together.

## Triple address-of for same-page BSS `lui` order

When the target ends with three same-`%hi` BSS symbols in a fixed lui order:

```
lui  a0, %hi(D_dst)
lui  v1, %hi(D_a)
lui  v0, %hi(D_b)
lw   v0, %lo(D_b)(v0)
lw   v1, %lo(D_a)(v1)
addiu v0, v0, -K
addu v1, v1, v0
jr   ra
 sw  v1, %lo(D_dst)(a0)
```

a direct `D_dst = D_a + (D_b - K)` or `p = &D_dst; *p = D_a + (D_b - K)`
almost always permutes the middle two luis (or folds the `-K` onto the wrong
load). Taking addresses of all three first forces the target order:

```c
int*    pDst;
size_t* pA;
size_t* pB;
size_t  temp;

pDst = &D_dst;
pA   = &D_a;
pB   = &D_b;
temp = *pB - K;
*pDst = *pA + temp;
```

`Mem_ConfigureAuxHeap` is the pure example (`D_80068F98 = D_80068F88 + (D_80068F90 - 0xA)`).

## Zero-tail loop: single index, `i & 0xFF`, increment at bottom

A 10-byte zero of `base[size - 1 - i]` that needs:

```
andi  a0, a1, 0xff
addiu a1, a1, 1
lw    v0, size
lw    v1, base
subu  v0, v0, a0
addu  v0, v0, v1
sb    zero, -1(v0)
andi  v0, a1, 0xff
sltiu v0, v0, 0xa
bnez  v0, loop
 andi a0, a1, 0xff
```

matches with a single `s32 i` and the mask at the use site — **not** a separate
`j = i & 0xFF` local (that steals `$a1` for `j` and puts the counter in `$a2`):

```c
i = 0;
do {
    *(u8*)((size - (i & 0xFF)) + base - 1) = 0;
    i += 1;
} while ((u32)(i & 0xFF) < 0xAU);
```

`Mem_ConfigureAuxHeap` is the pure example.

## McWork direntry walk from McWork base (size@0x48, head@0x50)

When the target walks directory entries with:

```
move  t0, s1          /* McWork* */
lw    v0, 0x48(t0)    /* DIRENTRY.size */
lw    a0, 0x50(t0)    /* DIRENTRY.head */
...
addiu t0, t0, 0x28
```

do **not** start a `struct DIRENTRY*` at `field_30` (that emits `lw …,0x18/0x20`).
Use an overlay whose fields sit at the McWork-relative offsets and advance by
`sizeof(struct DIRENTRY)`:

```c
typedef struct {
    u8  _pad[0x48];
    s32 size;
    s32 _pad4C;
    s32 head;
} McDirWalk;

register McDirWalk* walk asm("t0");
walk = (McDirWalk*)arg1;
size = walk->size;
head = walk->head;
walk = (McDirWalk*)((u8*)walk + sizeof(struct DIRENTRY));
```

## field_A24 fill: `p = (u8*)work + i; p[0xA24] = val`

Target init of the block map wants:

```
li    v1, -1
li    a3, 0xe
addu  v0, s1, a3
sb    v1, 0xa24(v0)
addiu a3, a3, -1
bgez  a3, loop
 addiu v0, v0, -1
```

`&work->field_A24[i]` folds the base (`addiu v0, s1, 0xa32; sb v1, 0(v0)`). Use:

```c
register s32 i asm("a3");
register s32 val asm("v1");
register u8* p asm("v0");
val = -1;
i = 0xE;
p = (u8*)arg1 + i;
do {
    p[0xA24] = val;
    i -= 1;
    p -= 1;
} while (i >= 0);
```

Keep `val` as `s32 -1` (not `0xFF`) so the `li` is `addiu …, -1`. Reuse `i` as
the later `MemCardGetDirentry(…, max=0xF)` so `$a3` is reloaded right after the
loop.

## Ceil blocks + head/64: early `headAdj = head` between size bias and sra

Target schedules:

```
bgez  v0, pos          /* size */
 move v1, v0
addiu v1, v0, 0x1fff
move  a2, a0           /* headAdj = head — before sra/andi */
sra   v1, v1, 0xd
andi  v0, v0, 0x1fff
sltu  v0, zero, v0
bgez  a0, head_pos
 addu a1, v1, v0       /* blocks */
addiu a2, a0, 0x3f
sra   a0, a2, 0x6
addiu a0, a0, -1       /* start = head/64 - 1 */
```

Pin size/head/temps and assign `headAdj = head` immediately after the size
bias, then `sizeAdj >>= 13` before the `andi`:

```c
register s32 size asm("v0");
register s32 head asm("a0");
register s32 sizeAdj asm("v1");
register s32 headAdj asm("a2");
register s32 blocks asm("a1");
register s32 start asm("a0");

sizeAdj = size;
if (size < 0) sizeAdj = size + 0x1FFF;
headAdj = head;
sizeAdj = sizeAdj >> 13;
blocks = sizeAdj + ((size & 0x1FFF) != 0);
if (head < 0) headAdj = head + 0x3F;
start = (headAdj >> 6) - 1;
```

## Loop epilogue: `new28c`/`n` temps put `sw field_28C` in the branch delay

Target end-of-iteration:

```
addiu t0, t0, 0x28
addiu a3, a3, 1
lw    v0, 0x28c(s1)
lw    v1, 0x288(s1)
addu  v0, v0, a1
slt   v1, a3, v1
bnez  v1, loop
 sw   v0, 0x28c(s1)
```

A plain `field_28C += blocks; } while (i < field_288)` stores early and leaves
`walk++` in the delay. Force the store last via temps pinned to `$v0`/`$v1`:

```c
register s32 new28c asm("v0");
register s32 n asm("v1");
walk = (McDirWalk*)((u8*)walk + sizeof(struct DIRENTRY));
i += 1;
new28c = arg1->field_28C + blocks;
n = arg1->field_288;
arg1->field_28C = new28c;
} while (i < n);
```

`Mc_StateScanDirFlags` is the pure example (memcard free-block map).

## Reuse `arg2` as `/3` quotient + explicit `%2` for SPRT UV frame index

When animating an 8x8 sprite UV from a frame counter `n = (u32)field >> 3`
with `u = (n % 3) * 8 - base_u` and `v = ((n / 3) % 2) * 8 + base_v`, the
target often:

1. Keeps `arg2` live through the y-position add, then **reassigns**
   `arg2 = n / 3` so the quotient lands in `$a2` (`subu a2, hi, sign`).
2. Copies the quotient to `$v1` (`move v1, a2`) before overwriting `$a2`
   with the remainder (`arg2 = n - row * 3`).
3. Expands signed `% 2` as two steps so `$a0` gets the result while `$v0`
   holds the bias intermediate:
   ```
   srl  v0, v1, 31
   addu v0, v1, v0
   sra  a0, v0, 1      /* half = row / 2 */
   sll  v0, a0, 1
   subu a0, v1, v0     /* half = row - half * 2 */
   ```
4. Scales/stores `u0` through `$v0` *before* scaling `v0` from `$a0`.

A plain `row = n / 3; p->u0 = (n % 3) * 8 - K; p->v0 = (row % 2) * 8 + B`
stalls around ~91%: quotient in `$v1`, early `u0` store, and `%2` folded
into the `v0` scale chain.

```c
register s32 row asm("v1");
register s32 t asm("v0");
s32 half;

/* ... y uses arg2, then: */
arg2 = n / 3;
row  = arg2;           /* move v1, a2 */
arg2 = n - row * 3;    /* rem back into a2 */
half = row / 2;
half = row - half * 2; /* %2 with v0 scratch, half in a0 (unpinned) */
asm("" : "+r"(half), "+r"(arg2)); /* keep both live before stores */
t     = arg2 * 8 - 0x18;
p->u0 = t;
t     = half * 8 + 0x30;
p->v0 = t;
```

Pinning `half` to `$a0` coalesces the bias into `$a0` (`addu a0,v1,v0`
instead of `addu v0,v1,v0`). Leaving `half` unpinned after the explicit
`/2` + subtract form yields the retail chain. `Ui_DrawCursor` is the pure
example (SPRT_8 cursor + DR_TPAGE, OT index 4).

## Scoped `register asm` pins for multi-section functions

When a large function has independent phases (e.g. parent-unlink, then a
later free path) that both want the same hard register (`$v1`), a function-
scope pin collides: the early pin keeps the register "live" and the later
phase spills to `$s0`/`$s2`.

Symptom: early section matches only with a pin; applying that pin shifts the
late section off `$v1` (and often flips `bnez`/`beqz` shapes around
`Task_ActiveList` save/restore).

Fix: scope the pin to a compound block that ends before the later phase, and
re-pin (or reuse) `$v1` in a second block for the late phase:

```c
{
    register Task* p asm("v1");
    register Task* n asm("a0");
    p = arg0->field_8;
    /* ... parent detach using p/n ... */
}

/* ... middle of function ... */

{
    register s32 t asm("v1");
    t = arg0->field_28;
    if (t == 1) { /* immediate free cases */ }
}
```

`Task_Kill` is the pure example: parent detach needs `$v1`/`$a0`, and the
immediate-free path reuses `$v1` for the type byte and `%hi(Task_ActiveList)`.

## `if (ptr == NULL)` vs `!= NULL` for `bnez` delay-slot stores

When the target does:

```
bnez  a0, has_next
 sw    v0, ActiveList(v1)   /* delay: always runs */
j     cont
 addiu v0, v0, 4            /* null path */
has_next:
addiu v0, a0, 4
```

writing the C test as `if (next != NULL) { non-null } else { null }` often
emits `beqz` with the arms swapped. Flipping to `if (next == NULL) { null }
else { non-null }` produces the retail `bnez` layout with the store in the
branch delay slot. Same logical code; only the branch polarity matches.

## `s8` flag vs `s32` call arg: prevent CSE of `li a0,K`

When the target keeps a small constant K in `$s1` across a call for byte
stores (`sb s1, field`) *and* also emits a fresh `li a0,K` for the call
argument (not `move a0,s1`), hold K in an `s8` (QImode) local:

```c
s8 one;

one = 1;
p->field_4C = one;     /* sb s1, ... */
func_X(1);             /* li a0, 1 — SImode cannot CSE from QImode s1 */
q->field_10b = one;    /* sb s1, ... after the call */
```

An `s32 one = 1` produces `move a0,s1` instead. Pair with
`register s32 saved asm("s1")` on a non-overlapping path that needs the same
register for a saved global (e.g. `lb s1, D_xxx` / `sb s1, D_xxx`) so
`CdCmdQueue* p` can claim `$s0`.

`GameFlow_StateByField34` is the pure example.

## Nested block for clear-loop regalloc (`a0`=ptr, `v1`=i)

A function-scope `u8* ptr; u32 i; for (...)` clear of `GameSession` often
allocates `i` to `$a0` and the pointer to `$v1` when other locals are live.
Wrapping the clear in a nested block with its own `clearPtr`/`clearI` restores
the `Game_ClearSession` pattern (`a0`=ptr, `v1`=counter) used by simple clears:

```c
{
    u8* clearPtr;
    u32 clearI;

    clearPtr = (u8*)Game_Session;
    for (clearI = 0; clearI < sizeof(GameSession); clearI++) {
        *clearPtr++ = 0;
    }
}
```

Use function-scope `ptr`/`i` when the target wants the inverse allocation
(e.g. after `a0` was already zeroed as the counter, as in `Game_ResetSessionAndBuffers`).

## Variable-bound search: plain `for` emits `beqz len`, not outer `if`

When `i` is already 0, a search over a `u16` length:

```c
for (; (u16)i < Fs_FolderTableLen; i++) {
    if (key == table[i & 0xFFFF].id) {
        break;
    }
}
```

compiles to `lhu v1, len; beqz v1, done` then a bottom-tested loop — the
initial `0 < len` collapses to `beqz`. Wrapping the same loop in
`if (Fs_FolderTableLen != 0) { for (...) }` inserts an *extra* `sltu`/`beqz`
pair (or peels a `do`/`while` into a first-element special case). Prefer the
plain `for` when the target has a single `beqz` on the length.

`Fs_PrepareFolderLoad` is the pure example (folder-table lookup).

## `asm("")` after `i = 0` before an unrelated global store

After a clear loop leaves `i` non-zero, the target often does:

```
move  s0, zero          /* i = 0 */
lui   v0, %hi(D_flag)
andi  v1, a1, 0xff      /* start of a later expression */
sb    zero, %lo(D_flag)(v0)
```

Writing `i = 0; D_flag = 0; expr = (u8)arg1 * ...;` reorders to
`lui` then `move s0, zero`. An empty `asm("");` between the two stores pins
the `move` first (same barrier as other schedule pins in this file):

```c
i = 0;
asm("");
D_flag = 0;
folderId = ((u8)arg1 * 100) + (u8)arg2;
```

`Fs_PrepareFolderLoad` (`D_8006ADE2` after the `Stream_Slots` clear loop).

## Two-phase scratch alloc: unpinned load, then `register … asm("v1")` adjust

When the target does:

```
lw   t0, 0x10(a3)     /* src = arg->field_10  first */
lw   v1, 0(a0)        /* head = *G_SCRATCH_HEAD */
…                     /* lo-load of a global, etc. */
addiu v1, v1, -0x88   /* head stays in $v1 */
sw   v1, 0(a0)
…                     /* flag test with || */
move s0, v1           /* delay of bne  */
li   s1, 1
move s0, v1           /* set-flag path */
```

pinning both `scratch` to `$a0` and `head` to `$v1` *and* writing
`head = *scratch` schedules the `*scratch` load before the `field_10` load
(`lw v1` then `lw t0`) — register pressure on `$v1` wins the schedule.

Fix: load the head into an **unpinned** temporary first, then assign the
adjusted pointer into the pinned `v1` register:

```c
{
    register void**             scratch asm("a0");
    register ScratchModelBlock* head asm("v1");
    void*                       tmp;

    scratch  = (void**)G_SCRATCH_HEAD;
    src      = arg0->field_10;          /* lw t0 first */
    tmp      = *scratch;                /* unpinned load */
    stream   = src->field_20;
    hi       = *(u32*)&g->field_4;
    head     = (ScratchModelBlock*)((u8*)tmp - 0x88); /* addiu v1, … */
    hi      &= 0xFFFF0000;
    *scratch = head;
    if ((hi == 0x020F0000) || (hi == 0x02100000)) {
        flag = 1;
    }
    ws = head; /* dual move s0,v1 from the || codegen */
}
```

Split `hi = load; hi &= 0xFFFF0000` (do not pin `hi` to `$a1` while computing
the mask) so the target emits `lw a1,4(v0)` / `lui v0,0xffff` / `and a1,a1,v0`
rather than preloading the mask into `$a1`. Free the scratch pointer after the
block and rematerialize the restore with a bare
`*(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x88`.

`Tmd_ProcessStream` is the pure example.

## Dual `lw` + dual `andi 0xFF` for store + switch of the same expression

When the target loads `*arg0` twice into `$v0`/`$v1`, applies `srl`/`andi 0xFF`
to both, stores one byte to a global, then switches on the other:

```
lui  a0, %hi(D_flag)
lw   v0, 0(s1)
lw   v1, 0(s1)
srl  v0, v0, 4
andi v0, v0, 0xFF
srl  v1, v1, 4
andi v1, v1, 0xFF
addiu v1, v1, -1
sb   v0, %lo(D_flag)(a0)
sltiu v0, v1, N
```

Plain `D_flag = (*arg0 >> 4) & 0xFF; switch ((*arg0 >> 4) & 0xFF)` CSEs to one
load and drops the store-path `andi` (sb only needs the low byte). Two plain
temps also CSE.

Force the dual load with volatile reads, and pin both results so the store-path
`andi` stays live:

```c
register u32 a asm("v0");
register u32 b asm("v1");

a = *(volatile u32*)arg0;
b = *(volatile u32*)arg0;
a = (a >> 4) & 0xFF;
b = (b >> 4) & 0xFF;
D_flag = a;
switch (b) {
    /* ... */
}
```

Without the pins, GCC still dual-loads but elides `andi` before `sb`. Without
`volatile`, it collapses to one `lw` + `move`. `CdStream_InitDisc` (CD init state
machine, sibling of `Cd_InitStateMachine`) is the pure example — also needs
`CdStreamState.field_56` as `u16` for the case-7 retry counter.

## Entry pointer in `$a0` for load-then-store of a `u8` field

When the target does:

```
addu  a0, s0, v0     /* entry = base + idx*stride */
lbu   v1, 4(a0)      /* cmd = entry->cmd */
li    a1, 0x61
bne   v1, a1, else
...
sb    a1, 4(a0)      /* entry->cmd = 0x61 */
```

keep the entry pointer and compare the field *through* it. Loading the field
into a named local first recolors the pointer into `$v1` and the value into
`$a0`:

```c
/* wrong — pointer ends up in $v1, value in $a0 */
cmd = entry->cmd;
if (cmd == 0x61) { ... }
else if (cmd == 0x62) { entry->cmd = 0x61; }

/* right — pointer stays in $a0, value in $v1 */
if (entry->cmd == 0x61) { ... }
else if (entry->cmd == 0x62) { entry->cmd = 0x61; }
```

GCC CSEs the two `entry->cmd` loads into one `lbu` either way; only the
register assignment differs. `CdCmd_HandleStreamDecode` is the pure example (cmds 0x61 /
0x62 on `CdCmdEntry`).

## Dual `global = Mem_Malloc(...)` arms share one `jal` and pin `%hi` in `$s0`

When the target zeros a global, then either allocates a fixed size or looks up a
size and allocates, and reuses `$s0` for `%hi(global)` from the zero through
the post-malloc store:

```
lui   s0, %hi(D_xxx)
sw    zero, %lo(D_xxx)(s0)
...
jal   Mem_Malloc          /* shared site */
sw    v0, %lo(D_xxx)(s0)  /* reuses s0 — no second lui */
lui   v0, %hi(D_xxx)
lw    v0, %lo(D_xxx)(v0)  /* return reloads with a fresh lui */
```

Write **two** `global = Mem_Malloc(...)` statements in separate `if` / `else if`
arms rather than a `goto` shared tail or a `doAlloc` flag:

```c
/* Matches: two arms, one jal, lui s0 at zeroing */
D_xxx = NULL;
if (cond) {
    D_xxx = Mem_Malloc(0x4B000, 1);
} else if (setup() < 0) {
    return NULL;
} else {
    size = lookup();
    if (size != 0) {
        D_xxx = Mem_Malloc(size, 1);
    }
}
return D_xxx;
```

GCC 2.8.1 merges the two call sites into one `jal` (the fixed-size arm jumps
with `a0` already set). A single shared site via `goto do_malloc` or a flag
keeps the control flow but reloads `%hi` into `$v0`/`$v1` after the call
instead of pinning it in `$s0` from the zeroing store. `CdCmd_SetupMdecBuffers` /
`D_8006AC00` is the pure example.

## Preload switch-case locals to control delay-slot fill and `lui` order

When a case compares two globals then branches on a field of the second, and the
target puts an independent store in the `bnez` delay slot:

```
lbu   v0, field_11(g)
nop
bnez  v0, else
 sb   zero, field_4D(ed)   /* delay: always runs for both arms */
```

write the store *after* an explicit load of the branch condition into an `s32`
temp (not `u8` — that inserts `andi v0,v0,0xff` before the branch):

```c
f11 = g->field_11;          /* s32 */
ed->field_4D = 0;
if (f11 == 0) {
    arg0->field_2a = flag;  /* flag also s32 from earlier lbu */
    ...
}
```

Same case: to get `lui %hi(Display_State+0x118)` *before* `lui %hi(Stage_Ctx)`,
preload the display field into a temp before materialising the other global:

```c
disp = Display_State.field_118;
g    = Stage_Ctx;
if (disp == g->field_24) { ... }
```

`Display_TransitionTask` case 1 is the pure example.

## Scratch jtbl normalize accepts single-address names

`tools/claude-decomp-env/normalize_asm.py` (and `dist.py`) only rewrote
`jtbl_HEX_HEX`. This project's splat labels are `jtbl_80013EB0` (one hex).
Extend the pattern to `jtbl_[0-9A-Fa-f]+(?:_[0-9A-Fa-f]+)?` so scratch scoring
does not treat the target name vs `.rodata` as a real diff.

## Force `move reg, zero` after a proven-zero branch

When a variable is computed, branched on as zero, then re-used as a loop
counter that the target re-zeros with `move a0, zero`, plain `slotIdx = 0;` is
CSE'd away (the compiler already knows it is zero).

```c
slotIdx = 8 - p->field_24;
if (slotIdx == 0) {
    slotIdx = 0; /* eliminated — no move */
    do { slotIdx += 1; ... } while (slotIdx < limit);
}
```

Defeat CSE with an output-constrained empty asm that rewrites the register:

```c
if (slotIdx == 0) {
    asm volatile("" : "=r"(slotIdx) : "0"(0)); /* emits move a0, zero */
    do { slotIdx += 1; ... } while (slotIdx < limit);
}
```

`Mc_StateFinishWrite` needs this so the checksum loop counter stays in `$a0` with an
explicit zeroing instruction matching the target.

## `register asm` for memcpy arg load order

When the target loads `size` into `$a2` before `dest` into `$a0` from the same
struct base (then `sll a2, a2, 1` in the `jal` delay slot), unconstrained C
often loads dest first. Pin the registers:

```c
{
    register McBufferSlot* slots asm("v0");
    register s32           size asm("a2");
    register void*         dest asm("a0");

    slots = Mc_BufferSlots;
    size  = slots[idx].field_4;
    dest  = slots[idx].field_0;
    memcpy(dest, src, size << 1);
}
```

Scoping the constraints inside a compound statement keeps them from disturbing
register colouring on the sibling branch (e.g. a checksum loop that also wants
`$a2` for the running sum). Pin the table base to `$v0` when the target places
`lui %hi(table)` in the `bnez` delay slot and finishes with
`addiu v0, v0, %lo(table)` on the memcpy path.

`Mc_StateFinishWrite` is the pure example.

## Place absolute jtbl consts after the C function that owns the preceding slot

When a `.rodata` segment mixes compiler-generated jump tables with absolute
`const s32 jtbl_…[]` copies for still-asm neighbours, emission order follows
compile order through the TU: a switch's table is emitted while that function
is compiled, and later `const` data lands after it.

Symptom: after matching function A you remove its absolute `jtbl_A`, but the
still-asm neighbour's absolute `jtbl_B` (still declared earlier in the source)
shifts up into A's slot and the checksum fails even though A's body matches.

Fix: declare the remaining absolute table *after* the newly matched function in
the `.c` file so A's compiler table occupies the old absolute slot:

```c
/* pad after previous matched switch */
static const s32 s_jtbl_pad = 0;

s32 func_A(void) { switch (…) { … } }  /* jtbl lands here */

/* absolute copy for still-asm func_B */
const s32 jtbl_B[] = { 0x800xxxxx, … };

INCLUDE_ASM(…, func_B);
```

`CdAudio_DriveSeek` / `jtbl_80014204` (still-asm `CdAudio_DriveRead`) is the example.

## Early `hdr` load + `register asm` pins for multi-use sector pointer

A volatile buffer pointer loaded once at function entry (`hdr = (T*)D_xxx`) and
reused for field reads, with a second volatile reload of `D_xxx` for base+index
addressing, needs:

1. Read the switch discriminator *before* the buffer load so `lbu` /
   `lw D_xxx` interleave and the table index stays in `$a0`.
2. Pin `hdr` to `$a2`, the second base to `$v1`, and the final
   `counter += 1` temp to `$a0` when the shared epilogue uses that colouring.
3. Prefer `table[idx]` (array form) over `*(s32*)((s32)table + (idx << 2))` once
   the pins are in place — the array form colouring matches the target.

```c
phase = state->field_3;
hdr   = (SectorHdr*)D_80082750; /* fills lbu delay; pin hdr to a2 */
switch (phase) {
case 8:
    idx = hdr->field_3;
    val = D_80068B18[idx];
    ptr = D_80082750;           /* pin ptr to v1 */
    audio->field_8 = val;
    …
}
tmp = counter; tmp = tmp + 1; counter = tmp; /* pin tmp to a0 */
```

`CdAudio_DriveSeek` is the pure example.

## Flip comparison operators to control load order of slt operands

When the target does:

```
lw  v1, field_C(a0)
lw  v0, field_4(t0)
slt v0, v0, v1        /* field_4 < field_C */
```

writing `p->field_4 < arg0->field_C` often loads `field_4` first. The equivalent
`arg0->field_C > p->field_4` evaluates the left operand first and produces the
target load order while still emitting `slt v0, v0, v1`.

```c
/* wrong load order: field_4 then field_C */
if (arg0->field_2 == -1 || p->field_4 < arg0->field_C)

/* correct: field_C then field_4 */
if (arg0->field_2 == -1 || arg0->field_C > p->field_4)
```

Pulling `field_C` into a temporary *before* the `field_2 == -1` test is too
aggressive — it fills the `lb` delay slot and shortens the function. Keep the
compare inline and only flip the operator.

`SndVoice_ScanCandidates` is the pure example (also: stash `score = p->field_4` before
paired `sb`/`sw` so the target gets `lw; sb; sw` rather than `sb; lw; nop; sw`).

## Separate stream pointers so timeout shares `$v1` while case-2 keeps `$s0`

A CD state machine with a shared timeout tail that stores `field_8`/`field_9`
through `$v1`, while an earlier case (live across `CdSync`) holds the same
object in `$s0` for an *inlined* copy of those stores, will merge both into
one `$s0` sequence if they share a single C variable.

Use two pointers:

```c
volatile CdAudioCtl* stream; /* case 2: coloured $s0, inlines field_8/9 then goto error */
volatile CdAudioCtl* p;      /* case 5/8/9 + timeout: coloured $v1, shared tail */

case 2:
    stream = &CdAudio_Ctl;
    if (stream->field_0 < 0x259) goto sync;
    stream->field_8 = phase;
    stream->field_9 = 1;
    goto error;
/* ... */
timeout:
    p->field_8 = phase;  /* no re-load of &CdAudio_Ctl — callers set p/$v1 */
    p->field_9 = 1;
error:
    ...
```

Also force early `CdAudio_Tbl` addressing before the `audio` pointer is built by
writing the cross-struct store with a cast, then assigning `audio`:

```c
CdAudio_Ctl.field_0 = 0;
CdAudio_Tbl.field_8 = ((volatile CdAudioLocEx*)&CdAudio_Loc)->field_4;
audio = (volatile CdAudioLocEx*)&CdAudio_Loc;
CdIntToPos(audio->field_4, (CdlLOC*)&audio->field_10);
```

`CdAudio_DriveRead` is the pure example.

## Force `move v0,v1` + reload: dual `asm("v0")` temps with empty barrier

When the target builds a small matrix block like:

```
lhu  v0, cos
lhu  v1, sin
sh   v0, m[1][1]
move v0, v1          /* copy sin into v0 for later negu */
sh   v1, m[1][2]     /* store still from v1 */
lhu  v1, cos         /* reload — v0 is now sin */
negu v0, v0
sh   v0, m[2][1]
sh   v1, m[2][2]
```

CSE will happily `negu v1,v1` in place and reuse the live cos in `v0`,
skipping both the `move` and the reload. Pin two pairs that share registers
and insert an empty asm so the store keeps reading the sin register:

```c
register u16 cos_u asm("v0");
register u16 sin_u asm("v1");
register s16 neg_s asm("v0"); /* reuses v0 after cos is stored */
register u16 cos2 asm("v1");  /* reuses v1 after sin is stored */
volatile MATRIX* vmat = &block->mat;

cos_u = block->cos_val;
sin_u = block->sin_val;
/* … zero the other entries … */
vmat->m[1][1] = cos_u;
neg_s = sin_u;
__asm__ volatile("" : "+r"(neg_s) : "r"(sin_u)); /* keep sin_u live for sh v1 */
vmat->m[1][2] = sin_u;
cos2 = block->cos_val;
vmat->m[2][1] = -neg_s;
vmat->m[2][2] = cos2;
```

`volatile` on the destination matrix prevents the stores from being reordered
around the move. `Gfx_MatrixToEuler` is the pure example (RotMatrixX-shaped block
on the scratch arena, then `gte_MulMatrix0_real`).

## Reuse unused `arg1` as an early `$a1` address temp

When the second parameter is discarded and a global base is needed later via
`lhu …, off(a1)` after a switch (so `$a1` is still the prologue value on that
case entry), reassign the parameter at the top of the function:

```c
void func(void* arg0, void* arg1)
{
    arg1 = &CdCmd_Queue; /* materialises into $a1 before the switch */
    …
    case 3:
        if (((CdCmdQueue*)arg1)->field_22E != 0) /* lhu v0, 0x22e(a1) */
```

A separate `CdCmdQueue* q = &CdCmd_Queue` local that is live across calls is
coloured into `$s0` instead and produces `lhu v0, 0x22e(s0)`. Absolute
`CdCmd_Queue.field_X` is still correct for stores that the target emits with
`%hi(CdCmd_Queue+off)`.

## Split call results: dying temp vs join-live `ret`

Two returns from the same callee where the first is only used in an immediate
conditional store (stays in `$v0`) and the second is produced in one branch of
an if/else then consumed after the join (needs `$s2`) must be different
variables:

```c
s32 ret;  /* join-live → $s2; also forces arg0 into $s3 */
s32 temp; /* dies right after the store → $v0 */

temp = func(…);
if (flag >= 0) {
    flag = temp; /* sh v0, … — no move */
}
…
if (secondary != NULL) {
    ret = func(…); /* move s2, v0 */
} else {
    other = -1;
}
if (other >= 0) {
    other = ret; /* sh s2, … */
}
```

Sharing one `ret` for both calls pulls the first result out of `$v0` into a
saved reg and shrinks the stack (no `$s3`). `Fs_BootImageMachine` is the pure
example.

## Shared kill tail: fall out of outer `if` instead of `goto` from both arms

When a shared epilogue label sits *after* an outer `if (ptr != NULL) { ... }`
and two inner paths both need that epilogue, writing an explicit `goto epilogue`
from *both* arms often lets GCC invert the first arm so it falls through into
the label (eliminating a `j`). The target may keep:

```
bne  field, 1, not_one
# is_one body
j    epilogue
 lui ...
not_one:
# ...
# fall into epilogue by leaving the outer if
epilogue:
```

Match by giving only the "early" arm an explicit `goto`, and letting the other
arm fall out of the outer `if` (no `goto`):

```c
if (temp != NULL) {
    /* ... */
    if (field == 1) {
        do_work();
        goto epilogue; /* only this arm jumps */
    }
    if (other_ cond == 0) {
        /* early return — never reaches epilogue */
        return;
    }
    /* fall out of outer if into epilogue */
}
epilogue:
    cleanup();
```

`Task_AllocIdMap` is the pure example (`field_1 == 1` → `SndEvt_EnqueueType2` then
shared `D_80062734 = 0xFF; Task_Kill`). Explicit `goto epilogue` on the
`Midi_IsBusy != 0` path inverted the `field_1` branch to `beq` with the
bodies swapped.

## Force `lui a0` before `lh a1` for global+halfword call args

When a call is `func(GlobalSym, p->halfword)` and the preceding loop leaves
`$a0`/`$a1` dirty, GCC often schedules `lh a1, …` *before* `lui a0, %hi(GlobalSym)`,
while the target wants:

```
lui   a0, %hi(GlobalSym)
lh    a1, off(sN)
jal   func
 addiu a0, a0, %lo(GlobalSym)
```

Pin a short-lived pointer to `$a0` and assign the global into it immediately
before the call:

```c
{
    register u8* fn asm("a0");
    fn = Mc_FileName;
    Mc_BuildFileName(fn, saved->field_2C);
}
```

Plain `Mc_BuildFileName(Mc_FileName, saved->field_2C)` keeps the swapped order.
`Mc_StateFileSelect` is the pure example.

## Dual block pointers reuse angle `$s0` for scratch field access

RotMatrix-style helpers that take `(MATRIX* m, s32 angle, s32 flag)` allocate a
scratch block, call `rsin`/`rcos(angle)`, then never need `angle` again. The
target keeps the block in both `$s2` (from `head - size`) *and* `$s0` (reusing
the dead angle register):

```
move  a0, s0        /* angle still in s0 */
move  s0, s2        /* s0 = block */
jal   rcos
 sh   v0, 0x20(s0)  /* store sin via s0 */
```

A single `block` variable stays in `$s2` and leaves `$s0` idle after the calls.
Force the split with an early alias used for the sin/cos stores and later GTE
column 0:

```c
block = (ScratchMat*)(head - 0x24);
*(ScratchMat**)G_SCRATCH_HEAD = block;
p = block;                          /* early: dies into s0 after angle */

p->sin_val = rsin(angle);
cos        = rcos(angle);
p->cos_val = cos;
/* if-path / gte_ldclmv use p; else-path matrix build uses block */
gte_MulMatrix0_real(arg0, p, arg0);
```

`Gfx_RotMatrixY` is the pure example (Y-axis rotate; siblings X/Z match the same
shape).

## Separate scratch-head temps: `lui v0` prologue vs `lui v1` free

When the target loads `G_SCRATCH_HEAD` into `$v0` only for the prologue
(`lw s4,0(v0)` / `sw block,0(v0)`) and reloads it into `$v1` at free
(`lw v0,0(v1)` / `addiu` / `sw`), a single live `void** scratch` variable is
coloured into `$v1` for *both* sites (~99.8%).

Split the accesses so the prologue has no named address temp that must also
serve the free path:

```c
head = *(u8**)G_SCRATCH_HEAD;              /* address in v0 */
block = (ScratchMat*)(head - 0x24);
*(ScratchMat**)G_SCRATCH_HEAD = block;

/* … body … */

{
    void** scratch = (void**)G_SCRATCH_HEAD; /* address in v1, value in v0 */
    *scratch = (u8*)*scratch + 0x24;
}
```

## Load-then-zero fills the sin-reload delay before `negu`

For the flag≠0 path that stores `-sin` into `m[2][0]`, the target reloads sin
and fills the load-delay with an unrelated zero store:

```
lhu  v0, sin(s0)
sh   zero, m[2][1]
negu v0, v0
sh   v0, m[2][0]
```

Writing `m[2][1] = 0` *before* starting the reload produces `sh` / `lhu` / `nop`
/ `negu` instead. Capture the reload first:

```c
t = p->sin_val;
arg0->m[2][1] = 0;
arg0->m[2][0] = -t;
```

Do **not** mark the destination `volatile MATRIX*` on this path — volatile
blocks the `j` delay-slot fill that stores `m[2][2] = cos`. The else path still
wants `volatile` plus the dual `asm("v0")`/`asm("v1")` move/reload pattern
documented under RotMatrixX scratch blocks.

## RotMatrixZ: early cos in `$v1`, and barrier before sin/cos reloads

Y-axis `Gfx_RotMatrixY` loads **sin** early into `$v1` so it pairs with
`li v0,ONE` and is stored after the zero/ONE block. Z-axis `Gfx_RotMatrixZ`
does the same shape but with **cos** — the flag≠0 path ends with
`m[2][2]=ONE` / `m[1][1]=cos` in the `j` delay, so:

```c
arg0->m[1][0] = p->sin_val; /* own lhu + nop + sh before the pair */
cos_u = p->cos_val;         /* register u16 cos_u asm("v1") */
arg0->m[1][2] = 0;
arg0->m[2][0] = 0;
arg0->m[2][1] = 0;
arg0->m[2][2] = ONE;
arg0->m[1][1] = cos_u;
```

Using `p->cos_val` only on the final store reloads cos *after* ONE and puts
the sin store into the `li`/zero schedule (wrong).

Else path needs `m[2][2]=ONE` **before** reloading sin/cos from the scratch
block. A volatile store alone does not stop GCC 2.8.1 from hoisting the
non-volatile `block->sin_val` / `block->cos_val` loads above it — insert a
compiler memory barrier:

```c
vmat->m[2][2] = ONE;
__asm__ volatile("" ::: "memory");
sin_u = block->sin_val;
cos2  = block->cos_val; /* asm("a0") */
```

Z-axis also inverts the Y-axis move/negu split: copy sin to `$v1`, negate the
**original** in `$v0`, store `-sin` then `+copy`:

```c
register u16 sin_u asm("v0");
register s16 copy asm("v1");
copy = sin_u;
__asm__ volatile("" : "+r"(copy) : "r"(sin_u));
vmat->m[0][1] = -sin_u; /* negu of original */
vmat->m[1][0] = copy;
vmat->m[1][1] = cos2;
```

`Gfx_RotMatrixZ` is the pure example (Z-axis; X sibling is `Gfx_RotMatrixX`).

## RotMatrixX: dual cos loads via `volatile ScratchMat*`

X-axis else path (flag==0) needs two back-to-back `lhu` of the same
`cos_val` into `$v0` and `$a0` before storing `m[1][1]`:

```
lhu  v0, cos(s2)
lhu  a0, cos(s2)
sh   v0, m[1][1](s2)
```

Plain `cos_u = block->cos_val; cos2 = block->cos_val;` CSEs the second load
into `move a0,v0` (+nop) and breaks the match. Force both loads with a
volatile view of the scratch block:

```c
volatile ScratchMat* vblock = block;
cos_u = vblock->cos_val; /* asm("v0") */
cos2  = vblock->cos_val; /* asm("a0") */
vmat->m[1][1] = cos_u;
/* zeros + m[2][2] = cos2; then sin move/negu as on Z-axis */
```

Flag≠0 path is non-volatile and uses load-then-zero before `-sin` into
`m[1][2]` (same delay-fill as Y-axis `m[2][0]`). `Gfx_RotMatrixX` is the pure
example (X-axis; siblings `Gfx_RotMatrixY` Y / `Gfx_RotMatrixZ` Z).

## Duplicate `setlen` in both branches for delayed-slot tpage if/else

When building a `DR_TPAGE` whose GPU command is chosen by a flag, writing:

```c
setlen(dr, 1);
if (!(flag)) {
    dr->code[0] = 0xE1000240;
} else {
    dr->code[0] = 0xE1000220;
}
```

puts `setlen` *before* the `bnez`/`lui`/`j`/`ori` tpage select. The target often
wants `setlen` *after* the select (with `li v0,1; sb` between the join and the
`sw` of the command). Duplicating `setlen` into both arms forces that order
while still CSE'ing the `li`/`sb` after the join:

```c
if (!(flag)) {
    setlen(dr, 1);
    dr->code[0] = 0xE1000240;
} else {
    setlen(dr, 1);
    dr->code[0] = 0xE1000220;
}
```

Also: a shared `tpage` temporary tends to hoist `lui ...,0xe100` and break the
`j`/`ori` delay-slot form; assigning the full constant in each branch avoids
that. `Display_StepFadeOverlay` is the pure example.

## OT index: `(idx << 2) + (s32)base` vs `base + idx`

`D_800710A0 + otIdx` (pointer arithmetic) and
`(u_long*)((otIdx << 2) + (s32)D_800710A0)` are equivalent, but the second form
matches the target's register/schedule for dual `addPrim`:

```
lui  a1, 0xff / ori     /* 0xFFFFFF mask */
lui  v0, %hi(D_800710A0)
sll  a0, a2, 2          /* idx in a2 → offset in a0 */
lui  a2, 0xff00
lw   v0, %lo(D_800710A0)(v0)
...
addu a0, a0, v0
```

The pointer form often loads the base early into another register and swaps
which of `$a0`/`$a1` holds the mask vs the OT entry. Prefer the shift-then-add
cast when the dual-`addPrim` tail refuses to match.

## Split mult into product temp for `lbu` into multiplicand reg

```
mult  v1, v0
lbu   v1, field(a0)   /* reuse multiplicand reg */
mflo  t1
addu  v1, v1, t1
```

`temp = field + temp * scale` often loads `field` into `$v0` instead. Split so
the multiplicand register is clearly dead before the load:

```c
product = temp * scale;
temp    = field;
temp    = temp + product;
```

`Display_StepFadeOverlay` needs this for the fade-step update.

## Separate vars when two loops need different register sets

When a function has two sequential loops over the same data but the target
assigns different hard registers to the counter/sum/pointer in each loop
(e.g. first loop: `t1`/`a2`/`a3`; second: `a0`/`a1`/`v1`), reusing one set of
locals forces a single colouring and cannot match both.

Symptom: first loop matches after pins, second loop has the right shape but
wrong registers (or vice versa).

Fix: give the second loop its own counter/sum/pointer locals and pin each set
independently. `Mc_StateVerifyFinish` is the pure example — block-checksum walk then
first-byte sum over `Mc_BufferSlots[1..8]`.

## Reuse a pointer var across phases to force shared hard registers

When two sequential phases need the same hard register for different logical
pointers (e.g. file-list base in phase 1 and stream-folder base in phase 2 both
in `$t2`), reusing one C variable across both phases forces the shared colouring:

```c
FsCdfFile* files = (FsCdfFile*)&Fs_CdSector;
/* phase 1: walk files[j] */
...
/* phase 2: reuse the same local for the stream-side folder entry */
files = (FsCdfFile*)(Fs_FolderTable + (i & 0xFFFF));
stream->offset += files->offset + stage;
```

Separate `files` / `folder2` locals often colour differently and shift every
`$tN` assignment. `Fs_BuildFolderTables` needs this so phase-1 file base and phase-2
stream folder share `$t2`.

## Index-first cast for `addu rd, index, base`

`ptr + i` / `&ptr[i]` with the base already in a `$tN` often emits
`addu v1, tN, v0` (base first). The target sometimes wants
`addu v0, v0, tN` (index first, result reuses the index reg). Force that shape
with a cast that puts the scaled index on the left:

```c
/* Wrong operand order when files is in $t2: addu v1, t2, v0 */
file = files + (j & 0xFFFF);
file = &files[j & 0xFFFF];

/* Right: addu v0, v0, t2 */
file = (FsCdfFile*)(((j & 0xFFFF) << 3) + (s32)files);
```

Same for stream stride `* 0x28`. `Fs_BuildFolderTables` needs both forms.

## `asm("")` after a move that must own the next `beqz` delay slot

When the body starts with `src = (u8*)stream` (a pure `move a2, a0`) but the
delay slot of the preceding `beqz` fills with a later independent init
(`move a1, zero` for `k = 0`, or a loop-invariant `addu` for `dst`), insert an
empty asm barrier immediately after the move:

```c
if (stream->field_C != 0) {
    src = (u8*)stream;
    asm("");
    dst = (u8*)(destBase + (j & 0xFFFF));
    /* offset update, then for (k = 0; ...) copy */
}
```

The barrier pins `src` before later independent inits, so the delay-slot filler
takes `move a2, a0`. Without it, `k = 0` or the `dst` `addu` wins the slot.
`Fs_BuildFolderTables` is the pure example (also uses the project’s existing `asm("")`
pattern from `Fs_PrepareFolderLoad`).

## Late `andi s0, src, 0xFFFF` in call arg setup

When the target schedules `andi s0, sN, 0xffff` *after* `a0`/`a1`/`a2` setup
for a call (just before `move a3, s0`), a plain

```c
temp_s0 = temp_s5 & 0xFFFF;
SetDefDrawEnv(p, 0, 0, temp_s0, h);
```

with `u32 temp_s0` hoists the `andi` *before* the `addiu a0`. Declaring
`temp_s0` as `char` and writing the width as a comma expression forces the late
schedule while still emitting `andi …, 0xffff` for the actual argument:

```c
char temp_s0;

SetDefDrawEnv(p, 0, 0, (temp_s0 = temp_s5, temp_s5 & 0xFFFF), h);
/* later args reuse the same expression so CSE keeps $s0: */
SetDefDispEnv(q, 0, y, temp_s5 & 0xFFFF, h);
```

`temp_s0 = temp_s5 & 0xFFFF` alone on a `char` becomes `andi …, 0xff`. The
comma form evaluates the full-width mask for the call while the dummy `char`
store reshuffles the scheduler. A second local pointer alias (`new_var = ds`)
used on one call site can also be required to keep the register set stable.
`Display_SetMode` is the pure example.

## Dual magic-division + remainder for NTSC/PAL frame scaling

When the target does two separate `multu` sequences gated on
`Display_State.field_124 == 1` (NTSC → `/ 6000`, PAL → `/ 3600`) — first for the
quotient, then again for remainder reconstruction — write two separate `if`
blocks rather than combining `/` and `%` in one:

```c
if (Display_State.field_124 == 1) {
    quot = temp / 6000U;
} else {
    quot = temp / 3600U;
}
if (Display_State.field_124 == 1) {
    rem_factor = (temp / 6000U) * 0x177; /* 375; common *16 → 6000 */
} else {
    rem_factor = (temp / 3600U) * 0xE1;  /* 225; common *16 → 3600 */
}
```

The remainder join does `sll 4` once after both arms (`* 0x10`). Force the
scaled product into `$v0` and the remainder into `$v1` with dual pins so the
join matches `sll v0; subu v1, a0, v0; move s1, a1; sw v1`:

```c
{
    register s32 scaled asm("v0");
    register s32 rem asm("v1");
    scaled = rem_factor * 0x10;
    rem = temp - scaled;
    ticks = quot;
    p->field_38 = rem;
}
```

`Midi_DriveTrack` is the pure example.

## Place an orphan early-exit block between if/else arms

When the target parks a cold return block *between* the then-arm's `j join` and
the else-arm of a dual `field_124` (or similar) branch, force that layout with
an explicit goto through the else label:

```c
if (flag == 1) {
    rem_factor = /* then */;
    goto rem_join;
} else {
    goto rem_else;
}

early_exit:
    /* orphan return — only reached by later goto early_exit */
    p->field_0 = 0;
    return;

rem_else:
    rem_factor = /* else */;
rem_join:
    /* ... */
```

GCC fills the then/else gap with the jump-only early_exit block. A plain
`if/else` without the intermediate label leaves the return at the end of the
function. `Midi_DriveTrack` is the pure example.

## Delay-slot subtract with restore: compare-first via a `less` flag

When the target has:

```
move  v1, a0
slt   v0, s1, v1
beqz  v0, loop      /* ticks >= delta */
 subu s1, s1, v1    /* delay: always subtract */
addu  s1, s1, v1    /* fallthrough: restore original ticks */
/* end */
```

a plain `if (ticks >= d) { ticks -= d; goto loop; }` either reuses an earlier
`subu` (branch to it with `nop` delay) or emits `bnez end; j loop; subu`. Force
the restore form by comparing *before* the subtract, storing the result, then
undoing on the less-than path:

```c
{
    register s32 d asm("v1");
    s32 less;
    d = delta;
    less = ticks < d;
    ticks -= d;
    if (less) {
        ticks += d;
    } else {
        goto loop_outer;
    }
}
/* fallthrough to end with original ticks */
```

`Midi_DriveTrack` is the pure example.

## Comma in call arg forces field store before callee materialization

When the target does `li v0, 1; sb v0, field; lw v0, handler; jalr v0` on one
arm of a MIDI/status dispatch, pre-loading the handler into a temp schedules
the `li` into `$v1` (handler owns `$v0`). Inline the handler load as the callee
expression and put the field store in a comma on the first argument:

```c
p->field_2C = (*(Handler*)((u8*)table + 4))(
    (p->field_3 = 1, p->field_2 | 0x90),
    p->field_2C - 1, parent, p);
```

Arg evaluation keeps `lbu`/`lw` of the call operands interleaved correctly and
the `li v0,1; sb; lw handler` order matches. `Midi_DriveTrack` is the pure
example (else / running-status arm).

## Mid-struct `s32*` at the last field for negative offsets

When the target bases a loop on the *last* field of a struct element
(`addiu s0, a1, 0x38` for `MidiTrack::field_38`) and stores earlier
fields with negative offsets (`sw -0x10(s0)`, `sw -0xc(s0)`, `sw -0x4(s0)`),
a typed overlay starting at that field cannot express the earlier members
(C fields only grow forward). Plain `MidiTrack*` access often rebases on
a mid field instead (e.g. `field_2C` at `a1+0x2c`).

Fix: take `s32* p = &entry->field_38` (pin to `s0` if needed) and access
via word offsets:

```c
register s32* p asm("s0");
p = &entries->field_38;
((u8**)p)[-4] = trackPtr; /* field_8[8] at -0x10 */
((u8**)p)[-3] = trackPtr; /* field_2C at -0xC */
p[-1] = delta;            /* field_34 at -0x4 */
*p = 0xE0F;               /* field_38 */
p += 15;                  /* next entry, +0x3C */
```

Pair with `register MidiTrack* entries asm("a1")` so the clear loop keeps
`a1 = entries` and `addiu s0, a1, 0x38` matches. `Midi_InitSequence` is the pure
example.

## `s32` temps for scheduled `lbu` without extra `andi`

When the target loads two bytes (`lbu v0` / `lbu v1`) early, interleaves other
stores (`sb` of flags), then does `sll v0, 8; or v0, v1`, capturing the loads
in `u8` locals re-introduces `andi ..., 0xff` before the shift. Use `s32`
temps so the `lbu` zero-extends into a full register and the later shift/or
need no mask:

```c
s32 d0, d1;
d0 = data[0xC];
d1 = data[0xD];
/* sb flags... */
obj->field_34 = (d0 << 8) | d1; /* sll; or — no andi */
```

`Midi_InitSequence` is the pure example (MIDI division word after field_6/4/7/5).

## Dual-lived `register … asm` pins for channel→entry and temp→pan

When the target reuses `$t0` first as a channel index (`lbu t0; sllv …, t0;
sll t0, 3; addiu t0, 0x484; addu t0, obj, t0`) and later as the entry pointer,
pin both names to the same register with non-overlapping live ranges:

```c
register s32 channel asm("t0");
register MidiOpcodeSlot* entry asm("t0");

channel = (u8)slot->field_1; /* lbu, not lb — cast the s8 field */
if (obj->field_C & (one << channel)) {
    entry = &obj->field_484[channel]; /* overwrites t0 in place */
    ...
}
```

The `(u8)` cast on an `s8` channel field is required for `lbu`; a plain
`s32 channel = slot->field_1` emits `lb`.

The same dual-live pattern applies to `$v0`: pin an early multiply operand
(`temp`) and a later pan temporary (`f3 = entry->field_3; f3 -= 0x40`) both to
`asm("v0")`. They must not be live at the same time.

For the pan offset, prefer the two-step form so GCC emits `addiu …, -0x40`
instead of `ori …, 0xffc0; addu` (the latter appears when
`(u8_field - 0x40)` is folded into an `s16` cast of the full expression):

```c
register s32 f3 asm("v0");
f3  = entry->field_3;
f3 -= 0x40;
Spu_ApplyPanVolume(sp18, slot->field_5 + f3, vol);
```

Pair with `register s32 temp asm("v0"); register s32 scale asm("v1");` for the
shared `(temp * scale) / 127U` path so the join `mult` is `mult v0, v1`.
Use unsigned division (`/ 127U`, `/ 16129U`) when the target has `multu` magic.
`Midi_UpdateVoiceVolumes` is the pure example.

## `s16` temp for `field = -1` on a `u16` field

Assigning `-1` directly to a `u16` field makes GCC 2.8.1 emit
`ori v0, zero, 0xFFFF` (unsigned 65535). The target often wants
`addiu v0, zero, -1` instead — a one-byte difference that fails the
checksum while the rest of the function is perfect.

Hold `-1` in an `s16` temporary, then store:

```c
s16 neg;

neg          = -1;   /* addiu v0, zero, -1 */
p->field_202 = neg;  /* sh v0, field */
```

Changing the field type to `s16` also works in isolation but can break
already-matched functions that load the same field as `lhu` / cast through
`(s16)`. Prefer the temporary when the field type must stay `u16`.
`Mdec_ResolveStreamBuffer` is the pure example.

## Zero-pad itoa: force magic-before-`'0'` load order

Seconds zero-padding (`Text_FormatTime`) needs setup:

```
lui  a3, 0xcccc
ori  a3, a3, 0xcccd   /* magic for place/10 */
li   v1, 0x30         /* '0' */
multu a2, a3
sb   v1, 0(a0)        /* store interleaved with mult latency */
mfhi t4
srl  a2, t4, 3
```

Plain C with `i = 0x30; do { *dest = i; place /= 10; ...}` gets the right
registers (`v1` / `a3`) but schedules `li` *before* `lui` (~99.6%). Emitting
the magic first via a separate variable then a barrier:

```c
u32 mag = 0xCCCCCCCD;
i = 0x30;
asm volatile("" : "+r"(mag), "+r"(i));
```

preserves load order, but `place /= 10` no longer CSEs with `mag` and reloads
the constant. Match the mult/store interleave and keep `mfhi` in `$t4` ($12)
with a short asm body (no clobber list — a `t4` clobber recolors the whole
function):

```c
do {
    asm volatile(
        "multu %0, %2\n\t"
        "sb %3, 0(%1)\n\t"
        "mfhi $12\n\t"
        "srl %0, $12, 3"
        : "+r"(place)
        : "r"(dest), "r"(mag), "r"(i));
    dest += 1; /* fills the bnez delay slot */
} while ((u32)arg1 < place);
```

`Text_FormatTime` is the pure example (minutes:seconds time string, same
unsigned-decimal digit loop as `Text_ItoaUnsigned` for the minutes half).

## s32 temp for QImode store of a loop-compared constant

When the same small constant K is both (a) compared every loop iteration
(`if (status == 2)`) as SImode and (b) stored to a byte field on a rare path
(`p->field_16 = 2`), CSE unifies them and hoists `li sN,K` into a callee-saved
register for the whole function. That steals a reg (often pushing another
constant into `$s8`/`$fp`) and rewrites every use.

```c
/* Wrong: pins 2 in s7, four ends up in s8 */
if (status == 2) goto case_2;
...
p->field_16 = 2;
```

Route the *store* through an SImode temporary so the QI store and the SI
compare no longer share one REG_EQUAL:

```c
s32 tmp;
if (status == 2) goto case_2; /* rematerializes li v0,2 each iter */
...
tmp = 2;
p->field_16 = tmp; /* QI store of SI temp — no hoist */
```

`SndVoice_DriveSlots` is the pure example (SndScript_Slots state machine, field_16 cases
1/2/4/8/0x10/0x20/0x80).

## `*(volatile u8*)&field` forces lbu+sll24+sra24 sign-extend

A plain `(s8)p->u8_field` often becomes a single `lb`. When the target does
`lbu` / `sll 24` / `sra 24` instead, load through a volatile byte:

```c
temp = (s8)(*(volatile u8*)&p->field_13);
temp = temp + step;
```

## Save sum before `if (step > 0)` for addu/blez/move order

For a ramp of the form "compute sum, branch on step sign, keep full sum in
`$a0` while truncating a copy for the compare", assign the saved sum *before*
the sign test so the scheduler emits `addu; blez; move a0,v0` rather than
stuffing `addu` into the `blez` delay slot:

```c
temp = (s8)(*(volatile u8*)&p->field_13);
temp = temp + step;
new_val = temp; /* move before blez */
if (step > 0) {
    temp <<= 16;
    temp >>= 16;
    if ((s8)p->field_14 < temp) { /* clamp */ }
    else { p->field_13 = new_val; }
}
```

`SndVoice_DriveSlots` field_13/15 envelope uses this with `register s32 temp asm("v0")`.

## Force both ALU ops before either store with `+r` barriers

When the target does:

```
lw   v1, field_28(s0)
addiu v0, v0, -1
srl  v1, v1, 1
sw   v0, field_24(s0)
sw   v1, field_28(s0)
```

writing the natural

```c
work->field_24 = f24 - 1;
work->field_28 = (u32)work->field_28 >> 1;
```

often schedules `sw field_24` between `addiu` and `srl`. Keep both values in
registers, finish both ALU ops, then barrier before either store
(`Mc_StateBackupBuffers`):

```c
register s32 f24 asm("v0");
register s32 f28 asm("v1");
f28 = work->field_28;
f24 = f24 - 1;
f28 = (u32)f28 >> 1;
asm volatile("" : "+r"(f24), "+r"(f28));
work->field_24 = f24;
work->field_28 = f28;
```

## Shared join with value loaded on both predecessors into `$v0`

When success does `lw v0, field; j update; nop` and the else path also loads
the same field into `$v0` before falling through (so `update` only does
`addiu v0, v0, -1` / stores), give the PHI its own register variable:

```c
register s32 f24 asm("v0");
/* success: */
f24 = work->field_24;
goto update;
/* else: */
tmp = work->field_1C + ...;
asm("" ::: "memory"); /* defeat CSE of the earlier field_24 in $a1 */
f24 = work->field_24;
work->field_1C = tmp;
update:
work->field_24 = f24 - 1;
...
```

Without the memory clobber, the else path becomes `move v0, a1` (reuse of the
function-entry load) instead of a fresh `lw`. Without `asm("v0")`, the load
lands in `$a0` and the join no longer matches.

## `addu` operand order: `off + base` vs `base + off`

MIPS `addu rd, rs, rt` encodes the two source registers distinctly. Semantically
`base + off` and `off + base` are identical, but the encodings differ and will
fail a matching decomp check.

Symptom: 99.9% match with a single-line diff like:

```
-addu    v1,a0,v1    /* a0=off, v1=base → off + base */
+addu    v1,v1,a0    /* base + off */
```

Fix: write the C expression so the offset is the left operand of the addition
before the base pointer is added:

```c
/* matches addu v1, a0, v1 (off in a0, base in v1) */
sp.field_4 = ((GlyphUvwh*)(off + (s32)arg0->field_14))->u + ...;

/* typically emits addu v1, v1, a0 */
sp.field_4 = arg0->field_14[glyphIdx].u + ...;
```

`TextStream_Draw` needs this for its 4-byte glyph table indexing.

## Reuse one `DR_TPAGE*` for sequential tpage prims

When a function allocates and inserts two (or more) `DR_TPAGE` primitives in
sequence — e.g. after each of two SPRTs — declare a **single** `DR_TPAGE* dr`
and reassign it for each block rather than `dr` / `dr2`:

```c
addPrim(ot, p);
dr         = D_80071190;
D_80071190 = dr + 1;
setlen(dr, 1);
dr->code[0] = 0xE100023F;
addPrim(ot, dr);

addPrim(ot, p2);
dr         = D_80071190;
D_80071190 = dr + 1;
setlen(dr, 1);
dr->code[0] = 0xE100025F;
addPrim(ot, dr);
```

Two live pointers force extra registers and scramble constant hoisting
(`0xE100023F` early into `$t6`, `0xFFFFFF` into freed `$a2`, `0xFF000000` after
the second SPRT alloc). One reused `dr` matches the target's `$t7` reuse and
~100% schedule. Prefer raw `setlen` + `dr->code[0] = 0xE1000xxx` over
`setDrawTPage` when the target stores the full GPU word as a constant (same
pattern as `Display_StepFadeOverlay` / `Text_DrawGlyphDualSprtTpage`).

## Empty `asm volatile` after field reads blocks pointer strength-reduction

When a loop walks a struct pointer (`raw++` / `raw += sizeof`) but only touches
fields at a fixed small offset (e.g. `raw->field_2` / `raw->field_3`), GCC 2.8.1
strength-reduces the induction variable to `base + offset` and emits
`lbu -1(s2)` / `lbu 0(s2)` with `addiu s2, base, 3` instead of the target's
`lbu 2(s2)` / `lbu 3(s2)` from the true base. Pinning `register … asm("s2")`
keeps the offsets but forces `lui s2; addiu s2,s2` instead of
`lui v0; addiu s2,v0`.

Fix: after the field reads, insert an empty multi-output asm that claims the
walker is modified. Place it **after** the loads (so SR of those loads is still
blocked by the loop-wide `+r`) but **not** at the loop tail (that steals the
`blez` delay slot from the offset increment):

```c
scratch->rawHi = raw->field_2;
scratch->rawLo = raw->field_3;
asm volatile("" : "+r"(raw)); /* keeps s2 as base; lbu 2(s2)/3(s2) */
buttons = ~*(u16*)&scratch->rawLo;
/* … */
raw++;
i++;
offset += 0x5C;
} while (i <= 0); /* blez delay: addiu offset */
```

`Pad_UpdatePort0` is the pure example. Pair with two-phase scratch alloc
(`register void* tmp asm("v0")` then `register PadScratch* scratch asm("s1")`)
for `lw v0; addiu v0,-N; move s1,v0; sw s1`.

## `volatile u8*` forces cooldown decrement reload (no delay-slot reuse)

When the target does `bnez field_A, else; nop` then inside else
`lbu / addiu -1 / sb / lbu` (reload for the zero check), a plain
`pad->field_A = pad->field_A - 1` reuses the compare load in the branch delay
slot (`bnez; addiu v0,v0,-1`) and skips the reload (`andi` instead of `lbu`).

Fix: access the byte through a volatile pointer so the decrement and the
follow-up test are real memory ops:

```c
} else {
    volatile u8* cooldown;
    cooldown = &pad->field_A;
    *cooldown = *cooldown - 1;
    pad->field_6 = 0;
    pad->field_8 = 0;
    pad->field_4 = 0;
    if (*cooldown == 0) {
        /* re-sample buttons into pad->field_4 */
    }
}
```

Do **not** mark the whole `PadState*` volatile — that turns `lh field_54` into
`lhu` + sign-extend. `Pad_UpdatePort0` is the pure example.

## Volatile store before `jal` (no delay-slot fill)

When the target stores a register to memory then `jal` with `nop` in the delay
slot, a plain store is often pulled into the delay slot:

```
move  v0, a0
jal   func
 sw   v0, %lo(sym)(v1)   /* wrong — target wants this before jal */
```

Fix: store through a volatile pointer so the store is a real side effect that
cannot fill the call delay:

```c
s32 id;
id = p->field_20; /* lhu */
*(volatile s32*)&D_800689E4 = id;
if (func(p->field_20, p->field_22) == -1) { /* still lhu via same load CSE */
    /* ... */
}
```

`SndLoad_ProcessSector` is the pure example (store of bank id before `SndBank_FreeById`).

## Call site without short prototype keeps `lhu` while callee stays `s16`

If the callee is matched as `s16 arg0` (needed for its own register allocation,
e.g. copy into `$a3`), a visible `s16` prototype at the call site turns

```
lhu a0, field
```

into `lh` (or an extra `lhu` + separate path). When the call is compiled *before*
the definition and *without* a prior prototype, default argument promotions keep
the argument as a full word and the load stays `lhu`.

Do not change a matched `s16` definition just to fix a caller — drop or avoid the
early prototype instead. `SndLoad_ProcessSector` → `SndBank_FreeById` is the pure example.

## Non-volatile `lui` + volatile `lbu` for early Display_State prologue slot

When the target interleaves `lui %hi(Display_State)` *between* `ori s0, scratch`
and `sw ra` (prologue gap), a plain C load of the field often either:

1. Emits `lui` late (after `sw ra`), or
2. Emits both `lui` and `lbu` early, reusing the wrong register for the byte.

Split the access into two asms:

```c
register u32 ds_hi asm("v1");
register s32 d asm("v0");

/* Non-volatile: scheduler may place this in the prologue gap before sw ra */
__asm__("lui %0, %%hi(Display_State)" : "=r"(ds_hi));
/* ... load field_10 / stream so v0 is free ... */
/* Volatile: pins the lbu after those loads, into v0 */
__asm__ volatile("lbu %0, %%lo(Display_State+0x128)(%1)" : "=r"(d) : "r"(ds_hi));
```

Making *both* volatile keeps the body order correct but parks `lui` after
`sw ra` (~99.6%). Making *both* non-volatile moves `lui` early but reorders
`lbu`/stores. Only the mixed pair matches.

`Tmd_SetupDraw` is the pure example.

## Handwritten light setup: SetColorMatrix + ldbkdir + transpose MulMatrix0

`Tmd_SetupDraw` loads `arg0->field_20` (color MATRIX, often `D_80074080`) with
`gte_SetColorMatrix`, then ambient from `t[]` via `gte_ldbkdir(t[0],t[1],t[2])`
(not `gte_SetBackColor` — no `<<4`). It then transpose-copies `D_80070F34` into
scratch (same `t4/t5/t6` halfword pattern as `Gfx_TransposeRot`), `gte_SetRotMatrix`
on `arg0->field_1C` (light dir, often `GsLIGHTWSMATRIX`), and in-place column
RTIR via `gte_ldclmv` + `gte_rtir_real` (`0x4A49E012`) + `gte_stclmv` three times
(same real-opcode rule as other GTE command macros).

## Local OT pointer for `D_800710A0` so `%hi` stays temporary

`GameMain_Loop` (and similar dual-buffer main loops) must both:
1. pin `Gpu_OtBuffers` as **two** regs (`s8` = `%hi`, `s7` = full via `addiu s7,s8,%lo`) for `Display_FrameFlipDraw` (`addiu a0,s8,%lo`) and `DrawOTag` (`addu v0,stride,s7`);
2. use `%hi(D_800710A0)` only temporarily in `$s0` around `ClearOTagR`, not as a function-wide pin.

Writing only through the global:

```c
D_800710A0 = ot;
ClearOTagR(D_800710A0, n);
*D_800710A0 = END;
D_800710A0 += 0x20;
```

makes GCC hoist `lui sN,%hi(D_800710A0)` into the prologue and steal the reg that
should hold `Gpu_OtBuffers`'s full address.

Fix: pass a **local** into `ClearOTagR`, then reload from the global for the
end-prim write so the `%hi` is only live in that block:

```c
{
    u_long* ot = D5F414_OrderingTables + flip * C5F414_OTAG_ENTRIES;
    D_800710A0 = ot;
    ClearOTagR(ot, C5F414_OTAG_ENTRIES);
}
{
    u_long* p = D_800710A0;
    *p = C5F414_OTAG_END_PRIM;
    D_800710A0 = p + 0x20;
}
```

Also keep the buffer index used for OT setup (`flip = field_114 ^ 1`) in a
short-lived temp so it can live in `$a1` until `ClearOTagR`'s second arg
clobbers it — do **not** reuse that variable for the later `PutDrawEnv` index.

## Preload halfword before conditional `+=` for `sh` / `lhu` / `bgez` / `addu` schedule

When the target does:

```
sh    v0, h(obj)        # unconditional store
lhu   v0, x(obj)        # load next field into same reg
bgez  t, skip
 addu v0, v0, t         # delay: always add
sh    v0, x(obj)        # store only if t < 0
```

a plain

```c
obj->h = obj->h + adj;
if (t < 0) {
    obj->x += t;
}
```

often schedules `bgez` *before* the `sh` of `h` (filling the delay with that
store), which then forces a load-delay `nop` before `addu` — wrong order and
an extra instruction.

Fix: preload `x` into a temporary *after* writing `h` and *before* the branch,
and write the updated value through that temp:

```c
obj->h = obj->h + adj;
xv = *(u16*)&obj->x;   /* lhu when RECT/short fields would otherwise lh */
if (t < 0) {
    obj->x = xv + t;
}
```

The early `lhu` can fill the slot after `sh h`, and `addu` fills the `bgez`
delay. Use `*(u16*)&` (or a `u16`/`u32` temp from an unsigned load) when the
target wants `lhu` but the field type is signed `short`.

`Ui_LayoutListPanel` is the pure example. Pair with signed field overlays when the
same function also needs `lb`/`lh` on counts/layout halfwords stored as `u8`/`u16`
in the shared struct.

## Force `li` into `beqz` delay ahead of `lui %hi(global)`

When a block starts with a screen-size constant then a prim-buffer load:

```
beqz  s3, skip
 li    v0, 0x140          /* delay — must be the constant */
lui   a2, %hi(D_80071190)
sh    v0, …               /* store the constant */
…
lw    s0, %lo(D_80071190)(a2)
```

plain statement order (`tw = 0x140; p = D_80071190; sp.w = tw`) often lets the
scheduler put `lui` in the delay slot instead. Pre-branch assigns sink; nested
blocks that finish the store before mentioning the global put `li` in the delay
but then emit `sh` before `lui`.

Materialize the constant, pin it with an empty asm constraint, then use it:

```c
s32 tw;
tw = 0x140;
asm volatile("" : "+r"(tw));
sp.w = tw;
sp.h = 0xF0;
p = (DR_AREA*)D_80071190;
```

The `+r` barrier forces `li` to complete before any following `lui`, so delay-slot
fill prefers the constant and the remaining schedule is `lui` / `sh` / …

Pair with `register DR_AREA* p asm("s0")` when the same function's `addPrim`
tails need `and s0, s0, a0` (mask in `$a0`). `Ui_DrawPanel` is the pure example.

## Global array access for `addiu v0, base, off` form

When the target does:

```
addiu  a3, …, %lo(CdReady_Queue)
…
sll    v1, idx, …          /* idx * stride */
addiu  v0, a3, 8           /* &entries[0] */
addu   v1, v1, v0
lw     a0, 0(v1)
```

writing `e = &p->entries[idx]` with `p` a local that already holds
`&CdReady_Queue` often produces the algebraically equal but different form
`addiu v1, scaled, 8; addu v1, v1, a3`.

Using the **global** for the array index keeps the `addiu v0, base, 8` shape
even when a register also holds the same address (e.g. assigned earlier for
another purpose, then reused after the block):

```c
a3 = (volatile CdStreamState*)&CdReady_Queue; /* may be needed for reg color */
…
e = (CdReadyEntry*)&CdReady_Queue.entries[idx]; /* global → addiu v0, base, 8 */
```

`CdStream_Start` is the pure example — 99.9% until this one form difference.

## Adjacent BSS: `p + 1` for the next object

When two BSS globals are laid out back-to-back (here `CdStream_State` size 0x58,
then `CdStream_Channels`), the original code often never names the second symbol and
instead uses `(NextType*)(p + 1)` / `(PrevType*)q - 1`. Prefer that over loading
`&CdStream_Channels` so codegen keeps a single base plus a fixed offset (`addiu t0,
a3, 0x58`). See also `CdStream_SetPitch` which walks the other direction.

## QImode `register asm` compare: `andi v0,tN` vs `andi tN,tN`

A pinned `register u8 flag asm("t1")` tested as `if (!flag)` often compiles to
the wrong form `andi t1,t1,0xff; bnez t1` (truncate in place). The target wants
the zero-extend form `andi v0,t1,0xff; bnez v0`.

Force the extend into `$v0` with an explicit temporary pinned there:

```c
register u8 flag asm("t1") = 0;
...
{
    register u32 valid asm("v0");
    valid = flag;
    if (valid == 0) {
        /* false path */
    }
}
```

`Fs_InitStage0TablesCb` is the pure example — the rest of the function already
matched with `flag` in `$t1`, only the compare form was wrong.

## Fallback path: reload entry so `$a3`/`$a2` can be reused

When an earlier `entry`/`entryValue` pair lives in `$t0`/`$a3` and a later
fallback must re-read the same sector word, reloading into fresh locals (and
storing the *reloaded* id, not the old `fileId`) lets the allocator put the
pointer in `$a3` and the value in `$a2` as the target does:

```c
register u32 v asm("a2");
words = sectorBuffer->words;
v = words[(u16)headerOffset];
if (v / 100000 != 0) {
    register FsCdfFile* tbl asm("v1");
    tbl = Fs_FileTable;          /* lui order: table before len */
    i   = Fs_FileTableLen;
    Fs_FileTableLen++;
    tbl[i].id     = v;           /* reloaded, not fileId */
    tbl[i].offset = ((FsCdfFile*)&words[(u16)headerOffset])->offset;
}
```

Assigning `tbl = Fs_FileTable` *before* reading the length also fixes the
`lui v1,table` / `lui a1,len` order for the generic file table.

## Cat tables: `FsCdfFileSmall*` first, then len (switch delay + lui order)

For the small (u16/u16) category tables, indexing `Fs_FileTableCatN[i]`
directly after reading the length emits `lui len` *before* `lui table`.
Target wants table-then-len for cases 1–3, and for case 4 it wants
`lui v0,%hi(Fs_FileTableCat4)` in the switch-tree delay slot of
`bnez …, case4` (with only `lui a1,len` left in the case body).

Fix: pin a `FsCdfFileSmall*` (not `FsCdfFile*` — wrong 8-byte stride) in
`$v0`, assign the flag early, then table then len:

```c
case 4: {
    register FsCdfFileSmall* tbl asm("v0");
    isValidCategory = true;       /* li t1,1 early — helps schedule */
    tbl = Fs_FileTableCat4;       /* lui order: table before len;
                                     %hi can fill the case4 delay slot */
    i   = Fs_FileTableCat4Len;
    Fs_FileTableCat4Len++;
    tbl[i].id     = fileId - fileCategory * 10000;
    tbl[i].offset = ((FsCdfFile*)entry)->offset; /* sector entry is full word */
    break;
}
```

Same pattern for cases 1–3. Using `FsCdfFile*` here silently switches to
`sll …,0x3` / `sw` and tanks the match even when lui order is right.

`Fs_InitStage0TablesCb` is the pure example (99.703% → 100% on this alone).

## Fresh `lui` for a late store when `%hi` is live in `$s0`

When an earlier block pins `%hi(global)` in `$s0` (several `lw`/`sw %lo(s0)`),
a later store to the same global often reuses `$s0` and omits the fresh
`lui $v0, %hi(global)` the target puts before `j` with `sw` in the delay:

```
lw   v1, 8(a0)
lui  v0, %hi(Fs_ChunkWritePtr)   /* rematerialize */
j    ret0
sw   v1, %lo(Fs_ChunkWritePtr)(v0)
```

Killing `$s0` only shifts the CSE into `$s1`/`$s2`/…. Pure C cannot force `$v0`.

Fix: rematerialize and store with a **tab**-`noreorder` asm block (maspsx only
treats `.set\tnoreorder` as noreorder — a space leaves reorder mode and inserts
`nop` after `j`), jumping to the shared `ret0` label:

```c
{
    register u8* val asm("v1");
    val = (u8*)blk->field_8;
    __asm__ volatile(
        ".set\tnoreorder\n\t"
        "lui $2, %%hi(Fs_ChunkWritePtr)\n\t"
        "j %1\n\t"
        "sw %0, %%lo(Fs_ChunkWritePtr)($2)\n\t"
        ".set\treorder"
        :
        : "r"(val), "i"(&&ret0)
        : "v0", "memory");
}
ret0:
    return 0;
```

`Fs_ProcessChunkData` case 4 is the pure example (99.478% → 100% on this alone).

## `lui v0,%hi` + `addiu v1,v0,%lo` when `p` is pinned to `$v1`

Pinning `register T* p asm("v1"); p = GlobalArray;` often yields
`lui v1,%hi; addiu v1,v1,%lo`. The target wants the high half in `$v0`:

```
lui   v0, %hi(SndScript_Slots)
addiu v1, v0, %lo(SndScript_Slots)
```

Force that pair with a short asm (do **not** over-pin path-local constants in
`$a0`–`$t0` if a later path needs a different delay-slot schedule — heavy pins
on one arm can change delay fill on the other):

```c
register SndScript* p asm("v1");
{
    register s32 hi asm("v0");
    __asm__ volatile(
        "lui %0, %%hi(SndScript_Slots)\n\t"
        "addiu %1, %0, %%lo(SndScript_Slots)"
        : "=&r"(hi), "=r"(p));
}
```

`SndScript_StopMatching` is the pure example. Pair with unpinned locals for the mask /
status constants so the case-2 `beq` can still put `lui %hi(D_80082748)` in its
delay slot.





## Case-4 early exit: known-zero `sector` must not merge with `return 0`

When a branch leaves `sector` still at its prologue value of 0, writing

```c
if (req[1] == 1) {
    /* search… */
    if (sector == 0) {
        return 0;                 /* j epilogue; move v0, zero */
    }
    /* ACC8 / ChunkMode / ReadSector (may join shared ReadSector) */
    return sector & 0xFFFF;
}
return sector & 0xFFFF;           /* ALSO sector==0 here */
```

lets GCC CSE both exits into the early `return 0` block. The target instead
branches the `req[1] != 1` path to the shared `andi v0,s0,0xffff` at the
function epilogue (same block used after the common `Fs_ReadSector`).

Fix: route every non-failure exit through one shared label, and keep only the
real failure as `return 0`:

```c
if (req[1] == 1) {
    /* search… */
    if (sector == 0) {
        return 0;
    }
    D5B498_8006ACC8 = 1;
    Fs_ChunkMode    = 0;
    Fs_ReadSector(sector);   /* often joins the setup_and_load ReadSector */
}
goto end_return;
/* … */
end_return:
    return sector & 0xFFFF;
```

Using `break` instead of `goto after4` for the table-search hit, or folding
both exits as bare `return sector & 0xFFFF`, reintroduces the merge (or
changes regalloc of `req` out of `$t0`).

## Compiler-generated jump tables need yaml `.rodata` ownership + pad

When a matched function grows a switch jump table, give C the jtbl range with
`.rodata` (leading dot) and leave any trailing pad / later tables as asm
`rodata`:

```yaml
- [0x3a74, .rodata, fs]   # 7-entry jtbl from Fs_LoadFile
- [0x3a90, rodata, fs_1]  # leading .word 0 pad + rest
```

Without the pad word owned by the next asm unit, later functions shift and the
whole checksum fails even if the C text matches.

## Scratch alloc via `v0` temp so store fills the `jal` delay slot

When the target opens a scratch-arena function as:

```
lw    s3, 0(v1)          # head = *G_SCRATCH_HEAD
lh    a0, 0(s0)          # first call arg
addiu v0, s3, -0x34      # p = head - size
move  s2, v0             # block = p
jal   rsin
 sw   v0, 0(v1)          # delay: *G_SCRATCH_HEAD = p
```

a single `block = head - size; *s = block` coalesces into `addiu s2,...` /
`sw s2,...` and often puts the scratch address in `$v0` instead of `$v1`.

Force the intermediate in `$v0` and assign both ways:

```c
void** s = (void**)G_SCRATCH_HEAD;
u8* head = (u8*)*s;
register void* p asm("v0");
ScratchRotXYZ* block;

p = head - 0x34;
block = p;
*s = p;
block->sin_x = rsin(angles->vx); /* sw v0 fills the jal delay */
```

Reload `G_SCRATCH_HEAD` in a nested block at the end (do **not** keep `s`
live) so the epilogue re-materialises it in `$v1` rather than pinning a
callee-saved reg. `Gfx_RotMatrixXYZ` is the pure example.

## Keep a live halfword across GTE with `move` + empty asm (no copy-prop)

When the target reuses a halfword still sitting in `$a2` from earlier loads:

```
lhu  v0, 0x24(s2)   # new load first
move v1, a2         # copy of the live cos
sh   v1, 0x30(s2)
sh   v0, 0x2c(s2)
```

plain `block->vec.vz = cos_y` emits `sh a2, ...` (copy-prop kills the move).
Assign through a pinned temp and barrier both the new load and the copy so
the scheduler cannot reorder them:

```c
register u16 cos_y asm("a2"); /* loaded earlier, still live */
register u16 sy asm("v0");
register u16 cy asm("v1");

sy = block->sin_y;
__asm__ volatile("" : "+r"(sy));       /* pin load before move */
cy = cos_y;
__asm__ volatile("" : "+r"(cy) : "r"(cos_y));
block->vec.vz = cy;
block->vec.vx = sy;
```

Same family as the sin/cos `negu` barriers on `Gfx_MatrixToEuler`, but for a
register-to-register copy rather than a negate. Needed between `gte_rtir_real`
and `gte_stclmv` when the original interleaves next-vector setup in the GTE
pipeline gap.

## Euler RotMatrix via `gte_ldsv` columns (not `gte_ldclmv`)

`Gfx_RotMatrixXYZ` builds `RotX * RotY * RotZ` on the scratch pad by:

1. Writing RotX into a scratch `MATRIX`, with the next rotation's column packed
   as an `SVECTOR` at the end of the block (`gte_ldsv` offsets 0/2/4).
2. `gte_SetRotMatrix` + `gte_ldsv` + `gte_rtir_real` + prep next column +
   `gte_stclmv` (matrix column offsets 0/6/12) — twice for RotY (col0, col2;
   col1 of RotX is already `(0,1,0)`-compatible), then again for RotZ (col0,
   col1).
3. `flag != 0`: word-copy the 5 halfword-pairs of the rotation into `out`.
   `flag == 0`: `gte_MulMatrix0_real(out, block, out)`.

Do **not** feed those temp columns through `gte_ldclmv` — they are contiguous
`SVECTOR`s, not matrix columns. `gte_ldsv` is the matching load helper.

## Euler RotMatrix ZYX (`Gfx_RotMatrixZYX`) — 0x44 scratch, dual cos store

Sibling of `Gfx_RotMatrixXYZ` / `Gfx_RotMatrixYXZ` that builds `RotZ * RotY * RotX`.
Needs a larger scratch block (`ScratchRotZYX`, 0x44) with three `SVECTOR`s
(`vec` / `vec2` / `vec3` at 0x2C / 0x34 / 0x3C) because both Y and X contribute
two non-trivial columns.

Two codegen details that stall at ~98% without them:

1. **Dual store of `rcos(vz)`.** Target does `sh v0, cos_z` then
   `sh v0, m[0][0]` from the return register. Write:

   ```c
   block->cos_z = rcos(angles->vz);
   *(s16*)(head - 0x44) = block->cos_z; /* CSE keeps v0 for both sh */
   ```

2. **`ONE` into `m[2][2]` must stay before the four halfword reloads.** A plain
   `block->mat.m[2][2] = ONE` sinks below the `lhu`s (~98%). Store through the
   `volatile ScratchRotZYX*`:

   ```c
   vblock = block;
   vblock->mat.m[2][2] = ONE;
   sin_z = vblock->sin_z; /* … cos_z, cos_y, sin_y */
   ```

Column targets use `head - 0x42` (col1) and `head - 0x40` (col2), same
`gte_ldsv` / `gte_rtir_real` / `gte_stclmv` pipeline gap pattern as B960.

## SndScript script interpreter layout (SndScript_Exec)

`SndScript_Exec` is a fourCC-dispatched music/script stepper over
`SndScript::field_48`. Layout notes that unblocked progress toward a match:

- `field_17` / `field_18[8]` / `field_20[8]` are a loop stack (depth, remaining
  counts, restart cursors) for `"Loop"`/`"endL"`. They fill the old `pad_18[0x28]`.
- `field_44` is a `SndScriptCtx*` (`u8* data`, `SndBank* bank`), not a bare `s32`.
- `field_48` is a script cursor (`SndScriptCmd*`); `"oneV"` payloads are
  0x18-byte `SndOneV` records (bank id, note, duration, pan/vol,
  reverb gate, pitch, oneA/oneE offsets).
- `"oneC"` advances the cursor by 0x10, resolves `field_4C` via
  `base[*(u16*)(base + (u8)field_0 * 2 + 8)]`, then falls into `"oneV"`.
- Shared wait-tick path: when high-half of `field_8` is below the command's
  duration, add `Display_State.field_124 == 1 ? 0x9999 : 0x10000` and return 0;
  on success subtract `duration << 16` and return 1 (caller loops while nonzero).
- Volume: `(scale * field_4C->field_5 * voice->field_A) / 16129` (127²), same
  as `SndVoice_ApplyMasterVolume`.

## `a3` prim pointer → `t0` copy frees `a3` for the `0xFFFFFF` mask

When `addPrim` follows an early `DR_MODE*` / prim allocation, the target often
does:

```
addiu  a3, s0, 0x68      /* allocate prim */
sw     a3, D_80071190    /* delay of first validity check */
...
move   t0, a3            /* delay of second check */
lui    a3, 0xff          /* reuse a3 as 0x00FFFFFF mask */
ori    a3, a3, 0xffff
...
and    a3, t0, a3        /* prim & 0xFFFFFF for OT link */
```

Pinning the pointer to `a3` for the whole function blocks the mask reuse and
yields `and t0, t0, a3` / wrong OT link regs.

Fix: scope the `a3` pin in a nested block, copy to an unpinned `dr`, then
`goto` the body so `a3` is free again:

```c
DR_MODE* dr;
{
    register DR_MODE* r asm("a3");
    r = (DR_MODE*)(p + 2);
    D_80071190 = (DR_TPAGE*)r;
    if (valid) {
        dr = r;
        goto body;
    }
    goto end;
}
body:
    setTexWindow(dr, &tw);
    addPrim(ot, dr); /* a3 free for 0xFFFFFF */
end:
    return;
```

`Ui_DrawWindowBorder` is the pure example. Pair with `register s32 y0r asm("v1")` if
the second validity compare must load the right-hand operand first
(`lh v1, y0; lh v0, y2; slt v0, v0, v1`).

## Animated RGB clamp: compute next delta before building the colour word

For a chain of `max(K - val, 1)` greyscale colours, the target interleaves:

```
t = K - val;
if (t <= 0) t = 1;
c = t & 0xFF;
t = NEXT_K - val;          /* next delta BEFORE colour construction */
color = (c<<16)|(c<<8)|c;
*rgb = color;              /* sw / bgtz t in delay for the next clamp */
```

Writing `color = ...; store; t = NEXT_K - val` puts the `li`/`subu` after the
stores and breaks the `bgtz` delay-slot shape. Also put the constant `K` in a
temp that lives in `$v0` (`color = 0xB0; t = color - val`) so you get
`li v0, K; subu a1, v0, a2` rather than `li a1, K; subu a1, a1, a2`.

First clamp only: after `t = 1` use `asm("" : "+r"(t)); c = t & 0xFF` so the
second `andi` is not constant-folded to `li a0, 1`.

## `CdStream_ReadyMts` (4A6E0 CD/SPU stream callback) notes

Large (~0xA68) CD ready callback (`CdStream_*` / `CdReady_*` subsystem).
Infrastructure landed; full match still open (~84% best). Key requirements for
the next attempt:

1. **`--expand-div` on the `cdstream.c` TU** (already in `ninja_config.py`). Target
   has the full signed `div` trap sequence (`break 7` / `break 6`); bare `div`
   from GCC will not match without maspsx expansion.

2. **Identical dual `SpuWrite` arms** when `(remaining % mtsPeriod) == 1` and the
   two `field_1C & 1` branches are byte-for-byte the same in the ROM. GCC 2.8.1
   cross-jumps them into one block. A slight asymmetry (e.g. `u8* buf = …` on
   only one arm) can force two `jal SpuWrite`s; then re-converge the codegen.

3. **CdStreamState field map** (in `game.h`): `spuAddr`, `countdown`,
   `mtsPeriod`/`mtsParam`, `remaining`, `voiceL`/`voiceR`/`mode`, `sector` as
   `MtsSector*`.

4. **Error counters** at `D_80068B5C+1` / `+3` and `D_80068B64+1` want
   `%lo(sym+N)` form (`lbu`/`sb` with folded reloc). Separate byte symbols or
   non-volatile struct fields; array index often emits `addiu` base + offset.


## Pinned `s0` entry after a clear loop: force `lui v0; addiu s0,v0`

When `register u8* entry asm("s0")` is reassigned after a short clear loop that
joins both wait paths, GCC often emits the pinned form:

```
lui   s0, %hi(Fs_CdSector)
addiu s0, s0, %lo(Fs_CdSector)
```

The target rematerialises through `$v0` (same shape as `SndScript_StopMatching`):

```
lui   v0, %hi(Fs_CdSector)
addiu s0, v0, %lo(Fs_CdSector)
```

Force it:

```c
register u8* entry asm("s0");
/* ... clear loop ... */
{
    register s32 hi asm("v0");
    __asm__ volatile(
        "lui %0, %%hi(Fs_CdSector)\n\t"
        "addiu %1, %0, %%lo(Fs_CdSector)"
        : "=&r"(hi), "=r"(entry));
}
```

`Fs_ScanIsoDirectory` is the pure example (99.978% → 100%). Pair with
`const char` ISO name strings declared just above the function so they land in
`.rodata` *between* earlier jtbls and this function's jtbl (non-`const` arrays
go to `.data` and shift the whole image).


## `addu dst, base, idx` vs `addu dst, idx, base` for `base + (i*2)`

When the target does:

```
sll   v0, a0, 1
addu  v0, v1, v0   /* v0 = entry + idx*2 */
lh    v0, 0x24(v0)
```

writing `(idx * 2) + (u8*)entry` often yields the commutative swap
`addu v0, v0, v1`. Force base-first with temps:

```c
register s32 base asm("v1");
register s32 off asm("v0");
base = (s32)entry;
off  = idx * 2;
off  = base + off;          /* addu v0, v1, v0 */
if (*(s16*)(off + 0x24) != 0) { … }
```

(`Mdec_ProcessDecode` field_24 / field_2A walks.)

## `u8` store of `-3`: need `li v0, -3` not `0xfd`

`(s8)-3` / bare assign into a `u8` global often becomes `li v0, 0xfd`.
Match the signed form used for `-8`:

```c
s32 t;
t = -3;
asm volatile("" : "+r"(t));   /* stop fold/hoist if needed */
D5B498_8006C234 = t;          /* sb of low 8 bits; li stays -3 */
```


## Force 3-way `%hi` s-reg order: pin + `lui` asm, then `%lo` accesses

When three loop-live globals need a fixed s-reg coloring for split-address
form (e.g. target `s5=D_8007A368`, `s7=Fs_ChunkMode`, `s6=D5B498_8006C233`)
but natural allocation rotates them, pin and materialise with one asm block,
then do every load/store through those his:

```c
register s32 hi368 asm("s5");
register s32 hiMode asm("s7");
register s32 hi233 asm("s6");
register s32 hi364 asm("s4");
__asm__ volatile(
    "lui %0, %%hi(D_8007A368)\n\t"
    "lui %1, %%hi(Fs_ChunkMode)\n\t"
    "lui %2, %%hi(D5B498_8006C233)\n\t"
    "lui %3, %%hi(D_8007A364)"
    : "=&r"(hi368), "=&r"(hiMode), "=&r"(hi233), "=r"(hi364));
/* loads: */  __asm__ volatile("lw %0, %%lo(D_8007A368)(%1)" : "=r"(base) : "r"(hi368));
/* stores with branch-delay `li`: use tab-noreorder */
__asm__ volatile(
    ".set\tnoreorder\n\t"
    "nop\n\t"
    "beqz %0, 1f\n\t"
    "li $2, 2\n\t"
    "sb $2, %%lo(Fs_ChunkMode)(%1)\n\t"
    "li $2, -8\n\t"
    "sb $2, %%lo(D5B498_8006C233)(%2)\n\t"
    "1:\n\t"
    ".set\treorder"
    :
    : "r"(flag), "r"(hiMode), "r"(hi233)
    : "$2", "memory");
```

Pin a second quartet for a following loop that reuses the same s-regs with a
different global set. `Mdec_ProcessDecode` is the pure example (99.84% → 100%).


## `bnez` + delay-slot `lui` hi + `j`/`ori` lo (ternary default constant)

When the target does:

```
lw    v0, %lo(g)(v1)
nop
bnez  v0, 1f
lui   v0, HI        /* delay: start of K = (HI<<16)|LO */
j     2f
ori   v0, v0, LO
1:
lw    v0, %lo(g)(v1)  /* reload on non-zero path */
2:
…
```

a pure C ternary / if-else (`v = *p ? *p : K`) leaves a `nop` after `bnez`
and builds `K` with a non-delay `lui`/`ori` pair. Force the delay-slot form with
tab-`.set noreorder` asm (same maspsx rule as ProcessChunkData case-4).
`SndLoad_ResolveSpuAddr` case 5 (`D_80082128 ?: 0x63810`) is the pure example.

## Const data between two compiler jtbls in one TU

When a small `.rodata` blob (e.g. a limits table) sits *between* two jump
tables that will both be compiler-generated once their functions are matched,
declare the blob as `const` **between the two functions** in source order:

```c
s32 first_with_jtbl(…) { switch (…) { … } }

const GBytes6 D_between = {{…}};  /* lands after first jtbl, before second */

s32 second_with_jtbl(…) { switch (…) { … } }
```

Then expand the TU's `.rodata` segment start earlier in `main.yaml` and drop the
hand-extracted `rodata, name_N` sibling that held the old jtbl + blob. Non-const
definitions go to `.data` and break the layout. `SndLoad_ResolveSpuAddr` + `D_80014124` +
`TaskIdMap_RemapIndex` is the pure example.

## Volatile load-status flags keep `%hi` in `$a0` across reloads

Shared status words reloaded twice in a prologue (e.g. `D_800689E8`) CSE into
one load when non-volatile, and the address often lands in `$a1`. Mark them
`volatile` so each access reloads and GCC keeps `%hi` in `$a0` after
`move sN,a0` frees the argument register — same pattern as `D_8005EC80` in
`GameMain_ShowLoading`.

Do **not** hard-pin the state pointer to `$s3` with `register … asm("s3")` while
also pinning `$s2/$s1/$s0`: that steals `$a0` from the flag address. Leave the
`SndLoadState*` unpinned and pin only bank/ret/index so arg naturally colours
`$s3` and the flag hi stays in `$a0`.

## Fail-path `j` / `move v0,s1` vs shared epilogue

When several paths merge on `field_14 = 0; field_18 = 0; return`, the fail path
needs `j epilogue; move v0,s1` while the free path after `F3D458_Free` must not
fall through a `block_ret: v0 = s1` that GCC would merge away. Force the free
exit with tab-noreorder `j label; move $2,s1`, land with a unique asm label, and
clear `field_14` via `*(volatile s32*)&p->field_14 = 0` so the store is not
stolen into an earlier delay slot. `SndLoad_Complete` is the pure example.

## Spu_GetVoiceRef: dual `lhu`/`lh` count + keep `$a0` for `sb`

Leaf that looks up or allocates into `Spu_LVoiceTable` (stride `0x44` =
`sizeof(SpuLVoiceAttr)`). Target keeps the voice id in `$a0` for every
`sb …,0(a1)` while sign-extending a copy into `$a3` for indexing, and loads
`count` twice (`lhu` for the +1 store, `lh` for the `*0x44` index). Pure C
either renames `$a0` early or turns the second load into `lhu`+`sll`/`sra`.
A single tab-noreorder block matching the target is the reliable match;
`field_664` is at decimal offset 1636 (`0x664`).

## Spu_GetVoiceRef hybrid C (not full-function asm)

Most of the leaf is ordinary C with s-reg pins (`t1` base, `a2` slot/count,
`a3` sign-ext index, `t2` ret, `v0`/`v1` scratch). Three small asm pockets:

1. Prologue: `lui $v1` / `addiu $t1` plus `sll $v0,$4,24` / `sra $a3,$v0,24`
   so the voice id stays in `$a0` for later `sb` (a C `(s8)` extend renames `$a0`).
2. Alloc count: `lhu` then `lh` of `Spu_LVoiceTable` via the kept `%hi` in `$v1`.
3. Alloc tail: noreorder `*0x44` / `sh voiceNum` / `addiu count+1` / attr `+8`
   — pure C reorders `count+1` ahead of the entry pointer and clobbers `$t1`.

Found path (`slot*0x44 + base - 0x3C`) is pure C. `field_664` is at offset
`0x664` from the table base via `base + idx`.

## Fs_ProcessChunkHeader: s0=ptr / s1=%hi for CdSector

`Fs_ProcessChunkHeader` prologue must colour `&Fs_CdSector` as:

```
lui  s1, %hi(Fs_CdSector)
addiu s0, s1, %lo(Fs_CdSector)
```

with type later loaded via `lbu v1, %lo(Fs_CdSector)(s1)`. Pin `register FsSector *sec asm("s0")`, zero `register s32 s2 asm("s2")` early, and read type through the absolute `Fs_CdSector.chunk.type` (not `sec->chunk.type`) so GCC keeps the hi half in `$s1`.

Also force loadAddr-before-size with:

```c
void *loadAddr = sec->chunk.loadAddr;
asm("");
Fs_ChunkWritePtr = loadAddr;
size = sec->chunk.size;
```

That alone lifted the baseline from ~74% to ~81%.

## Fs_ProcessChunkHeader: finish (100%)

### Prologue without early `sw ra`

Do **not** set `s0`/`s1` with a volatile asm block before `CdGetSector` — that forces
early `sw ra`. Pure C:

```c
register s32 s2 asm("s2");
register FsSector *sec asm("s0");
s2 = 0;
sec = &Fs_CdSector; /* lui s1 / addiu s0,s1,%lo */
CdGetSector(sec->bytes, 0x200);
```

Keep `$s1` live by later reading the absolute symbol
`Fs_CdSector.chunk.type` (not `sec->chunk.type`).

### Setup: keep `a1` = `%hi(D_8006C4D4)` across the field_2 update

Post-`CdGetSector` setup matches only if the final
`sw …, %lo(D_8006C4D4)(a1)` reuses the same `a1` from the opening `lui`.
Do the bulk in asm that **outputs** `a1`, then interleave type load in C:

```c
register s32 d4_hi asm("a1");
/* asm: lui a1, … through endFlag store — do not clobber a1 */
f2 = sec->chunk.field_2;
type = Fs_CdSector.chunk.type; /* lbu %lo(s1) between lhu and addu */
/* addu + sw via d4_hi */
```

Un-pin `type` from `asm("v1")` — a pinned `type` made GCC use `t0` then
`move v1,t0`.

### Shared `set_phase_stream` (phase in `$v0`, hi in `$v1`)

```c
set_phase_ff:
    __asm__ volatile("li $2, 0xFF" ::: "v0");
set_phase_stream:
    __asm__ volatile("sb $2, %%lo(Fs_LoadPhase)($3)" ::: "memory");
    Fs_Streaming = 1;
ret0:
    return 0;
```

Callers jump with `j set_phase_stream; li $2, <phase>` and must have
`%hi(Fs_LoadPhase)` already in `$v1` from a prior delay slot.

### Branch delay must carry the hi; do not leave a residual `lui`

When the fallthrough after an always-taken asm jump is phase code, GCC still
emits a dead `lui` for that “fallthrough”. Prefer:

1. **Explicit noreorder branch** that sets the delay itself:

```c
__asm__ volatile(
    ".set noreorder\n\t"
    "bne %0, %1, %2\n\t"
    "lui $3, %%hi(Fs_LoadPhase)\n\t"
    ".set reorder"
    : : "r"(a), "r"(b), "i"(&&target));
```

2. Or a **whole loop tail in one asm** so `bnez` + `lui 0x8007` + fall into a
   shared `j set_phase_stream; li phase` label (case 5). Put the join label on
   the `j`, **not** on the `lui`, so mode==3 can skip the loop-exit `lui` and
   reuse the mode-check delay `lui` instead.

### `0x8007` vs `%hi(Fs_LoadPhase)`

`Fs_LoadPhase` / `Fs_Streaming` / `Fs_ChunkEndFlag` all sit under `%hi == 0x8007`.
ROM `target.o` shows absolute `lui …, 0x8007`; GCC emits `%hi(symbol)`. Same
encoding after link; for local objdump score, case 5 needs the literal
`lui $3, 0x8007` in the loop-exit delay.

### Case 1: force double-`lui` delay / EndFlag path

```c
__asm__ volatile(
    ".set noreorder\n\t"
    "bne %0, %1, %2\n\t"
    "lui $2, %%hi(Fs_LoadPhase)\n\t" /* delay */
    ".set reorder" : : "r"(status), "r"(one), "i"(&&phase2));
endFlag = Fs_ChunkEndFlag; /* starts with lui EndFlag — same 0x8007 */
```

Phase2 then reuses `$2` without a third `lui`.

## GameMain_Loop: prologue `lui v0` / `move sN` without breaking body SRA

Target opens with interleaved callee-save + init:

```
move s3, zero
lui  v0, %hi(Display_State)
addiu v0, v0, %lo(Display_State)
sw   s2, …(sp)
move s2, v0
lui  v0, %hi(D_8005EC80)
sw   s4, …(sp)
move s4, v0
sw   s5; lui s5, %hi(D_8005EC70)
…
```

Pinning `ds` to `$s2` and assigning `ds = &Display_State` emits `lui s2` /
`addiu s2` (2 insns short). Forcing `move s2,v0` via

```c
__asm__("move %0, %1" : "=r"(ds) : "r"(tmp));
```

matches the prologue but makes `ds` an **opaque asm result**: later stores
through `nv = ds` stop filling load-delay slots (e.g. `sb field_10d` after
`lw field_114` becomes `nop`), and the flip/`ClearOTag` block reorders.

**Fix — CSE with a pure C assignment** so GCC still knows `ds == &Display_State`:

```c
register DisplayState* t asm("v0");
t = &Display_State;
ds = &Display_State; /* CSE → move s2,v0; alias-known */
{
    register s32 t4 asm("v0");
    /* "r"(t) keeps Display load in v0 before EC80 hi reuses it */
    __asm__("lui %0, %%hi(D_8005EC80)" : "=r"(t4) : "r"(t));
    __asm__("move %0, %1" : "=r"(s4r) : "r"(t4)); /* s4 only holds %hi */
}
__asm__("lui %0, %%hi(D_8005EC70)" : "=r"(s5r)); /* direct lui s5 is fine */
```

Rules of thumb:

1. Long-lived pointer used for many field stores must stay a **pure C**
   definition of a known global (`ds = &Display_State`), not an asm `move`
   into a hard reg — otherwise body SRA dies.
2. `%hi`-only bases (EC80 / EC70) can use asm `move` into the pin; they are
   only used as `%lo(sym)(reg)` address bases, so opacity is harmless.
3. Chain the next `lui` with an input `"r"(t)` so the Display value occupies
   `$v0` first; that preserves the save interleave (`sw s2` between `addiu`
   and `move`) without a volatile barrier (barriers bulk-save the rest).

## Early-image hasm under `src/main/hasm/`

Named handwritten helpers (see `src/main/hasm/README.md`):

| Symbol | Role |
|--------|------|
| `Fs_DecompressChunk` (+ jtbl in same `.s`) | Resumable LZ for FS CD chunks |
| `Fs_DecompressImage` | Non-resumable LZ for image strips |
| `Tmd_SetupGteMatrices` | TMD draw: GTE light matrices + transforms |

```yaml
options:
  hasm_in_src_path: True
- [0x808, .rodata, hasm/Fs_DecompressChunk]  # sibling → same .s as hasm
- { start: 0x824, type: hasm, name: hasm/Fs_DecompressChunk, linker_section_order: .rodata }
```

Jump table + code live in **one** `Fs_DecompressChunk.s` (`.rodata` then
`.text`), same pattern as matched TUs with embedded jtbls. Stay hasm forever:
signed `sub`/`addi`, fixed resume PCs, early-image order, hand GTE.

## Global store before struct field RMW (pointer temp)

When the target does a global store first, then load/add/store a struct field
(`sb` to a BSS flag, then `lw`/`addiu`/`sw` of `task->field_30`), writing the
obvious C:

```c
D_80071068 = 1;
arg0->field_30++;
```

lets GCC 2.8.1 schedule the field load *before* the global store and delete the
load-delay `nop`, shrinking the function and shifting every later address.

`volatile` on the global alone does **not** stop that reorder. Take the address
of the field into a local pointer first so the RMW is a pair of `*p` ops after
the store:

```c
s32* p = &arg0->field_30;

D_80071068 = 1;
(*p)++;
```

That restores `lui`/`li`/`sb` then `lw`/`nop`/`addiu`/`jr`/`sw` (delay slot).

## Early `a2` for CdCmd param block, stores via stack slots

When the target sets `addiu a2, sp, 0x10` early (third arg of `CdCmd_Enqueue`)
but still writes the 4-byte param block with `sb …, 0x1N(sp)` (not `sb …, N(a2)`):

```c
s8  param2[4];
u8* p2;

arg0 = arg0 + 0xA;   /* addiu a0, a0, 0xa early */
p2   = (u8*)param2;  /* addiu a2, sp, 0x10 early */

/* … scratch alloc, Game_Session side effects … */

param2[0] = arg0;    /* sb a0, 0x10(sp) — write through the array */
param2[3] = 0;
param2[2] = 0;
param2[1] = 0;
CdCmd_Enqueue(0x21, param1, p2);  /* use p2 only at the call */
```

Writing through `p2[i]` forces `sb …, N(a2)` and breaks the match. Keep `p2`
live only as the call argument; store via `param2[]`. Combined with
`register void** scratch asm("s0")` + free via `*scratch = (u8*)*scratch + 8`
(s0 kept across the jal). `func_80094B90` (title overlay) is the pure example.

## Title overlay switch jtables live after header, from compiler `.rodata`

`src/title/header.s` holds the package header through the last *still-asm*
function's jump table. When you decompile a switch that used a jtable at the
end of that header (e.g. `jtbl_8009387C` for `func_80094A08`), remove that
jtable from `header.s` and let GCC emit it in `title.c.o(.rodata)`.

Linker order is:

```
header.s.o(.text)   /* package header, shrinks as jtables move out */
title.c.o(.rodata)  /* compiler jtables, in decompile order */
title.c.o(.text)
menu.data.s.o(.data)
```

If you leave the hardcoded absolute jtable in `header.s` *and* the compiler
emits its own, the overlay grows by `4 * ncases` and every later VRAM address
shifts — function body can be 100% in a scratch while `build/USA/out/title`
checksum fails.

When decompiling the next header jtable function (`func_800947C8`), drop its
`.word` block from `header.s` the same way so both jtables land only in
`title.c.o(.rodata)` in original order (947C8 then 94A08).
