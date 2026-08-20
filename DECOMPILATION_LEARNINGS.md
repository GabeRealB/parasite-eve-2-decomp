# Decompilation Learnings

Notes on the GCC 2.8.1 (`-O2 -mips1`, aspsx 2.77) toolchain used by this project.
Each entry was verified against real target assembly.

## Pin `ws = arg0` so `move a1, a0` precedes an independent `lui`

`ws = arg0` followed by a CVECTOR copy from a global (`col = D_80093820;
gte_ldrgb(&col)`) lets `-fschedule-insns` lift the `lui %hi` above the copy:

```
addiu  sp, sp, -8
lui    v0, %hi(D_80093820)
move   a1, a0
```

The target keeps the copy first (`move a1, a0` then `lui`). Pin `ws` so the
`move` cannot sink:

```c
ws = arg0;
__asm__ volatile("" : "+r"(ws));
col = D_80093820;
gte_ldrgb(&col);
```

`func_8009AA5C` is the example. Same `+r` pin as `func_8009EAA4`'s `prev = -1`.

## Scope a `| k` temp so the OR reuses the load dest

`temp = save->field_22; obj->field_18 = temp | packed` with a function-level
`temp` that is reused later (e.g. as a later `lhu`) emits `or v0, v1, s0` or
`or s0, v1, s0`. The target is `or v1, v1, s0` / `sw v1` in the `jal` delay.
Give each `| packed` its own block-scope temp so it dies at the store:

```c
{
    s32 temp;
    temp          = save->field_22;
    obj->field_1C = size;
    obj->flags    = 4;
    obj->field_18 = temp | packed;
    func_800E15AC(0, obj);
}
```

`func_80100B78` is the example.

## Pin the next call's 0 to `$a0` so `move a0, zero` precedes independent setup

After a call that clobbers `$a0`, the next `func(0, obj)` wants `move a0, zero`
before an independent `field_C` address and `flags |=`. An unpinned `0` sinks
into an `lhu flags` delay (`ori` / `addiu link` then `move a0, zero`). Pin the
argument, assign 0, then a volatile `+r` so the rest cannot float above it:

```c
register s32 zero asm("a0");
zero = 0;
asm volatile("" : "+r"(zero));
link = (GpRec18*)actor->field_94;
obj->flags |= 0xF200;
func_800E15AC(zero, obj);
```

`func_80100B78` is the example.

## Combine fade-done as `flag == 0 && count <= 0` so the miss jumps into `else if`

A fade-in/fade-out overlay that sets `flag = 0` on the way down and `flag = 1`
on the way up wants the incomplete fade-in (`flag == 0 && count > 0`) to land
on the `else if (flag == 1)` compare (`bgtz` delay `li v0, 1` / `bne s1, v0`),
and the completed fade-in to share `GameMain_SetFrameTiming` / `Task_Kill`
with the fade-out tail (`beq spawnArg, 4` / `j kill`). Nested

```c
if (flag == 0) {
    if (count <= 0) { /* kill */ }
} else if (flag == 1) {
    /* fade-out done */
}
```

proves `flag` is 0 on the miss and jumps to the epilogue instead. Write the
gate as one `&&` so both `flag != 0` and `count > 0` are “else”:

```c
if ((flag == 0) && (count <= 0)) {
    if (arg == 4) {
        GameMain_SetFrameTiming(0);
    }
    Task_Kill(arg0);
} else if (flag == 1) {
    if (count >= 8) {
        /* fade-out done; shared SetFrameTiming / Task_Kill tail */
    }
}
```

`func_800BF738` is the example. TILE RGB must still be stored in *both*
arms of the `< 8` color `if` (see “Per-branch stores”) or `-fschedule-insns`
lifts `addPrim`’s `lui 0xFFFFFF` / `lui 0xE100` above the three `sb`s.

## Nested scopes so a later copy does not sink an earlier split `la`

Four copies of `p = global_list; tbl = global_ptr; if (tbl) for (; p->id != 0xFF; p++)`
reuse of the same `p`/`tbl` locals sinks each list's `lui %hi` into the `lw tbl`
delay (`lui v0` of the list, `addiu a0, v0, %lo` in `beqz tbl`). The target
keeps the split-address form that a *single* copy emits:

```
lui    v1, %hi(list)
lui    v0, %hi(tbl)
lw     a1, %lo(tbl)
nop
beqz   a1, skip
 addiu  a0, v1, %lo(list)
lbu    v1, %lo(list)(v1)
li     v0, 0xFF
beq    v1, v0, skip
 lui    v1, %hi(next_list)    # first 3 copies; last is move a2, v0
```

Give each copy its own block-scope `p`/`tbl` (a macro expansion does the same).
The next list's `lui %hi` then fills the `== 0xFF` delay, with a second `lui`
after the loop for the NULL / fall-through paths. `func_800AE7AC` is the example.

## Write `if (x != K)` so the `== K` body is the `beq` target

`if (state == 0x20) { A } else { B }` emits `bne ..., B` with A first and no
jump after B (B falls into the join). The target wants A physically second:

```
beq    v1, v0, path_eq      # state == 0x20
nop
jal    Text_MeasureWidth    # != path
addiu  s0, v0, 0xB
...
j      join
move   s0, s1
path_eq:
jal    Text_MeasureWidth
move   s0, v0
li     a0, 1
join:
```

Invert to `if (state != 0x20) { B } else { A }`. The `j join` after
`if (width < other) width = other` only appears when the `==` body is
still below it. `func_800CA25C` is the example.

## Inline the first `0x606060`; assign `color` only after a later call

A `color` local reused across two `Text_DrawPrompt` calls is allocated to
`$v1` from the start, because the later call needs `$v0` for the returned
x. That also pulls `lh a1` / `lh a2` *after* the tail-merged join (the
free `$v0` is used for `li v0, 1` early). The target loads the first
color in `$v0`:

```
lui    v0, 0x60
ori    v0, v0, 0x6060
lh     a1, field_1C
lh     a2, field_18
j      join
addiu  a3, a3, %lo(...)
...
sw     v0, 0x10(sp)
li     v0, 1
```

Inline `0x606060` on the first (and one-line) prompt so that temp dies at
the `jal`. Assign `color = 0x606060` only after the middle call, when
`$v0` holds the returned width and the constant naturally lands in `$v1`.
`func_800CA25C` is the example.

## Pin later `$s` regs so an early arg lands in `$fp` *and* is saved

`register ... asm("fp")` (or `"s8"`) uses `$fp` but GCC 2.8.1 does not emit
`sw fp` / `lw fp` — it treats the omitted frame pointer as not live. Nine
live-across-call values want `$s0`–`$s7` plus `$fp`. Pin the *later*
locals (counter, mask, `Display_State`) and leave the incoming arg
unpinned so normal allocation gives it `$fp` with a prologue save:

```
sw     fp, 0x30(sp)
move   fp, a1
sw     s4, 0x20(sp)
move   s4, zero
lui    a1, %hi(...)
sw     ra / s7 / s6 / ...
```

`func_800AC790` is the example.

## Two array pointers so `p++` and `i++` fill different load delays

One pointer through a `0x14`-byte record increments both `$s3` (base)
and `$s1` (base+0xC) together. A second copy (`elem` for offset-0
`tpage`, `cur` for the rest) lets `elem++` fill the `wh` load delay
while `cur++` waits until after both OT-index loads.

A loop counter that is only compared at the backedge sinks into the
later delay (`addiu s4` with `wh` instead of `xy`). `asm volatile("" :
"+r"(i))` after `i++` keeps the increment in the `xy` load delay.
`func_800AC790` is the example.

## Stage `u16 - N` through an `s32` so GCC emits `addiu -N`

`req.x = obj->baseX - 1 + arg1` with `baseX` a `u16` and `req.x` an `s16`
emits `li v1, 0xffff` / `addu`. A named `s32` temp matches the neighboring
text-draw functions and gets `addiu v0, v0, -1` / `addu v0, v0, arg1`:

```c
x     = obj->baseX - 1;
req.x = x + arg1;
y     = obj->baseY - 2;
req.y = y + arg2;
```

Assign `color = 0x606060` in the same branch so `lui a2, 0x60` fills the
`beqz equipped` delay slot. Inlining `req.field_8 = 0x606060` puts the
constant in `$a1` and delays `la a1, D_80097024`. `func_800C22D8` is the
example.

## Save incoming `$a2` first, then split `G_SCRATCH_HEAD` so `lui` sits in the prologue

A 3-arg function that keeps `arg2` in `$s4` and then allocates from
`G_SCRATCH_HEAD` wants:

```
sw     s4, 0x20(sp)
move   s4, a2
lui    v0, 0x1F80
ori    v0, v0, 0x3FC
sw     ra/s3/s2/s1/s0
lw     s2, 0(v0)
```

A 3-arg function that keeps `arg1` in `$s4` and `arg2` in `$s5`, then
allocates from `G_SCRATCH_HEAD` into `$v1` (not `$v0`), wants:

```
sw     s4, 0x20(sp)
move   s4, a1
sw     s5, 0x24(sp)
move   s5, a2
lui    v1, 0x1F80
ori    v1, v1, 0x3FC
sw     ra/s3/s2/s1/s0
lw     v0, 0(v1)
```

Copy `arg1` first, empty non-volatile `asm("" : "+r"(id))` so that store
lands before `arg2`, then `lui`/`ori` with a fake input dependency on
`arg2` pinned to `$s5` / scratch to `$v1`. `func_80100FCC` is the example.

Do not keep a scratch pointer live from alloc to free. A local `scratch`
that is used at both ends is allocated to an extra `$s` register and
grows the frame (`0x30` with `$s5` instead of `0x28`). Write two
independent `G_SCRATCH_HEAD` accesses so the compiler reloads into `$v1`
and can put `lui 0x1F80` in the first-branch jump delay:

```c
*(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD - 8;
/* ... */
*(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
```

`func_800B0278` is the example.

`scratch = (void**)G_SCRATCH_HEAD` first hoists `lui`/`ori` above `sw s4`.
`register s32 thresh asm("s4"); thresh = arg2;` without a use delays
`move s4, a2` into the later `beqz` delay (the copy is only needed on
the call-clobber path). `asm volatile` after the copy keeps `move s4`
early but parks `lui` *after* every prologue `sw`.

Non-volatile `lui`/`ori` with a fake input dependency on the saved arg
keeps `lui` after `move s4` and lets `-fschedule-insns` drop it between
that move and the remaining stores:

```c
register void** scratch asm("v0");
register s32    hi asm("v0");
register s32    thresh asm("s4");

thresh = arg2;
asm("lui %0, 0x1F80" : "=r"(hi) : "r"(thresh));
asm("ori %0, %1, 0x3FC" : "=r"(scratch) : "r"(hi));
```

The later `(s16)thresh < dist` compare must write the cast into `$v0`
(`sll v0, s4, 16` / `sra` / `slt v0, v0, a1`). A dead `$s4` after the
branch becomes `sll s4, s4, 16` in place. Pin `SquareRoot0`'s result
through `$v0` then copy to `$a1` (`move a1, v0`) so the compare and
`ratan2` share that register. `func_80102D20` is the example.

## Pin `three`/`packed`/`flag` so `flags = 3` interleaves with `arg << 8`

`obj->flags = 3; actor->field_124 = (arg1 << 8) | (arg2 | 0x20000)` hoists the
pack immediately after the preceding call. Pin `three` to `$v0`, `packed` to
`$v1`, `flag` to `$v0` (reusing extra's pin after the copy), assign `three = 3`
then `packed = id << 8` then `obj->flags = three`, then `flag = 0x20000`. A
`memory` barrier after `obj->field_C` keeps that block from floating above the
coordinate stores. Keep a dead `$s4` / `$s1` live with `asm volatile("" :: "r"(id))`
so `table[id]` is `sll v1, s4, 1` and `&actor->field_32C` is `addiu v0, s1, 0x32C`.
`func_80100FCC` is the example.

## Split `la` into `$s0` so `%hi` lands in `$v0` and `%lo` is a C `addiu`

`menu = &D_8010E9CC` with `menu` pinned to `$s0` emits `lui s0` / `addiu s0, s0`.
The target wants the two-register form so the `lui` can sit between
`lhu val` and `sh field_2E = 0`, and the `addiu` can fill the `bnez state`
delay slot:

```
lhu    s2, spawnArg1
lui    v0, 0x8011          # adjusted %hi(D_8010E9CC)
sh     zero, field_2E(s1)
lw     v1, state
nop
bnez   v1, not_zero
 addiu  s0, v0, 0xE9CC     # %lo
```

Dest `$s1`/`$s2` naturally uses `$v0` as the `lui` temp (`func_800C8B40`).
Dest `$s0` does not. `asm("addiu %0, %1, %%lo(...)")` will not fill a
delay slot; a C add will.

`%hi(sym)` + a C add of the signed `%lo` plus a `.reloc` LO16 is wrong:
ld *adds* `%lo` to the existing immediate (`0xE9CC + 0xE9CC = 0xD398`).
Emit the already-adjusted pair instead (`0x80110000 + (s16)0xE9CC`):

```c
register char* hi asm("v0");
register UiList* menu asm("s0");

asm volatile("lui %0, 0x8011" : "=r"(hi) : "r"(val));
obj->field_2E = 0;
state = task->state;
menu = (UiList*)(hi + (s16)0xE9CC);
if (state == 0) {
```

`func_800C9654` is the example.

## Split `la` of a later-reused table so `%hi` lands in `$v1` and `%lo` in `$a1`

A global table kept across a loop that also needs `$a1` for a `u16` compare
(`andi a1, v1, 0xffff`) wants:

```
lui    v1, %hi(D_table)
sw     ra/s1/s0
lbu    v0, idx(sess)
addiu  a1, v1, %lo(D_table)
...
move   t1, a1
andi   a1, v1, 0xffff
```

`p = D_table` unpinned lookahead-allocates the `la` dest to `$t1` (the
loop-resident copy), so you get `addiu t1, v1, %lo` and no `move`.
`register ... asm("a1")` on the same pointer forces `lui a1` / `addiu a1, a1`.

Form the address with a non-volatile split pair and read the first index
between them:

```c
register s32         hi asm("v1");
register GpBit2Bank* tmp asm("a1");
register GpBit2Bank* banks asm("t1");

sess = (GameSessionFrom4*)&Mc_SaveData.field_4;
asm("lui %0, %%hi(D_8010D230)" : "=r"(hi));
idx8 = sess->field_3;
asm("addiu %0, %1, %%lo(D_8010D230)" : "=r"(tmp) : "r"(hi));
lists = tmp[idx8].field_0;
...
banks = tmp;
```

Non-volatile (not `asm volatile`) lets `-fschedule-insns` hoist the `lui`
above the prologue `sw`s and keep `addiu` after the `lbu`. `volatile` parks
the `lui` *after* those stores. `func_800B6950` is the example.

## Expand `* 100` so the last `<< 2` can land in a pinned `$s1`/`$s2`

`x * 100` strength-reduces to `((x*2+x)*8+x)*4`. An unpinned dest often
takes `$s2` first (highest free callee-saved), swapping the two results.
Pinning the dest to `$s1` makes the *whole* chain accumulate in `$s1`
(`sll s1, v0, 1` / `addu s1, s1, v1` / …) instead of the target's
`sll v0, v1, 1` … `sll s1, v0, 2`.

Keep a `$v0` temp for the intermediates and write only the last shift
into the pinned dest:

```c
register s32 scaled asm("v0");
register s32 val1 asm("s1");
register s32 val2 asm("s2");

scaled = (temp2 << 1) + temp2;
scaled = (scaled << 3) + temp2;
val1   = scaled << 2;
scaled = (temp4 << 1) + temp4;
scaled = (scaled << 3) + temp4;
val2   = scaled << 2;
```

The same `$v0` pin also forces `ret * 8` as `sll v0, a0, 3` rather than
clobbering `$a0` in place. `func_800A1634` is the example.

## Pin the next object pointer to `$a2` after a 3-arg call

`ApplyTransposeMatrixLV(m, v0, v1)` leaves `$a2` free. The target then
reloads a global into that same register so the later field walk and the
next call reuse it:

```
jal   ApplyTransposeMatrixLV
 addiu a0, a0, 0x24
lw    a2, %lo(D_80115448)(s2)
lhu   v0, 0x10(s0)
lw    v1, 0(a2)
lhu   a0, 0x14(a2)
```

A named `p = D_80115448` without a pin lands in `$a3` instead. That
shifts every subsequent field load and the second call's
`lw a0, 0(p)` / `move a2, out`. Pin the pointer:

```c
register GpGridParams* p asm("a2");

ApplyTransposeMatrixLV(&D_80115448->field_0->workm, in, out);
p = D_80115448;
```

Also pass `(VECTOR*)(head - 0x20)` (not `&block->pos0`) so the follow-up
call is `addiu a0, s1, -0x20` from the original scratch head. Write the
independent `pos.vy = 0` *after* the `pos.vx` store so `-fschedule-insns`
lifts `sw zero` between `addu` and `subu`. `func_800DEAFC` is the example.

## Keep the raw table pointer so it stays in `$a1` until after the NULL check

Loading a per-stage name table and immediately folding it into the
final string pointer:

```c
text = D_8010F0B8[stage - 1];
if (text != NULL) {
    text = text + (room * 0x20 - 0x20);
```

assigns that load to `$s2` (stealing `arg0` from `$s2` into `$s3`) and
emits `addu / addiu -0x20` instead of `addiu -0x20 / addu`. Keep the
table entry in its own temp so the `lw` stays in `$a1`; index a
32-byte record with `arr[i - 1]`:

```c
names = D_8010F0B8[stage - 1];
if (names != NULL) {
    text = names[room - 1].text;
```

`arr[i - 1]` on a 0x20-byte struct is `sll 5` / `addiu -0x20` / `addu`
in the `bnez state` delay slot. `func_800D1BAC` is the example.

## Index each table in its `if` arm so `la` lands in `$v1`

Selecting one of two pointer tables and then indexing:

```c
if (mode == 0 || mode == 2) {
    table = D_8010F9F4;
} else {
    table = D_8010FA0C;
}
rec = table[stage];
```

emits `lui v0, %hi(...)` / `addiu v1, v0, %lo(...)` and fills the
`bne` delay slot with the else `lui`. The target wants a nop in that
delay slot and a same-register `la`:

```
bne   a1, v0, else
 nop
lui   v1, %hi(D_8010F9F4)
j     join
 addiu v1, v1, %lo(D_8010F9F4)
else:
lui   v1, %hi(D_8010FA0C)
addiu v1, v1, %lo(D_8010FA0C)
join:
sll   v0, a2, 2
addu  v0, v0, v1
lw    a1, 0(v0)
```

Write the index in both arms so the table address is the phi:

```c
if (mode == 0 || mode == 2) {
    rec = D_8010F9F4[stage];
} else {
    rec = D_8010FA0C[stage];
}
```

`register ... asm("v1")` on the table pointer also works but is
unnecessary. `func_800DB128` is the example.

## if/else on the same field keeps the phi in `$v0`; a ternary steals `$t0`

Both arms writing the same field (`p->clut = 0x3C09` / `0x3C01`) let GCC emit
the classic delay-slot default plus override, then one store through `$v0`:

```
lw    v1, 0xc(a0)
li    v0, 1
bne   v1, v0, join
 li   v0, 0x3c01
li    v0, 0x3c09
join:
sh    v0, 0xe(a3)
```

A ternary or a named temp (`clut = cond ? 0x3C09 : 0x3C01`) allocates that
value to `$t0`/`$t1`. Incoming `a1`/`a2` that must be saved then shift
(`a1→t2`, `a2→t1` instead of `a1→t1`, `a2→t0`), even when the instruction
stream is otherwise identical. `asm("")` after the store keeps it early but
does not fix the coloring.

A named temp that matches in a shorter sibling (`flag = arg->field;
if (flag == 0) flag = 0x38; else flag = 7; dest = flag`) can flip the
same phi to `$v1` once extra callee-saved regs are live (`s3` for
`extra` in `func_80104B54`). Writing both constants onto the destination
field recovers `$v0`.

```c
if (arg0->field_C == 1) {
    p->clut = 0x3C09;
} else {
    p->clut = 0x3C01;
}
```

`func_800C0B98` is the example.

The same field-in-both-arms rule applies to `DR_TPAGE` blend codes.
A named `code` temp is hoisted as default+override (`lui`/`ori` 0xE1000220
early, then only the 0x240 arm after the branch — no `j`) and lands in
`$t2`, so `setlen` / `sw code` interleave with `addPrim`. Writing both
constants onto `dr->code[0]` (and `setlen(dr, 1)` in each arm) CSE's
`setlen` after the join and emits the two-`lui` jump if/else in `$v1`:

```c
if (t->spawnArg2 == 0) {
    setlen(dr, 1);
    dr->code[0] = 0xE1000240;
} else {
    setlen(dr, 1);
    dr->code[0] = 0xE1000220;
}
```

`func_800B1EFC` is the example; `Display_StepFadeOverlay` uses the same
shape (its branch delay is a shared `lui 0xE100` because `D_80071190`
was already incremented).

## Independent `= 0` store last so it fills a stack-arg load delay

A late `lw` of a stack argument (`arg6 << 4` into two halfwords) wants an
independent `sb zero` in its load delay slot:

```
lw    v0, 0x38(sp)
sb    zero, 0x17(s1)
sll   v0, v0, 0x4
sh    v0, 0xe(s1)
sh    v0, 0xc(s1)
```

Writing `slot->field_17 = 0` *before* `val = arg6 << 4` lets GCC hoist the
constant store and leaves `lw / nop / sll`. Write the zero store *after*
the uses of that stack arg; `-fschedule-insns` lifts `sb zero` into the
load delay and keeps the two `sh`s together.

```c
val            = arg6 << 4;
slot->field_E  = val;
slot->field_C  = val;
slot->field_17 = 0;
```

`func_800B4538` is the example. The same tail is in `func_800B4114` /
`func_800B47A8`.


## `s16` step in `$a0`, extra copies, sequential clamp for `bgez`

A pad-held increment that later reuses the same register as a decay step
needs the step to be `s16` so it lives in `$a0` (now-dead first arg). A
`s32` step takes `$v1` and spills the saved button mask out of `$v1`.

The decay path `sra`s the s16 field (`lhu` / `sll 16` / `beqz` / `sra 19`)
then wants two copies plus a third register for the min-step clamp:

```
sra   v0, v0, 0x13
move  a0, v0
move  v1, v0
...
bgez  v1, pos
 li   v0, 0x40
li    v0, -0x40
move  a0, v0
```

Assign the shift to an `s32`, copy it into both the `s16` step and a
compare temp, then write the clamp through the shift temp — not back
into the compare temp:

```c
s16 delta;
s32 val;
s32 temp;

val   = actor->field_6A >> 3;
delta = val;
temp  = val;
if (ABS(temp) < 0x40) {
    val = 0x40;
    if (temp < 0) {
        val = -0x40;
    }
    delta = val;
}
```

`if (temp >= 0) val = 0x40; else val = -0x40` flips to `bltz` with the
constants swapped. Assigning the clamp to `temp` (already in `$v1`)
emits `li v1` instead of `li v0`. `func_80109720` is the example.

## `s16` divisor after `s32` `>> 3` and zero-clamp

A scale derived from an `s16` (`sll 16` / `sra 19`) that is then
clamped `0 → 1` and used as a signed divisor wants the shift in an
`s32` and the clamped value in an `s16`:

```
sra   v0, v0, 0x13
bnez  v0, use
 move v1, v0
li    v1, 1
andi  a0, ..., 0xff
sll   v0, v1, 0x10
sra   v0, v0, 0x10
div   zero, a0, v0
```

```c
s16 scale;
s32 temp;

temp = arg >> 3;
if (temp == 0) {
    scale = 1;
} else {
    scale = temp;
}
end = (byte & 0xFF) / scale;
```

Keeping `scale` as `s32` (or writing `(s16)s32_scale` after GCC already
knows the value fits in 16 bits) drops the `sll`/`sra 16` recast, writes
the shift into `$v1` instead of `$v0`, and swaps saved-arg registers.
`func_800E8E00` is the example.



## Zero the s16 loop index before independent table walks so it takes `$t0`

A search loop written as `s16 idx; if (count > 0) { idx = 0; do { ... } }`
gives the incoming arg `$t0` and the index `$v1`. The target wants the
opposite: arg saved in `$t1`, `idx` in `$t0`, and `idx++` as the s16 CSE
`addiu v0, t0, 1` / `move t0, v0` (not `addiu t0, t0, 1`).

Initialize `idx = 0` at function entry, before the table lookups. That
raises the index's allocation priority so it wins `$t0`. GCC sinks the
zero into the `blez count` delay slot, so there is no extra instruction:

```c
s16 idx;
idx = 0;
sess  = (GameSessionFrom4*)&Game_Session->field_4;
limit = *(s16*)&table40[...];
bytes = table54[...];
if (limit > 0) {
    do {
        if (bytes[idx] == (u8)arg0) {
            return idx + 1;
        }
        idx++;
    } while (idx < limit);
}
```

`func_800ACEBC` is the example. `register s16 idx asm("t0")` got the
register but rewrote the increment in place and stuck at 96%.

## 18-byte MATRIX rotation copy: `u8[16]` + trailing `s16`, not `u8[18]`

An 18-byte assignment (MATRIX `m[3][3]`) that the target copies with four
`lwl`/`lwr` pairs plus a final `lh`/`sh` is **not** `u8 data[0x12]`. That
alignment-1 object emits `lb`/`sb` for the last two bytes.

Give the helper alignment 2 and a halfword tail:

```c
typedef struct {
    u8  data[0x10];
    s16 field_10;
} GBytes18;

*(GBytes18*)dst = *(GBytes18*)src;
```

`s16 data[9]` is the same size/alignment and also works, but the mixed
layout matches the existing `GBytes4`/`GBytes8` "unaligned word chunks +
remainder" pattern. A word-aligned `MATRIX` assignment uses `lw`/`sw`
instead of `lwl`/`lwr`.

`func_800A8A48` is the example (rotation to `D_80070E44`, then a separate
`VECTOR3` assign of `mtx.t` to `D_80070F28`).

## 0x50-byte `GsCOORDINATE2` assign needs word alignment

A 0x50-byte struct copy that the target does as five aligned 16-byte
`lw`/`sw` chunks (`t1`..`t4`, dest in `$v0`, end `src+0x50` in `$a0`) is
`*dest = *src` of a word-aligned type (`GsCOORDINATE2`, or `s32 data[0x14]`).

`u8 data[0x50]` has alignment 1, so GCC emits an `or`/`andi 3` check plus
an `lwl`/`lwr` fallback. Keep the object 4-aligned.

`func_8010C1FC` is the example (`GpActorD4.coord = *extra->field_8`).

## 56-byte assign needs a word-aligned member, not just size `% 4 == 0`

A 0x38-byte stack copy that the target does as three aligned 16-byte
`lw`/`sw` chunks plus an 8-byte tail (`dest` in `$a1`, `src+0x30` in
`$v0`) is `rec = table[i][j][k - 1]` of a 4-aligned type.

`byte pad[0x36]; u16 field_36;` is the same size but only 2-aligned, so
GCC emits `andi src, 3` plus an `lwl`/`lwr` fallback. A leading `s32`
(or any 4-aligned member) is enough.

`func_800ADF3C` is the example (`D_8010CB90` / `GpCb90Rec`).

## Barrier after scratch alloc so GTE setup cannot fill load-delay nops

A scratch-head alloc followed by `gte_SetRotMatrix(&obj->field->workm)` is
independent of the alloc. GCC hoists `lw v0, 8(a0)` into the
`lw v1, 0(scratch)` delay slot and folds the `sw` later:

```
lw    v1,0(a3)
lw    v0,8(a0)
addiu a2,v1,-0x30
addiu v0,v0,0x24
sw    a2,0(a3)
```

The target keeps the alloc sequential (`lw / nop / addiu / sw`) and only
then loads the matrix pointer (`lw / nop / addiu`):

```
lw    v1,0(a3)
nop
addiu a2,v1,-0x30
sw    a2,0(a3)
lw    v0,8(a0)
nop
addiu v0,v0,0x24
```

Store the new scratch pointer, then emit a memory barrier before the GTE
setup:

```c
*scratch = vec;
__asm__ volatile("" ::: "memory");
gte_SetRotMatrix(&((GsCOORDINATE2*)arg0->field_8)->workm);
```

`func_800E08CC` is the example. Without the barrier the body still
matched and only those two delay-slot nops were gone (90.5%).

## Capture the table pointer before `idx + (byte - K)`

`table[arg2 + byte - 2]` reassociates: GCC either folds the subtract into
the load (`lbu -2(ptr)`) or into the symbol (`%lo(table-2)`). The target
subtracts from the loaded byte first, then adds into the still-live arg:

```
lui    v1,%hi(table)
lui    v0,%hi(global)
lbu    v0,%lo(global+off)(v0)
addiu  v1,v1,%lo(table)
addiu  v0,v0,-2
addu   a2,a2,v0
addu   a2,a2,v1
lbu    v0,0(a2)
```

Assign the table to a local, then the subtract, then index:

```c
table = D_80112DFC;
type  = Wip_SysConfig.field_26 - 2;
task  = Task_Spawn(7, table[arg2 + type] + arg3 * 2 + arg1, 0, 0);
```

`func_80104258` is the example. Inlining the table stuck at 96.8%
(`lbu -2`) or 97.4% (`%lo(D_80112DFC-2)`).

## Split two `arg0->actor` reloads so they take `$a3` then `$v0`

Reusing one local for both `actor = arg0->actor` reloads (across
`func_800B3F84` / `func_801038F8`) merges the live range. GCC then
parks it in `$t0` because the same name is still live after the first
call. Overwriting the original saved `actor` puts the second reload in
`$s0` instead.

The target wants the first reload in `$a3` (it becomes
`&inner->field_7A8`, the 4th arg) and the second in `$v0` (store-only
scratch before `func_80103A18`):

```
lw    a3,0x1c(s1)
...
addiu a3,a3,0x7a8
jal   func_800B3F84
...
lw    v0,0x1c(a0)
sh    zero,0x954(v0)
```

Use three distinct locals: keep the original actor in a saved register
for the later `field_914` kill, one local that dies at `func_800B3F84`,
and a third that exists only for the post-`func_801038F8` stores:

```c
actor = arg0->actor;
/* Task_Kill(actor->field_91C); actor->field_91C = NULL; */
inner            = arg0->actor;
inner->field_93A = table[idx] + addend;
inner->field_928 = ptrs[inner->field_93A];
func_800B3F84(..., &inner->field_7A8, ...);
func_801038F8(arg0, 1);
next            = arg0->actor;
next->field_954 = 0;
/* ... */
func_80103A18(arg0, 1, 0, 4);
/* Task_Kill(actor->field_914); */
```

`func_8010B674` is the example. One reused reload stuck at 98.8%
(`$t0`); overwriting `actor` stuck at 99.3% (`$s0` for the stores).

## Capture `list->funcs` before writing both vtable slots

`menu->funcs[0] = A; menu->funcs[1] = B;` reloads the function-table
pointer for the second store (`lw v1, %lo(list)(a1)` twice) and parks
`%hi(list)` in `$a1`. The target loads the pointer once:

```
lui   v1,%hi(list)
addiu s1,v1,%lo(list)
lw    v1,%lo(list)(v1)
sw    A,0(v1)
sw    B,4(v1)
```

Assign the table to a local first so the second store reuses `$v1` and
the `lui` stays in `$v1`:

```c
table    = menu->funcs;
table[0] = func_A;
table[1] = func_B;
```

`func_800D2384` is the example. Direct `menu->funcs[i]` stuck at 98.8%
with an extra load and the wrong `lui` register.

## Reuse the extra pointer so `lw v0,8(v0)` feeds the `+ N` delay slot

`bone = (T*)extra->field_8; f(bone + N, ...)` allocates `bone` to `$a0`
(the first-arg dest), so the target's

```
lw    v0,8(v0)
...
jal   f
addiu a0,v0,off
```

becomes `lw a0,8(v0); addiu a0,a0,off`. Extra was already in `$v0`; a
new local is free to coalesce with `$a0`.

Overwrite the same local, then add through that pointer:

```c
extra = arg0->extra;
...
extra = (GameActorExt*)extra->field_8;
func_801040A0((GsCOORDINATE2*)extra + 4, coord, rot);
```

`func_8010BE5C` is the example. A separate `bone` stuck at 99.9% with
only those two registers different.

## Assign `val = ratan2(...) - field` before the wrap helper

`val = wrap(cur, ratan2(...) - facing)` (one call) does `lhu facing`
and keeps the subtract in `$v0`, so the helper's second arg and the
later clamp live in `$a0`. The target instead does

```
lh    v1,facing
lh    a0,cur
subu  a1,v0,v1
sll   a1,a1,16
jal   wrap
sra   a1,a1,16
sll   v0,v0,16
sra   a1,v0,16
```

Split it. The s32 difference is a real value (forces `lh`), and
reusing `val` as the helper's second arg and its result keeps the
clamp in `$a1`:

```c
val = ratan2(dx, dz) - actor->field_52;
val = func_80103E7C(actor->field_6A, val);
```

`func_8010BE5C` is the example. The fused call stuck at 96.3% with
`lhu` / `$v0` subtract / clamp in `$a0`.

## Assign `mask = 1` before `ptr->arr[i]` so `li v0,1` stays live

`flags = ptr->arr[i]; mask = 1 << bit` (or `1 << bit` inlined after the
load) uses `$v0` for the scaled index (`sll v0, which, 2`). `which` then
lands in `$a0`, `li v0,1` happens *after* the load, and `bit` is pushed
out of `$a2`.

The target instead does

```
li    v0,1
sll   v1,v1,2
addu  a0,ptr,v1
lw    a1,off(a0)
sllv  v1,v0,a2
```

Assign `1` to an `s32` *before* the indexed load. That pins the constant
in `$v0` across the address calc, so `which` shifts in place in `$v1`
and `1 << bit` is `sllv v1, v0, a2`. Write the test as
`(mask << bit) & flags` (not `flags & (mask << bit)`) so the `and` is
`and v0, v1, a1`:

```c
s32 mask;
s32 flags;

mask  = 1;
flags = bank->field_4[which];
if (((mask << bit) & flags) == 0) {
    bank->field_4[which] = flags | (mask << bit);
}
```

`func_800ABF1C` is the example. `flags = bank->field_4[which];
mask = 1 << bit` stuck at 98% with only those registers swapped.

## Hoist `&Global` into a saved register with a local pointer

A single `Global.field = x` after earlier calls rematerializes the
address at the store (`lui; sh %lo(Global+off)`). The target instead
computes `&Global` in the prologue (`addiu s1, %lo(Global)`) and stores
through that saved register (`sh v0, off(s1)`). Without the hoist, the
task argument also drops from `$s2` to `$s1` and the frame shrinks.

Assign the address to a local at the top, then store through it:

```c
CdCmdQueue* queue;

queue = &CdCmd_Queue;
...
queue->field_22A = D_8011565C;
```

`func_800E646C` is the example. The direct
`CdCmd_Queue.field_22A = D_8011565C` stuck at 93.9% with only that
address and the extra saved register different.

## Write a two-global fail tail through the globals, not hoisted pointers

A shared no-match tail that zeros two BSS objects wants:

```
lui    v0, %hi(Mc_SaveData)
lui    v1, %hi(Game_Session)
lw     a0, %lo(Game_Session)(v1)
addiu  v0, v0, %lo(Mc_SaveData)
sb     zero, field(v0)
sb     zero, field(a0)
lw     v1, %lo(Game_Session)(v1)
sb     zero, field(v1)
```

Hoisting `save = &Mc_SaveData; session = Game_Session` clumps `lui`+`addiu`
before the session load, or delay-fills the wrong `lui` into the incoming
`beqz`. Store through the globals by name so `-fschedule-insns` splits the
first `la` around the second:

```c
Mc_SaveData.field_13    = 0;
Mc_SaveData.field_5C7   = 0;
Game_Session->field_124 = 0;
Game_Session->field_125 = 0;
```

`func_800ABA4C` is the example. The hoisted-pointer form stuck at 99.6%
with only that `addiu` / `lui Game_Session` pair swapped.

## Reload `&Global` into a second local so the first pointer can die

A sibling of a short last-ref helper (same `p = &Global` prologue, then
`Global.field_0 = 2` via the `%hi` register) must let that first pointer
die before later uses of the same address. Reusing `p` keeps it live
through the last-ref block, so GCC stores `field_1` early (`li v0,0x3C;
sb v0,1(a0)` before `lui Game_Session`) and loads `Game_Session` into
`$v0` instead of `$v1`.

Use a new local for the later stores:

```c
p = &D_801153F0;
if (p->field_6 != 0) {
    p->field_6--;
    if (p->field_6 == 0) {
        D_801153F0.field_0 = 2;
        p->field_2         = 0;
        p->field_3         = 0;
        p->field_1         = 0x3C;
        /* ... */
    }
    rec = arg0->field_20->field_50;
    if (rec != NULL) {
        q = &D_801153F0;
        q->field_8 += rec->field_6;
    }
}
```

`func_800DB558` is the example. Reusing `p` for the second half stuck
at 94.4% with only that last-ref schedule and the `$a0`/`$v1` reload
different.

## Assign `lhs = *p = expr` so the temp stays in `$v0`

`vec = expr; *scratch = vec` computes `expr` straight into the dest
register (`addiu s1, s0, -K; sw s1, 0(v1)`). The target instead does

```
addiu v0, s0, -K
move  s1, v0
sw    v0, 0(v1)
```

and, because that temp reuses `$v0`, any earlier value living there
(e.g. `extra`) must be consumed first (`lw a0, 8(v0)` before the
`addiu`). The two-statement form never conflicts, so the loads reorder.

Write the store and the dest assignment as one expression. The store
keeps the value in `$v0` and the dest is a copy:

```c
vec = *scratch = (ScratchTurn*)(head - 0x14);
```

`func_8010BD88` is the example. `vec = (ScratchTurn*)(head - 0x14);
*scratch = vec` stuck at 96% with only those three instructions (and
the extra/head load order) wrong.

## Write the `|=` first so its address takes `$a1`

Two independent stores of different globals fight over `$a0`/`$a1`. A
load-modify-store (`flags |= mask`) keeps its address live across the
load, `or`, and store, so it wins `$a0` if it is generated second:

```
lui  a1, %hi(mode)     /* simple store */
lui  a0, %hi(flags)    /* |= */
```

Write the `|=` first. Its address is allocated later and lands in `$a1`,
and the simple store takes `$a0`:

```c
D_8011570A |= mask; /* lui a1 */
D_80115714 = 1;     /* lui a0 */
```

`func_800E9BDC` case 3 is the example. `D_80115714 = 1; D_8011570A |= mask`
stuck at 99.3% with only those two addresses swapped.

## Assign `u16 - K` to an `s32` before the `s16` store

`req.y = arg0->baseY - 3 + arg2` (u16 field, s16 dest) emits
`li v1,0xfffd; addu` — GCC does the add in 16-bit and materializes
`(u16)-3`. `req.y = arg0->baseY + arg2 - 3` is closer (`addu` then
`addiu -3`) but still reassociates K onto the sum.

Assign the subtraction to an `s32` first. The 32-bit dest cannot use
the wrapped constant, so the load stays `lhu; addiu -K` and the later
`+ arg2` is a separate `addu`:

```c
s32 y;

y     = arg0->baseY - 3; /* lhu; addiu -3 */
req.y = y + arg2;        /* addu s2 */
```

`func_800CDBEC` is the example. A cast alone (`(s32)arg0->baseY - 3`)
folds away before RTL.

## Assign `(div + K)` inside the add so K stays on the dividend

`(s8)u8_field << 6` in a larger add emits `lbu; sll 24; sra 18`. Assigned
to an `s32` temp first, the same cast becomes `lb; sll 6` and the loads
reorder. And `tpage + ((x + 1) / 2 + K)` reassociates: GCC folds K onto
the tpage (`addiu v1, K` before the divide) or onto the sum (`addu; addiu
v1, K`) instead of onto the dividend (`addiu v0, K; addu v1, v0`).

Assign the `(div + K)` term inside the add. That forces `lbu/sll/sra` for
the tpage and pins K onto the divide result:

```c
arg1->x = ((s8)extra->field_24 << 6) + (x = (arg2->x + 1) / 2 + 0x180);
```

`func_800DB28C` is the example. The natural
`tpage + (x + 1) / 2 + 0x180` stuck at 94% with only the `addiu 0x180`
moved.

## New temp (not the index) so `lhu` targets `$a1`

`table[arg1] + extra - 1` as a `Task_Spawn` argument emits `lhu a1` but
reassociates to `addu a1, extra; addiu a1, -1`. Assigning
`arg1 = table[arg1] - 1` pins `-1` on the load (`addiu` then `addu`) but
uses `$v0` as the load dest because `arg1` is still live as the index.

A *new* `s32` (not the index variable) lets the load overwrite `$a1` after
the address is computed:

```c
s32 type;

type = D_80112DF4[arg1] - 1; /* lhu a1; addiu a1, -1 */
task = Task_Spawn(7, type + arg2, arg3, 0);
```

`func_80104364` is the example. Reusing `arg1` stuck at 99.7% with only
`lhu v0` / `addiu a1, v0, -1` different.

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

The same wider temporary is how you *force* CSE the other way: a switch's
SImode compare constant (`li v0,K` in the first `beqz` delay slot) will not
reuse itself for a later HImode/QImode store (`p->field = K` rematerializes
`li v0,K` after the next load). Assign K to an `s32` first, then store that:

```c
s32 flag;

switch (p->field_95E) { /* lhu; beqz; li v0,1; beq */
case 0:
    flag = 1;
    p->field_95E = flag; /* sh v0 — same reg, nop in the load delay */
    ...
}
```

`func_80109684` is the pure example. A bare `p->field_95E = 1` stuck at ~94%
with an otherwise identical switch.

## Assign the delay-slot default first so `bnez` keeps the `== 0` overwrite

When the target does

```
lw    v0, field
nop
bnez  v0, skip
li    a2, DEFAULT    /* delay: always */
li    a2, OTHER      /* only if field == 0 */
```

an if/else with the nonzero arm first emits the inverted `beqz` + `li OTHER` in
the delay slot. Assign the default, then overwrite on `== 0`:

```c
arg2 = 1;
if (p->field_934 == 0) {
    arg2 = 6;
}
```

`func_801094D4` is the example. `if (p->field_934) { arg2 = 1; } else { arg2 = 6; }`
stuck at 96% with only that branch flipped.

## Write the `== 0` arm first so a sibling store rematerializes the field

When the target reloads a u8 after `beqz` (`lbu field; beqz; lbu field`) and
uses `bnez` to the non-zero store, the natural

```c
if (p->field) {
    item = table[p->field + K].id; /* CSE reuses the first lbu */
    if (item) {
        p->out = item + C;
    } else {
        p->out = 0;
    }
} else {
    p->out = 0;
}
```

folds the two zero stores together: one load, `beqz` to the shared `sb zero`.
The sibling `p->out = 0` is never in the same block as the second read, so CSE
keeps the first `lbu`.

Write the empty arm first. That store through `p` kills CSE, the else
rematerializes the field, and the inner `== 0` / else-value pair emits
`bnez` to the value store with `sb zero` in the `j` delay slot:

```c
if (p->field_21 == 0) {
    p->field_22 = 0;
} else {
    item = D_80072330[p->field_21 + 0x7F].field_0; /* second lbu */
    if (item == 0) {
        p->field_22 = 0;
    } else {
        p->field_22 = item + 0x61;
    }
}
```

`func_800BBF1C` is the example. `if (p->field_21) { ... if (item) ... }` stuck
at 88% with the load reused and the inner branch inverted.

## `volatile` copy of `p` so a range check reloads the same u8

When the target does

```
lbu   v0, off(p)
addiu v0, v0, -K
sltiu v0, v0, N
beqz  v0, zero
lui   v0, 0x5555
lbu   a0, off(p)
```

the second `p->field` is CSE'd into the first load. GCC keeps the original
byte in `$a0` so the subtract can use `$v0`, and the reload disappears.
Writing `*out = 0` first does not kill that CSE — the store is through a
different pointer, not `p`.

Assign `p` to a `volatile` typed copy in the else and read the field
through that. The qualifier forces both `lbu`s and leaves `p` in `$a0`:

```c
} else {
    vp = p;
    if ((u32)(vp->field_22 - 0xA) < 6U) {
        *arg0 = ((vp->field_22 - 1) % 3) << 24;
        if (*arg0 < 0) {
            *arg0 = 0;
        }
    } else {
        *arg0 = 0;
    }
}
```

A second non-volatile pointer (`q = &Global` in the else) is still CSE'd.
`func_801095BC` is the example. The plain `p->field_22` pair stuck at 97.8%
with only that reload missing.

## Write the `== 0` early-return first so a sibling `== K` beq's back to it

When two conditions store the same value and return, the combined

```c
if ((id & FLAG) && ((id & MASK) != K)) {
    p->out = expr; /* % 10U, etc. */
} else {
    p->out = 0;
}
```

emits `beqz` on the flag and places the shared `sb zero` after the expr tail.

Write the `== 0` return first, then the `== K` return with the same store.
GCC merges them: `bnez` skips the first `jr / sb zero`, and `li K; beq` jumps
back to that store.

```c
if ((id & 0x8000) == 0) {
    p->field_5D = 0;
    return;
}
if ((id & 0x3F) == 0x31) {
    p->field_5D = 0;
    return;
}
p->field_5D = D_80114C08.field_0 % 10U;
```

`func_800E301C` is the example. The `&&` / else-zero form stuck at 87% with
only the branch inverted and the zero store at the end.

## Store `== K` in an `s32` and test `== 0` so the false return is `beqz`

`if (a && b == K) return X; return Y;` becomes two compare-branches
(`bne` / `li K; bne`). The target instead materializes the combined
predicate (`move v0,zero` in the first `bne` delay slot, then
`lbu; xori K; sltiu 1`) and `beqz` to Y.

Assign the `==` to an `s32` so it is a *value*, not a branch, then write
the false return first:

```c
s32 cond;

if (word != MAGIC) {
    cond = 0;
} else {
    cond = p->field == 4; /* xori / sltiu, not li/bne */
}
if (cond == 0) {
    return y; /* beqz */
}
return x;
```

`if (cond) { return x; } return y;` flips to `bnez` and swaps the two
tails. `func_800A746C` is the example; `func_800A74C4` is the same
predicate returned as the `s32` itself.

## Comma-assign so `li sN,K` lands only on the else path

When K is compared on one side of a short-circuit and later stored from a
callee-saved on the other side, assigning K at function entry (`s32 flag = 1`)
pulls `li sN,1` into the prologue and shifts the whole frame.

The target instead does `li s3,1` only after the first `&&` fails, then
`beq v0,s3` and (on the other branch) `sb s3,field`. A comma operator in the
later `||` term assigns at that point without evaluating it when the first
term is already true:

```c
s32 flag;

if ((node != NULL && state < 2) || (flag = 1, state == flag) || extra) {
    /* uses K only as the compare; s3 is unset on this path if term 1 won */
} else {
    p->field = flag; /* sb s3 */
}
```

`func_80108FD4` is the example. A MIPS delay-slot `sb zero,field` on
`beqz node` also means the clear always runs once the outer flag is set —
write `p->field = 0` *before* `if (node != NULL)`, not inside it.

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
*org = GPU_OT_END_PRIM; /* 0xFFFFFF stays in $a1 */
size /= 2;                   /* signed /2 after the store */
saved      = Gpu_CurrentOt;
Gpu_CurrentOt = ot[i].org;      /* second load of org — do not reuse `org` */
D_80071190 = base + i * size;
```

Putting `saved = Gpu_CurrentOt` immediately after `*org = …` steals the delay
slot for `%hi(Gpu_CurrentOt)` and parks the constant in `$a0` instead. Computing
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

`SndBankSlot_Free` needs this form so `SndHeap_Free` can take `p->field_0` with the
base already in `$v0` before the stride multiply lands in `$s0`.

## Copy `arg1` to a dest local so `arg0` keeps `$s0`

When both pointer parameters are live across calls, GCC 2.8.1 gives `$s0` to
whichever one is used first (often `arg1`, via `arg1->field`). The target
usually wants the opposite: `$s0 = arg0`, `$s1 = arg1` (save `$s1` / `move
s1,a1` first, then save `$s0` / `move s0,a0`).

Copy the second argument into a local and only touch the object through that
name. Incoming `arg0` is then the first param that needs a callee-saved
register:

```c
GsCOORDINATE2* dest;

dest = arg1;
if (dest->sub != arg0) {
    func_A(arg0);
    func_A(dest);
    dest->sub = arg0; /* also fills the next jal delay slot */
    func_B(&arg0->workm, &dest->workm, &dest->coord);
    dest->flg = 0;
}
```

`func_800B57EC` is the pure example. Using `arg1` directly swapped `$s0`/`$s1`
(~89%) even with the rest of the body identical.

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

When the target is signed `slti`/`bnez` (not `sltiu`) and the body also
increments a second counter, a pointer `p++` plus `i++` still becomes
`li N-1; bgez`. Index the typed array instead (`func_800D9618`):

```c
count = 0;
for (i = 0; i < 8; i++) {
    if (D_80114F30[i].field_0 != 0) {
        count++;
    }
}
```

GCC strength-reduces `&arr[i]` to `p++` but keeps the incrementing `i` and
`slti`. A `while (1) { ...; i++; if (i >= N) break; }` also matches, but
array indexing is the natural form.

The same `&arr[i]` form is required when the loop starts at a runtime index
(`i = arg0`) and the target computes the scaled address *after* the entry
`slti`/`beqz` (`lui`/`addiu`/`sll`/`addu` in the delay slot). Putting
`p = &arr[arg0]` in the for-init hoists that multiply *before* the check
and inserts a dead `li v0,-1` in the `beqz` delay slot.

```c
/* BAD ~83%: p = &arr[arg0] in the for-init */
for (i = arg0, p = &D_8010D2F8[arg0]; i < 8; i++) {
    if (func(scan, p->field_1)) {
        return i;
    }
    p++;
}

/* GOOD: assign inside the body; GCC still emits p++ */
for (i = arg0; i < 8; i++) {
    p = &D_8010D2F8[i];
    if (func(scan, p->field_1)) {
        return i;
    }
}
```

`func_800BBD40` is the example. The outer `(u32)arg0 >= 8` early `-1` is a
separate `sltiu`/`bnez` and must stay outside the loop.

## Hoist `%hi(store_global)` before the array base

When the target sets up two callee-saved addresses as

```
sw   s1
lui  s1, %hi(store_global)
lui  v0, %hi(array)
sw   s0
addiu s0, v0, %lo(array)
```

LICM emits hoists in source order, *after* any explicit pre-loop `p = arr`.
`p = arr` before the loop therefore always wins and you get `lui s0` first.

Put the store-global first *inside* the loop and form the walker with
`&arr[i]` (no pre-loop `p = arr`). LICM hoists `&store_global` first — and
because the actual store still uses the global name, that hoist is the
split-address `lui s1, %hi` (no `addiu`), then strength-reduces `&arr[i]` to
the walking `s0`:

```c
ds = &Display_State;          /* explicit: a3 first */
for (; i < 50; i++) {
    out  = &D_8011568C;       /* hoist 1: lui s1, %hi */
    slot = &D_8006C338[i];    /* hoist 2: lui v0 / addiu s0 */
    ...
    D_8011568C = slot->field_4; /* sw %lo(D_8011568C)(s1) */
}
```

A real pointer store (`*out = ...`) keeps the `addiu` and becomes `sw 0(s1)`.

## `register s32 k asm("v0"); k = CONST` rematerializes a loop compare

A structured loop that hoists a symbol address will also hoist a small
compare constant (`li a2, 3` in the prologue, `nop` after `lbu`). The target
wants `lbu v1, 0(s0)` / `li v0, 3` / `bne v1, v0` so the `li` fills the load
delay.

Assign the constant to a `v0` register local *after* the load, inside the
loop. `v0` is clobbered by the loop's calls, so LICM cannot hoist it:

```c
register s32 type3 asm("v0");

slot  = &arr[i];
type3 = 3;                    /* li v0, 3 — fills the lbu delay */
if (slot->field_0 == type3) { /* lbu v1; bne v1, v0 */
```

A bare `if (slot->field_0 == 3)` still hoists. `func_800E6D60` is the example.

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
`gte_rtv0tr` (add TR) is `mvmva 1,0,0,0,0` (`0x4A480012`). `func_801040A0`
is the template: `gte_SetRotMatrix` + `gte_SetTransMatrix` + `gte_ldv0` +
that command + `gte_stlvnl`.

`D_80070F34` is `D_80070F10.workm`. After `func_800A8864(&D_80070F34, ...)`,
recover the parent as
`(GsCOORDINATE2*)((u8*)world - OFFSET_OF(GsCOORDINATE2, workm))` so the
compiler emits `addiu s0, s0, -0x24`. A second `&D_80070F10` symbol load
does not match.

`gte_MulMatrix0` from `gtemac.h` is fine if `gte_rtir` is swapped for
`gte_rtir_real` — load/store helpers (`gte_SetRotMatrix`, `gte_ldclmv`,
`gte_stclmv`) already emit real MIPS. `Gfx_MatrixToEuler` is the template.

TMD POLY_FT3 draw (`func_8009D388`) needs the same treatment for RTPT /
NCLIP / AVSZ3 (splat still tags these as "Handwritten" because of COP2):

```c
#define gte_rtpt_real()  __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_nclip_real() __asm__ volatile("nop; nop; .word 0x4B400006")
#define gte_avsz3_real() __asm__ volatile("nop; nop; .word 0x4B58002D")
```

Hoist `opz = &ws->field_28` *before* `ds` / `0xFFFFFF` / `0xFF000000` so
`&field_28` lands in `$t3`. Name a `u_long* ot` temp and GCC CSEs the
shifted OT slot (~85%); write both `addPrim` halves as the full
`((((u32)otz << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14`
expression, same as `func_800AD410`.

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

`Spu_KeyOff` is the pure late-`nor` example: pointer on `Spu_KeyOffMask`,
then `Spu_KeyOnMask &= ~channel` before `Spu_KeyOnMaskExtra &= ~channel` so
that EBAC is the early-loaded `$a2` value. `Spu_KeyOn` is the pure
early-`nor` counterpart: pointer on the `|=` target plus `channel = ~channel`
before the two clears.

`Spu_ArmKeyOn` is late-`nor` with an extra struct-field clear in the middle
(`A |= mask; field &= ~mask; B &= ~mask; C &= ~mask`). Same recipe as E71C:
local pointer on A (`&Spu_KeyOnMaskExtra`) and three inline `~channel` uses — not
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
parse of units that only include a few `include/main/` headers.

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

When calling PsyQ `GsClearOt`, declare it with `GameOt*` (now `GpuOtBuf*` in `display.h`) rather
than including `libgs.h` or casting through `GsOT*`:

```c
void GsClearOt(unsigned short offset, unsigned short point, GameOt* otp);
/* ... */
GsClearOt(0, 0, &ot[temp->field_118]);
*ot[temp->field_118].org = GPU_OT_END_PRIM;
Gpu_CurrentOt = ot[temp->field_118].org;
```

`Gpu_InitOt` is the reference: sets both `Gpu_OrderingTables` slots to depth `0xA`
with `Gpu_OtTags` / `+ GPU_OT_ENTRIES`, clears the active buffer
(`Display_State.field_118`), then points `Gpu_CurrentOt` at the OT base.

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

A store between the copy and the indexed call will steal `$a1` from the
3-word multi-load (copy becomes `t1`/`a2`/`a3`/`t0`) and sink into the
`jalr` delay slot, leaving `move a0,s0` as a real instruction. `do { flag = 1; }
while (0)` pins the store but rematerializes the local table as
`lw 0x10(sp+idx)` instead of `addiu v1,sp,0x10` / `lw 0(v1)`. A memory clobber
keeps the `addiu` form but issues it *before* `lw task->state`.

Make both the flag store and the index load volatile so they stay in source
order; `addiu v1,sp,0x10` then fills the load delay slot:

```c
*(volatile u8*)&flag = 1;
sp.funcs[((volatile Task*)task)->state](task);
```

`func_800AC0F0` is the example (`D_801153F4` + `GameFlow_DispatchTable` shape).
Do not flip the global to `volatile` just for this — other writers of the same
byte already match with a plain store.

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

Pin width matters: `register u8 x asm("a1")` is ignored (QImode does not
stick to a hard GPR), so `$a0`/`$a1` stay swapped. `register s32 x asm("a1")`
with the same assignments matches. `func_801061F0` is the pure example
(`f21` must live in `$a1` so `0x20000` can take `$a0` after `&D_80073B88`).

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
ot = Gpu_CurrentOt;
*ot = GPU_OT_END_PRIM;
Gpu_CurrentOt = ot + 0x20; /* addiu reuses ot — no reload */
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
addPrim(Gpu_CurrentOt + otz, p);
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
entry = (TaskIdPair*)(temp + (s32)D_8006273C[idx]);

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

plain `Mem_Set(Fs_ImgBuffers, 0, 0x25800)` usually loads the global first
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
   Mem_Set(Fs_ImgBuffers, ch, size | 0x5800);
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

For multi-way dispatch that ends every arm with `Fs_BootTimPrimary = some_table;
*arg0 = K`, collecting `K` into a shared `var_v0` and one trailing `*arg0 =
var_v0` looks like the target's shared `sb v0,0(s0)` epilogue — but it often
compiles the stores as `lui v1,dest; lui v0,src; sw v0,(v1)` with the wrong
symbol in the branch delay slot (`lui v0,src` instead of `lui v0,dest`).

Writing each arm as an early return matches both the shared epilogue *and*
the dest-first store pattern:

```c
/* BAD ~88%: shared phi for *arg0 flips store regs / delay-slot lui */
case_arm:
    Fs_BootTimPrimary = D_80062E50;
    var_v0 = 0xC;
    goto store;
...
store:
    *arg0 = var_v0;

/* GOOD 100%: early return; GCC still emits j + li v0,K into the shared sb */
case_arm:
    Fs_BootTimPrimary = D_80062E50;
    *arg0 = 0xC;
    return;
```

Also: sparse outer + dense inner irregular trees (`beq` / `slti` / `bnez`) need
explicit if/goto decision trees (not `switch`) so case *body* order is
`[fallthrough default][case4][case5]` after the dispatch, and the outer test
order is `== 0x1B`, then `< 0x1C`, then `!= 0x11`. Keep the switch key in an
`s32` (not `u8`) so the load is plain `lbu` without `andi`/`sltiu`.

`Fs_SelectLoadHandlers2` is the pure example (FS load-table select by
`Fs_LoadParams.field_2` × `GameFlag_GetNibble(0x7A)`).

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
writing `SndHeap_Malloc(0x582)` in straight-line code often hoists
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
    state->field_40->field_1C = SndHeap_Malloc(0x582);
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
`addPrim(Gpu_CurrentOt + (s16)obj->field_14 + 1, p)` when the target uses `lh`
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

`include/main/display.h` owns `GpuOtBuf` (the 0x14 OT descriptor). Including
`<psyq/libgs.h>` in a TU that already has a conflicting `GsClearOt` prototype
fails with `conflicting types for GsClearOt`.

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
/* x/y/w/h then addPrim(Gpu_CurrentOt - 0x10, p); DR_TPAGE with setDrawTPage */
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

Fix: inline the `ret < 2` / `ret == 2` / `Fs_RetryReadN` tails at every
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
lui  v1, %hi(Gpu_CurrentOt)
lui  a1, 0xFF000000
lw   a2, %lo(Gpu_CurrentOt)(v1)
```

Without pins, GCC often swaps `$a1`/`$a2` (mask in `$a2`, OT in `$a1`) or
reloads the OT base. Pin both:

```c
register u32  mask_hi asm("a1");
register u32* ot asm("a2");

mask     = 0xFFFFFF;
/* set color / setlen / setcode first so $a0 holds 0xFFFFFF */
ot       = Gpu_CurrentOt;
mask_hi  = 0xFF000000;
p->tag   = (p->tag & mask_hi) | (ot[(s16)idx + 1] & mask);
ot[(s16)idx + 1] = (ot[(s16)idx + 1] & mask_hi) | ((u32)p & mask);
```

Assign `ot` before `mask_hi` so the `lui %hi(Gpu_CurrentOt)` precedes
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

## Nested `register asm` blocks so `$v0`/`$v1` can be reused

The 8-byte scratch alloc above wants `$v1` = `G_SCRATCH_HEAD` and `$v0` =
head (`addiu v0, -8; move s1, v0; sw s1`). A later packed key wants the
same two registers for a different pair:

```
lbu    v0, 3(a3)
lbu    v1, 2(a3)
sll    v0, v0, 8
or     v1, v0, v1
```

Pinning both pairs at function scope collides. Give each pair its own
block-scope `register … asm("v0")` / `asm("v1")` so the live ranges do
not overlap. For the `or v1, v0, v1` form, load hi then lo then
`hi <<= 8; key = hi | key` (a single `(hi << 8) | lo` emits
`or v1, v1, v0`). `func_800B48FC` is the example.

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
halfword-aligned, so assign through `GBytes8` (already in `session.h`) for
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
the rest. `Title_InitTask` (title init) is the pure example.

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
LoadImage(&rect, Fs_ImgBuffers);
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
    "lui %0, %%hi(Fs_WorkEntries)\n\t"
    "addiu %1, %0, %%lo(Fs_WorkEntries)"
    : "=&r"(ace_hi), "=r"(base));
/* … loop on a walking copy of base … */
__asm__("lhu %0, %%lo(Fs_WorkEntries)(%1)" : "=r"(t) : "r"(ace_hi));
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

`Gpu_CurrentOt` is `u_long*`. Asm immediates on loads/stores are *bytes*, so a
target `lw v0, -0x40(a2)` is one OT entry stride of `0x10` words:

```c
/* WRONG — scales by sizeof(u_long) → -0x100 bytes */
addPrim(Gpu_CurrentOt - 0x40, p);

/* RIGHT — matches lw/sw -0x40(reg) */
addPrim(Gpu_CurrentOt - 0x10, p);
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

`Gpu_CurrentOt + otIdx` (pointer arithmetic) and
`(u_long*)((otIdx << 2) + (s32)Gpu_CurrentOt)` are equivalent, but the second form
matches the target's register/schedule for dual `addPrim`:

```
lui  a1, 0xff / ori     /* 0xFFFFFF mask */
lui  v0, %hi(Gpu_CurrentOt)
sll  a0, a2, 2          /* idx in a2 → offset in a0 */
lui  a2, 0xff00
lw   v0, %lo(Gpu_CurrentOt)(v0)
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

## Local OT pointer for `Gpu_CurrentOt` so `%hi` stays temporary

`GameMain_Loop` (and similar dual-buffer main loops) must both:
1. pin `Gpu_OtBuffers` as **two** regs (`s8` = `%hi`, `s7` = full via `addiu s7,s8,%lo`) for `Display_FrameFlipDraw` (`addiu a0,s8,%lo`) and `DrawOTag` (`addu v0,stride,s7`);
2. use `%hi(Gpu_CurrentOt)` only temporarily in `$s0` around `ClearOTagR`, not as a function-wide pin.

Writing only through the global:

```c
Gpu_CurrentOt = ot;
ClearOTagR(Gpu_CurrentOt, n);
*Gpu_CurrentOt = END;
Gpu_CurrentOt += 0x20;
```

makes GCC hoist `lui sN,%hi(Gpu_CurrentOt)` into the prologue and steal the reg that
should hold `Gpu_OtBuffers`'s full address.

Fix: pass a **local** into `ClearOTagR`, then reload from the global for the
end-prim write so the `%hi` is only live in that block:

```c
{
    u_long* ot = Gpu_OtTags + flip * GPU_OT_ENTRIES;
    Gpu_CurrentOt = ot;
    ClearOTagR(ot, GPU_OT_ENTRIES);
}
{
    u_long* p = Gpu_CurrentOt;
    *p = GPU_OT_END_PRIM;
    Gpu_CurrentOt = p + 0x20;
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

3. **CdStreamState field map** (in `cdstream.h`): `spuAddr`, `countdown`,
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
needs `j epilogue; move v0,s1` while the free path after `SndHeap_Free` must not
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
(s0 kept across the jal). `Title_EnqueueDemoScene` (title overlay) is the pure example.

## Title overlay: header rodata TU + code TU (no jtable align sed)

Split so package header and switch jtables are not in the same object:

```
title_rodata.c  /* Title_Header, PhaseTable, demo strings, Title_Padding */
title.c         /* tasks; .rodata is only GCC switch jtables */
```

Linker (section_order rodata → text): `title_rodata.o(.rodata)` then
`title.o(.rodata)` then texts. Jtables are first in `title.o`’s `.rodata`
(offset 0), so GCC’s `.align 3` is a no-op and they land at 0x5C after the
0x5C-byte header — no ninja `sed`.

Give splat the jtables as a `.rodata, title` sibling at 0x5C (so the title
unit matches `title.c.o`). splat will then emit `title.c.o(.rodata)` first
in `title.ld` — `fix_title_linker_rodata_order()` swaps it back.

Do **not** tag this overlay `exclusive_ram_id` when it is splat'd alone.
That leaves splat's default global VROM at `0x0–0x1000`, which overlaps
the pe2pkg file; `getLabelForOffset` then looks up branch targets in the
empty global segment and skips emitting `.L8009…:` / uses `func_8009…`
for same-TU `jal`s. The linked overlay still matches; objdiff does not.
Treat title as a normal global segment in `title.yaml` and keep main-exe
imports `absolute:True` (plus `global_vram_*` ending at the load address
`0x80093800`).

`Title_Padding = 0xE122` must follow `Title_DemoCardRestoreMsg` (retail is not
zero-pad after the string NUL).

## Overlay rodata file splits from jumptable 8-align remainder

On PSX, GCC 8-aligns switch tables inside a TU but does not 8-align the
section itself. When several original `.c` files are concatenated into one
splat `rodata` blob, `jtbl.vram - segment.vram` rem 8 changes at each
original file. Those offsets are splat's yaml split hints.

Pair each rodata chunk with a `c` TU starting at the first function that
uses that chunk's `jtbl_*`. Keep the `rodata` names distinct from the `c`
names (`rodata_D4` vs `D4`) while everything is still `INCLUDE_ASM`, or
splat's `migrate_rodata_to_functions` errors on an undotted same-name
sibling.

## Title state machine: shared `advance` between last fallthrough and exit case

When the target lays out `case N` fallthrough into `field_30++` *before* a later
case that returns without advancing (e.g. case 6 → advance → case 7 kill), a
plain `switch` + trailing `field_30++` puts the increment *after* case 7 and
emits an extra jump. Use gotos like `Boot_LoadInitialFile`:

```c
switch (task->field_30) {
case 0: goto L0;
/* ... */
case 6: goto L6;
case 7: goto L7;
}
return;
L0: /* ... */; goto advance;
/* ... */
L6: /* ... */;
advance:
    task->field_30 = task->field_30 + 1;
    return;
L7: /* kill path; no advance */;
```

`Title_DemoStreamTask` is the title-overlay example.

## Force arg regs with `asm("aN")` + empty asm so stores fill jal/branch delays

When the target has `move a0, zero` in a `beqz` delay and `sh/sb` in a following
`jal` delay (or `sb v0` of a prior call's return in a `jal CdCmd_Enqueue` delay),
plain C often schedules the address `lui` / stack `addiu` into those slots instead.

Pin the call arguments in hard registers and barrier them so setup wins the
delay slots:

```c
/* After a successful Pad_CheckFlag800: */
register s32 mask asm("a0");
mask = 0;
asm("" : "+r"(mask));
Title_SkipFadeFlag = 0;       /* sh fills jal delay */
SetDispMask(mask);    /* beqz delay already has move a0, zero */

/* After Stream_FindSlot: */
register s32 cmd asm("a0");
register s32 zero asm("a1");
register u8* p asm("a2");
cmd = 0x61; zero = 0; p = slotParam;
asm("" : "+r"(cmd), "+r"(zero), "+r"(p), "+r"(slot));
slotParam[0] = slot;  /* sb v0 fills Enqueue delay */
CdCmd_Enqueue(cmd, zero, p);
```

Keep the slot temp as `s16` (FindSlot's return type) so the barrier does not
insert `sll`/`sra` sign-extend. Same pattern for case-4 `CdCmd_Enqueue(0x21, …)`
arg setup before `D_800691DE = 1` (absolute alias of `CdCmd_Queue.field_23E`).
`Title_DemoStreamTask` is the pure example.

## `s32` temps for preserved `s8` loads (`lb`, not `lbu`)

When a function loads two `s8` struct fields early, keeps them in saved regs
across a large body, then stores them back with `sb`, a `s8` temp is not enough:

```c
s8 save = p->field_s8;  /* still emits lbu — high bits never observed */
```

Promote through `s32` so the load is a true signed byte load:

```c
s32 save = p->field_s8; /* emits lb */
…
p->field_s8 = save;     /* sb */
```

`Title_RestoreDemoCard` (title demo-card restore) needs this for
`Mc_SaveData.field_21` / `field_23`. The struct fields themselves must also be
`s8` (see also the `D_80072189` / `D_8007218B` aliases).

## Interleave `lui` into a `index * 0xE4` multiply (title `D_80073670`)

Target schedule after a preceding memcpy remainder:

```
addiu s0, s0, 0x24
sll   v0, s2, 3          /* start bank*0xE4 */
lui   v1, %hi(arr)
addiu v1, v1, %lo(arr)
subu  v0, v0, s2
sll   v0, v0, 3
addu  v0, v0, s2
sll   v0, v0, 2
addu  a0, v0, v1
```

`memcpy(arr[bank], …)` and `memcpy((u8*)arr + bank * 0xE4, …)` put `lui` either
before the whole multiply or after it. Force the first `sll` then the symbol
load by splitting:

```c
s32 t    = bank * 8;
u8* base = (u8*)D_80073670;
memcpy(base + ((t - bank) * 8 + bank) * 4, src, 0xE4); /* == bank * 0xE4 */
```

Same size, same ops, but the address load is scheduled one insn into the
multiply. `Title_RestoreDemoCard` is the pure example.

## Split `0xE1000000 | tpage` so mask loads before tpage OR (`Title_MenuTask`)

When a TILE+`DR_TPAGE` block needs target order:

1. `li` / load `0xFFFFFF` (for `setSemiTrans` / OT mask) **before**
2. `lui …,0xE100` / `ori …,tpage`

writing a single immediate `dr->code[0] = 0xE1000240` (or any pre-folded
`0xE1000xxx`) often schedules the tpage constant too early and steals the reg
that should hold the mask — or it hoists color math and breaks a later
`sll`/`addiu` color schedule.

**Fix:** write the GPU command as two operands so CSE/const folding emits
`lui 0xE100` + `ori tpage` *after* the mask materialization, matching
`Title_DrawSpriteRow`-style schedules:

```c
setlen(dr, 1);
dr->code[0] = 0xE1000000 | 0x240; /* not 0xE1000240 as one literal */
addPrim(Gpu_CurrentOt, dr);
```

Do **not** pin `t1 = 0xFFFFFF` early to force the mask into a delay slot: that
consistently wrecks TILE color (`sll a1,a1` / early hoist). Keep the color path
with a live `a1` pin of `prev` instead:

```c
register TILE* a0 asm("a0");
register s32 a1 asm("a1") = prev; /* prev from prologue lw a1,0(s3) */
c = a1 * 16; c = c - 0x3831;      /* sll v0,a1,4; addiu v0,-0x3831 */
a0->b0 = a0->g0 = a0->r0 = c;
asm("" : "+r"(a1));               /* keep a1 live → shift dest is v0 not a1 */
```

Related delay-slot / pin patterns used on the same function (exit + menu):

```c
/* CallExit delay: force move a0,s4 */
asm volatile("" ::: "a0");
Task_CallExit(s4);

/* GetResetCount result stays in v0; first field_12c store survives CSE */
register u32 v0 asm("v0");
v0 = GameMain_GetResetCount();
ds = &Display_State;
asm("" : "+r"(v0), "+r"(ds));
v0 = v0 + 2;
ds->field_12c = v0;
asm("" : "+r"(v0), "+m"(ds->field_12c));

/* Menu loop: bump s0/s1 before jal so addiu s2 fills delay */
do {
    register s32 a0 asm("a0") = s0;
    register s32 a1 asm("a1") = s1;
    register s32 a2 asm("a2") = work->field_10;
    s1 += 0x10;
    s0 += 0xE;
    asm("" : "+r"(a0), "+r"(a1), "+r"(a2), "+r"(s0), "+r"(s1));
    Title_DrawSpriteRow(a0, a1, a2);
    s2 += 1;
} while (s2 < 3);
```

## `u8` switch: `if (kind)` plus dummy `case 0` keeps table index 0

A dense `switch` on a byte whose real cases start at 1 normally subtracts 1
(`addiu v1,v1,-1` / `sltiu …,0xB`). The target for `func_8010A42C` instead
does `andi v1,a1,0xFF` / `beqz v1,default` / `sltiu v1,0xC` and indexes a
12-entry table that still has a default slot at 0.

```c
kind = arg1; /* u8 */
if (kind != 0) {
    switch (kind) {
    case 0:
        break;
    case 1:
        /* … */
        break;
    }
}
```

`if (kind != 0)` supplies the `beqz`. Dummy `case 0` stops the min-case
subtract so the table stays 0-based. Either piece alone is not enough.

Per-case `inner` locals (compound-statement declarations) let `$s0` start as
`arg0` and be overwritten with `arg0->field` once `arg0` is dead. A single
function-scope `inner` pins it in `$s0` and shoves `arg0` into `$s1`;
`register … asm("s0")` on `arg0` keeps that register live and blocks the
overwrite.

## Splat `-c` reverts unnamed data labels

`ninja_config.py -c` re-splits the ROM. Function `glabel`s that live in
`sym.main.txt` keep their names, but data that was only renamed in `.s` files
(and never added to the symbol map) comes back as `D_800xxxxx`.

Add the new name to `configs/USA/sym.main.txt` *before* a clean split, or
re-run the token rename after splat. `INCLUDE_ASM` units are not ninja
inputs — touch/rebuild the C object after renaming a referenced `.s`.

## Overlay: let splat-migrated INCLUDE_ASM jtbls fill the hole

When a new switch is matched in a gameplay overlay TU that already owns a
later compiler jtbl, expanding the TU's `.rodata` range (yaml `.rodata, 3FB8`
start moved earlier) makes splat's `migrate_rodata_to_functions` attach the
intervening still-asm table to that function's `.s`. That include then emits
`.section .rodata` at the `INCLUDE_ASM` site, which sits in source order
between the two C switches.

Do **not** also emit a C `const s32 jtbl_XXXXXXXX[]` copy: the included
`dlabel` and the C symbol collide (`symbol already defined`). Drop the
absolute copy and let the migrated `.s` own the middle slot until that
function is matched. `func_80109170` / `jtbl_80097A68` / `func_8010A42C` is
the example.

## Overlay imports use the same name as main

`configs/USA/sym.gameplay.imports.txt` must use the `sym.main.txt` name
at that address. splat regenerates overlay ASM from those names; do not
hand-edit `asm/USA/gameplay` or keep a leftover `func_800*` alias next
to the renamed import. Overlay C should include the matching main/psyq
header (or a local prototype with the same name). Keep a *local*
overlay prototype only when the match needs a different signature
(dummy extra arg, `void` vs `s32` return) — do not change the main
header. `func_800CFD78` / `Task_Kill` is the example.

## `s32 flag = (s8)u8_field` gives `lb` and hoists the next store

A `u8` field compared as signed (`lb` + `bnez`) must not go through an `s8`
temporary. `s8 flag = field` (or `s8 flag = (s8)field`) is QImode, so the
compare promotes with `lbu` + `sll 24`. Assign the cast into an `s32`:

```c
s32 flag;

flag = (s8)ds->field_122;
ds->field_103 = 2;
if (flag == 0) {
    /* calls */
}
```

`lb` fills a 32-bit register; the independent store then sits in the `bnez`
delay slot. Using the `(s8)` cast only in the `if` condition emits `lb` but
does not hoist the store. `func_800A99E0` is the example.

## Volatile object pointer pins field loads after a global store

An independent `global = NULL` can sink past later field loads on the same
object (and into a `beqz` delay slot). Making only the global `volatile` still
lets GCC hoist `obj->field` before the store.

Fix: load the object through a `volatile` pointer so each field load is a
volatile access and stays in source order after the global store. Assign a
non-volatile sibling store *before* the global so it can fill the `lui`
delay:

```
lui  v0, %hi(D_cur)
sw   zero, 0x18(s1)          /* sibling; fills lui delay */
sw   zero, %lo(D_cur)(v0)
lw   a0, 0x914(s0)           /* first volatile field load */
```

```c
volatile GameActor* inner;
Task*               task;

inner          = work->actor;
work->field_18 = NULL; /* must precede the global */
D_80115760[0]  = NULL; /* declare as T* volatile, not volatile T* */
task           = inner->field_914;
if (task != NULL) {
    Task_Kill(task);
}
```

Copy each field into a temp before the call: a volatile `if (inner->field)` /
`Task_Kill(inner->field)` pair loads twice.

Declare the global as `T* volatile`, not `volatile T*`. The latter makes the
*pointee* volatile; `g = NULL` is then a non-volatile pointer store and sinks
into the following `beqz` delay slot. `func_80101408` is the example.

## `if (ptr != NULL) { work; return 0; }` shares a leaf epilogue

A no-call leaf with `if (mode == K) { if (ptr) … } else { store; } return 0`
must not start from the empty arm. `if (ptr == NULL) return 0; work; return 0;`
splits the else-path return (`jr ra` + store in the delay slot, leftover
`jr ra` for NULL). Dropping the extra `return 0` after `work` inverts to
`bnez` and puts `li v0,1` in that delay slot.

Write the live arm first with an early return. NULL and else then share one
`jr ra; move v0,zero`, and the work path can `jr ra` immediately (v0 is
already 0 from the `beqz` delay slot):

```c
if (D_8007218B == 9) {
    task = D_801156B8;
    if (task != NULL) {
        task->spawnArg1 = 1;
        D_801156B8      = NULL;
        return 0;
    }
} else {
    Game_Session->field_68 = 0;
}
return 0;
```

`func_800E73E8` is the example. The sibling `func_800E7434` can use
`if (ptr == NULL) return 0; Task_Kill(ptr);` because the call creates a
stack frame and a shared epilogue.

## Duplicate a call in both branches so a constant address lands in `$a0`

When the target does:

```
bnez  a0, else
 lui   a0, %hi(K)     /* empty/default path: address already in $a0 */
j     call
 addiu a0, a0, %lo(K)
else:
jal   lookup
 move  a2, zero
move  a0, v0          /* lookup result → first arg */
call:
jal   use
 move  a1, zero
```

a shared temp (`str = cond ? K : lookup(...); use(str);`) allocates `str` to
`$v0` (the lookup return) and emits `lui v0, %hi(K)` plus a jump *onto*
`move a0, v0`. Write the `use(...)` call in **both** arms instead:

```c
if (id == 0) {
    use(K, 0, 0);
} else {
    use(lookup(id, 1, 0), 0, 0);
}
```

GCC CSE's the two calls into one `jal` and materializes `K` directly in `$a0`,
jumping past `move a0, v0`. `func_800CEB40` is the pure example (`D_8010F8D0`
empty string + `func_800B8EB0` + `func_80049D34`).

## If/else stores of two constants keep `bnez; nop; j join`

When the target does:

```
lw    v1, 0x1C(a0)
bnez  a2, else
 nop
j     join
 li   v0, 1
else:
 li   v0, 3
join:
 sh   v0, OFF(v1)
```

a shared temp (`val = cond ? 1 : 3; p->field = val`) lets `-fdelayed-branch`
park `li v0, 3` in the `bnez` delay slot and drop the `j` (~76%). Store the
constants in **both** arms instead:

```c
if (arg2 == 0) {
    inner->field_958 = 1;
} else {
    inner->field_958 = 3;
}
```

cc1 emits `bne` in reorder mode followed by a noreorder `j join; li 1`. aspsx
then inserts the `nop` delay on the `bne`. `func_80105A8C` is the pure example.

## `mc.h` exports `D_8007216C` as `u8`; 268.c needs a word load of that symbol

`D_8007216C` is `Mc_SaveData.field_4`. `mc.h` declares it `u8` because `stage.c`
stores a byte (`sb`). `func_800B92CC` needs `lw` of the same symbol so
`& 0xFFFF0000` sees `field_6`/`field_7`. Including `mc.h` and also writing
`extern u32 D_8007216C` is a conflicting-types error.

Fix: keep the `mc.h` include and load the overlay as `*(u32*)&D_8007216C`. The
relocation stays on `D_8007216C` and codegen stays `lw`. Do not switch the
access to `&Mc_SaveData.field_4` — that rebases the reloc onto `Mc_SaveData`.

## Take `&global` so its `lui` is emitted first into `$v1`

`a - b` of two `u16` globals expands left-first:

```
lui  v0, %hi(a)
lui  v1, %hi(b)
lhu  v0, a
lhu  v1, b
subu v0, v0, v1
```

The target sometimes wants the `lui`s swapped (`lui v1,b` then `lui v0,a`) with
the same loads and `subu`. Assigning `b` to a temp first does emit `b`'s `lui`
first, but parks the value in `$a0` and uses `subu v0,v0,a0`.

Take the address of `b` instead and subtract through the pointer:

```c
u16* p;
p = &b;
return a - *p;
```

GCC 2.8.1 folds `*p` back into a `%hi/%lo` load of `b` (no extra `addiu`) but
materialises `b`'s address first into `$v1`. After a call, do `p = &b` inside
the `if` body so `lui v1,%hi(b)` fills the `beqz` delay slot; assigning `p`
before the call puts the `lui` too early.

`func_800BC2C4` is the example (`p = &D_800739B8`; `(s16)(D_80072174 - *p) >= 2`).
The same shape is how `func_800BC230` emits `lui v1,D_800739B8` first as a leaf.

## Hoist an independent field load so an increment fills the `jal` delay

`ptr->count--; Mem_Free(obj->field); later_use(obj)` looks like the natural
order, but GCC 2.8.1 then copies `obj` into `$s0` in the `lhu` delay slot,
stores the decremented count *before* the call, and leaves `nop` in the `jal`
delay. The target often wants:

```
sw   s0, ...
move s0, a0          /* early copy */
sw   ra, ...
lhu  v0, 0(v1)
lw   a0, OFF(s0)     /* field load fills the lhu delay */
addiu v0, v0, -1
jal  Mem_Free
 sh   v0, 0(v1)      /* store in the jal delay */
```

Assign the later-used field to a local *before* the increment so the scheduler
treats that load as ready to interleave:

```c
void* mem;

mem = arg0->spawnArg2;
D_80115740->field_0--;
Mem_Free(mem);
Task_Kill(arg0);
```

Inlining `Mem_Free(arg0->spawnArg2)` after the decrement is the 83% form.
`func_800EC824` is the example.

## `s32 val = func(); byte_global = val` rematerialises same-`%hi` store

`Display_State` and `D_80071068` (`Display_State.field_100`) share `%hi ==
0x8007`. Taking `&Display_State` into a local and then writing

```c
D_80071068 = func_800ACF8C();
```

lets CSE keep that shared high half in `$s0` across the call (`sw s0` /
`lui s0,%hi(D_80071068)` in the `jal` delay). The target only saves `$ra` and
rematerialises after the call:

```
jal  func_800ACF8C
nop
lui  v1, %hi(D_80071068)
sb   v0, %lo(D_80071068)(v1)
```

Route the return through an `s32` temporary so the QImode store cannot fold
into the earlier address:

```c
s32 val;

val        = func_800ACF8C();
D_80071068 = val;
```

A direct assignment or a `u8*` to the global still pins `$s0`.
`func_800AD65C` is the example.

## Gameplay overlay `memset` is imported as `memset`

`memset` lives at `0x800420F8` in the main exe. Overlay C should call
`memset` (include `<psyq/memory.h>`) and `sym.gameplay.imports.txt`
should list that same name at that address. `func_800BBEC0` is the
example.

## Mutate `arg <<= N` before a call so the shift wins the schedule

When a call packs an incoming register as `(arg << 16) | other`, writing the
shift in the argument expression lets GCC hoist the other setup first
(`sw a0, 0x10(sp)` / `lui a0, %hi(desc)`), and the `sll` lands in the middle
of that sequence.

The target often wants the shift immediately after the prologue `addiu $sp`:

```
addiu sp, sp, -0x20
sll   a3, a3, 16
sw    a0, 0x10(sp)
```

Assign the shift back onto the incoming argument first so it is already done
before the call setup:

```c
arg3 <<= 16;
Ui_SpawnFromDesc(&desc, arg3 | arg1, one, one, arg0);
```

A `s32 packed = arg3 << 16` local can also work, but mutating the argument
keeps the shift in `$a3` (`sll a3, a3, 16` / `or a1, a3, a1`). `func_800D4E40`
is the example.

## Same-constant stores to a 5-word global want mid-first order

A leaf that writes the same constant to five consecutive words of a global
array is *not* the sequential `arr[0]=…; arr[1]=…` form. That emits:

```
lui  v0, %hi(arr)
li   v1, -1
sw   v1, %lo(arr)(v0)
addiu v0, v0, %lo(arr)
sw   v1, 4(v0)
…
```

The target computes the full address first, parks `-1` in `$v0`, then stores
offsets `8`, `4`, `%lo(arr)($a0)`, `0xC`, `0x10` (the last one in the `jr`
delay slot):

```
lui   a0, %hi(arr)
addiu v1, a0, %lo(arr)
li    v0, -1
sw    v0, 8(v1)
sw    v0, 4(v1)
sw    v0, %lo(arr)(a0)
sw    v0, 0xc(v1)
jr    ra
 sw   v0, 0x10(v1)
```

A pointer temporary plus assignments in that store order is what matches:

```c
p   = arr;
val = -1;
p[2] = val;
p[1] = val;
p[0] = val;
p[3] = val;
p[4] = val;
```

A counted `for` loop is not unrolled. `func_800CDEF4` is the example;
`func_800CCDC8` inlines the same five stores.

## Nested `!= 0` then `== 1` keeps the extra `beqz`

`if (field == 1)` on a `u16` compiles to `lhu; li 1; bne` — the load-delay
slot is filled with the compare constant. The target sometimes wants:

```
lhu   v1, field(s0)
nop
beqz  v1, done
li    v0, 1
bne   v1, v0, done
nop
```

That extra zero test is a *separate* compare, not CSE of `== 1`. Write it as
two nested checks so the first branch only needs the loaded value:

```c
if (inner->field_95E != 0) {
    if (inner->field_95E == 1) {
        /* body */
    }
}
```

`func_8010ABD4` is the example — it is `func_8010AB70` behind that guard.

## Do not pre-assign a later call argument that is live across an earlier call

If the target sets `$a2` only in branch delay slots after each poll (`li a2,1`
after the first `jal`, `move a2,zero` after the second), writing the "then"
value *before* the first call pins it in a callee-saved register:

```
li    s0,1
jal   poll
...
move  a2,s0
```

The extra `$s0`/`$s2` save plus a larger frame is a large diff for a small
logical change. Assign the argument only after each call so it can stay in
`$a2`:

```c
if (Pad_CheckButtons(0, 0, 0x8000) != 0) {
    flag = 1;
} else if (Pad_CheckButtons(0, 0, 0x2000) != 0) {
    flag = -1;
} else {
    flag = 0;
}
return func(arg0, pos, flag);
```

`func_800DADE4` is the example. `flag = 1` before the first poll scored ~77%.

## Take `&local` into a pointer temp before the first call

When the target computes `&local` in an early `jal` delay slot (`addiu s0,sp,0x10`)
and keeps it in a callee-saved register until a later call, writing `&local`
only at the final use lets GCC form the address in that last delay slot and
drop the extra saved register (0x28 frame vs 0x30). Assign the address first:

```c
VECTOR3 pos;
VECTOR3* p;

p = &pos;
if (Pad_CheckButtons(0, 0, 0x8000) != 0) {
    flag = 1;
} else if (Pad_CheckButtons(0, 0, 0x2000) != 0) {
    flag = -1;
} else {
    flag = 0;
}
return func(arg0, p, flag);
```

`func_800DAD78` is the example — the same flag pattern as `func_800DADE4`, but
with a stack `VECTOR3`. Bare `&pos` at the call scored ~78%.

## `if (next == NULL)` keeps the unlink `j` / `nop` delay; `!=` fills it

A doubly-linked unlink that writes either the list-tail global or `next->prev`
must test **equality with NULL** so GCC inverts to `bnez` and lays out the
global-address arm as fall-through:

```
bnez  v0, use_next
 nop
lui   v0, %hi(D_xxx)
j     merge
 addiu v0, v0, %lo(D_xxx)
use_next:
addiu v0, v0, 4
```

`if (next != NULL) { pp = &next->prev; } else { pp = &D_xxx; }` is the same
logic but `-fdelayed-branch` parks `addiu v0,v0,4` in the `bnez` delay slot
and drops the `j`. `Task_Kill`'s inline unlink and `func_80099258` both need
the `== NULL` form.

The tail must also be a **standalone symbol** (`extern TmdListHead* D_800711C4`,
then `&D_800711C4`). `&Tmd_ListAlt.prev` CSEs to `Tmd_ListAlt+4` and enables
the same delay-slot fill even with `== NULL`. Overlay imports already split
that field off as `D_800711C4`.

## Two `arr[i].field` loads CSE into same-reg table select

When the target picks one of two array bases, then does a shared
`sll` / `addu` / `lbu`:

```
bnez  a1, else
 lui   v1, %hi(A)
j     join
 addiu v1, v1, %lo(A)
else:
lui   v1, %hi(B)
addiu v1, v1, %lo(B)
join:
sll   v0, a0, 2
addu  v0, v0, v1
lbu   v1, 0(v0)
```

A temp `table = cond ? B : A; ret = table[i].field` materialises the address
as `lui v0; addiu v1, v0` and leaves a `nop` in the `bnez` delay slot.

Two field loads with a separate `ret = 0` CSE into the shared-base form, and
the `lui v1, %hi(A)` fills the delay slot:

```c
ret = 0;
if ((u32)idx < N) {
    if (sel == 0) {
        ret = A[idx].field_0;
    } else {
        ret = B[idx].field_0;
    }
}
return ret;
```

`func_800BB938` is the pure example. The `register … asm("v1")` pin also
forces the same-reg `lui`/`addiu`, but the two-load shape is the source that
produces it without a hard register.

## `ABS()` / `>=` ternary delays the prologue; `if (x < 0)` does not

A 2D hypot that abs-then-squares each operand and then calls `SquareRoot0`
(`func_8003B8A0`) wants the prologue parked in the second `mult` latency:

```
bgez  a0, join0
 nop
negu  a0, a0
join0:
mult  a0, a0
mflo  a0
bgez  a1, join1
 nop
negu  a1, a1
join1:
mult  a1, a1
addiu sp, sp, -0x18
sw    ra, 0x10(sp)
mflo  a1
jal   SquareRoot0
 addu a0, a0, a1
```

`if (arg0 < 0) arg0 = -arg0;` emits the prologue first and fills the first
`bgez` delay with `sw ra` (~70–88%, even with an empty `asm volatile("")`
after each abs). The PSY-Q `ABS()` macro (ternary `(x) >= 0 ? (x) : -(x)`)
lets the scheduler sink `addiu`/`sw ra` after the second `mult` and leaves
`nop` in both abs delay slots. `func_80103D8C` is the pure example:

```c
arg0 = ABS(arg0);
arg0 = arg0 * arg0;
arg1 = ABS(arg1);
arg1 = arg1 * arg1;
return func_8003B8A0(arg0 + arg1);
```

## Two-case switch: put the fall-through case first

A `switch` on two consecutive values plus default normally emits
`beq == 1, case1` / `beq == 2, case2` / `j default` with case 1 as the first
body. The target often inverts the last test and falls through into case 2:

```
beq  v1, v0, case1     /* == 1 */
li   v0, 2
bne  v1, v0, default   /* != 2 */
nop
/* case 2 */
j    end
/* case 1 */
j    end
/* default */
```

List `case 2` before `case 1` in the source so GCC emits that `bne` and the
body order `[2][1][default]`. Numeric `case 1` first (or an `s32 kind = field`
temp) goes back to `beq == 2; j default` (~74%). `if` / `else if` inlines the
`== 1` body with `bne` instead of branching to it.

```c
switch (arg0->field_2) {
case 2:
    table = D_80114C20;
    break;
case 1:
    table = D_80114D70;
    break;
default:
    table = D_80072314;
    break;
}
```

`func_800BB5BC` is the pure example.

## Second pointer so `addu` dest is `$v0`, not the table reg

After a switch that leaves the table in `$v1`, `return table[i].field_0`
emits `addu v0, v0, v1` (offset first). Assigning the address back into
`table` then loading (`table = &table[i]; return table->field_0`) flips
the dest to `$v1` (`addu v1, v1, v0` / `lbu v0, 0(v1)`). A *second*
pointer keeps dest `$v0` and base-first operands:

```c
rec = &table[arg0->field_0 + arg1]; /* addu v0, v1, v0 */
return rec->field_0;                /* lbu  v0, 0(v0) */
```

`func_800BB610` is the load-from-row companion of `func_800BB5BC`.

## Split `&=` / `|=` so the formal stays in `$a0`

A combined `arg0 = (arg0 & mask) | val` allocates the masked value to a new
register (`and v1, a0, v1` / `or a0, v1, v0`). The target often mutates the
incoming formal in place (`and a0, a0, v1` / `or a0, a0, v0`). Split the update:

```c
/* BAD — masked result lives in $v1, then OR into $a0 */
arg0 = (arg0 & 0xF0FFFFFF) | (Game_Session->field_7 << 24);

/* GOOD — both ops write $a0 */
arg0 &= 0xF0FFFFFF;
arg0 |= Game_Session->field_7 << 24;
```

`func_800E3E30` is the pure example — otherwise a 99% body with only those two
ops wrong.

## Assign `&global` before an earlier call so `%hi` lands in `$s0`

When the target does:

```
jal  earlier_call
 lui  s0, %hi(Global)
addiu a3, s0, %lo(Global)
...
sw    val, %lo(Global)(s0)
```

`$s0` holds only `%hi(Global)` (for the offset-0 store) and `$a3` is the full
address (for other fields and as a call arg). Assigning `p = &Global` *after*
`earlier_call` puts `%hi` in a caller-saved (`$v1`) and shrinks the frame — the
live range no longer crosses a call.

Assign the pointer *before* the call so the address is live across it. Keep a
second local that is a typed view of some other pointer you then index, so `$s0`
stays as `%hi` rather than being completed to the full address (`addiu s0, s0, %lo`
/ `sw val, 0(s0)`):

```c
s32*           raw;
GsCOORDINATE2* coords;
GpEffArg*      params;

params          = &D_80113358;          /* before the call — pins $s0 */
slot            = Game_GetPtrSlot(3);
raw             = extra->field_8;       /* extra local is required */
params->field_4 = 0xC0;
coords          = &((GsCOORDINATE2*)raw)[3];
params->field_0 = coords;               /* sw %lo(Global)(s0) */
func_800FDB18(2, coords, 0, params);
```

Dropping `raw` and writing `coords = &((GsCOORDINATE2*)extra->field_8)[3]` in
one go completes `$s0` to the full address and mismatches. `func_8010B520` is
the pure example.

## Bitwise `|` of two pointer-null tests for `sltiu`/`or`

`if (!a || !b)` (or `a == NULL || b == NULL`) emits two `beqz`s. The target
for a combined early-out is:

```
sltiu v0, a, 1
sltiu v1, b, 1
or    v0, v0, v1
bnez  v0, fail
```

Use bitwise OR so both compares stay live and fold into `sltiu` + `or`:

```c
if (!work | !actor) {
    return 0;
}
```

`((u32)work < 1) | ((u32)actor < 1)` and `(work == NULL) | (actor == NULL)`
are equivalent. `func_801043F4` is the pure example.

## 3-way band: seed `ret = 1` then overwrite, do not `else if`

A classifier that returns 0 / 1 / 2 from two `slt`s wants the last arm as
`bnez` + delay `li 1` + fall-through `li 2`. `else if (a < b) ret = 1; else
ret = 2` inverts that to `beqz` + `li 2` / `li 1`. Seed 1 on the non-zero
path and overwrite only when the second compare fails:

```c
if ((temp >> 17) < hp) {
    ret = 0;
} else {
    ret = 1;
    if ((temp >> 18) >= hp) {
        ret = 2;
    }
}
```

`(u16)s16_field << 16` is what produces the shared `lhu` / `sll 16` used by
both `sra 17` and `sra 18`. `func_80103B1C` is the pure example.

## Split-constant `lui v0` before a 1-based index

A leaf that compares `arr[arg0 - 1]` to a >16-bit constant (`0x1869E` →
`lui` + delayed `ori`) wants:

```
lui    v0, HIGH
addiu  a0, a0, -1
lui    v1, %hi(base)
addiu  v1, v1, %lo(base)
```

A bare `arr[arg0 - 1] <= 0x1869E` emits the decrement *after* the address
`lui`/`addiu` and colours the constant into `$v1`. Assign the cap first and
materialise the index as its own statement, then pin the cap so the split
immediate stays in `$v0`:

```c
register s32 cap asm("v0");
s32          idx;

cap = 0x1869E;
idx = arg0 - 1;
if (p->arr[idx] <= cap) {
    p->arr[idx]++;
}
```

`func_80106518` is the example.

## `cln(n << 12) / 2839` is integer log2

`cln` is libgte's 20.12 natural log. `4096 * ln(2) ≈ 2839.13`, so
`cln(n << 12) / 2839` is integer `log2(n)`. GCC emits magic `0x5C56347B`
plus `sra 10` for signed `/ 2839`. Zero is special-cased because `ln(0)`
is undefined:

```c
val = *arg0 << 12;
if (val != 0) {
    ret = cln(val) / 2839;
} else {
    ret = 0;
}
```

An early `return cln(val) / 2839` fills the `beqz` delay with `move v0,zero`
and drops the jump to the epilogue. The if/else `ret` local keeps
`move v0,zero` on the else path after `j` / `subu`. `func_800E1ACC` is the
example; the same divide appears in `func_800E1B24`.

## Unread `s32[2]` store keeps `sw` + register `andi`

`val = 1 << arg0` then `(u8)val << 12` is `sllv` / `andi` / `sll` in a
0x18 frame — the word never hits the stack. The target wants that same
`andi` (not `lbu`) *plus* `sw v0, 0x10(sp)` and a 0x20 frame.

A scalar, `volatile`, union, or `u8*` cast either drops the store or
reloads with `lbu`. Store the SImode value into an unread `s32[2]` and
take the byte from the original register:

```c
s32 mask[2];
s32 val;
s32 tmp;

val = 1 << arg0;
mask[0] = val;           /* sw — array store is not DSE'd */
tmp = (u8)val << 12;     /* andi of the register, not lbu */
```

`s32[1]` is only 4 bytes (0x18 frame). `u8 buf[4]` stores with `sb`.
Reading `mask[0]` back as `u8` becomes `lbu`. `func_800E1B24` is the
example.

## Second pointer before `if` fills `move a3, slot` in the branch delay

When the target computes a slot pointer, then:

```
addu  v1, v1, v0      /* slot = &base[idx] */
bnez  a1, else
 move a3, v1          /* copy */
...
lbu   a2, 0(v1)       /* if: use original */
...
lbu   a2, 2(a3)       /* else: use copy */
```

a single `slot` used in both arms puts `addu` *in* the delay slot and both
`lbu`s use `$v1`. Assign a second pointer *before* the `if` and load the
else-path field through it:

```c
slot = &D_80072330[arg0];
alt  = slot;
if (arg1 == 0) {
    ret = func(&((GpItemBlock*)D_80072330)->scan, arg0, slot->field_0, 0);
} else {
    ret = func(&((GpItemBlock*)D_80072330)->scan, arg0, alt->field_2, 0);
}
return ret == 0;
```

`alt = slot` inside the else is deleted as redundant. `return func(...)` in
each arm inverts the branch and schedules `sltiu` before `lw ra`. The `ret`
local plus a call in both arms keeps `bnez` / `lw ra; sltiu` and duplicates
the `addiu a0, v0, off` / `move a1, a2` setup.

A separate `extern` for the nearby BSS symbol rematerializes `%hi/%lo`. An
overlay struct on the same base (`GpItemBlock.scan` at +0x3F4) is what
emits `addiu a0, v0, 0x3F4`. `func_800BB418` is the pure example.

## Write a dead `||` as one expression, not `if` / `else if`

When a boolean is stored and then only used in a way GCC later deletes
(`if (cond) return 0; return 0;`), the last operand of `A || B` must stay a
branch (`bnez` / `li 1` / `move 0`), not `sltu`.

```c
if ((p->field_0 == 1 && p->field_6 != 0) || p->field_1 != 0) {
    cond = 1;
} else {
    cond = 0;
}
if (cond) {
    return 0;
}
return 0;
```

The equivalent `if (A) cond = 1; else if (B) cond = 1; else cond = 0;` lets
combine turn the last arm into `sltu v1, zero, v0`. Dropping the `if (cond)`
lets DCE delete the whole predicate. `func_800A7CB0` is the example.

## Pin a later-used call arg to `$s0` so the `Task*` stays in `$s1`

When the target saves `$s1` first (`sw s1; move s1,a0; sw ra; sw s0`) the
incoming `Task*` is in `$s1` and a value only loaded later (then passed
across calls) is in `$s0`. First-use allocation gives `$s0` to the task and
swaps the two.

Pin the later value; leave the parameter unpinned:

```c
register s32 val asm("s0");

switch (task->state) {
case 1:
    if (task->killCountdown == 0) {
        val = arg->field_0; /* live across func_800AC464 */
        ...
    }
}
```

Pinning both (`task` to `$s1` and `val` to `$s0`) also matches, but only
`val` is required. `func_800E6F60` is the example. Inverse of "Copy `arg1`
to a dest local so `arg0` keeps `$s0`".

## `ret += idx` keeps a pointer chain in `$v0`

`return ptr + idx` treats both sides as add operands: GCC loads `idx`
into `$v0` and leaves the pointer in `$v1`, so the chain is

```
lw   v1, 0(v0)
lhu  v0, 2(a1)
lw   v1, 0(v1)
sll  v0, v0, 0x2
addu v0, v1, v0
```

The target keeps the pointer as the result being built (`lw v0,0(v0)` /
`lhu v1,2(a1)` / `addu v0,v0,v1`). Assign into the return temp, then add
in place:

```c
ret = table[idx]->field_0;
ret += slot->field_2; /* pointer stays in $v0; idx loads into $v1 */
return ret;
```

`func_800B4668` is the example. A sentinel `switch (idx) { case 0x7FFF:
return NULL; default: ... }` is what emits `beq` to the trailing
`jr ra; move v0,zero` instead of an inverted `bne` early-out.

## `x & func()` dest register picks `s0`/`s1` for live args

When two args stay live across a call and one of them is a bitwise-AND
operand with the return value, operand order decides which s-reg holds
which arg. `func() & arg2` writes the AND into `$v0` and parks `arg2` in
`$s1` (`arg1` takes `$s0`). `arg2 & func()` writes the AND back onto
`arg2`'s s-reg (`and s0, s0, v0`) and parks `arg1` in `$s1`:

```c
/* target: s2=arg0, s1=arg1, s0=arg2; and s0,s0,v0 */
p->field = arg1 + (arg2 & func_80037164());

/* swapped AND: s2=arg0, s0=arg1, s1=arg2; and v0,v0,s1 */
p->field = arg1 + (func_80037164() & arg2);
```

## Pin both the unmasked arg and its in-place-masked copy

When the target saves an incoming arg and immediately copies it so the copy
can be `andi`'d in place after a call:

```
move  s6, a1          /* unmasked: later sb s6, … */
move  s3, s6          /* copy */
…
jal   …
andi  s3, s3, 0xff    /* in-place mask, then beqz / bne == K */
```

`flag = arg1` without pins either copy-propagates (one register, `andi` from
the saved arg into a new dest) or colors `arg1` into `$fp`/`$s8`. Pin both:

```c
register s32 saved1 asm("s6");
register s32 flag asm("s3");

saved1 = arg1;
flag   = saved1;
if ((u8)arg0 == 0) {
    return;
}
/* first call uses unmasked arg0 */
if ((u8)flag != 0) {
    ((u8*)head)[-8] = saved1; /* sb of the unmasked copy */
    /* … */
    if ((u8)flag == 5) { /* reuses the already-masked s3 */
```

Do not write `flag = (u8)flag` before the first call — that hoists the `andi`
above the `jal`. Named locals for constants reused on both enqueue setups
(`c50`/`c4`/`c6`) claim `$s7`/`$s5`/`$s4` and leave `$fp` for `%hi(Game_Session)`.
`func_800A9CBC` is the example.

Same schedule either way — only the dest of the `and` and the s-reg
pairing change. `func_8010BF7C` is the example.

## Keep a call result in `$v0` so a wide constant's `lui` fills the jump slot

When an if/else produces a pointer that is then passed to a 7-arg call
together with a wide constant (`0x606060`) and a small stack arg (`3`),
GCC often coalesces the pointer with `$a3` at the end of each arm.
That frees `$v0` for the constant, so you get `j; move a3,v0` and
`lui v0,0x60` instead of the target's `j; lui v1,0x60` plus a late
`move a3,v0` / `li v0,3`.

Pin the result in `$v0` so the color materializes in `$v1` (and the
`lui` can delay-slot-fill the jump). Then copy it to `$a3` in an inner
block that reuses `$v0` for the small constant:

```c
register u8* text asm("v0");
/* if/else assigns text from the two calls */
{
    register s32       color asm("v1");
    register UiObject* obj asm("a0");
    register u8*       str asm("a3");
    register s32       mode asm("v0");

    color = 0x606060;
    obj   = arg1;
    str   = text; /* move a3, v0 — $v0 now free */
    mode  = 3;    /* li v0, 3 */
    Text_DrawPrompt(obj, x, y, str, color, mode, 0);
}
```

The inner block is required: `text` and `mode` cannot both be
`asm("v0")` in the same scope. `func_800CF28C` is the example.

Related: `(u16)s32_field` assigned to an `s32` temp is `lhu` + `slti`
(u16 promotes to signed int). Assigning the same load to a `u16` temp
adds `andi` + `sltiu`.

Overlay C that calls a main-exe symbol needs that same name in
`configs/USA/sym.gameplay.imports.txt`. Without it the overlay links
with an undefined reference even when the object bytes already match.

## Two-global table walk: `s32` key + `s32` base, then `i * sizeof + base`

A walk that stops on `entry->term == -1` *or* `entry->key == global` wants:

```
li    a2,-1
lui   v0,%hi(key)
lui   v1,%hi(table)   /* interleaved with the key lui */
lh    a1,%lo(key)(v0)
/* i*sizeof, then */ lw v1,%lo(table)(v1)
addu  v1,v0,v1        /* offset + base */
```

and an unrotated `lw 8(p)` / `lbu 5(p)` / `j` loop (not a `+8` IV).

`while` / `for (;;)` rotates and strength-reduces a second IV at `+8`
(`lbu -3(p)`). A `goto` loop keeps the true base (see “Independent
`entry++` + mid-loop `i++`”).

`p = table + i` (or `i + table`) gives the interleaved `lui`s but
`addu p, base, offset`. Inlining `(s32)table` in the add gives the
offset-first `addu` but delays the table `lui` until after the multiply.
Split it:

```c
flag = -1;
id   = key_global;          /* s32 = s16 → lh, not lhu+sll/sra */
base = (s32)table;
p    = (T*)(i * sizeof(T) + base);
loop:
    if (p->term == flag) {
        goto done;
    }
    if (p->key == id) {
        goto done;
    }
    p++;
    i++;
    goto loop;
done:
    return i;
```

The argument/return must be `s32` so GCC does not emit `sll`/`sra 16`
around the index. Callers compiled against the old `s16` prototype still
need that extend: pass `(s16)u16_index` so the call site keeps
`sll`/`sra 16` in the `jal` delay (`func_800E704C` / `D_801155AE`).
`func_800E6EA0` is the example.

## Sparse switch on `x & 0xFFFF0000` needs a signed mask for `slt`

`0xFFFF0000` does not fit in signed 32-bit, so it is unsigned.
`switch (val & 0xFFFF0000)` is therefore an unsigned switch and GCC 2.8.1
emits `sltu` for the binary-search compares.

A 7-case sparse switch on stage-id high words wants `slt` (the constants
are all positive). Use a signed mask so the AND result stays SImode:

```c
val = *(s32*)&D_8007216C;
switch (val & ~0xFFFF) { /* not val & 0xFFFF0000 */
    case 0x1130000:
        /* ... */
}
```

A cast on the switch operand (`switch ((s32)(val & 0xFFFF0000))`) is not
enough: the unsigned AND is still CSE'd and the compares stay `sltu`, or
the load is rescheduled next to the AND.

`func_800D4D2C` is the example.

## Volatile load+store pair pins `lw` before an independent `sw zero`

A word load of one global and `other = 0` are independent, so GCC 2.8.1
hoists the `sw zero` first. The target sometimes wants:

```
lui  v0, %hi(src)
lw   v1, %lo(src)(v0)
lui  v0, %hi(dst)
sw   zero, %lo(dst)(v0)
lui  v0, 0xFFFF
and  v1, v1, v0
```

A lone volatile load does not stop the non-volatile store from moving
before it. Mark both accesses volatile so they stay in source order:

```c
val = *(volatile s32*)&src;
*(volatile s32*)&dst = 0;
switch (val & ~0xFFFF) {
```

`do { val = src; } while (0)` also pins the load first but then parks
`sw ra` in the prologue instead of the first `beq` delay.

`func_800D4D2C` is the example (`D_8007216C` then `Wip_UiHolder`).

## Overlay imports of main functions may pass a dummy extra arg

A gameplay overlay call of a 3-arg main function can still emit
`move a3, a1` (`$a3 = 0`) before the `jal`. The target did that for
`Ui_InsetLayout(panel, NULL, NULL)` — the extra zero is not used by the
callee, but omitting it drops an instruction and fails the match.

If the callee has no other C callers whose codegen would change, add
the unused parameter to the real prototype and pass `0`. An unused
register argument does not change the callee body. `func_800D6AA4` /
`Ui_InsetLayout(..., 0)` is the example.

## Non-void callee return occupies `$v0` so the next `li` uses `$v1`

After `jal`, GCC 2.8.1 treats `$v0` as holding the callee's return even if
the caller ignores it. The next rematerialized small constant then lands in
`$v1` (`li v1,K` / `sh v1,...`), and the function's own `return 0` is
emitted later as `move v0,zero`.

Declaring that same callee `void` frees `$v0` immediately, so you get
`li v0,K` instead — a one-register miss on an otherwise identical body.

```c
s32 func_80105070(GpActorWork* arg0, s32 arg1, GpVecArg* arg2, GpOverrideArg* arg3);

func_80105070(arg0, arg1, arg2, arg3); /* jal; nop — $v0 still "holds" the return */
actor->field_956 = 8;    /* li v1,8; sh v1,0x956(s2) */
return 0;                /* move v0,zero after the restores */
```

`func_8010C708` is the example. Pair with assigning the callee-saved
pointer *before* the saved byte (`actor = arg0->actor; saved = p->field_24`)
so the target's `lw s2` / `lbu s1` order is preserved; declaration order
still assigns `saved` to `$s1` and `actor` to `$s2`.

## Keep the `i * sizeof(slot)` overlay inside the loop body

`GameActor` helpers slide the actor pointer by `i * sizeof(GameActorSlot)` and
then store through a field on that overlay (`func_80105894` / `func_801058BC`).
A 1-based walk that the target implements as

```
addiu a0, a1, 0x28     /* actor + 1*slot */
sb    a2, 0x441(a0)    /* overlay field */
addiu a0, a0, 0x28
```

will not come from `slot[i - 1].field_21` — that strength-reduces to base
offset 0 and store offset `0x469`. Precomputing the first slid pointer
before the `i < count` test is also wrong: `i = 1` gets hoisted into the
previous branch delay slot and `i++` moves *before* the store.

Keep the multiply in the store so GCC strength-reduces it *after* `i = 1`
has filled the `lh count` delay:

```c
i = 1;
if (i < actor->field_938) {
    do {
        ((GameActor*)((i * sizeof(GameActorSlot)) + (s32)actor))->field_441 = arg2;
        i++;
    } while (i < actor->field_938);
}
```

`func_801058BC` is the example.

## Variable-shift bit test: `p += i/32` then `i %= 32` then `val = *p & (1 << i)`

`(flags[i / 32] & (1 << (i % 32))) != 0` is combined into a bit extract:

```
srav  v0, word, bit
andi  v0, v0, 0x1
```

The target instead does `sllv` of `1`, `and`, then `sltu v0, zero, v0`. Keep
the word pointer in its own register (`p = flags; p += i / 32`, not
`flags[i / 32]`), overwrite `i` with the remainder, then capture the AND:

```c
p = flags;
arg0 &= 0x7F;
p += arg0 / 32;
arg0 %= 32;
val = *p & (1 << arg0);
return val != 0;
```

Inlining `(1 << (arg0 % 32))` without first assigning `arg0 %= 32` drops
back to the `srav` form. `func_800BB4BC` is the example.

## `if (p != NULL) goto body; return NULL` emits `bnez` + `j` epilogue

A shared-epilogue `return NULL` after `Task_Spawn` / `Mem_Calloc` wants:

```
bnez  s0, body
li    a0, SIZE     /* delay: first insn of body */
j     epilogue
move  v0, zero
body:
jal   Mem_Calloc
```

`if (p == NULL) return NULL;` and `if (p != NULL) { ... } return NULL;`
both invert to `beqz` into a trailing epilogue. Force the taken branch
to be the success path and put the zero return *immediately* after the
test:

```c
if (p != NULL) {
    goto body;
}
return NULL;
body:
    q = Mem_Calloc(SIZE, 0);
```

The same layout places a shared `Task_Kill` *between* two allocs: jump
forward over the kill on the first success, jump back on the second
fail. Nested `if (p != NULL)` without those gotos parks the kill after
the success path and uses `beqz`.

`func_8010BAC8` is the example.

## Keep `$a0` live so `li v1,K` fills the load-delay of `lw v0,0(v1)`

A 2-bit extract that walks `bank->field_4[idx >> 4]` wants this tail:

```
lw    v1, 4(v1)
sll   a1, a1, 1
addu  v1, v1, v0
lw    v0, 0(v1)
li    v1, 3
sllv  v1, v1, a1
```

`p = bank->field_4; p += idx >> 4; word = *p` is the right address math
(`addu v1,v1,v0` needs the pointer pinned in `$v1`), but once `$a0` dies
after the `lbu` of the bank index, GCC hoists `li a0,K` and uses `$a0`
for the mask. `bank->field_4[idx]` is worse: it copies the address into
`$v0` (`addu v0,v0,v1`) so `$v1` is free for an early `li v1,K`.

Pin the pointer and keep the now-unused first argument live across the
load so K waits for `$v1` and lands in the load delay:

```c
register u32* p asm("v1");

p = table[arg0->field_3].field_4;
p += arg1 >> 4;
shift = (arg1 & 0xF) * 2;
word = *p;
asm volatile("" :: "r"(arg0));
return (word & (3 << shift)) >> shift;
```

`word` must be a separate statement: folding `*p` into the return lets
`li a0,K` sneak back in front of the load. `func_800BB974` is the example.

The one-arg sibling `func_800BB470` already keeps `$a0` live (the index is
shifted in place), so the `:: "r"(arg0)` barrier is not needed. `$a1` is
free instead, and GCC hoists `li a1,K` at the top. Clobber `$a1` after
the load so K rematerializes in the now-dead pointer register:

```c
word = *p;
asm volatile("" ::: "a1");
return (word & (3 << shift)) >> shift;
```

## Barrier the shift so `li v0,K` is not hoisted above `andi`/`sll`

The 2-bit writer wants the mask built *before* the bank pointer, but *after*
the shift amount:

```
andi  a3, a0, 0xF
sll   a3, a3, 1
li    v0, 3
sllv  t0, v0, a3
lui   v0, %hi(table)
```

Pinning the constant to `$v0` (`register s32 temp asm("v0"); temp = 3;
mask = temp << shift`) emits the right `li`/`sllv` pair, but the scheduler
hoists `li v0,3` above the `andi`. A volatile barrier that consumes the
shift keeps the order:

```c
shift = (arg0 & 0xF) * 2;
asm volatile("" :: "r"(shift));
temp = 3;
mask = temp << shift;
```

Reuse `mask` for `val << shift` and put `~mask` in `$a0` after `$a0` has
been consumed as the word offset (`arg0 >> 4`). `func_800BB8E8` is the
example.

The two-arg sibling that takes the bank index from a global byte
(`Mc_SaveData.field_7`) needs that byte assigned to the same `$v0` temp
*after* the mask, not used as a subscript. `table[global.field].ptr`
emits `lui table` first; assigning the byte first issues `lui global`
so the table `lui`/`addiu` and the `sra` of the bit index fill the load
delay:

```c
temp = 3;
mask = temp << shift;
temp = Mc_SaveData.field_7;
p = D_8010D230[temp].field_4;
p += arg0 >> 4;
```

`func_800BAC34` is the example.

## Do not hoist the jump-table index object before the struct copy

Dispatchers that copy a function-pointer table onto the stack
(`sp = D_xxx; sp.funcs[idx](arg0)`) need that copy to be the first
memory work. Pulling `inner = arg0->actor` (or any other index source)
above the assignment hoists `lw actor` ahead of the multi-load/store,
which also swaps the table pointer from `$a3` to `$t0`.

Index through `arg0->actor->field` in the call itself:

```c
sp = D_800979F8;
sp.funcs[(u16)arg0->actor->field_96C](arg0);
```

`func_80108E40` is the example.

## Mid-loop unlink: `goto` resists loop rotation; `s32 mask = ~0x78` keeps `li -0x79`

A walk that clears every node, then stores `next = NULL` only when another
node follows, compiles as a top-tested loop with a mid-body exit:

```
beqz  v0, end
 move v1, v0
sw    zero, 0(head)
li    a1, -0x79          /* ~0x78 */
loop:
lbu   v0, flags(v1)
lw    a0, 0(v1)
sw    zero, 4(v1)
and   v0, v0, a1
beqz  a0, end
 sb   v0, flags(v1)
sw    zero, 0(v1)
j     loop
 move v1, a0
```

`do { ...; if (next == NULL) break; node->next = NULL; node = next; } while (1)`
is rotated: the condition lands at the bottom (`bnez`) and the first
iteration jumps into the middle. An explicit top label plus `goto` keeps
the `j` / `beqz` shape.

Two other pieces have to stay wide:

- `node->field_3A &= ~0x78` (or `&= 0x87`) on a `u8` folds to `andi 0x87`
  each iteration. Hold `s32 mask = ~0x78` and AND an `s32 flags` so the
  constant is hoisted as `li a1, -0x79` / `and`.
- `node = head->next; if (node != NULL)` allocates the pointer in `$v1`
  from the first load and fills `beqz` with `li a1`. A separate temp
  (`temp = head->next; if (temp != NULL) { node = temp; ... }`) forces
  `lw v0` / `move v1, v0` in the delay slot, then `sw` / `li a1`.

`func_800E1884` is the example (sibling `func_800E1758` is the same shape
on `GpObj4A` / `D_8010FAB0`).

## Chained `r = g = b` plus the `<= 0` arm first for CVECTOR stores

Separate `c.r = x; c.g = x; c.b = x` stores r first via `%lo` then g, b.
The target stores b, g, then r as `%lo(symbol)(hi)`. Chained assignment
evaluates right-to-left and matches:

```c
D_80114BA4.r = D_80114BA4.g = D_80114BA4.b = arg0;
```

Write the `<= 0` clamp arm first so the compiler emits `bgtz` to the
positive path (`slti` in the delay slot) instead of `blez` to the zero
arm.

`func_8009EA50` is the example.

## `asm("")` so `andi` wins the `bnez` delay slot over `addiu`

When a `u8` increment and a `u16` index are both ready after `bnez`
(`lb` / `lbu` of the counter already hoisted), the first scheduler puts
`addiu dest, count, 1` first and `-fdelayed-branch` fills the slot with
it. The target wants the mask instead:

```
bnez  v0, skip
 andi  v1, a1, 0xFFFF
addiu v0, a0, 1
sll   v1, v1, 2
sb    v0, field
```

Assign the index first, then barrier, then increment. `asm("")` stops the
`addiu` from being pulled above the `andi`, so the filler takes the mask:

```c
if ((s8)p->field == 0) {
    idx = arg1 & 0xFFFF;
    asm("");
    p->field = count + 1;
    entry = &table[idx];
    func(0, 1, entry->field_0, entry->field_2);
}
```

`idx` must be `s32` (`arg1 & 0xFFFF`) so the `andi` is emitted at the
assignment, not later at the array use (`u16 idx = arg1` is only a
subreg trunc). `func_801041FC` is the example.

## `if (p == NULL) return 0` so the break block stays after the load

A `for` that `break`s after `rec = cur` places that assignment out of
line, after the post-loop if. Writing the non-NULL return first inverts
the test (`bnez` to `lbu`) and parks the load *after* the break block.
The target wants `beqz` to the epilogue with `lbu` as fall-through
*before* `j check; move rec, cur`:

```c
for (; i < count; i++) {
    if ((s8)cur->field_1 == key) {
        rec = cur;
        break;
    }
    cur++;
}
if (rec == NULL) {
    return 0;
}
return rec->field_0;
```

A `do { ... } while (i < count)` of the same body inlines the break
(i++ / i-- / `bne`) and never emits that out-of-line block. `func_800CEA00`
is the example.

## Keep the byte index live so `base + idx` dest is `$v0`, not the index reg

When the target does

```
lui    v0,%hi(obj)
addiu  v0,v0,%lo(obj)
addu   v0,a2,v0      /* tmp = idx + base; idx stays in $a2 */
lb     v0,OFF(v0)
```

a dying `idx` is coalesced into the add: `addu a2, a2, v0` / `lb v0, OFF(a2)`.
Pin `idx` in `$a2` and mention it *after* the indexed load so the add must
write a new temp (the live `$v0` base):

```c
register s32 ret asm("a1");
register s32 idx asm("a2");
Item*        p;

idx = arg0 - 0x60;
ret = 0;
if ((u32)idx < 0x20) {
    p   = &table[arg0];          /* sll then lui — see "indexed volatile" */
    ret = p->field_5;
    ret += save->field_908[idx];
    asm volatile("" ::"r"(idx)); /* idx still live → addu v0, a2, v0 */
    if (ret >= 0xB) {
        ret = 0xA;
    }
}
```

`p = &table[arg0]` (not `table[arg0].field`) is what schedules `sll` before
the table `lui`. `func_800BC324` is the example.

## Load the global first so `lui` precedes `xori`/`sltu` of `!= 1`

`flag = arg != 1` is `xori a1, a1, 1` / `sltu s0, s1, a1`. If the next use
of a global is only at the call, both `lui %hi` and `lbu %lo` sit after the
`sltu`. The target often issues the `lui` right after `move s1, zero` and
keeps the `lbu` after the compare.

Assign the load to a local *before* the compare. The scheduler starts the
address immediately and fills the gap with the independent xor/sltu:

```c
ret  = 0;
item = cfg.field;     /* lui early */
flag = arg1 != 1;     /* xori + sltu between lui and lbu */
if (func(item + K, flag) == 1) {
    other(arg0, flag, ret);
    ret = 1;
}
```

A bare `func(cfg.field + K, arg1 != 1)` leaves `lui`/`lbu` glued together
after `sltu` (~98%). `func_801062DC` is the example.

## Pin only the result so `v1`/`a1` swap without unsharing the load tail

A multi-range byte lookup that ends in one shared `lbu` often allocates the
result to `$a1` and the later index (`arg - 0x60` / `0x80` / `0xA0`) to `$v1`.
The target wants the opposite: `move v1, zero` / `li v1, 0x1000` / `lbu v1, 0(v0)`
and `addiu a1, a0, -K`.

Pinning *both* locals (`ret` to `$v1`, `idx` to `$a1`) gets the names right
but breaks the shared tail: the first range emits its own `lbu` + `j`, and
the last range does `addu a1, a1, v0` / `lbu v1, 0(a1)` instead of
`addu v0, a1, v0` / `lbu v1, 0(v0)`.

Pin only the result. The unpinned index then lands in `$a1` and the address
temp stays in `$v0`, so every range still jumps to one load:

```c
register s32 ret asm("v1");
s32          idx;

ret = 0;
if (arg0 == 0) {
    ret = 0x1000;
} else if ((u32)(arg0 - 1) < 0x5F) {
    ret = table0[arg0];
} else {
    idx = arg0 - 0x60;
    if ((u32)idx < 0x20) {
        ret = table1[idx];
    } else {
        /* idx = arg0 - 0x80 / 0xA0 … same form */
    }
}
if (ret == 0) {
    ret = arg0 + 0x100;
}
return ret;
```

`func_800BC18C` is the example. The unpinned version is a 98% register swap;
pinning both drops to ~89%.

## Pin table `$v1` + session `$a0` so a two-level lookup hoists both `lui`s

A leaf `table[session->field_7 - 1][session->field_6].byte` wants:

```
lui   v0,%hi(Game_Session)
lui   v1,%hi(table)
lw    a0,%lo(Game_Session)(v0)
addiu v1,v1,%lo(table)
lbu   v0,7(a0)
lbu   a0,6(a0)          /* field_6 overwrites the session pointer */
/* field_7-1 in $v0, lw table[i] into $v1, then *14 into $v0 */
addu  v1,v1,v0          /* recs + offset, dest is the pointer */
lbu   v1,0xC(v1)
lui   v0,%hi(out)
sb    v1,%lo(out)(v0)
jr    ra
andi  v0,v1,0xff
```

Without pins GCC loads `Game_Session` first, reuses `$v0` for the table
address, puts `field_7` in `$v1`, starts `*14` before the `lw`, and
`addu`s `offset + base` into `$v0`. The store then uses `$v1` for `out`
and `andi v0, v0, 0xff` — a large register-swap diff of an otherwise
identical function.

Pin the table to `$v1` and the session pointer to `$a0` so both `lui`s
issue before the `lw`. Keep the record pointer in `$v1` (`recs = recs +
f6`, not `recs[f6]`) so the `addu` dest is `$v1`. Load the byte in a
nested block (`register u8 val asm("v1")`) — `table` / `recs` already
own `$v1` in the outer scope, and two `asm("v1")` names in one scope
force the `lbu` into `$v0`:

```c
register GameSession* session asm("a0");
register GpMapRec**   table asm("v1");
register s32          idx asm("v0");
register u8           f6 asm("a0");
register GpMapRec*    recs asm("v1");

session = Game_Session;
table   = D_table;
idx     = session->field_7 - 1;
f6      = session->field_6;
recs    = table[idx];
recs    = recs + f6;
{
    register u8 val asm("v1");

    val = recs->field_C;
    out = val;
    return val;
}
```

`func_800D1FD4` is the example. `field_C` must be a `u8` (or the load is
`lhu` + `andi`); neighbouring functions that `lhu` the same halfword can
overlay it later.

## Pin the loop index, not the switch-selected table

A switch that picks a global table address wants the split form that
reuses `$v0` as the `%hi` temp and writes the table into `$v1`:

```
bne   v1, v0, default
 lui  v0, %hi(D_default)     /* delay: preload default */
lui   v0, %hi(D_case2)
j     join
 addiu v1, v0, %lo(D_case2)
default:
addiu v1, v0, %lo(D_default)
```

That is the same coloring as the sibling leaf (`func_800BB5BC`) that
has no extra locals. Adding a loop index and a `-1` return value
steals `$a2` for the table and `$v1` for the index — the instruction
stream stays identical, only those two registers swap (~98%).

Pinning the table to `$v1` is the wrong pin. It forces
`lui v1; addiu v1, v1, %lo` and drops the default-table `%hi` from the
`bne` delay slot (see the `lui v1; addiu v1, v1` entry). Pin the
competing index instead so the table can still coalesce with the
switch temp:

```c
GpItemRec*   table;
register s32 i asm("a2");
s32          ret;

switch (scan->field_2) {
case 2:
    table = D_case2;
    break;
case 1:
    table = D_indirect;
    break;
default:
    table = D_default;
    break;
}
ret = -1;
table += scan->field_0;
for (i = 0; i < scan->field_1; i++) {
    if (table == rec) {
        ret = i;
        break;
    }
    table++;
}
```

`func_800BB540` is the example.

## Named u16 local pins an lhu for a later mixed-width compare

A u16 field used both as `!= K` and later as `s8_global == field` must be
copied into a named `u16` local. CSE of `actor->field` rematerializes the
`lhu` for the second compare (the s8 load kills it) and parks the first
load in `$v1` instead of `$a2`.

Symptom: target has one `lhu a2, field` then `bne v1, a2`; your build has
`lhu v1, field` for the `!= K` and a second `lhu` after `lb` of the global.

```c
u16 mode;
s32 flags;

mode = actor->field_954;
if (mode != 2) {
    flags = actor->field_962;
} else {
    flags = D_80071624;
    if (D_80072310 == mode) { /* reuse a2; no second lhu */
        ...
    }
}
```

`func_801060E0` is the example. `s32` for the later button word also
avoids a redundant `andi flags, 0xFFFF` before the mask `and`.

## Write the `== 0` arm so a 0-vs-K assign emits `bnez` + `move zero`

A 3-way select that assigns `2`, then `0`, then `K` wants:

```
bnez  bit4, use
 li    a1, 2
bnez  bit0, use
 move  a1, zero
li    a1, K
```

The natural `else if (bit0) { mode = 0; } else { mode = 3; }` is inverted by
GCC to `beqz bit0` with `li 3` in the delay slot and `move zero` as
fall-through. Write the zero-test first so the inversion lands on the
target polarity:

```c
if (flags & 4) {
    mode = 2;
} else if (bit0 == 0) {
    mode = 3;
} else {
    mode = 0;
}
```

`func_800E337C` is the example.

## Pin `ONE` in `$v0` before a global `lui`, then mix SP / pointer matrix stores

A stack `GsCOORDINATE2` whose `coord` is an identity matrix (same shape as
`Gfx_InitCoordinateTrees`) needs `li v0,0x1000` *before* `lui` of the parent
(`D_80070F10`) and a `MATRIX*` in `$v1` used for only two of the stores
(`sw one, 8(v1)` / `sh one, 0x10(v1)`). The rest stay SP-relative.

`one = ONE` as the first statement is scheduled too late: the parent address
steals `$v0` and `m = &coord.coord` reuses it. A first *store* of `ONE` that
belongs in the later store block hoists the `li` without emitting that store
early:

```c
vec.vx = 0;
vec.vy = 0;
vec.vz = ONE; /* first use of ONE — li v0, then the store waits */
one    = ONE;
m      = &coord.coord;
coord.sub = &D_80070F10;
*(s32*)&coord.coord         = one; /* sw one, 0x24(sp) */
*(s32*)&coord.coord.m[0][2] = 0;   /* sw zero, 0x28(sp) */
*(s32*)&m->m[1][1]          = one; /* sw one, 8(v1) */
*(s32*)&coord.coord.m[2][0] = 0;   /* sw zero, 0x30(sp) */
m->m[2][2]                  = one; /* sh one, 0x10(v1) */
```

Writing every matrix word through `m` turns the zero stores into
`sw zero, 4/0xC(v1)`. Writing them all through `coord.coord` drops
`addiu v1, sp, 0x24` and uses `0x2C(sp)` / `0x34(sp)` instead.

`func_800A8D5C` is the example.

## Initialized local jump table keeps `sw ra` first

A 2-entry stack dispatcher that materializes function addresses with
`lui`/`addiu`/`sw` (not a struct copy from a global table) wants:

```
addiu sp,sp,-0x20
sw    ra,0x18(sp)
lui   v0,%hi(func_A)
addiu v0,v0,%lo(func_A)
sw    v0,0x10(sp)
lui   v0,%hi(func_B)
addiu v0,v0,%lo(func_B)
sw    v0,0x14(sp)
lw    v0,0x30(a0)
sll   v0,v0,2
addu  v0,sp,v0
lw    v0,0x10(v0)
jalr  v0
```

Element-wise assignment (`funcs[0] = A; funcs[1] = B;`) matches the stores
but lets the scheduler park `sw ra` after the second `addiu %lo`. An
initialized local array forces the prologue store first and indexes as
`lw v0, 0x10(sp+idx)`:

```c
TaskFunc funcs[2] = { func_A, func_B };

funcs[arg0->state](arg0);
```

A `TaskFuncTable2` aggregate initializer produces the same code. This is the
sibling of the global-table struct copy (`sp = D_xxx`) used by
`GameFlow_DispatchTable`: no `.data` table, so the addresses are built in
place. `func_800D96C8` is the example.

## Hoist `one = 1` so a loop bit-test stays `sllv` + `and`

`word & (1 << bit)` inside a 0..31 loop is rewritten as a bit extract:

```
srav  v0, word, bit
andi  v0, v0, 0x1
```

The target instead hoists `li t0, 1` once and reuses it:

```
sllv  v0, t0, v1
and   v0, a1, v0
```

A literal `1` is folded before RTL. Assign it to a local first so the shift
operand stays a register:

```c
s32 one;

one = 1;
for (i = 3; i >= 0; i--) {
    bit  = 0;
    word = *p;
    do {
        if (word & (one << bit)) {
            count++;
        }
        bit++;
    } while (bit < 32);
    p++;
}
```

`bit = 0` before `word = *p` matches the target's `move v1, zero` / `lw`
order. This is the loop counterpart of the single-bit `p += i/32; i %= 32;
val = *p & (1 << i)` form (`func_800BB4BC`). `func_800BAF08` is the example.

## Init `ret = NULL` so a table pointer stays in `$a1`

A two-level pointer walk that returns either NULL or `&nested[i]` wants:

```
lw    a1, table[idx]
beqz  a1, exit
 move v1, zero
...
lw    v1, rec[j].field_0
beqz  v1, exit
...
addu  v1, v1, v0
jr    ra
 move v0, v1
```

Writing the empty arm as `if (rec == NULL) { ret = NULL; } else { ... }`
lets GCC coalesce `rec` and `ret` onto `$v1`, invert the first test to
`bnez` + `j`, and drop `$a1` entirely.

Assign the result first so the temps stay distinct and delay-slot filling
puts the zero in the `beqz` delay:

```c
rec = table[key->field_3];
ret = NULL;
if (rec != NULL) {
    ret = rec[key->field_2].field_0;
    if (ret != NULL) {
        ret = &ret[key->field_5];
    }
}
```

Sibling `if (rec == NULL) ret = NULL; else ret = rec[i].field_4;` is still
correct when the result is loaded into `$v0` and never added to
(`func_800B5A08`). `func_800B5CE8` is the example that needs the early
init.

## Index `&base[i]` so the IV is a raw offset, not a pointer

When the target walks a mid-struct array with

```
li    s2, 0x460          /* first element */
addu  a1, s1, s2         /* obj + offset */
...
addiu s2, s2, 0x28       /* stride */
```

a running pointer (`slot = (Type*)obj->pad + 1; slot++`) emits
`addiu sN, obj, 0x460` / `move a1, sN` and often swaps the callee-saved
regs that hold `obj` vs the walker.

Index the array instead (counter starts at 1 so the first offset is
`base + stride`):

```c
i = 1;
if (i < obj->count) {
    do {
        func(ctx, (Slot*)obj->pad_438 + i);
        i++;
    } while (i < obj->count);
}
```

GCC strength-reduces `i * sizeof(Slot) + 0x438` to a single offset IV
starting at `0x460`. `func_80105B0C` is the example; sibling `func_80103AC0`
is the same loop with only the index passed through.

## Unsigned range-fail early return keeps `sltiu; beqz` fall-through

When the target hoists a signed `/32` `%32` bit mask, then does

```
sltiu v0, a0, N
beqz  v0, ret_k
 lui  v0, %hi(base)    /* delay: address for the in-range load */
...
jr    ra
 sltu v0, zero, v0
jr    ra
 li   v0, K
```

write the out-of-range path as an early return *before* taking the struct
address:

```c
word = arg0 / 32;
bit  = 1 << (arg0 % 32);
if ((u32)arg0 >= N) {
    return 1;
}
p   = &base;
val = p->flags[word] & bit;
return val != 0;
```

The inverted `if ((u32)arg0 < N) { work; return val != 0; } return 1;` is
swapped to `bnez` and CSE reuses the shift's `li v1,1` as `move v0,v1`.
Taking `&base` before the check also hoists `lui`/`addiu` above the divide.

`func_800BC06C` is the example.

## `&table[i]` then `*slot` vs `table[i]` for prologue / `$v0` reuse

`table[i]` lets GCC hoist `lui %hi(table)` into the delay slot of an earlier
independent load (often a global pointer), which claims `$v1` for the table
base and forces the first load *after* `addiu $sp`. The target instead wants:

```
lui  v0, %hi(ptr)
lw   v0, %lo(ptr)(v0)
addiu sp,sp,-N          /* delay of the pointer load */
sw   ra / s1 / s0
lbu  s0, 0(v0)          /* index */
lui  v0, %hi(table)     /* reuse $v0 now that the pointer is dead */
addiu v0, v0, %lo(table)
sll  v1, s0, 2
addu v1, v1, v0
lw   v0, 0(v1)
```

Take the address of the slot first, then dereference. The address depends on
the index, so the `lui` cannot move above the `lbu`, and `$v0` is reused for
the table base:

```c
void (**slot)(UiObject*, Task*);

id   = *D_80114DD4;
slot = &D_8010D3A0[id];
if (*slot != NULL) {
    ...
}
```

`if (D_8010D3A0[id] != NULL)` and `fn = D_8010D3A0[id]; if (fn != NULL)` both
stuck at ~92% with the table `lui` hoisted and the stack adjust first.
`func_800CFE68` is the example.

## Reassign `(u16)arg` after a narrower store so `andi` reuses the arg reg

A u8 store of an s32 parameter (`sb a2, field`) followed by a u16 compare
(`andi a2, a2, 0xffff; bne a2, v0`) needs the mask to rewrite the same
register the store just used. Writing

```c
p->field_u8 = arg;
if ((u16)arg == 1) {
    p->field_u16 = arg;
}
```

andi's into a fresh dest (`andi v1, a2, 0xffff`) *before* the `sb`, and the
`sb` slips into the `bne` delay slot. That also kicks `li v0, 2` out of the
delay slot.

Store first, then narrow the parameter in place:

```c
p->field_u8 = arg;
arg = (u16)arg;
if (arg == 1) {
    p->field_u16 = arg; /* sh a2 */
} else {
    p->field_u16 = 2;   /* li v0, 2 was the bne delay slot */
}
```

`func_8010B2D4` is the example.

## Sentinel-bit walk: `if (flags & LAST) break; p++` vs `do … while`

When the target ends a table walk with `bnez` to the epilogue and `j`
back to the head (increment only on the continue path, often in the `j`
delay slot), a `do { … } while (!((p++)->flags & LAST))` is the wrong
shape: GCC emits `beqz` to the loop head and strength-reduces a derived
pointer *before* the flag reload.

Write the terminator as a test-then-advance so the increment is not on
the break path:

```c
for (;;) {
    if (p->field_0 & 1) {
        /* occupy-bit work; may store back into field_0 */
    }
    if (p->field_0 & 2) { /* last-element bit */
        break;
    }
    p++;
}
```

`func_800E1A6C` is the example. The sibling `func_800E1A1C` *does* match
as `do … while (!((arg0++)->field_0 & 2))` because it never stores
`field_0` and the target uses the post-increment form.

## Adjacent stack words must be an array (or struct) to escape together

If the target stores two consecutive words (`sw …, 0x10($sp)` / `sw …, 0x14($sp)`)
and passes `$sp+0x10` as one pointer, two separate locals will not match.
The compiler treats the second store as dead (nothing reads that local) and
DCE's it, then often hoists the first computation above an earlier call.

```c
s32 a, b;
a = ...;
b = ...; /* dead store — compiled away */
func(..., (s32)&a, 0);
```

Use an array (or a 2-word struct) so both stores escape through the same
pointer. Call `Game_GetPtrSlot` (or any earlier call) into a temp first so
the payload setup stays after that call:

```c
s32   sp[2];
void* slot;

slot  = Game_GetPtrSlot(3);
sp[0] = ...;
sp[1] = ...;
func(slot, cmd, (s32)sp, 0);
```

`func_800AF41C` is the example.

## Second local so a post-call pointer reload lands in `$v0`

When the target keeps a pre-call pointer in `$s1`, writes one field through
it, then reloads the same slot into `$v0` for a following store group:

```
lw   s1, 0x1C(s0)
jal  helper
nop
li   v0, K
sb   v0, off(s1)     /* first field — pre-call pointer */
lw   v0, 0x1C(s0)    /* reload into $v0, not $s1 */
sh   zero, ...(v0)
```

reassigning the same local (`inner = arg0->actor`) coalesces the reload into
`$s1` and every later store uses `off(s1)`. A second local is a different
pseudo-register, so the allocator parks the reload in `$v0`:

```c
GameActor* inner;
GameActor* actor;

inner            = arg0->actor;
func_8010B210(arg0);
inner->field_97A = 0x12; /* sb K, off(s1) */
actor            = arg0->actor; /* lw v0, 0x1C(s0) */
actor->field_954 = 0;    /* sh zero, off(v0) */
```

`func_8010C180` is the example. A single `inner` reused for both groups stuck
at 98% with only the reload register wrong.

## `s32 val = field & K; return val != 0` keeps `andi` + `sltu`

`(u8_field & POWER_OF_TWO) != 0` as a single expression is combined into a
bit extract (`srl` / `andi 1`). So is `(u32)(u8_field & K) > 0` when the AND
is still in the compare. Assign the mask result to an `s32` first:

```c
val = obj->field_1 & 2;
return val != 0; /* andi 2; sltu v0, zero, v0 */
```

`func_800B59A8` is the example. Same shape as `func_800BB4BC` / `func_800BC06C`
but needed even for a constant 1-bit mask.

## Assign loop setup before consuming a jal return so it fills `andi` / `addiu`

When the target keeps a call result in `$v0` across independent setup:

```
jal    func
nop
move   a2, zero              /* i = 0 */
lui/addiu t0, table
andi   v0, v0, 0xff
addiu  a3, v0, -1            /* target = (u8)ret - 1 */
```

`target = (u8)func() - 1` (or assigning the return to a `u8` first) consumes
`$v0` immediately. Then `i = 0` and the table pointer land *after* the
`andi`/`addiu` and steal the register that should hold `li v0, K` inside the
loop.

Keep the raw return in an `s32` and write the independent setup first:

```c
raw    = func();
i      = 0;
table  = D_8006C338;
target = (u8)raw - 1;
```

`func_800A91CC` is the example. Pair with `register s32 typeN asm("v0")` (see
`func_800E6D60`) so the per-iteration kind compare rematerializes as `li v0, K`
instead of a hoisted `li t0, K`.

## Assign the pointer chain first so a later store stays after the loads

When the target walks a pointer (`lw extra; nop; lw field`) and only then
writes an independent field (`sw zero, flg` / `sw parent+1, sub`), putting
the store first lets GCC emit it immediately and steal the first load's delay
slot. Assign the loaded pointer to a temp first:

```c
parent     = (GsCOORDINATE2*)((GameActorExt*)slot->extra)->field_8;
coord->flg = 0;          /* after both loads, not before */
coord->sub = parent + 1; /* addiu 0x50 in the jal delay */
```

`coord->flg = 0` before the `parent =` load stuck at 99% (`func_800F6C2C`).

## Pin the later live-across-call local so sched1 does not swap `$s2`/`$s3`

`-O2` runs `schedule-insns` before local-alloc. A pointer used twice (scratch
`head`, for `vec = head - 0x10` and later `head[-0x10].vx`) appears first in
the scheduled RTL and takes `$s2`. A single-use local that still has to live
across both calls (the `GameActor*` loaded only for `field_52` after `ratan2`)
is scheduled just before the first `jal` and takes `$s3`.

The target often wants the opposite (`head` in `$s3`, actor in `$s2`) even
though the load order is the same. Declaration order and assigning the actor
first do not flip it: combine deletes the early copy, so first appearance is
still the scheduled load.

Pin the later local:

```c
register GameActor* actor asm("s2");
```

`func_8010BCF4` is the example. The unpinned form stuck at 99% with only those
two callee-saved registers swapped.

## Zero the `$s2` global first so the `$s3` one is restored first

Two globals whose `%hi` must live across calls (`lbu` early, `sb zero` in the
epilogue) take `$s2` / `$s3` in **C store order**, not load order. The
epilogue always stores through `$s3` first so it can restore `$s3` before
`$s2`:

```
lw    ra, 0x20(sp)
sb    zero, %lo(A)(s3)
lw    s3, 0x1C(sp)
sb    zero, %lo(B)(s2)
```

`A = 0; B = 0` assigns `A→s2`, `B→s3` and the machine stores are `B` then
`A`. Swap the C stores so the symbol that should sit in `$s2` is written
first:

```c
D_80114CDB = 0; /* $s2 — stored second in the epilogue */
D_80114CDA = 0; /* $s3 — stored first, then s3 is restored */
```

`func_800AE36C` is the example. The load-order variant
`D_80114CDA = 0; D_80114CDB = 0` stuck at 99.5% with only those two
`%hi` registers swapped.

## `*(s32*)&` strips volatile and lets an independent store hoist past earlier ones

A reset that writes translation on one `GsCOORDINATE2` *then* the identity
word of a different `MATRIX` wants:

```
lui   a1, %hi(coord)
addiu v0, a1, %lo(coord)
li    a0, 0x1000
lui   v1, %hi(matrix)
sw    zero, 0x18(v0)     /* t[0] */
sw    zero, 0x1c(v0)     /* t[1] */
sw    a0, 0x20(v0)       /* t[2] */
sw    a0, %lo(matrix)(v1)
```

`*(s32*)&matrix = one` is not a volatile access, even when `matrix` itself is
declared `volatile`. sched1 then parks that store next to the just-defined
`lui v1` and the translation stores slip after it (~95%). Making every
global `volatile` has the same hole unless the packed word store is also
volatile, and it additionally knocks the final `flg` store out of the `jr`
delay slot.

Keep the earlier object on a `volatile` pointer, write the packed identity
through a volatile cast, and leave the last `flg` target non-volatile:

```c
volatile GsCOORDINATE2* c1 = &D_80070E90;
s32 one = ONE;

c1->coord.t[0] = 0;
c1->coord.t[1] = 0;
c1->coord.t[2] = one;
*(volatile s32*)&D_80070E44 = one; /* must stay volatile */
m = &D_80070E44;
```

`func_800A8B14` is the example. A bare `*(s32*)&D_80070E44 = one` after the
`t[]` stores stuck at 95% with only those two stores swapped.

## Assign the sibling walk onto unused `arg1` and the payload back onto `arg0`

A circular `firstChild` / `nextSibling` search that early-returns `-1` when
the list is empty wants the iterator in `$a1` and the `spawnArg2` payload
in `$a0` (`lw a0, 0x20(a1)` then `lhu` off `$a0`). A local `cur` / `obj`
pair swaps those registers (~99% with only `a0`/`a1` flipped).

`arg1` is otherwise unused, so assign the head to it and reuse `arg0` as
the payload pointer:

```c
arg1 = child;
do {
    arg0 = arg1->spawnArg2;
    if (((((GpWorkObj*)arg0)->field_A >> 8) == 9) && (((GpWorkObj*)arg0)->field_8.as_u16 == arg2)) {
        *arg3 = arg1;
        ret   = 0;
        break;
    }
    arg1 = arg1->nextSibling;
} while (arg1 != child);
```

`func_800B5E08` is the example. The same early `if (child == NULL) return ret;`
is what emits `bnez` + `jr` instead of `beqz` to a shared tail.

## Same-offset `lhu` vs `lbu` needs a union, not a cast

Two sibling walkers can compare the same id bytes at `+0x8` with different
load widths (`lhu` vs `lbu`). `(u8)obj->field_8` still emits `lhu` then
`andi`, and splitting the member into two `u8`s breaks the `lhu` caller.

Put both widths in a union so each access keeps its own load:

```c
union {
    u16 as_u16;
    u8  as_u8;
} field_8;
```

`func_800B5E08` uses `field_8.as_u16`; `func_800B5E78` uses `field_8.as_u8`
and inverts the work-type test (`!= 9` instead of `== 9`).

## Save `nextSibling` before calling through the iterator

A circular walk that calls a function *on the current node* (`Task_CallExit(arg0)`)
needs the iterator in `$a0` and the next pointer saved first. Writing
`arg0 = arg0->nextSibling` after the call makes GCC keep the node in `$s0`
and only move it into `$a0` in the `jal` delay slot (~83%).

Assign `next` after the `field_A` load (so `lw next` fills the `lhu` delay)
and reuse `arg0` as the iterator:

```c
child = arg0->firstChild;
if (child == NULL) {
    return 0;
}
arg0 = child;
do {
    work = (GpWorkObj*)arg0->spawnArg2;
    type = work->field_A >> 8;
    next = arg0->nextSibling;
    if (type == 9) {
        Task_CallExit(arg0);
    }
    arg0 = next;
} while (arg0 != child);
return 0;
```

The early `if (child == NULL) return 0;` is what puts `move v0, zero` at the
*start* of the shared epilogue. A trailing-only `return 0` after
`if (child != NULL) { ... }` schedules it after the callee-saved restores
(~98%). `func_800B5EE8` is the example.

## Pin a join-crossing `field & 0xFE` load to `$v0`

When both arms load a `u8` field and only the insert arm stores a small
constant (`field_6 = 1`), local-alloc gives `$v0` to the block-local
`li 1` and global-alloc puts the crossing load in `$v1`. The shared tail
then becomes `andi v0, v1, 0xfe` instead of the target's
`lbu v0` / `li v1, 1` / `andi v0, v0, 0xfe`.

A bare `s32 val` does not flip it. Pin the load:

```c
register s32 val asm("v0");

if (node->field_6 == 0) {
    /* append … */
    val = node->field_4;
    node->field_6 = 1;
} else {
    val = node->field_4;
}
node->field_4 = val & 0xFE;
```

`u8 val` with the same pin does not stick (QImode). `func_800DABEC` is the
example; the sibling `func_800DACAC` needs no pin because its load and
`= 1` stay in one block.

## `<< 1` keeps the doubled operand; `* 2` gets reassociated

`(s16)x * ((s16)y * 2)` and `((s16)y * 2) * (s16)x` both reassociate. GCC
puts the `* 2` on whichever multiply operand it wants, so the fused
`sll 16; sra 15` lands on the wrong argument.

`<< 1` does not reassociate. The shift stays glued to its operand and
fuses with that operand's sign-extend:

```c
/* target: sll v1,a1,16; sra 16; sll v0,a0,16; sra 15; mult v1,v0 */
result = (arg1 * (arg0 << 1)) / arg2;
```

`func_800EA318` is the example. `arg1 * (arg0 * 2)` swapped the operands
and stuck at 99.7%.

## 1-based record as `Task_Spawn` arg: offset-first, then `ptr - 1`

When the target scales a 1-based `u8` index into `$a3`, adds the saved
base, then subtracts one element in the `jal` delay slot:

```
sll    a3, v0, 3
addu   a3, a3, v0
sll    a3, a3, 2
addu   a3, a3, s0
jal    Task_Spawn
addiu  a3, a3, -0x24
```

`&recs[idx - 1]` subtracts first (`addiu a3, a3, -0x24` then
`addu a3, s0, a3`). `recs + idx - 1` / `&recs[idx] - 1` mutates the base
(`addu s0, s0, v1` / `addiu a3, s0, -0x24`).

Build the address offset-first, then decrement the typed pointer:

```c
rec = (GpCb2CRec*)(idx * sizeof(GpCb2CRec) + (s32)recs);
Task_Spawn(0, 0xF, 0, (s32)(rec - 1));
```

Same integer form (`idx * sizeof + (s32)recs - sizeof`) also matches.
Pairs with “Index-first cast for `addu rd, index, base`”. `func_800A8B6C`
is the example; the sibling `func_800A8C08` can keep `&recs[idx - 1]`
because that address is a return value, not a call argument.

## Two-phase switch table, then pin, so `lui v0` survives a later `$v1` walk

A switch that picks a global table (`D_case2` / `*D_indirect` / default)
wants the sibling leaf coloring:

```
bne   v1, v0, default
 lui  v0, %hi(D_default)
lui   v0, %hi(D_case2)
j     join
 addiu v1, v0, %lo(D_case2)
```

Assigning the table directly into `register T* table asm("v1")` rewrites
that as `lui v1; addiu v1, v1, %lo` and drops the default `%hi` from the
`bne` delay (see “Pin the loop index, not the switch-selected table”).
A later byte-offset walk of the same pointer also needs `$v1`, so you
cannot leave the table unpinned either — GCC then parks it in `$a1`/`$a2`
and CSE's `addiu v1, table, 2` / `sb -1(v1)` / `sh 0(v1)` for the
`u8`/`u8`/`u16` zero stores.

Assign the switch through an unpinned temp, then copy into the pinned
pointer. Copy-coalesce keeps `lui v0; addiu v1, v0, %lo`, and the pin
keeps the walk on `$v1` with `sb 0` / `sb 1` / `sh 2`:

```c
GpItemRec*          tmp;
register GpItemRec* table asm("v1");

switch (scan->field_2) {
case 2:
    tmp = D_case2;
    break;
case 1:
    tmp = D_indirect;
    break;
default:
    tmp = D_default;
    break;
}
table = tmp;
```

Keep the loop index live after the empty-count path (`asm volatile("" ::
"r"(i))`) so `i = 0` stays at the switch join (`move a1, zero` in the
case-1 delay) instead of sinking into the `field_1 != 0` arm. Pair with
`off + (s32)table` and `register s32 off asm("v0")` for `addu v1, v0, v1`
(see the `off + base` entry). `func_800BAC8C` is the example.

## Copy the scan count into a limit so `$v1` can become the walk pointer

When the same table-select switch has more live values (occupied-count plus a
walk pointer distinct from the base), the base lands in `$a3` and the target
loads `field_1` into `$v1`, then:

```
beqz  v1, skip
 move  a2, a1      /* ret = i */
move  a0, v1      /* limit = count */
sll   v0, start, 2
addu  v1, v0, a3  /* rec = off + table */
```

`&table[start]` coalesces dest back into `$a3` and keeps the count in `$v1`
(or `$t0`). Assign `limit = count` *after* `if (count != 0)` so the copy
frees `$v1` for the walk, and keep `off + (s32)table` so dest is a new `rec`.
Pin the base with `register GpItemRec* table asm("a3")` via the same
tmp/switch as `func_800BAC8C`. `ret = i` (or `ret = 0` after `i = 0`) fills
the `beqz` delay with `move a2, a1`. `func_800BAF5C` is the example.

## `jalr` with the iterator left in `$a1` is a 2-arg callback

A child-ring walk that does `jalr` after `lw a0, spawnArg2(a1)` and keeps
the current `Task*` in `$a1` is `cb(child->spawnArg2, child)`, not a
1-arg `cb(spawnArg2)`.

A 1-arg call parks the iterator in `$v0` and saves the function pointer
in the first `beqz` delay slot. Passing `child` as the second argument
forces it into `$a1` and emits `move s2, a1` / `lw a1, firstChild` up
front.

Assign `child = next` *before* the reloaded-`firstChild == NULL` break
so `move a1, s0` fills that `beqz` delay slot. `func_800BF2C8` is the
helper; `func_800BD2FC` and `func_800BCC44` inline the same walk with
`func_800BF398` / `func_800BC634`.

## Snapshot `(u16)s32` before an early-out so `lhu` fills the load delay

When the target does

```
lhu   v0, field_a(s0)
lhu   v1, field_b(a1)
bnez  v0, skip
li    a0, 2
```

and later `lw a0, field_b(a1)` for a call, the 16-bit switch value and the
32-bit call argument are *separate* accesses. Reading `(u16)arg1->field`
only inside the `if` (especially after a store to another object) is an
aliasing barrier: you get `nop; bnez` and a late `lhu`.

Assign the discriminator to a `u32` *before* the early-out. The `lhu`
schedules next to the other field load; the later `lw` of the same s32
stays a distinct access.

```c
kind = (u16)arg1->field_4; /* lhu, hoisted */
if ((u16)inner->field_96C == 0) {
    inner->field_993 = arg2; /* sb sits in the first beq delay slot */
    /* == 2 / < 3 / == 3 / != 4 tree — see "explicit decision tree" */
    inner->field_96E = func(arg1->field_4, 0); /* lw */
}
```

A `switch (kind)` on that temp still pivots at the median (`== 3`) and
loses `li a0, 2` CSE into `field = 2`. The goto tree is required.

`func_8010B348` is the example.

## Assign consecutive call results before the branch that uses them

When the target does `jal A` / `jal B` back-to-back and only *then*
picks a later argument (`li a0,7; bne field,1; li a0,6`), writing the
branch between the two calls (or leaving B as a call-argument expression)
lets GCC hoist the compare into the slot after A:

```
jal    func_A
...
li     s0,7
lhu    v1, field
bne    v1,sN,skip
sra    s1,v0,24
li     s0,6
jal    func_B
```

Assign both results first. The branch no longer sits between the jals, so
B stays next to A and the `6`/`7` select happens after both returns:

```c
temp  = (s8)func_A(obj);
temp2 = (s8)func_B(obj);
snd   = 7;
if ((u16)p->field == 1) {
    snd = 6;
}
func_C(snd, temp, temp2);
```

`func_8010B120` is the example. Same pair as `func_801064A4`, but that
helper takes the first `SndEvt` argument as a parameter so it never has
this hoist.

## Spill a 3-level table walk into pointer temps so `$v0`/`$v1` stay paired

A wrapper-then-3-level lookup (`tbl->field_0[a-1][b-1][c-1]`) written as
one expression (or with only `tbl`) inverts `$v0`/`$v1` versus the 2-level
sibling (`func_800AEEFC`): each `lw` lands in the other register and the
last `lw` schedules after the final `lbu`.

Assign each pointer level to its own temp. If the last index lives on a
`byte` (`signed char`) field whose address is also overlaid as `u8`, read
it through the overlay so the load stays `lbu` (`GameSession.field_4` is
`byte`; `sess->field_0` is the same byte as `u8`):

```c
mid   = D_table[sess->field_3 - 1]->field_0;
inner = mid[sess->field_2 - 1];
bytes = inner[sess->field_1 - 1];
return bytes[sess->field_0 - 1]; /* not session->field_4 — that is `lb` */
```

`func_800AD284` is the example. The one-liner stuck at 88% with only the
register pair flipped. `session->field_4` matched in a scratch `u8` mock
and became `lb` against the real `GameSession`.

## Copy the table pointer so the NULL test uses `$v0` and the cursor stays in `$v1`

When the target does

```
lw    v0, field        /* table = p->field_24 */
nop
bnez  v0, body
 move v1, v0           /* cursor = table */
j     ret0
 move v0, zero
```

a single live pointer (`entry = p->field_24; if (entry == NULL) return 0;`)
loads straight into `$v1` (`lw v1; bnez v1; nop`). The load has to land in
`$v0` so the delay-slot `move v1, v0` can fill.

Give the NULL test its own short-lived copy, then assign the long-lived
cursor afterwards:

```c
temp = arg0->field_24;
if (temp == NULL) {
    return 0;
}
entry = temp;
```

`func_800AC464` is the example. The one-pointer form stuck at 92% with only
that `lw`/`move` pair missing.

## Init a NULL result before a call so it rematerializes into leftover `%hi`

When the target forms `&global.field` with split addresses (`lui s0, %hi`;
`addiu s1, s0, %lo`), calls a function, then does

```
lbu  v1, %lo(G+off)(s0)   /* first field via leftover hi */
move s0, a1               /* rec = 0, reused from i */
lbu  a2, 1(s1)            /* second field via the pointer */
```

the result pointer and the leftover hi share `$s0` because their live
ranges do not overlap. Assign `rec = NULL` *before* the call so the
constant 0 is rematerialized after the hi is consumed:

```c
rec   = NULL;
scan  = &Mc_SaveData.field_5BC;
table = func_800BB500(scan);
i     = 0;
table = &table[scan->field_0];
count = scan->field_1;
```

`rec = NULL` after the call CSEs with `i = 0` and lands in `$a3`, so the
hi stays in `$s0` for the whole function (~92%). `func_800D6994` is the
example; `func_800CE980` is the same search with the scan passed in
(result stays in `$a3` because there is no leftover hi).

## Load the stream word into a temp so `setlen` stays after the `lw`

A POLY_F4 header loop that writes `setlen` / RGB word / `setcode` will
schedule the independent `sb` *before* `lw arg2[2]` if the load is an
operand of the color store:

```c
setlen(poly, 5);
*(s32*)&poly->r0 = arg2[2]; /* GCC emits sb 5 first, then the lw */
setcode(poly, 0x28);
poly++;
```

The target uses the load-delay slot for `poly++`:

```
lw    v0, 8(a2)
addiu a3, a3, 0x18
sb    t1, -4(a1)
sw    v0, -3(a1)
sb    t0, 0(a1)
```

Assign the stream word to an `s32` first so the `lw` is a real statement
the delay-slot filler can pair with `poly++`:

```c
color = arg2[2];
setlen(poly, 5);
*(s32*)&poly->r0 = color;
setcode(poly, 0x28);
poly++;
```

`func_8009F49C` is the example (opcode 0x44 POLY_F4 header init).

## NULL result before `Game_GetPtrSlot`, `s32` key, goto-if-not-head

A circular `firstChild` / `nextSibling` search that returns the matching
`spawnArg2` (or NULL) wants the result in `$s0` and the incoming `u16` id in
`$s1`. Initializing `work = NULL` *after* the child load leaves the result in
a caller-saved reg and drops the extra save. Assign it before the call so it
is live across `Game_GetPtrSlot` and pins `$s0`.

`do { advance; if (iter == head) break; load; } while (id != key)` rotates:
the back-edge becomes `bne iter, head` and the first `iter = head` is CSE'd
into loads off `$a1`. Keep the id compare as the back-edge with a goto, and
copy the key into an `s32` after the first `spawnArg2` load so `andi` fills
the `lhu` delay and `move v1, a1` survives:

```c
work = NULL;
head = ((Task*)Game_GetPtrSlot(4))->firstChild;
if (head != NULL) {
    iter = head;
    work = iter->spawnArg2;
    key  = arg0; /* s32 key — andi after lhu, keeps the iterator copy */
    if (work->field_8.as_u16 != key) {
    loop:
        iter = iter->nextSibling;
        work = NULL;
        if (iter != head) {
            work = iter->spawnArg2;
            if (work->field_8.as_u16 != key) {
                goto loop;
            }
        }
    }
}
```

`func_800B584C` is the example.

## Index the table through the stored `u16` so `addiu %lo` splits around `sh`

A `u16` written then used as a pointer-table index wants

```
lui    v1, %hi(table)
sh     v0, field
andi   v0, v0, 0xFFFF
addiu  v1, v1, %lo(table)
sll    v0, v0, 2
```

A local `u16 idx` that is stored *and* used as the index keeps `lui`/`addiu`
as a tight pair *before* the `sh`. Index through the field so the store is a
hard dependency of the table address:

```c
/* lui/addiu glued; sh comes after */
idx = table16[i] + addend;
p->field = idx;
p->ptr   = table32[idx];

/* lui, sh, andi, addiu %lo — target schedule */
p->field = table16[i] + addend;
p->ptr   = table32[p->field];
```

`func_80103874` is the example (`D_80112D6C[actor->field_93A]`).

## Empty `while (x == K) p++` inverts; `while (1)` + `!=` break does not

GCC 2.8.1 `-O2` rotates an empty occupancy scan

```c
while ((*(s32*)&slot->field_0 & 3) == one) {
    slot++;
}
```

into a peeled first iteration plus a bottom-tested increment/decrement
do-while (`addiu` / `beq` / `addiu -size`). The target is a top-tested
`bne` / `j` with the `addiu` in the jump delay slot.

Write the scan as `while (1)` + `!=` break so the compare stays at the top
and the increment stays on the back-edge:

```c
one = 1;
while (1) {
    if ((*(s32*)&slot->field_0 & 3) != one) {
        break;
    }
    slot++;
}
```

A `for (; cond; slot++)` or `while (cond) slot++;` stuck at ~79% with only
that loop inverted. `func_800E1C58` is the example.

## Reload the stored field for later `~` / `& 1`, not the source temp

After `field = src;` a later `field & ~other` / `(field >> K) & 1` must read
*the field*, not `src`. Keeping `src` in a temp lets CSE skip the `move` that
copies the value before `and` clobbers it; the extra live register then hoists
an unrelated byte load and the `& 1` rematerializes as a bare `andi` in the
wrong slot.

Use the stored field for both bitwise ops, and route the `1` through an `s32`
temp (same wider-constant rule as the CSE entry above):

```c
buttons          = session->field_58;
actor->field_962 = buttons;
actor->field_966 = actor->field_962 & ~actor->field_964;
actor->field_968 = actor->field_964 & ~actor->field_962;
flag             = 1;
actor->field_977 = (actor->field_962 >> 6) & flag;
```

`actor->field_966 = buttons & ~actor->field_964` (and a literal `& 1`) stuck
at 84% with only that register move missing. `func_80103804` is the example.

## Hoist the list-head load before the already-linked early-out

`func_800E1688` (and the same-shape `func_800E17B4` / `func_800E15AC`)
computes `&table[index]` first, then interleaves `lbu flags` with `lw head`.
Loading the head *inside* the `!(flags & 0x20)` arm delays that work until
after `bnez` and also inverts the empty-list `beqz`.

Load the head first, then split `head->next` through a temp so the empty
check is `lw v0` / `beqz` / `move v1, v0` (same temp trick as
`func_800E1884`). Put the non-empty walk first so `beqz` goes to the
empty insert:

```c
head  = D_8010FAB0[arg0];
flags = arg1->field_4A;
if (!(flags & 0x20)) {
    arg1->field_4A = flags | 0x20;
    temp           = head->next;
    if (temp != NULL) {
        node = temp;
        while (node->next != NULL) {
            node = node->next;
        }
        node->next = arg1;
        arg1->prev = node;
    } else {
        head->next = arg1;
        arg1->prev = head;
    }
    arg1->next = NULL;
}
```

`func_800E1688` is the example. The empty-first `if (node == NULL)` form
stuck at 39%.

## Load the predicate into the same `s32` so `li` overwrites `$v0`

The assign-default-then-overwrite-on-`== 0` form matches the `bnez` /
delay-slot `li DEFAULT` / `li OTHER` shape, but a *fresh* temp keeps the
`lw` of the predicate in `$v0` and puts the constants in `$v1`:

```
lw    v0, field
nop
bnez  v0, skip
li    v1, DEFAULT    /* want v0 */
li    v1, OTHER
sb    v1, dest
```

Load the field into that same `s32` first, then overwrite both arms. The
predicate dies in the delay slot and the `li`s reuse `$v0`:

```c
flag = arg2->field_10;
if (flag == 0) {
    flag = 0x38;
} else {
    flag = 7;
}
p->field_983 = flag;
```

`func_80104CAC` is the example. `flag = 7; if (arg2->field_10 == 0) flag = 0x38;`
stuck at 99.7% with only the register different.

## `s32` temp for `s16` switch key + store

Switching on an `s16` field and then storing that same value back (`obj->field = child->field`)
emits `lh` for the signed compare plus a second `lhu` halfword copy. An `s16` temp does
the same: the switch promotes it to `int` with `lh` and the store reloads with `lhu`.

Assign the field to an `s32` first. One `lh` sign-extends; the store reuses that register
(`sh v1`). That also frees `$a1`, so the switch-case constant lands in `$a1` (delay slot
of the null check) and is reused for another arm's store of the same constant.

```c
s32 flag;

flag = child->field_2E; /* lh v1 */
switch (flag) {
    case -1:
        obj->field_2E = flag; /* sh v1, not a second lhu */
        break;
    case 9:
        obj->field_2E = 6; /* sh a1 — same 6 as the case-6 compare */
        break;
}
```

`func_800C7444` is the example. `s16 flag` stuck at 97.3% with `lhu a1` + `li a2,6`.

## `&&` / `else if` so a flag is reloaded and `1` stays in `$v0`

Two related checks on the same halfword flag that the target writes as

```
lhu  v1, flag
li   v0, 1
bne  v1, v0, else
 lui  v0, %hi(flag)     /* delay: address for the else-if reload */
jal  pred1
...
bnez v0, else
 lui  v0, %hi(flag)
... A ...
j    after
else:
lhu  v0, flag
bnez v0, after
jal  pred2
li   v1, 1
bne  v0, v1, after
... B ...
```

must be `if (flag == 1 && pred1 == 0) { A; } else if (flag == 0 && pred2 == 1) { B; }`.

An `if (flag == 1) { if (!pred1) A; } else if (flag == 0) { if (pred2 == 1) B; }`
CSE's `1` into a callee-saved (`li s0, 1`) and skips the second load. The
`&&` / `else if` form:

- short-circuits so `pred1` is skipped when `flag != 1`
- jumps over B after A (the explicit `j after`)
- still evaluates `flag == 0` when `flag == 1` but `pred1 != 0`, which forces
  the reload and parks `%hi(flag)` in the first two delay slots
- rematerializes each `1` in `$v0` / `$v1` instead of `$sN`

`func_800C32A8` is the example. The nested if/else stuck at 88% with only the
flag block different. `func_800C3CE0` and `func_800C46B4` share the same shape.

## Copy a packed halfword to a temp so `lhu` sits between two stores

`Ui_GetCursorFixed` returns two `s16`s packed in an `s32`. After storing that
to a local pair, a later `obj->field = pair.hi` emits `li K` first and `lhu`
after both stores.

Assign the halfword to a temp *between* the two stores so `lhu` fills the
slot after `sw zero` and before `li K`:

```c
obj->status = 0;
y = cursor.unk2;     /* lhu v1, 0x1A(sp) */
child->status = 0x17; /* li v0, 0x17; sw v0 */
child->field_2C = y;  /* sh v1 */
```

`func_800C32A8` is the example.

## Pass the id into `SndVoice_HasActiveId` so the load targets `$a0`

`SndVoice_HasActiveId` remaps its argument via `SndBank_RemapId` and checks
for an active voice. A caller that only tests a global then calls
`SndVoice_HasActiveId()` loads that global into `$v0`. Passing the same
value as the argument forces `lw a0`:

```c
if (D_80114CF0 == 0 || SndVoice_HasActiveId(D_80114CF0) == 0) {
    D_80114CD6++;
}
```

`func_800AE150` is the example. The void call stuck at 99.75% with only the
load dest different.

## Assign `&global` in each arm so `%lo` rematerializes and `lui` fills `bnez`

A set/clear sibling that shares one `p = &Global` before the `arg == 0`
test CSEs the address into a later register (`$a3` / `$t0`) and hoists
`lui` into the range-check `beqz` delay:

```
beqz  v0, ret        /* range */
lui   v0, %hi(G)
bnez  a1, set
addiu a3, v0, %lo(G) /* CSEd base, reused by both arms */
```

The target keeps `lui` in the `bnez` delay and rematerializes `addiu %lo`
in *each* arm (`addiu v0, v0, %lo` then `sll v1; addu; lw  off(v1)`).
Early-return the range check, write the `== 0` (clear + `return`) arm
first, and assign `p` separately in both arms:

```c
word = arg0 / 32;
bit  = 1 << (arg0 % 32);
if ((u32)arg0 >= 0x180) {
    return;
}
if (arg1 == 0) {
    p = &Mc_SaveData;            /* addiu v0, %lo in this arm */
    p->field_6D0[word] &= ~bit;
    return;
}
p = &Mc_SaveData;                /* addiu v0, %lo rematerialized */
p->field_6D0[word] |= bit;
```

A single `p = &Mc_SaveData` before the `arg1` test stuck at 83%
(`sllv t0` for the mask, CSEd base). Same `p->arr[i]` form as the
reader (`func_800BC06C`). `func_800BB7C0` is the example.

## Share the pointer temp across switch cases so the load dest stays `$v1`

A sibling `case` that only compares `extra->field` will reuse the extra
register (`lw v0, field(v0)` / `bne v0, s0`). The target wants the same
load dest as the case that walks that pointer (`lw v1, field(v0)`).

Assign the field to the **same** pointer temporary the other case already
uses. That pins the load in `$v1` instead of overwriting `$v0`:

```c
case 1:
    extra = task->extra;
    coord = extra->field_8; /* lw v1, 8(v0) */
    /* walk coord */
    break;
case 2:
    extra = task->extra;
    coord = extra->field_8; /* lw v1, 8(v0) — not lw v0 */
    if (coord == arg0) {
        found = 1;
    }
    break;
```

`func_8009988C` is the example. `if ((GsCOORDINATE2*)extra->field_8 == arg0)`
stuck at 99.8% with only that load dest different.

## Assign `one = 1` after the first global so LIM emits `lui t4` then `li t3`

A loop that tests `*bits & (one << bit)` needs a named `s32 one` — a literal
`1 << bit` becomes `srav` / `andi 1`. The remaining mismatch is hoist order
of that 1 against another loop-invariant global.

`one = 1` *before* the loop emits `li t4, 1` then `lui t3, %hi(G)`. The
target wants `lui t4, %hi(G)` / `li t3, 1` / `move t2, v0` (the 0xFFFF
compare CSE). Assign `one` *inside* the loop after the first use of the
global so LIM sees the `lui` first:

```c
do {
    item  = *p;
    bits  = Mc_SaveData.field_5AC; /* lui t4 hoisted first */
    one   = 1;                     /* li t3, 1 hoisted second */
    bit   = item & 0x7F;
    bits += bit / 32;
    bit  %= 32;
    if (*bits & (one << bit)) {
        /* ... */
    }
    p++;
} while (*p != 0xFFFF);
```

Do not preload the sentinel either (`end = 0xFFFF` before `if (*p != 0xFFFF)`).
That pins 0xFFFF in `$t2` too early and lets the first `lhu` clobber `$v0`.
Leave the literal in both compares so CSE does `li v0, 0xFFFF` / `beq` /
`move t2, v0`.

`func_800BB668` is the example. Pre-loop `one = 1` stuck at 94% with only
the `t4`/`t3` swap.

## Late `i = 1` needs other loop constants live first

A case that does `i = 1; field += i;` then a `func_800B47A8` walk wants
`li s1, 1` in the *second* `bnez` delay (after the poll), `addu` with `$s1`,
and `$s2 = arg0` / `$s0 = actor`. Assigning `i = 1` before the poll makes `i`
live across the jal — correct registers and `addu`, but `li s1, 1` fills the
*first* delay slot instead of `move a0, s2`. Assigning `i = 1` only after the
poll rematerializes `addiu field, 1` and gives `$s0 = arg0`.

Assign the other loop-invariant `s32`s first, then `i`, all after the poll:

```c
if (func_8010583C(arg0, 0, 0, 0) != 0) {
    break;
}
anim  = 9;
extra = 5;
i     = 1;
actor->field_95E += i;
actor = arg0->actor;
if (i < actor->field_938) {
    do {
        func_800B47A8(..., anim, ..., extra, ...);
        i++;
    } while (i < actor->field_938);
}
```

`anim` / `extra` live at the increment force a 5-s-reg coloring (`s4`/`s3`/`s1`)
so `i` is not rematerialized. The scheduler then parks `li s1, 1` in the poll
`bnez` delay, `li s4, 9` in the `lw actor` delay, and `li s3, 5` in the loop
`beqz` delay. `func_80101848` is the example.

## Assign `&global` at the top so the address lives in `$sN` from the prologue

A single later store (`CdCmd_Queue.field_20A = 1`) rematerializes
`lui v1,%hi; sh v0,%lo(global+off)` at the use, so `$s4` is never allocated
and the stack frame shrinks (`sw ra,0x28` instead of `0x2C`). The target
computes `&CdCmd_Queue` in the prologue — even on the early-return path that
never uses it — because the address is live across every jal.

Assign the pointer at the top of the function, before the first call:

```c
CdCmdQueue* queue;

queue = &CdCmd_Queue;
func_800A7320(&arg0->killCountdown);
...
queue->field_20A = 1;
```

That forces `sw s4` / `addiu s4,%lo` in the prologue and `sh v0,0x20a(s4)`
at the store. `func_800A0504` is the example. Bare `CdCmd_Queue.field_20A = 1`
stuck at 95.6% with only the frame / `$s4` save-restore different.

## Two `&global`s: force `addiu v0, %lo` then `move dest` (not `addiu dest`)

`-msplit-addresses` turns `p = &D_xxx; q = &D_yyy` into `lui dest; addiu dest,
dest, %lo` (or `lui v0; addiu dest, v0, %lo`). The target sometimes wants the
full address in `$v0` first, then a copy — typically because the values stay
live in `$a2`/`$a1` across a later loop:

```
lui    v0, %hi(D_xxx)
addiu  v0, v0, %lo(D_xxx)
move   a2, v0
lui    v0, %hi(D_yyy)
addiu  v0, v0, %lo(D_yyy)
move   a1, v0
```

Pin the dests to `$a2`/`$a1` and a scratch to `$v0`, assign each address into
the scratch, then copy. GCC still combines the first pair (`lui a2; addiu a2`)
unless copy-prop is blocked after each materialization:

```c
register MATRIX* mtxA asm("a2");
register MATRIX* mtxB asm("a1");
register s32     addr asm("v0");

addr = (s32)&D_xxx;
__asm__ volatile("" : "+r"(addr));
mtxA = (MATRIX*)addr;
addr = (s32)&D_yyy;
__asm__ volatile("" : "+r"(addr));
mtxB = (MATRIX*)addr;
```

Keep the later store of a call result (`arg0->spawnArg2 = result`) *after*
those copies so the result can live in `$v1` (`register s32 result asm("v1")`)
instead of being stored immediately from `$v0`. `func_800D9D18` is the example.

## Store the task pointer before `Mem_Set` so `sw` fills the jal delay slot

`Mem_Set(actor, 0, SIZE); task->idMap = actor;` puts `li a2, SIZE` in the jal
delay slot and the `sw` after the call. The target has `li a2` before jal and
`sw` in the delay slot.

Assign the pointer first. The store is independent of the call, so the
scheduler parks it in the delay slot and leaves the size `li` in the setup:

```c
task->idMap = (TaskIdMap*)actor;
Mem_Set(actor, 0, 0x998);
```

`func_801036FC` is the example. The same `Mem_Set` then assign order used by
`func_8010BAC8` stuck at 99.1% with only that delay-slot swap.

## Overlay: still-asm dispatcher tables after expanding `.rodata`

When a new overlay switch moves the TU `.rodata` range earlier, splat migrates
jtbls and some data tables onto `INCLUDE_ASM` functions. Tables used only by
already-matched C (e.g. `D_80097940`, `D_800979F8`) land in a standalone
`nonmatchings/.../D_*.s` or in `matchings/<func>.s`. `INCLUDE_RODATA` the
splat-owned standalone file. For a table stuck in a `matchings/` dump, emit
the words with inline `.section .rodata` / `.globl` in the C file — a C
`GpActorFuncTable` definition is collected to the end of the TU and shifts
later tables; a hand-written `.s` under `nonmatchings/` is deleted on the next
`ninja_config.py` splat.

## Nested mid-struct object so `%lo(sym+off)` wins against a later `&sym`

When the target stores a mid-struct halfword block as

```
lhu    v0, src
lui    a0, %hi(Global)
sh     v0, %lo(Global+0x10)(a0)
lhu    v0, src2
addiu  s0, a0, %lo(Global+0x10)
sh     v0, 2(s0)
```

and later rematerializes `&Global` for other fields, a flat
`Global.field_10 = x` plus `p = &Global.field_10` CSEs with that later
`cfg = &Global` and emits `addiu v0, %lo(Global); sh 0x10(v0); addiu s0, v0, 0x10`.

Nest the block as its own struct at that offset. Load the first halfword into
an `s32` temp, then take the nested address and store through it:

```c
temp = coord->field_18;          /* lhu v0; lui cannot sneak in first */
p = &Wip_SysConfig.field_10;     /* nested WipSysPos */
p->field_0 = temp;               /* sh %lo(Wip_SysConfig+0x10) */
p->field_2 = coord->field_1C;
...
cfg = &Wip_SysConfig;            /* later, separate lui/addiu of the base */
save->field_14 = cfg->field_8;
```

`func_800BB9B8` is the example. The same stores as four bare `s16` fields
stuck at 90% with only the `addiu`/`%lo` addressing different.

## Keep the `lb` in a temp so the later store is `sb $v1`, not `lbu`

When the target does `lb v1, field; bnez; ...; sb v1, other`, writing
`other = field` after `if (field == 1)` reloads as `lbu` (or rematerializes
the constant 1). Capture the signed load once and reuse it for both the
compare and the store:

```c
temp = inner->field_973;
if (temp == 0) {
    if (inner->field_975 != 0) { /* invert so GCC emits beqz + li 9 */
        mode = 0xD;
    } else {
        mode = 9;
    }
} else if (temp == 1) {
    inner->field_97E = temp; /* sb v1 */
}
```

`func_80108770` is the example. `inner->field_97E = inner->field_973` (or
`= 1`) stuck at 95% with an extra `lbu` and inverted `field_975` polarity.

## Access a pointer field per block so temps stay in `$v1` / `$a1`

A function-scope `Task* task` is coalesced into `$a0` for every
`field_914` load. The target instead uses a fresh scratch in each arm
(`$v1` on a NULL-check store, `$a1` when a sibling `s32 value` occupies
`$a0`, `$v1` again on a read-modify-write).

Write `actor->field_914` at each use. Combined with the usual
assign-default-then-overwrite (`value = 1; if (p->spawnArg1 == 2) value = 3;`),
the 1/3 path becomes `lw a1; bne; li a0,1; li a0,3; sw a0,0x34(a1)`:

```c
if (actor->field_914 != NULL) {
    actor->field_914->spawnArg1 = -1; /* lw v1; sw v0,0x34(v1) */
}
/* ... */
value = 1;
if (actor->field_914->spawnArg1 == 2) {
    value = 3;
}
actor->field_914->spawnArg1 = value;
```

`func_80106350` is the example. A shared `task` local stuck at 99.4% with
only `$a0`/`$a1` swapped on that path and `$a0` on the 0x16 NULL check.

## Rematerialize `arg0` into `$a0` before a store+jal so the store fills the delay

When the target does

```
bnez  v0, ret
 move  a0, s0      /* rematerialize arg0 */
li    v0, 2
jal   func
 sb    v0, field(a1)
```

writing `p->field = 2; func(arg0)` puts `li v0, 2` in the `bnez` delay and
`move a0, s0` in the `jal` delay. Assign `arg0` to a `register … asm("a0")`
temp *before* the store. That emits the rematerialize first, so delay-slot
filling takes `move a0, s0` for the branch and the store for the `jal`:

```c
register GpActorWork* a asm("a0");

a       = arg0;
p->field = 2;
func(a);
```

`func_801093DC` is the example. The same function also needs the first-half
actor pointer pinned to `$a1` (`register GameActor* inner asm("a1")`) so
`arg0` stays in `$a0` for the earlier `jal` (`nop` delay, not `move a0, s0`).
A later install block can reuse `$a1` for the new node if that pin is scoped
to the first half (or redeclared in the join block).

Assign `flag = 1` *before* the install `if` so `li v0, 1` fills the `beq`
delay and CSE cannot keep the new node in `$v0` for the pointer store
(`sw a1` / rematerialized `li v0, 1` after the `sw`):

```c
arg1 = next;
flag = 1;
if (node != arg1) {
    if (node != NULL) {
        node->field_5 = 0;
    }
    actor->field_90C = arg1; /* sw a1 — v0 already holds 1 */
}
arg1->field_5 = flag;
```

## Overlay header rodata: copy the symbol, do not re-emit a local initializer

A 5-byte `lwl`/`lwr`/`lb` + `swl`/`swr`/`sb` stack copy is struct
assignment of a `u8[5]` object. A local initializer

```c
u8 table[5] = { 4, 3, 2, 5, 6 };
```

matches the body (scratch score ~99.8%) but emits *new* `.rodata` in the
C TU. On the gameplay overlay that data already lives in
`header.rodata.s` (`D_800938CC`); extra bytes from `D4.c.o(.rodata)`
land after the pre-split rodata and shift `.text`.

Type the existing symbol and assign it:

```c
typedef struct {
    u8 field_0[5];
} GpTbl5;

extern GpTbl5 D_800938CC;
GpTbl5        table;

table = D_800938CC; /* lwl/lwr/lb of D_800938CC */
```

`func_800A9B3C` is the example.

## Pin the table `lhu` to `$v1` so a live `u8` can keep `$a0`

A value loaded early (`lbu` of a `u8`, used again after two early
returns) wants `$a0` — the incoming pointer has already been copied to
`$a1`. Local-alloc runs first and gives `$a0` to the later block-local
`lhu` of a `u16` table. Global-alloc then sees the `u8` conflict with
`$a0` and puts it in `$a2`. The instruction stream matches; only the
registers slide (`lhu a0` / `mult a2,a0` / `mfhi v1` instead of
`lhu v1` / `mult a0,v1` / `mfhi t0`).

Assign the table load to a `register s32` pinned to `$v1`. That is the
same register the `%hi/%lo` of the table already uses, so the `lhu`
overwrites the address after `addu`. `$a0` stays free for the `u8`,
the product stays in `$v1`, and `mfhi` lands in `$t0`:

```c
register s32 scale asm("v1");

val   = arg0->src->field_E; /* lbu a0 */
scale = table[arg0->idx];   /* lhu v1 */
if (arg0->count >= (val * scale) / 100) {
    ret = 1;
}
```

A plain `s32 scale` (no pin) is not enough: local-alloc still hands the
`lhu` `$a0`. `func_800E2F7C` is the example.

## Copy `arg2` inside the inner if so `$a2` can hold a table

`if (arg2 != 0)` emits the target prologue (`move a0, a2` then
`addiu s0, a3, K`). Using `arg2` again in that body keeps `$a2` as a
second home, so `table = global` lands in `$a3`. Copying `arg2` into a
function-scope local first fixes the table register but sinks
`move a0, a2` *after* the flags `addiu`.

Use `arg2` only for the outer predicate, then copy it to a new local
*inside* the next `if` (before the table load) and use that local from
there. `$a2` dies in time for `addiu a2, v0, %lo(table)`, and the
prologue `move a0, a2` stays before the flags add:

```c
flags = arg3 + 0x10;
if (arg2 != 0) {
    if ((obj->status >> 16) == 1 || obj->status == 1) {
        value = arg2; /* kill $a2 before the table load */
        tmp   = arg3;
        asm volatile("" : "+r"(tmp)); /* move a1, a3 in the beq delay */
        table = D_8010E8F8;           /* addiu a2, %lo */
        ...
    }
}
```

The `+r` barrier on `tmp` is the existing "delay slot ahead of `lui`"
trick: without it, `lui %hi(table)` steals the `beq` delay instead of
`move a1, a3`. `func_800CDDA0` is the example (same 3-slot table loop as
`func_800CDE80`).

## Incoming-arg copies use arg order; explicit locals use register order

When `a1`/`a2` live across a call, GCC 2.8.1 saves them in *parameter*
order (`move s4, a1` then `move s3, a2`) as a prologue batch, before
later statements. An early `idx = 3` that kills `$a0` does **not** by
itself delay those saves.

Copying into new locals after the kill *does* delay them to the target
spot (after `li a0, 3`), but the two moves then emit in *callee-saved
register* order (`s3` then `s4`):

```c
other = global;
idx   = 3;          /* kills $a0; node already saved */
msk   = mask;       /* want move s4, a1 first */
mch   = match;      /* want move s3, a2 second */
slot  = Game_GetPtrSlot(idx);
```

Pinning both with `register ... asm("s4")` / `asm("s3")` restores source
order, but then `(u16)match` cannot rewrite `$s3` in place: the target's
`andi s3, s3, 0xFFFF` in the `beqz` delay slot becomes `andi v1, s3,
0xFFFF` / `bne v0, v1`. Assigning `s3match = (u16)s3match` after the
call gets the in-place `andi` back and hoists the `$s3` save to the
very start.

`func_800E06AC` (sibling of matched `func_800E0608`) is the example.
Natural C is 88.75% (saves too early). Explicit locals are 99.167%
(only those two pairs swapped).

## Assign `a - b` before `ABS()` so the negate stays `negu`

`ABS(a - b)` rewrites the negative arm as the swapped subtract `b - a`
(`subu v0, b, a` in the `bgez` delay / false path). The target instead
does `subu; bgez; nop; negu`.

Assign the difference first, then abs that temp. `-(temp)` cannot be
rewritten as an operand swap:

```c
temp = cur - tgt;
if (temp < 0) {
    temp = -temp; /* or temp = ABS(temp); */
}
```

`func_80108BD8` is the example. `ABS(cur - tgt)` stuck at ~60% with only
that subtract flipped.

Assigning `temp = a - b` can still swap the two loads
(`lw v1, a; lw v0, b; subu v0, v1, v0`). The target wants the minuend in
`$v0` (`lw v0, a; lw v1, b; subu v0, v0, v1`). Store the minuend first,
then subtract:

```c
temp = cur;
temp = temp - tgt;
temp = ABS(temp);
```

`func_80100E40` is the example. The one-shot `temp = cur - tgt` was a
99.8% register swap of those two `lw`s.

## Assign `tgt - K` before `cur - wrap` so K stays on `tgt`

`cur - (tgt - 0x1000)` reassociates to `(cur + 0x1000) - tgt`
(`addiu v0, a0, 0x1000; subu v0, v0, a1`). The target subtracts K from
`tgt` first (`addiu v0, a1, -0x1000; subu v0, a0, v0`).

Assign the inner subtract to its own temp, then subtract that from
`cur`. In a short-circuit `||` the two assigns can sit in a comma so
the wrap is only computed when the first abs misses:

```c
if (temp < 0x41 || (wrap = tgt - 0x1000, temp = cur - wrap, temp = ABS(temp), temp < 0x41)) {
    /* snap */
}
```

`func_80108BD8` is the example. The reassociated add-then-sub stuck at
91.8% with only those two instructions different.

## Pre-scale one switch arm with `<< 16 >> 15` so it skips the shared `sll 1`

A switch that picks a `u16` table and then indexes it wants two
addressing predecessors that join at `addu s0, s0, v0`:

```
# already-sext cases
sll   s0, s0, 1
# raw s16 case (delay-slot sra)
sll   s0, s0, 16
j     join
 sra  s0, s0, 15
join:
addu  s0, s0, v0
lhu   a0, 0(s0)
```

`table[arg0]` (or `(s16)arg0 << 1` on every arm) emits `sll 1` for all
paths, or `sll 16; sra 15` for all paths. Sign-extend into `arg0` on the
arms that can overwrite `$s0`, then write the raw arm as an in-place
shift pair and jump past the shared `<<= 1`:

```c
case already_sext:
    arg0 = (s16)arg0;
    if (arg0 >= limit) {
        goto fail;
    }
    table = D_sext;
    break;
case raw_s16:
    if ((s16)arg0 >= limit) {
        goto fail;
    }
    table = D_raw;
    arg0 <<= 16;
    arg0 >>= 15; /* (s16)arg0 * 2, in place on $s0 */
    goto lookup;
}
arg0 <<= 1;
lookup:
    arg0 += (s32)table; /* addu s0, s0, v0 — not table + arg0 */
    entry = (u16*)arg0;
    return GameFlag_GetNibble(*entry & 0x7FF) + (*entry & 0x800);
```

`(s16)arg0 << 1` uses `$v0`/`$v1` as the shift temp and lands the table
in the other register (`addiu v1, %lo` / `addu s0, v1, s0`). The two
statement form keeps both the shift and the later add on `$s0`.
`*(u16*)((u8*)table + arg0)` is `addu s0, v0, s0`.

`func_800AEBA4` is the example. Shared `table[arg0]` stuck at 97.2% with
only the raw arm's three-instruction join missing.

## Index `rec[i].field[j]`, not `(&rec[i].field_0)[j]`

A 16-byte record of eight `u16`s accessed as `(&table[idx].field_0)[j]`
(or via a `(u16(*)[8])` cast) computes the struct index first and loads
the symbol late (`lui a0` after the `*3` chain, `mfhi a3`). The target
loads the table base into `$t0` immediately after the bit extracts,
then `sll a2, a1, 1`.

Declare the record as `u16 field[8]` and write the natural 2D access so
GCC treats the symbol as the array base:

```c
val = D_8011398C[(a * 3 + b) * 3 + c].field[arg1];
```

`func_800D50D4` is the example. `(&rec[idx].field_0)[arg1]` matched in a
one-function scratch and failed the overlay checksum.

## `head` then `child = head`; write `field &= mask` not a shared `flags` temp

A circular `firstChild` / `nextSibling` walk that the target opens with

```
lw    v0, 0xC(s3)
nop
beqz  v0, end
li    s4, 1
move  a1, v0
lui   s1, 0xFFFE
ori   s1, s1, 0xFFFF
```

must not assign the load straight to the iterator. `child = owner->firstChild;
if (child)` keeps the pointer in `$a1` for both the `beqz` and the loop.
Load into a separate `head`, then copy after `one = 1`:

```c
head = owner->firstChild;
if (head != NULL) {
    one   = 1;
    child = head;          /* move a1, v0 */
    mask  = 0xFFFEFFFF;    /* lui/ori s1 — after the copy so next takes s2 */
```

Two switch cases that both do `p->status = 1; p->field_4 &= mask` (one also
stores a global) must be written as `&=`. Routing the mask through a shared
`flags` temp:

```c
flags         = p->field_4;
p->status     = one;
p->field_4    = flags & mask;
```

is identical enough that GCC 2.8.1 cross-jumps the `and`/`sw` into one tail
(`lw v1; j shared; sw status`). The compound `&=` hoists the load before the
independent stores and keeps a dedicated `lw`/`and`/`sw` per case.

`func_800CEE5C` is the example.

## Combine same-result predicates with `||` so `ret` takes `$s2`

Several range/id checks that all store `ret = 1` look natural as `if` /
`else if` arms. That form allocates the pointer to `$s2` and `ret` to
`$s1`, loads the u8 field straight into `$a0` (`lbu a0; beqz a0; jal`),
and merges the field==0 exit with the range-fail epilogue.

One `||` chain and a single `ret = 1` after it puts `ret` in `$s2` and
the pointer in `$s1`. CSE of `p->field` then stays in `$v0`:

```
lbu    v0, field(s1)
nop
beqz   v0, ret0
nop
move   a0, v0
jal    lookup
addiu  a0, a0, K
```

A named temp (`equipped = p->field; func(equipped + K)`) is born in
`$a0` and keeps the `$s1`/`$s2` swap.

```c
ret = 0;
p   = &Wip_SysConfig;
if ((range_a && p->sel_a == id - Ka) ||
    (range_b && p->sel_b == id - Kb) ||
    (range_c && p->sel_a != 0 &&
     (lookup(p->sel_a + K)->field_0 == id ||
      lookup(p->sel_a + K)->field_2 == id))) {
    ret = 1;
}
```

`func_800CEB84` is the example. The if/else-if form stuck at 96.6%;
dropping the temp but keeping if/else-if reached 98.7% with only the
register swap and `lbu a0`.

## Use the parameter, not `flags = arg1`, so `~arg1` reads `$s0`

A saved incoming arg (`move s0, a1`) is live on both the call-clobbered
apply path and the no-call else path. The first bit test sits *after*
`jal Game_GetPtrSlot`, so it must read the saved copy (`andi v0, s0, 1`).
The else is `field &= ~arg1` (`nor a0, zero, s0`).

A new local `flags = arg1` copy-propagates back to `$a1` on the else
path: that arm never calls, so the incoming register is still valid and
you get `nor a0, zero, a1`. Use `arg1` itself for the first test and the
else. The extra local is only the copy that must survive `$s0` being
reused for the actor pointer:

```c
mask = arg1;          /* move s2, s0 */
if (arg0 == 0) {
    work = Game_GetPtrSlot(3);
    if (arg1 & 1) {   /* andi v0, s0, 1 — a1 is dead after jal */
        inner = work->actor;
        ...
    }
    if (mask & 2) {   /* later bits — s0 now holds inner */
        ...
    }
} else {
    Wip_SysConfig.field_25 &= ~arg1; /* nor a0, zero, s0 */
}
```

`func_8010A1B0` is the example. `flags = arg1; ... &= ~flags` stuck at
99.969% with only that `nor` source swapped.

## Split `spawnArg1` (`s32 val`) from the `Text_SkipLines` result

A two-line prompt (`Text_DrawPrompt` / `Text_SkipLines` / `Text_DrawPrompt`)
that keeps the string in one `u8* text` — assign from `spawnArg1`, then
`text = Text_SkipLines(text, one)` — is 98.9% with only `$s2`/`$s3` swapped:
the `UiObject*` lands in `$s3` and the string in `$s2`. The target wants the
object in `$s2` and the string in `$s3`.

Keep `spawnArg1` as an `s32 val` used only through the first draw + skip, and
a separate `u8* text` for the skip result (same shape as `func_800CE4F4`):

```c
val = arg0->spawnArg1;
if (val != 0) {
    Text_DrawPrompt(..., (u8*)val, ...);
    text = Text_SkipLines((u8*)val, one);
    Text_DrawPrompt(..., text, ...);
}
```

`register UiObject* obj asm("s2")` also matches, but the split temps are
enough. `func_800BF4FC` is the example.

## Store via `(T*)(head - K)` before assigning `vec`

Scratch allocation that first writes `sw v0, -K(head)` and only then
`addiu vec, head, -K` is not `vec = (T*)(head - K); vec->field = expr`.
That computes the pointer first. Write the first field through the
unadjusted head, then bind `vec`:

```c
((VECTOR3*)(head - 0x10))->vx = arg0->vx - arg1->vx;
vec                           = (VECTOR3*)(head - 0x10);
vec->vy                       = arg0->vy - arg1->vy;
```

`func_80103DD4` is the example. `vec = (VECTOR3*)(head - 0x10)` first
stuck with `addiu` before the `vx` store.

## Assign `x = x * x` before `SquareRoot0(x + y)` so `mflo` stays in `$a0`

`SquareRoot0(x * x + y)` builds a fresh temp for the product
(`mflo a3; addu a0, a3, v1`). Assign the square back into `x` first.
`mflo` then reuses `x`'s register and the add is in-place in the `jal`
delay slot:

```
mult  a0, a0
mflo  a0
jal   SquareRoot0
addu  a0, a0, v1
```

```c
vx = vx * vx;
vx = SquareRoot0(vx + absz);
```

`func_80103DD4` is the example. The inlined `x * x` stuck at 99.762%
with only that `mflo` dest swapped.

## Pin an early `ABS` temp to `$v1` so it does not steal `$a0`

A standalone `absz = ABS(vz)` emitted *before* the `SquareRoot0`
argument is set up takes `$a0` (first free arg register). The later
reload/ABS/square that should live in `$a0` then lands in `$v0`.

Keep the original `vz` in `$v0` for the store (`bgez v0; move v1, v0`)
and pin the ABS dest:

```c
register s32 absz asm("v1");
register s32 vx asm("a0");

absz = ABS(vz); /* move v1, v0 — vz stays in v0 for the store */
vec->vz = vz;
absz = absz * absz;
vx = ((VECTOR3*)(head - 0x10))->vx; /* lw a0 */
vx = ABS(vx);
vx = vx * vx;
vx = SquareRoot0(vx + absz);
```

`func_80103DD4` is the example. Unconstrained `absz` stuck at 98.2%
with `ABS(vz)` in `$a0` and the `vx` reload in `$v0`.

## Index a global array field by name so dest is `base+off` then scale

`ds = &Display_State` plus `MoveImage((RECT*)&ds->dispEnv[i], …)` folds the
array offset into the scaled index (`addiu a0, scaled, 0x20; addu a0, ds`).
The target computes the array base first (`addiu v0, ds, 0x20`) and adds
the scaled index in the `jal` delay slot.

Write dest through the global name and x/y through the local pointer:

```c
MoveImage(
    (RECT*)&Display_State.dispEnv[ds->field_1f ^ 1],
    ds->dispEnv[ds->field_1f].disp.x,
    ds->dispEnv[ds->field_1f].disp.y);
```

That also rematerializes `field_1f` between the two `lh`s (`lbu` in the
`lh a1` delay slot). A local `DISPENV* dest = ds->dispEnv` emits the
`addiu` base but schedules the reload after both loads.

Keep the `^ 1` as a HImode `u16 one = 1` (used for later `sh`s) so CSE
cannot turn `xori` into `xor s1`. See "Constant CSE across
differently-sized stores".

`func_800AAA68` is the example. `&ds->dispEnv[i]` stuck at 97.5% with
only those three dest instructions different.

## Separate temps so a later switch can keep the child in `$a0`

A signed halfword loaded for an early compare (`lh a0, field`) pins that
variable in `$a0`. Reusing the same `s32` for a later `switch (child->field)`
keeps the second value in `$a0` too, so the child pointer lands in `$v0`
and the case-6 call emits `move a0, v0` (plus `slti` out of the `beq`
delay slot).

The target wants the child in `$a0` from the first load (it is the first
arg of `Ui_TeardownTree`), the compare-constant `6` in `$a1` (also stored
in case 9), and the switch value in `$v1`.

Use a *new* temp for the first compare so the switch variable is free:

```c
sel = menu->field_22; /* lh a0 — dies before the child walk */
if (sel != 0x20) {
    ...
}
child = arg0->firstChild; /* lw a0 */
flag  = child->field_2E;  /* lh v1 */
switch (flag) {
case 6:
    Ui_TeardownTree(child, child->owner); /* a0 already child */
    ...
case 9:
    obj->field_2E = 6; /* sh a1 */
}
```

`func_800CB188` is the example. Reusing `flag` for both loads stuck at
97.8% with only the switch registers (and one extra `nop`/`move`) wrong.

## Overlay the mid-byte of a little-endian `s32` so the load is `lbu`

An 8.8 interpolator stored as `s32` (`start << 8`) is posted as its
integer byte. `(u8)(state->field_4 >> 8)` emits `lw; srl; andi`. The
target is a single `lbu` at offset +1 of that word (LE byte 1).

Put a union on the word so the mid-byte is a real field. GCC then
emits `lbu`:

```c
union {
    s32 as_s32;
    struct {
        u8 pad_4;
        u8 as_u8; /* (as_s32 >> 8) */
    } bytes;
} field_4;

Pad_PostEvent(0, 1, state->field_4.bytes.as_u8, 1);
```

`func_800E9498` is the example. The shift form compiles and is
semantically identical but cannot match.

## Index the copy (`dest[i] = src[i]`) so `count` wins `$a1`

A word copy whose count is loaded from `arg->count` and whose source
is `arg->src` (offset 0) fights over `$a1`. `*dest++ = *src++` treats
`src` as a live pointer and schedules `lw a1, 0(arg)` first, so count
lands in `$a3` and the loop never emits the target's

```
move  a1, dest
move  a0, src
```

remap. Indexed stores keep `src` as a base, so count takes `$a1` and
src takes `$a3`:

```c
src   = arg2->field_0;
count = arg2->field_4;
if (count >= 0x21) {
    return 1;
}
dest = ((Blk*)dest)->field_BC;
for (i = 0; i < arg2->field_4; i++) {
    dest[i] = src[i];
}
```

Assign dest from the table as `s32*` (not the block struct). A
`Blk* dest = table[i]` load goes to `$v1`; `(s32*)table[i]` reuses
`$a0` so `dest += 0xBC` is `addiu a0, a0, 0xBC`.

`func_80105914` is the example. The increment form stuck at 93.8%
with only those two loads (and the two `move`s) swapped.

## Share `i = x - 1` and hoist one array so both tables use `lw 0`

Two external pointer tables indexed by the same `field - 1` fight over
the subtract. `array[x - 1]` on a global is `addiu -1; sll 2; lw 0`.
The same index through a hoisted `T** p = array` is `sll 2; lw -4`, so
the scaled `(x-1)*4` is not CSE'd and a later `p[x - 1]` rematerialises
the second base.

Assign the decremented index once, add it onto the hoisted pointer
immediately (keeps `&array[i]` live, load delayed), then index the
other global with the same `i`:

```c
tbl68 = D_8010CB68;
i     = sess->field_3 - 1;
tbl68 = &tbl68[i];          /* addu a2, v1, a2 */
tbl   = D_8010CB54[i];      /* addu v1, v1, v0; lw 0(v1) */
...
tbl2  = *tbl68;             /* delayed lw 0(a2) */
```

Direct `D_8010CB68[sess->field_3 - 1]` rematerialises that address
later (`func_800AD2E8`). `func_800ACF8C` is the example.

## `volatile` walk pointer so a field reloads after `sltiu`

Two reads of the same `u8` on a loop pointer (`table->field_0` for a
range check, then again for `id - K`) CSE into a spare register. The
first `lbu` is kept, `-K` fills the `beqz` delay slot, and the extra
temp steals `$a0` from the pointer so `i` / the bound swap (`$a1`/`$a2`
vs `$a2`/`$a1`).

The target overwrites `$v0` with the range subtract, reloads, and
leaves `nop` in the delay slot:

```
lbu   v0, 0(a0)
addiu v0, v0, -0x60
sltiu v0, v0, 0x20
beqz  v0, skip
 nop
lbu   v0, 0(a0)
lbu   v1, off(s2)
addiu v0, v0, -0x5F
```

Mark the walk pointer `volatile`. CSE cannot keep the first `lbu`, and
`-fdelayed-branch` will not speculate the second volatile load into
the `beqz` slot. Also use `scan->field_1` directly in the `for`
condition — a `count = scan->field_1` local takes `$a1` and puts `i`
in `$a1` instead of `$a2`.

```c
volatile GpItemRec* table;
...
for (; i < scan->field_1; i++) {
    if (((u32)(table->field_0 - 0x60) < 0x20U) &&
        (p->field_23 != table->field_0 - 0x5F)) {
        count++;
    }
    table++;
}
```

`func_800CF090` is the example. A plain `GpItemRec*` stuck at 92% with
only the load reused and those two registers swapped.

## Assign a negative constant to `s32` before storing it to a `u16` field

`obj->field_E = -0x5C` on a `u16` dest converts the constant to
`0xFFA4` in the front end and emits `ori v0, 0xffa4`. The target
materializes it as signed (`addiu v0, -0x5c` / `li v0,-0x5c`).

Assign the constant to an `s32` first, then store that. The 32-bit
temp keeps the signed immediate; `sh` just takes the low 16 bits:

```c
s32 y;

y            = -0x5C; /* addiu v0, -0x5c */
obj->field_E = y;     /* sh v0 */
```

A cast alone (`obj->field_E = (s32)-0x5C`) folds back to the field's
unsigned type. `func_800CF940` is the example. The bare
`obj->field_E = -0x5C` stuck at 99.8% with only those two `li`
encodings different.

## Split 0xFFFF sentinels; reuse the id register as the dest pointer

A 2-bit bank writer that walks `-1`-terminated list nodes and
`0xFFFF`-terminated records wants the mask built *before* the dest
address, two different `0xFFFF` registers (`$t3` for the first `if`,
`$t0` for the inner `do`/`while`), and `-1` rematerialized at the
outer tail (`li v0,-1` / `bne a2,v0`).

Pin the inner temps (`id` in `$a0`, word in `$v1`, field_6 in `$a1`,
scratch in `$v0`, `3` in `$t1`, first sentinel in `$t3`). After the
nibble is extracted, overwrite `id` with the dest word pointer so the
mask `sllv` stays in `$v0` and `$a0` becomes `dest[id >> 4]`:

```c
register u32 id asm("a0");
register s32 tmp asm("v0");
register u16 term asm("t3");
u16 inner;

term = 0xFFFF;
if (id != term) {
    inner = 0xFFFF;
    do {
        tmp = tmp * 2;
        tmp = three << tmp;           /* mask first */
        id = (u32)(dest + (id >> 4)); /* then dest in $a0 */
        ...
        id = rec->field_0;
        tmp = id & 0xF;
    } while (id != inner);
}
```

Two sentinels are required: CSE of a single `0xFFFF` becomes
`move t0,t3` in the prologue. Do **not** pin the table/rec pointers —
pinning them made the first `lw` use `$a0` instead of `$a3`. Assign
`tmp = -1` at the outer tail so the compare rematerializes instead of
CSE into `$t4`. `func_800BB838` is the example (sibling
`func_800BAB64` is the same walk with a bank lookup in front).

## Put `&Table` in `$v0` before overwriting it with the compare constant

A bank lookup that must emit

```
lui   v0, %hi(Table)
addiu v0, v0, %lo(Table)
sll   v1, a0, 3
addu  v1, v1, v0
li    v0, 3
lw    a1, 0(v1)
```

cannot assign the `3` first. `tmp = 3; bank = &Table[arg0]` (or a
separate `banks` local) hoists `li v0, 3` and turns the scale into
`sllv v1, a0, v0`, and `&Table` lands in `$a1` instead of `$v0`.

Assign the table address through the same `$v0` temp *before* the
constant so the address occupies `$v0` during the scale. Then overwrite
it:

```c
register s32 tmp asm("v0");
GpBit2Bank*  bank;

tmp  = (s32)D_8010D230;
bank = (GpBit2Bank*)tmp + arg0;
tmp  = 3;
val  = (u32)bank->field_0; /* keeps the first lw in $a1 */
dest = bank->field_4;
if (arg0 == tmp) {
    return;
}
table = (GpBit2List*)val; /* delay-slot move a3, a1 */
```

`func_800BAB64` is the example. `bank = &D_8010D230[arg0]; tmp = 3`
stuck at 90–94.5% with only that prologue different. This is the
inverse of the `mask = 1` before `ptr->arr[i]` rule: here `$v0` must
hold the *address* first so `3` cannot be reused as the shift.

## Find-or-allocate: `if (found) goto update` inside the miss arm

A table walk that keeps a match in `found`, then on miss scans for a
free slot and finally mutates the chosen slot, looks like two
independent `if`s:

```c
if (found == NULL) {
    /* allocate */
}
if (found != NULL) {
    found->timer = 0x14;
    found->val += arg1;
}
```

That second `if` rotates the alloc loop (peeled first load, `i` in
`$t0` instead of `$a2`). The target is `bnez found, update` around
the alloc scan, then `beqz found, return` into one shared tail.

Write the update as the else of the miss test, and jump to it from
inside the miss arm when alloc succeeds:

```c
if (found == NULL) {
    /* alloc loop */
    if (found != NULL) {
        goto update;
    }
} else {
update:
    found->field_6 = 0x14;
    found->field_4 += arg1;
}
```

`func_800DA6E8` is the example. The follow-up `if (found != NULL)`
form stuck at 75% with only that control-flow shape different. The
first search also needs a goto-back loop (not `do`/`while`) so the
first `lw 0(p)` is not peeled off `&D_80115270` — same anti-peel
trick as `Text_SkipLines`.

## Pin the table to `$v0` inside the `if` so session `lui` takes `$v1`

A task callback that inlines a 2-bit bank lookup after `if (state == 1)`
wants the `bne` delay slot to start the session pointer:

```
bne   v1, v0, ret
 lui   v1, %hi(Game_Session)
lui   v0, %hi(table)
addiu v0, v0, %lo(table)
lw    v1, %lo(Game_Session)(v1)
```

`p = table[Game_Session->field].field_4` evaluates both addresses in
parallel but puts the session in `$v0` and the table in `$v1`. Assign
the session first and pin the table to `$v0` so the session load uses
the dest register (`lui v1` / `lw v1`). Pin `p` to `$v1` so the bank
pointer stays there and `id >> 4` can reuse `$v0`:

```c
if (arg0->state == 1) {
    register GpBit2Bank* banks asm("v0");
    register u32*        p asm("v1");
    GameSession*         sess;

    sess  = Game_Session;
    banks = D_8010D230;
    p     = banks[sess->field_7].field_4;
    p    += id >> 4;
}
```

Declare the pins *inside* the `if`. Function-scope `asm("v0")` /
`asm("v1")` steals `$a1` from the task argument (it slides to `$a2`,
`extra` to `$a3`). `func_800BBC10` is the example. The unpinned
one-expression form stuck at 98% with only those two registers swapped.

## Scratch-head `+r` barrier so `&global` lui fills the load delay

A function that allocates from `G_SCRATCH_HEAD` and also takes
`&D_global` wants:

```
lw    t1, 0(v0)          /* head = *G_SCRATCH_HEAD */
lui   t0, %hi(D_global)  /* delay fill */
addiu a2, t1, -8
sw    a2, 0(v0)
lw    a0, 0x1C(s1)
nop
...
beq   v1, a1, case1
 addiu a3, t0, %lo(D_global)
```

Writing `params = &D_global` next to the alloc lets GCC hoist the `lui`
before `sw ra` and stuff `lw a0` in the scratch-head delay (losing the
`nop` after the actor load). A memory clobber after the load keeps the
`lui` in place but materializes `addiu a3` immediately instead of in the
`beq` delay.

Take the scratch pointer first, then an empty `+r` on that pointer so
the later `&D_global` cannot hoist past the load. Pin the loaded head to
`$t1` so `$a1` stays free for the switch's `li 1`:

```c
register u8* head asm("t1");

scratch = (void**)G_SCRATCH_HEAD;
__asm__ volatile("" : "+r"(scratch));
head   = *scratch;
params = &D_global;
vec    = (SVECTOR*)(head - 8);
*scratch = vec;
```

`func_8010AD64` is the example.

The same function also reloads `arg0->actor` in `case 2` after the
switch already loaded it into `$a0`. Without a barrier GCC emits
`move s0, a0`. A memory clobber forces `lw s0, 0x1C(s1)`:

```c
case 2:
    asm("" ::: "memory");
    inner2 = arg0->actor;
    func_8010B210(arg0);
```

## Assign `one = 1` before `n = count` so `li` precedes the copy

A scan that loads `count` into `$a0` (`lbu a0, 1(scan)`) and later
reuses `$a0` for each row's id needs an explicit copy
(`move t2, a0`). `n = count` at the top of the loop produces that
copy, but if it is the first statement after `p = &Global` the
hoisted `1` for `if (ok == 1)` lands *after* the copy:

```
addiu t1, %lo(Global)
move  t2, a0
li    t3, 1
```

Assign the compare constant first. `one = 1` then `n = count` keeps
`li t3, 1` ahead of `move t2, a0`, and the loop test uses `$t2`
while `$a0` is free for `id`:

```c
if (count != 0) {
    p   = &Wip_SysConfig;
    one = 1;   /* li t3, 1 */
    n   = count; /* move t2, a0 */
    do {
        ok = 1;
        id = table->field_0; /* lbu a0 */
        ...
        if (ok == one) {
            arg1--;
        }
        ...
    } while (i < n);
}
```

`func_800CECC0` is the example. `n = count` before `one = 1` stuck at
98% with only those two instructions swapped, and without `n` the
count never entered `$a0` at all (`lbu t1` / no `move`).

## Split the pre-call and post-call object pointers

A switch on `obj->field` that also passes `obj` into an early call, then
reloads the same object after later calls for stores, must use *two*
locals. One variable is live across the calls, so it is allocated `$s0`
from the first load:

```
lw    s0, 0x1c(s2)
lhu   s1, 0x95e(s0)
addiu a0, s0, 0x424
```

The target keeps the first pointer in `$a1` (caller-saved) and only
loads `$s0` after the calls:

```
lw    a1, 0x1c(s2)
lhu   s1, 0x95e(a1)
addiu a0, a1, 0x424
...
jal   func
...
lw    s0, 0x1c(s2)
```

Assign the pre-call uses to one local and reload into a second after
the calls:

```c
actor = arg0->actor;
switch (actor->field_95E) {
case 1:
    if (func(actor->ctx, actor->slot + 1) != NULL) {
        if (func2(...) == 0) {
            inner = arg0->actor; /* second local → $s0 */
            inner->field_954 = 0;
            ...
        }
    }
}
```

`func_80108224` is the example. Reusing `inner` for both loads stuck at
99.7% with only `$a1` vs `$s0` on the first pointer.

## Nest `if (x != 0)` so the zero case is a real else, not a delay-slot assign

`if (x == 0) A; else if (x == 1) B; else C` fills the `beqz` delay slot
with A's assignment and falls through after B (no jump). The target
instead does

```
beqz  v1, zero
li    v0,1
bne   v1,v0, join
li    a1,5
j     join
li    a1,6
zero:
li    a1,1
```

Nest the nonzero tests so the zero case is the outer else. GCC then
uses `$v0` for the `== 1` compare (delay slot of `beqz`) and emits the
jump after B:

```c
if (x != 0) {
    if (x == 1) {
        mode = 6;
    } else {
        mode = 5;
    }
} else {
    mode = 1;
}
```

`func_801066DC` is the example. The `== 0` / `else if == 1` form (used
by the similar `func_80108620`) stuck at 81% with the extra jump
missing and `li a1,1` in the `beqz` delay slot.

## Reassign the compared local to `1` so it stays in `$v1` for later stores

`temp = ptr->s8; if (temp != -1) { ptr->a = 1; ... ptr->b = 1; }`
recomputes `1` in `$v0` for each store. The target overwrites `temp`
in the `!= -1` delay slot and reuses that register:

```
beq   v1,v0, else   /* temp != -1, v0 is -1 */
li    v1,1
sh    v1, a(ptr)
...
sh    v1, b(ptr)
```

Assign `temp = 1` after the compare, then store through `temp`. That
kills the old value, so `li v1,1` fills the delay slot and both stores
use `$v1`:

```c
temp = inner->field_973;
if ((inner->field_962 & 0x40) && (temp != -1)) {
    temp             = 1;
    inner->field_95A = temp;
    ...
    inner->field_958 = temp;
}
```

`func_801066DC` is the example. Two literal `= 1` stores stuck at 81%
with `$v0` for both the constant and the `Mc_SaveData` address.

## Pin `&node->field` in `$v1` so `$a0` can hold `ONE` then a global

A calloc'd node with an embedded coord at +0x10 needs `&node->coord` live
in `$v1` across the identity-matrix stores. Without that pin GCC gives the
pointer `$a0`, `ONE` lands in `$v1`, and `&Global` is delayed until after
the zero stores (the pointer still occupies `$a0`).

`register Type* coord asm("v1")` assigned *before* the NULL check puts
`addiu v1, s0, 0x10` in the `beqz` delay slot. `$a0` is then free for
`li a0, 0x1000` and, once `ONE` is consumed, `lui; addiu a0, %lo(Global)`:

```c
register GpDisp2dCoord* coord asm("v1");

node  = Mem_Calloc(0x60, 0);
coord = &node->coord;
if (node != NULL) {
    node->field_C = 1;
    node->field_8 = coord;
    coord->sub    = &D_80070F10;
    one           = ONE;
    ...
    list = &Tmd_ListAlt;
}
```

`func_80099098` is the example. The same body without the register pin
stuck at 92.6% with only those registers (and the late `&Tmd_ListAlt`)
different.

## Join timeout + confirm with `||` so `one` stays in `$s0`

`one = 1` is saved in `$s0` for `Text_DrawMultiLine(..., one, 0)` and
`status == one`. The first `Pad_CheckButtons(0, one, mask)` should reuse
that register (`move a1,s0`). Splitting the timeout and confirm into
separate `if` / `else if` arms with the same body rematerializes the
constant (`li a1,1`).

Write them as one `||` so `one` stays live into the call:

```c
one = 1;
Text_DrawMultiLine(obj, x, y, text, color, one, 0);
task->killCountdown--;
if (obj->status == one) {
    if ((task->killCountdown <= 0) ||
        (Pad_CheckButtons(0, one, maskA | maskB) != 0)) {
        obj->field_2E      = 6;
        task->killCountdown = 0x7FFF;
    } else if (Pad_CheckButtons(0, 1, maskCancel) != 0) {
        obj->field_2E = -1;
    }
}
```

`func_800D540C` is the example. The duplicated-body `if` / `else if`
stuck at 97.1% with only that `move` vs `li`.

## Split `A*N + B*M + K + C` so `K` stays on `B*M`

`item = idx * 16 + slot * 4 + 0x300 + count` folds `0x300` onto
`count` (`addiu v0, s0, 0x300` after the load). Parentheses
(`idx * 16 + (slot * 4 + 0x300) + count`) reassociate the other way:
`0x300` lands on `idx * 16` (`sll v1, 4; addiu v1, 0x300`).

The target does both shifts first, then `addiu a0, a0, 0x300` on the
*second* scaled term, then adds `count`:

```
sll    v1, idx, 4
sll    a0, slot, 2
addiu  a0, a0, 0x300
lbu    count, ...
addu   v1, v1, a0
addu   item, v1, count
```

Assign each scaled term to its own `s32` *in that order*. A new temp
for `slot * 4 + 0x300` (not a write-back to `slot`) keeps `slot` live
for the earlier indexed load, so `a0` is reused only after the
address is done:

```c
s32 off;
s32 base;

count = table[slot + idx * 3];
off   = idx * 16;
base  = slot * 4 + 0x300;
item  = off + base + count;
```

`func_800D27E8` is the example. The one-expression form stuck at 98–99%
with only those three instructions different.

## Index a u16 table with the stored u8, not `(u8)arg`

A function that stores `slot->field = arg` and later indexes
`table[(u8)arg]` coalesces the truncation into the argument register
(`andi a1, a1, 0xff; sll a1; addu a1, table; lhu v0, 0(a1)`).

The target CSEs the stored byte as a *new* dest:

```
andi  v0, a1, 0xff
sll   v0, v0, 1
addu  v0, v0, a0
lhu   v0, 0(v0)
```

Write `table[slot->field]` (the same u8 just stored from `arg`). CSE
still emits `andi` from the live argument, but the dest is `$v0`.

Load an independent byte into a temp *before* nearby zero stores so
those stores fill the `lbu` delay. Writing the zeros first schedules
them into an earlier load delay instead.

```c
slot->field_15 = arg1;
slot->field_6  = sets[arg2]->field_4[slot->field_15];
op             = recs[slot->field_6].field_3;
slot->field_10 = 0;
slot->field_B  = op & 0xF;
```

`func_800B3FA8` is the example. `table[(u8)arg1]` stuck at 99.3% with
only those five index instructions using `$a1` instead of `$v0`.

## `+r`(index) / `"r"(loaded)` keeps `lbu` before `id -= K`

A store of one byte plus an independent `id -= K` used by a later
`sltiu` wants the subtract in the load delay:

```
lbu    v0, field(map)
addiu  v1, v1, -K
sb     v0, dest(slot)
sltiu  v0, v1, bound
```

`dest = map->field; id -= K; slot->out = dest` (and the same statements
without a temp) schedules the ALU first: `addiu; lbu; nop; sb`. Volatile
on the load does not stop that move — `addiu` is not a memory op.

Take the load into a temp, then an empty `+r` on the index with the
loaded value as an input. The asm cannot move before the load, and the
subtract cannot move before the asm, but the empty asm emits nothing so
the subtract still fills the `lbu` delay:

```c
mapped = map->field_2;
asm volatile("" : "+r"(id) : "r"(mapped));
id -= 0x80;
slot->field_0 = mapped;
```

`func_800B6CF0` is the example. Pair with `register s32 count asm("a1")`
so the default `count = 0` stays in `$a1`, and `&arr[i]` each iteration
(not `p++`) so GCC does not strength-reduce `&p->field_2` into a second
IV. `off + (s32)base` still supplies `addu a0, v0, base`.

## Duplicate the store and increment inside both `if` arms

A loop-counter opcode that then shares `pc++` with the previous case
wants the `field_E` load *duplicated* in both arms (`lbu` / `j merge` /
`srl` in the delay slot, sibling `lbu`, then one `sb` of the counter).
Writing the store and `pc++` *after* the `if` CSEs the load into a
single `lbu` below the merge:

```
bnez  v1, shared_load
 addiu v1, -1
srl   v1, cmd, 8
shared_load:
lbu   v0, pc
sb    v1, counter
```

Put the store and increment in *both* arms. Tail-merge keeps one `sb`
of the counter and the shared `pc++`, but each arm keeps its own `lbu`:

```c
tmp = p->counter;
if (tmp == 0) {
    tmp         = cmd >> 8;
    p->counter  = tmp;
    p->pc++;
} else {
    tmp--;
    p->counter = tmp;
    p->pc++;
}
```

`func_800E8A90` case 3 is the example. The shared-after-if form stuck at
97% with only those two `lbu`s merged. The extra live ranges also
swapped `cmd` / table-pointer coloring (`$v1`/`$a2`) without a pin.

## Index the stored `u8` field so CSE emits `andi v0, arg, 0xff`

After `slot->field_15 = arg3`, `table[(u8)arg3]` (or a `u8` parameter)
zero-extends in place (`andi a3, a3, 0xff`) and the pointer load takes
`$v0`. The target instead does

```
lw    a0, 0(a2)
andi  v0, a3, 0xff
lw    a0, 4(a0)
sll   v0, v0, 1
```

Index with the field that was just stored. CSE reuses the original
argument register and the `andi` dest is `$v0`, so the pointer stays in
`$a0`:

```c
slot->field_15 = arg3;
slot->field_6  = sets[arg2]->field_4[slot->field_15];
```

`func_800B404C` is the example (`func_800B3FA8` already uses
`field_4[slot->field_15]` for the same reason). The `(u8)arg3` form
stuck at 98.8% with only those six registers swapped.

## Write switch cases in target body order, not numeric order

GCC 2.8.1 emits `switch` case bodies in **source order**. The jump table
still indexes by `arg - first`, but the `.rdata` words point at whatever
label order the bodies were written.

`func_800D6170` has case 8's `lb` immediately after case 4's `lb`, then
cases 5 / 6-7. Writing `case 8:` after `case 7:` stuck at 95% with the
case-8 block inserted after 6-7 and the table slots swapped. Moving
`case 8:` to sit under `case 4:` matched the table and the instruction
stream.

```c
case 4:
    if (D_80114C08.field_16 == 0) {
        ret = 0;
    }
    break;
case 8: /* body is next in the target, even though 5-7 come later */
    if ((s8)D_80114C08.field_17 == 0) {
        ret = 0;
    }
    break;
case 5:
    ...
```

## Overlay jtbl: move `.rodata, TU` to the new C table, leave later asm

A gameplay switch whose jump table sits in the middle of an asm rodata
block cannot just emit another table at the current C `.rodata` cursor
(that is the *next* already-matched jtbl). Move the yaml
`[offset, .rodata, TU]` start to this table. Splat then:

- keeps earlier unmatched tables in the preceding `rodata_TU` file
- migrates later unmatched strings/jtbls onto the `INCLUDE_ASM` functions
  that reference them
- leaves a hole for symbols used only by already-matched C

Fill that hole with a `const` in the C file **after** the `INCLUDE_ASM`
that emits the previous rodata, so `.rdata`/`.rodata` concatenation stays
packed. `func_800D6170` is the example: yaml `.rodata, 3A34` moved from
`0x3CB4` to `0x3B3C`, and `D_80097454` is a C `const char[]` after
`func_800D6334`.

## Pin the flag, not the pointer, so `&Global[i]` keeps `$v1`

Two equally-lived locals (`slot` and `found`) fight over `$a2`/`$a3`.
Natural allocation puts `found` in `$a2` and the pointer in `$a3`, with
the address calc in `$v1`:

```
sll    v0, a0, 3
lui    v1, %hi(Mc_SaveData)
addiu  v1, v1, %lo(Mc_SaveData+0x1C8)
addu   a3, v0, v1
```

`register ptr asm("a2")` swaps the coloring but also folds the `lui` /
`addiu` into `$a2`, so there is no `$v1` temp. Pin the *other* local
instead. The pointer then falls into `$a2` and keeps the `$v1` sequence:

```c
register s32 found asm("a3");
GpItemSlot*  slot;

found = 0;
slot  = &Mc_SaveData.field_1C8[arg0]; /* sll; lui v1; addiu; addu a2,v0,v1 */
```

`func_800BB0CC` is the example. Pinning `slot` to `$a2` stuck at 99.6%
with only those three address instructions different.

## Reuse the mask temp so the second `pad & MASK` writes `$v0`

A d-pad word used twice (`bits = pad & 0xF000`, then `else if (pad & 0x5000)`)
lets GCC and in-place on `pad` (`andi v1, v1, 0x5000` in the `bne` delay).
The target wants `andi v0, v1, 0x5000` so the following `beqz` reads `$v0`.

Assign the second mask back into the same `s32` that held the first. `bits`
stays in `$v0` across both compares; `pad` stays in `$v1` for the delay-slot
andi. A dedicated `next` temp for the timer reload (not the countdown `timer`)
also keeps `$v0` live through `lhu pad`, so the store fills the load delay:

```c
next             = (rand() & 0x1F) + 0xA;
pad              = inner->field_962;
inner->field_990 = next;          /* lhu v1; sb v0 */
bits             = pad & 0xF000;  /* andi v0, v1, 0xF000 */
if (bits == left || bits == (right = 0x2000)) {
    ...
} else {
    bits = pad & 0x5000;          /* andi v0, v1, 0x5000 */
    if (bits) {
        ...
    }
}
```

`(s8)timer` after `timer = u8_field - 1` is the zero test as `sll 24; bnez`
(not `andi 0xff`). Reusing `timer` for `next`, or writing `else if (pad & 0x5000)`
without the `bits =` assign, stuck at 97% with only that load/store order and
the andi dest swapped. `func_8010A670` is the example.

## Reconstruct a 2-case switch with a shared (case 1 / default) tail

A `switch (kind)` on `{0, 1, default}` where case 1 and default share
`p->u0 = u; p->v0 = K` lowers to if-else that tests `== 0` first, or to
`bne != 1` with the case-1 block *before* the range check. The target is
the range-check switch form:

```
li    v0,1
beq   a2,v0,case1
slti  v0,a2,2
beqz  v0,default
nop
bnez  a2,default
# case 0: unique stores, jump over
j     mid
li    v0,u_case1
# default: otIdx = -1; u = u_default
# mid: sb u0; li v0,K
```

`if (kind == 1) { u = … }` emits `bne` and plants case 1 above `slti`.
A clean `switch` does not emit this layout under `-O2`. Spell the
expansion with gotos so case 1 sits between case 0 and default:

```c
if (kind == 1) {
    goto case1;
}
if (kind >= 2) {
    goto default_case;
}
if (kind != 0) {
    goto default_case;
}
p->u0 = 0xA0;
p->v0 = 0x88;
goto after_uv;
case1:
    u = 0xA8;
    goto store;
default_case:
    otIdx = -1;
    u     = 0xA0;
store:
    p->u0 = u;
    p->v0 = 0x80;
after_uv:
```

`func_800A63B4` is the example. `if (kind != 1) { … slti … } else { u = 0xA8; }`
was 98.3% — same tests, but default landed before case 1.

## Assign the loop compare constant before the item-table switch

A scan that compares `rec->field_0 == K` after the usual
`switch (scan->field_2)` table select wants `li tN, K` in the delay slot
of the first `beq field_2, 1`. Writing `if (rec->field_0 == 0x81)` (or
assigning `item = 0x81` after the switch) materializes K later, often
clobbering the table pointer's register and skipping the shared `i = 0`
epilogue.

Assign the constant *before* the switch, then compare against that
local. GCC keeps it live across the table select and plants it in the
`beq` delay slot:

```c
item = 0x81;
switch (scan->field_2) {
    case 2:
        tmp = D_80114C20;
        break;
    case 1:
        tmp = D_80114D70;
        break;
    default:
        tmp = Mc_SaveData.field_1AC;
        break;
}
table = tmp;
...
if (rec->field_0 == item) {
    acc += rec->field_2;
}
```

`func_800BAA58` is the example. The rest of the loop is the same shape
as `func_800BB6FC` (`off + (s32)table`, `limit = count`). A literal
`== 0x81` after the switch stuck at 84% with the table in `$v1` and no
`$t0`.

## Call the just-stored global so the first `jal` CSE's into delay-slot `andi`

A u16 local stored to a global and then passed to a function wants

```
lhu   a0, field
sh    a0, Global
...
jal   func
 andi a0, a0, 0xffff
```

on the first call, but a *reload* (`lhu a0, Global`) on a later call after
`$a0` has been clobbered. `func(item)` CSE's the compare's zero-extend
into the first `jal` (`move a0, v1` / `nop`). `func(Global)` CSE's the
store instead: the first call reuses the live `lhu` and promotes in the
delay slot; the second call reloads.

A nested block that pins the pointer/`lhu` dest (`asm("v1")` / `asm("a0")`)
and copies out to an unpinned `u16` is what keeps `item` in `$a0` for the
compare without pinning it across the `jal`:

```c
{
    register Obj* obj asm("v1");
    register u16  ritem asm("a0");

    obj    = arg0->spawnArg2;
    ritem  = obj->field_8;
    Global = ritem;
    ritem  = obj->field_A;
    Item   = ritem;
    item   = ritem;
}
if (item < 0xA0) {
    if ((u16)(item - 0x60) < 0x20U) {
        if (func(Item) != 0) { /* andi a0, a0, 0xffff */
            Item = REMAP;
        }
    }
}
```

`func_800BF624` is the example. `func(item)` stuck at 96% with only that
delay slot and the compare's `andi` dest different.

## Volatile `field + K` reloads into dest; pin `v0` + `+r` for `lbu v0; addiu dest, v0, K`

A max-update of a volatile byte (`if (acc < p->h + 2) acc = p->h + 2`)
forces the second load (good) but combines it as

```
lbu    a3, 3(v1)
addiu  a3, a3, 2
```

when `acc` lives in `$a3`. The target keeps the load in `$v0`:

```
lbu    v0, 3(v1)
addiu  a3, v0, 2
```

Load into a `register s32 tmp asm("v0")`, barrier it, then add:

```c
volatile GlyphUvwh* glyph;

glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)table);
if (acc < glyph->h + 2) {
    tmp = glyph->h;
    asm volatile("" : "+r"(tmp));
    acc = tmp + 2;
}
```

`&table[code & 0x3FF]` flips the `addu` operands (`table + scaled` vs
`scaled + table`). The integer `idx * sizeof + (s32)table` form matches
`addu v1, v0, t3`. `func_800E6AD4` is the example; 99.8% with only that
`lbu` dest wrong.

## Pin the return through `$v0` + `+r` so `move v0, src` stays before the restores

A function that accumulates in `$a2` and returns that sum wants

```
move    v0, a2
lw      ra, off(sp)
lw      s1, off(sp)
lw      s0, off(sp)
jr      ra
```

`return count` (and even `register s32 ret asm("v0"); ret = count; return ret`)
sinks the copy after the restores, because `$a2` / `$v0` are not clobbered by
those `lw`s:

```
lw      ra, off(sp)
lw      s1, off(sp)
lw      s0, off(sp)
move    v0, a2
jr      ra
```

Assign to a `register s32 ret asm("v0")` and barrier it so the copy is a
real instruction in the body:

```c
register s32 ret asm("v0");

ret = count;
asm volatile("" : "+r"(ret));
return ret;
```

`func_800BAFF4` is the example. A bare `return count` stuck at 98.9% with
only that `move` delayed.

## Split `end` / `limit` so the bound is `addu v1` then `move t0, v1`

A loop bound that is live through the body wants `$t0`, but the
`start < end` compare wants the add in `$v1`.
`limit = start + n; if (start < limit)` assigns the add straight to `$t0`.
Compute the bound, compare, then copy:

```c
end = start + arg0->field_1;
if (start < end) {
    slots = Mc_SaveData.field_1C8;
    limit = end;
    /* ... */
    for (; start < limit; start++, rec++) {
```

That is `addu v1, a1, v0; slt; ...; move t0, v1`. `func_800BAFF4` is the
example.

## Share `state++` with `else { goto epilogue; }` after a 3-way dispatch

A task callback that increments `state` only for 0 and 1, then always runs
a countdown, wants one shared `state++` block. Incrementing inside each arm
duplicates the load/add/store; falling through after `if / else if` also
increments for `state > 1`.

Put the increment after the dispatch and skip it with an explicit else:

```c
if (arg0->state == 0) {
    work0();
} else if (arg0->state == 1) {
    work1();
} else {
    goto countdown;
}
arg0->state++;
countdown:
    arg0->killCountdown--;
```

That is `j increment` from state 0, `bne state, 1, countdown` into state 1,
then one `lw / addiu / sw` of `state`. `func_800A0718` is the example. Write
the inner `if (!(flags & bit)) { spawn; } else { flag = 0xFF; }` so the
spawn path is the fall-through (`bnez` to the store).

## Pass the zero `ret` local as a call argument so `move a2, s1`

A helper called with a literal `0` after `ret = 0` emits `move a2, zero`.
The target instead copies the saved return local:

```
jal    helper
move   a2, s1
```

Pass that local, not a fresh `0`:

```c
ret = 0;
if (actor->field_954 != 2) {
    /* ... */
    func_80106350(arg0, p->field_21, ret);
    /* ... */
} else {
    ret = 1;
}
return ret;
```

`func_80105754` is the example. `func_80106350(..., 0)` stuck with only
that delay-slot source different.

## Put a `found:` label between switch cases so the load sits in the gap

A search loop after a 1/2/default table switch wants

```
case 1:  lw table; j after_qty; move t0, 0
found:   lhu t0, 2(rec); j after_loop
default: addiu table, Save+off
after_qty:
    move t0, 0
    /* walk the table */
    beq  rec->id, item, found
after_loop:
```

`if (rec->id == item) { qty = rec->field_2; break; }` (or an `else` after
`!=`) emits the `lhu` after the loop plus an extra `j after; nop` to skip
it. GCC 2.8.1 will not move that block into the hole between `case 1` and
`default`.

A regular label *inside* the switch, between those cases, is the hole:

```c
switch (scan->field_2) {
    case 2:
        tmp = D_80114C20;
        break;
    case 1:
        tmp = D_80114D70;
        break;
    found:
        qty = rec->field_2;
        goto after_loop;
    default:
        tmp = Mc_SaveData.field_1AC;
        break;
}
table = tmp;
qty   = 0;
asm volatile("" ::"r"(qty));
/* ... */
if (rec->field_0 != item) {
    i++;
    rec++;
    if (i < loop_end) {
        goto loop;
    }
} else {
    goto found;
}
after_loop:
```

`qty = 0` plus the volatile keeps `move t0, 0` in the case-1 delay slot
(so that jump skips the join). Without the pin, `qty = 0` sinks into a
later delay slot and the gap collapses. `func_800BAD28` is the example.
A loop-local `qty = rec->field_2` stuck at 93% with only that block
after the loop instead of in the switch.

## Write `addPrim` OT as `mask + (s32)Gpu_CurrentOt`, no `&Global` hoist

`addPrim` is a macro that evaluates the OT address twice. A sibling
`Gpu_CurrentOt + (z >> 4)` becomes a scaled Z when
`Display_State.field_128` is the shift:

```
lbu   v0, 0x128(ds)
sllv  v0, z, v0
srl   v0, v0, 2
andi  v0, v0, 0xFFC
addu  v0, v0, ot
```

Hoisting `ds = &Display_State` pulls the address into the
`setSemiTrans` window, so `0xFF000000` lands in `$t0` instead of `$a3`
and `Gpu_CurrentOt` steals `$a1`. Write `Display_State.field_128`
directly in the `addPrim` argument. Operand order matters too:
`(s32)Gpu_CurrentOt + mask` is `addu v0, ot, v0`; the target is
`addu v0, v0, ot`.

```c
addPrim((u_long*)(((((u32)arg2 << Display_State.field_128) >> 2) & 0xFFC)
                  + (s32)Gpu_CurrentOt), p);
```

`func_800EC914` is the example. The hoisted `ds` form stuck at 87%
with only those registers and the `addu` operands swapped.

## Overlay `save + i*stride` so two `lbu`s share `$v0` and `$v1`

`save->arr[i*3] > save->arr[i*3 + 1]` computes the address twice.
GCC copies it (`move v1, v0`), loads the first byte into `$a0`
(coalesced with the `sltu` dest), and the second from the copy:

```
addu  v0, idx3, save
move  v1, v0
lbu   a0, off(v0)
lbu   v0, off+1(v1)
sltu  a0, v0, a0
```

The target keeps one base in `$v0`, first load in `$v1`, second
clobbering the base (`lbu v0, off+1(v0)`), then `sltu a0, v0, v1`.

Form a `McSaveData*` overlay at `&save->byte0[i * 3]` and read
`p->arr[0]` / `p->arr[1]`. The shared pointer pins the base, so the
first load cannot take `$v0` and lands in `$v1`:

```c
p    = (McSaveData*)&save->unknown_0[idx * 3];
slot = p->unknown_850[0] > p->unknown_850[1];
```

Later accesses must rematerialize through `save` (`save->arr[slot +
idx * 3]`), not `p`. Reusing `p` folds the address into one `addu`
and shuffles `save` / `idx*3` out of `$t0` / `$a2`.

`func_800CC41C` is the example. The two-index form stuck at 97.8%
with only that extra `move` and the first load in `$a0`.

## Pin the wrap dest so `temp = delta` is `move v1, s0`

`temp = delta; if ((s16)delta >= 0x802) temp = delta - 0x1000` coalesces
`temp` into `$s0` (`addiu s0, s0, -0x1000`) and the next `(s16)` hoist
steals the `bnez` delay slot (`sll v0, s0, 16`). The target copies first:

```
bnez  v0, skip
move  v1, s0
addiu v1, s0, -0x1000
```

Pin `temp` to `$v1`. Then write the later `(s16)temp` as two shifts onto
a *new* local (`val = temp << 16; val = val >> 16`) so the `sll` dest is
`$v0` and fills the second wrap's `beqz` delay. `(s16)temp` with the pin
emits `sll v1, v1, 16; sra v0, v1, 16` instead.

```c
register s32 temp asm("v1");
s32          val;

temp = delta;
if ((s16)delta >= 0x802) {
    temp = delta - 0x1000;
}
if ((s16)temp < -0x800) {
    temp += 0x1000;
}
val = temp << 16;
val = val >> 16;
```

`func_80103B88` is the example. Unpinned `temp` stuck at 96% with only
those wrap registers different.

## Load `field` into a temp before independent `param[i] = 0` stores

`param1[0] = a + b; param2[1] = 0; param2[3] = 0; param2[2] = 0;
param2[0] = session->field` lets the three zero stores float into the
gap after the `a`/`b` loads and before `addu`. The target instead does
the add, then `lbu` the field, then the zeros, then the field store in
the `jal` delay slot:

```
addu  v0, v0, v1
sb    v0, 0x10(sp)
lbu   v0, off(a0)
li    a0, cmd
sb    zero, 0x19(sp)
sb    zero, 0x1b(sp)
sb    zero, 0x1a(sp)
jal   CdCmd_Enqueue
sb    v0, 0x18(sp)
```

Assign the field to a local *after* the add and *before* the zeros.
The load becomes its own statement, so it cannot stay glued to the
final store, and the zeros can no longer slip in front of the add:

```c
param1[0] = D_80114DF0 + D_80114DF1;
stage     = Game_Session->field_7;
param2[1] = 0;
param2[3] = 0;
param2[2] = 0;
param2[0] = stage;
CdCmd_Enqueue(0x21, param1, param2);
```

`func_800D131C` is the example. The fused
`param2[0] = Game_Session->field_7` stuck at 97.4% with only those
three `sb zero` moved before the `addu`.

## Accumulate the byte offset in `$v1` so `lhu` is `0(v1)`

`return table[row].field[col]` (or `*(u16*)((u8*)table + off)`)
computes the row/column term in `$v0` and folds the base onto it
(`addu v0, t0, v0; lhu v0, 0(v0)`). The target instead does

```
sll   v1, a3, 1          /* col * 2 */
sll   v0, a0, 1
addu  v0, v0, a0
addu  v0, v0, a1
sll   v0, v0, 4          /* (row * 3 + rank) * 16 */
addu  v1, v1, v0
addu  v1, t0, v1
lhu   v0, 0(v1)
```

Pin the running offset in `$v1`, add the scaled index onto it, then
add the base as `off = (s32)table + off` so the sum stays in `$v1`:

```c
register s32 off asm("v1");

off = arg0 * 2;
off += (idx * 3 + ret) * 16;
off  = (s32)recs + off;
return *(u16*)off;
```

`return recs[idx * 3 + ret].field[arg0]` stuck at 99.4% with only those
last three instructions using `$v0` instead of `$v1`. `func_800A1558`
is the example.

## Assign `i = ret` before `if (ret < n)` so it fills the `beqz` delay

A loop that starts at the same 0 already sitting in `$s4` (`ret`) wants

```
slt   v0, s4, count
beqz  v0, skip
 move  s2, s4        /* i = ret */
lui   s6, %hi(dest)
sll   v0, start, 2
```

`if (ret < n) { i = ret; ... }` rematerializes `i = 0` and lets the
`start << 2` steal the delay slot (`sll v0` / `nop`). Assign `i` *before*
the compare so the delay-slot filler copies `$s4`:

```c
asm volatile("" : "+r"(ret)); /* keep ret from folding to 0 */
i = ret;
if (ret < src->field_1) {
    destHi = 0x80110000; /* %hi of dest; must precede rec = table + start */
    off    = start << 2;
    rec    = (GpItemRec*)(off + (s32)table);
```

`&global` as the later call argument hoists `lui %hi` *after* the rec
address calc (and often reuses the table's `$s5`). Materializing the hi
as a constant first pins `$s6` and keeps `addiu a1, s6, %lo` in the jal
delay. `func_800BC50C` is the example.

## Extra store before the shared assignment so `sh` merges

Two pad paths both write `obj->field_2E`, and the cancel arm also writes
`obj->status`. `field_2E = -1` *before* `status = 0` emits the `sh` in
the else and jumps past the shared store (`j` delay = `sw status`).
Write `status = 0` first so both arms end on the `field_2E` assignment;
GCC merges them with the later `field_2E = 6` into one `sh v0, 0x2E`
(`j` delay = `sw status`).

```c
if (obj->owner->flags != 0) {
    obj->field_2E = 6;
} else {
    obj->status   = 0;
    obj->field_2E = -1;
}
```

`func_800BDDC4` is the example. The swapped store order stuck at 95.7%
with a separate `sh` on the -1 path.

## Switch case order: fall-through-to-epilogue last

A switch whose last live case is a `jal` (then the function epilogue)
must be emitted last. Putting a later simple-store case after it inserts
`j end; nop` after the call. Source order `-1`, `9`, `6` (store, store,
teardown+call) matches the target body order.

`func_800BDDC4` is the example. Case order `-1`, `6`, `9` stuck at 95.7%
with the case-9 `sh` after the teardown `jal`.

## Start the dest-arg local at `extra` and reload between the two loads

`extra = slot->extra; raw = extra->field_8; f(..., &((T*)raw)[i], ...)`
puts extra in `$v1` and `field_8` in `$a1`:

```
lw    v1,0x2C(s3)
move  a2,zero
lw    a1,8(v1)
```

The target loads extra straight into the call dest, fills that delay
slot with an independent `lhu`, then overwrites the same register:

```
move  a2,zero
lw    a1,0x2C(s3)
lhu   v1,0x2A(s0)
lw    a1,8(a1)
```

Assign `slot->extra` to the pointer that becomes the call argument,
reload a live field (here `killCountdown`) between the two assigns,
then overwrite that same local with `field_8`:

```c
coords = (GsCOORDINATE2*)slot->extra;
count  = arg0->killCountdown;
coords = (GsCOORDINATE2*)((GameActorExt*)coords)->field_8;
coords = &coords[index + 1];
func_800FDB18(3, coords, 0, params);
```

`func_8010B3F8` is the example. A separate `extra`/`raw` pair stuck at
97.4% with only those three loads (and the `field_6` store) reordered.

## Assign loop-invariant constants before the hoisted global load

A loop that compares against `-2` / `-3` / `-1` and also reads a global
table hoists all four. Source assignment order is emit order. Writing
the table first (`table = D_8011567C; do { if (code == -2) ...`) puts
`lui/lw` before the `li t5,-2` / `li t4,-3`. Assign the constants first:

```c
if (shifted >> 16 != -1) {
    newline = -2;
    skip    = -3;
    table   = D_8011567C;
    do {
        if (shifted >> 16 == newline) {
            ...
        } else if (shifted >> 16 != skip) {
            ...
        }
    } while (shifted >> 16 != -1);
}
```

Target wants `li -2; li -3; lui/lw table; li -1`. `func_800E69F4` is the
example; 97.8% with only those four setup insns swapped.

## Leaf scratch alloc: `tmp` then `s = tmp` for `move a0, v0`

A no-call `G_SCRATCH_HEAD` helper that keeps the block pointer in `$a0`
emits:

```
lui   a0, 0x1F80
ori   a0, a0, 0x3FC
…                 /* color/const hoists into $a1/$a2/$a3/$t0 */
lw    v1, 0(a0)
addiu v0, v1, -N
sw    v0, 0(a0)
move  a0, v0
```

`s = (T*)(head - N); *scratch = s;` computes `s` straight into `$a1` (or
`$a0` if pinned) and skips the move. A short-lived `tmp` plus a copy
lets the add live in `$v0` while `$a0` is still the arena pointer:

```c
scratch  = (void**)G_SCRATCH_HEAD;
color    = 0x808008;          /* named local → $a1 */
head     = *scratch;
tmp      = (T*)(head - 0x10);
*scratch = tmp;
s        = tmp;               /* move a0, v0 */
```

Free by rematerialising `*(void**)G_SCRATCH_HEAD` — `$a0` now holds `s`,
so a saved `scratch` local would need another register. `func_8010133C`
is the example. Direct `s = head - N` stuck at 95.8% with `$a0`/`$a1`
swapped and no `move`.

## Keep the `$v0` tmp live so a dead `$a0` does not eat `addiu` + `move`

The sibling of the leaf-scratch copy above, when `$a0` is a just-consumed
`s16` argument rather than the arena pointer. `tmp = head - N; s = tmp`
is then peepholed to `addiu a0, a2, -N` because `$a0` is free after
`subu a1, a1, a0`. The target still wants

```
addiu v0, a2, -N
move  a0, v0
sw    …, -N(a2)
```

Pin `tmp` to `$v0` and force a read of that first value after the copy
so it is not forwarded away (and so the peephole cannot fold the
`addiu` into `$a0`). `+r`(s) keeps the `move` before the first field
store:

```c
register s32 tmp asm("v0");

tmp   = (s32)(head - 0xC);
block = (T*)tmp;
__asm__ volatile("" : "+r"(block) : "r"(tmp));
((T*)(head - 0xC))->field_0 = delta;
```

Because `$v0` is reserved, rematerialise the free through `tmp` as well
(`tmp = (s32)G_SCRATCH_HEAD; *(void**)tmp = (u8*)*(void**)tmp + 0xC`).
The idiomatic `*(void**)G_SCRATCH_HEAD = …` steals `$a0` for the `lui`
and breaks the `lhu a0` return. `func_80103E7C` is the example.
Without the `"r"(tmp)` read the first `tmp = head - N` was deleted
(98.1%, lone `addiu a0`).

## Store a field on the first-arg object before the call

`f(obj, ...); obj->field = 0;` cannot put the store in the `jal` delay
slot: the callee may alias `obj`, so GCC fills the slot with `move a0`
and stores after the return. The target wants:

```
move  a0,s0
jal   f
sh    zero,off(s0)
```

Write the store first. The store is allowed in the delay slot because
the callee still sees the new value:

```c
obj->field_2E = 0;
Ui_DrawText((UiPanel*)obj, text);
```

`func_800C5A5C` is the example. The store-after-call form stuck at
99.5% with only those two instructions swapped.

## Dead `0xFF` in `$s4` plus `v0` scratch temp, store dest not temp

A scratch-RECT `LoadImage` walk that the target opens with

```
move  s3, zero
lui   v1, 0x1F80
ori   v1, v1, 0x3FC
sw    s4, 0x20(sp)
li    s4, 0xFF
...
lw    v0, 0(v1)
addiu s0, s2, 0xC
addiu v0, v0, -8
move  s1, v0
sw    s1, 0(v1)
```

needs three things together. `done = 0` first so `$s3` is zeroed
before the scratch address. Then take `G_SCRATCH_HEAD` *before*
assigning the unused `0xFF`, or `li s4` lands above the `lui`/`ori`.
The `0xFF` is never read; pin it to `$s4` and mention it after the
free so the save/restore stays:

```c
register void* temp asm("v0");
register s32   max asm("s4");

done    = 0;
scratch = (void**)G_SCRATCH_HEAD;
max     = 0xFF;
head    = *scratch;
temp    = (u8*)head - 8;
dest    = temp;
*scratch = dest; /* sw s1, not sw v0 */
...
*(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
asm("" :: "r"(max));
```

`temp = head - 8; dest = temp` keeps the subtract in `$v0` and the
`move` into the dest s-reg. `*scratch = dest` (not `temp`) is the
`sw s1`. Walk with `if (rec->field_0 == 0) { copy; LoadImage; } else
{ done = 1; } rec++;` so work is the `bnez` fall-through. `func_800DB31C`
is the example. Assigning `max` before the scratch address stuck at
97.9% with only those two instruction pairs swapped.

## Pin `div` in `$v0` so `sltiu` cannot hoist into `multu`

`n % 100U / 10U` then `if (n >= K)` are independent, so `-O2` fills the
`multu` latency with the compare (`sltiu` / `bnez` / `srl` in the delay
slot). The target finishes the divide first:

```
multu  v0, v1
mfhi   t0
srl    v0, t0, 3
andi   v1, v0, 0xFFFF
sltiu  v0, a0, K
bnez   v0, lt
li     v0, 3          /* delay: else-arm compare */
```

`$v0` holds the quotient until the `u16` truncate, so `sltiu` must wait.
Without that pin, `sltiu` takes `$v0` and the quotient lands in `$v1`.

Assign the divide to `register s32 temp asm("v0")`, then truncate:

```c
register s32 temp asm("v0");
u16          tens;

temp = (id % 100U) / 10U;
tens = temp;
if (id >= 0x259U) {
    if (tens == 1) {
        p->field = 0;
    } else {
        p->field = 1;
    }
} else if (tens == 3) {
    p->field = 2;
} else if (id < 0x12CU) {
    p->field = 1;
} else {
    p->field = 0;
}
```

`if (tens == 1)` (zero arm first) is required so the later `< 0x12C`
case emits `bnez` back to the shared `sh v0`. `if (tens != 1)` first
stuck at 93% with an extra `j` / `sh v0` and `beqz` to the zero store.

`func_80109290` is the example. A plain `u16 tens = (id % 100U) / 10U`
stuck at 85.7% with only that schedule and the last branch inverted.

## Overwrite `shifted` so the signed code lives in `$a1`

A `lhu` text-code loop that compares as `s16` (`sll 16; sra 16`) keeps
`$a1` as the unshifted value if the body uses `shifted >> 16` each
time (or a separate `val`). The target does one `sra a1, a1, 16`
before the loop and again in the back-edge delay slot, then compares
`$a1` against `-2` / `-3` / `bltz`.

Overwrite the same local at the top of the loop. The latch reconstructs
`shifted = code << 16` and the delay slot signs it again:

```c
shifted = code << 16;
if (shifted >> 16 != -1) {
    do {
        shifted = shifted >> 16; /* sra a1, a1, 16 */
        if (shifted == -2) {
            /* ... */
        }
        code    = arg0[(s16)i];
        shifted = code << 16;
    } while (shifted >> 16 != -1);
}
```

`func_800E67C8` is the example. A separate `val = shifted >> 16` used
for both the empty check and the body stuck at ~75% with `lh` + `lhu`.

## Label increment inside `if (x < 0)` so `== K` is not inverted

A chain of `if (code == C) { x += K; goto inc; }` plus a glyph path
that skips increment makes `== 0x8400` profitable to invert (`beq` to
a sunk `addiu` just before `inc`). The target keeps

```
ori   v0, 0x8400
bne   v1, v0, not_eq
nop
j     inc
addiu t0, t0, 0x10
```

Put the shared increment *inside* `if (shifted < 0)` and `goto` that
label from the special cases. The `== K` arm is then a jump into a
block that is not a merge-with-fall-through tail, so GCC emits `bne` /
`j` / delay-slot `addiu` instead of `beq` + sink:

```c
if (masked == 0x8400) {
    lineW += 0x10;
    goto do_inc;
}
if (shifted >= 0) {
    /* increment + glyph; skip shared inc */
    goto after_load;
}
if (shifted < 0) {
do_inc:
    /* increment + load next */
}
after_load:
    /* loop latch */
```

`func_800E67C8` is the example. The same gotos with `inc` after the
`if/else` stuck at 91% with only that `beq` / sunk `addiu`.

## Compare temp in `$v1` before `li v0, -1`

`shifted = code << 16; v0tmp = -1; while (shifted >> 16 != v0tmp)`
emits `sll a1; li v0, -1; sra v1; bne`. The target signs first:

```
sll   a1, a2, 16
sra   v1, a1, 16
li    v0, -1
bne   v1, v0, loop
sra   a1, a1, 16
```

Assign the compare to a `$v1` temp *before* reloading `-1`. The latch
still delay-slots `shifted >>= 16` into `$a1`:

```c
register s32 width asm("v1");
register s32 v0tmp asm("v0");

shifted = code << 16;
width   = shifted >> 16;
v0tmp   = -1;
} while (width != v0tmp);
```

A literal `!= -1` hoists `-1` into `$t3`. `func_800E67C8` is the
example. `v0tmp = -1` before the `sra` stuck at 98.8% with only those
two instructions swapped.

## Load the later `$s0` byte before the nearby call

`SndEvt(...); val = *ptr;` keeps `val` in `$a1` (not live across the
call) and parks `arg0`/`arg1` in `$s0`/`$s1`. The target loads first
so the byte is live across `SndEvt` and lands in `$s0`, pushing the
incoming args to `$s2`/`$s1`. GCC also sinks the `lbu` into the call
setup:

```
move   a1,zero
lui    v0,%hi(ptr)
lw     v0,%lo(ptr)(v0)
nop
lbu    s0,0(v0)
jal    SndEvt
move   a2,a1
```

Assign the load first:

```c
val = *D_80114DD4;
SndEvt_EnqueueType6(3, 0, 0);
```

`func_800CAB40` is the example. The post-call load stuck at 68.6%
(wrong saved-reg assignment and the `jal` before the `lbu`).

## Use a fresh temp for the last `base + saved` add

Reusing the first `y = baseY - N; req.y = y + yOff` locals for a
second draw request lets `yOff` die at that add. GCC then coalesces
the sum into the saved register:

```
addu   s2,v0,s2
sh     s2,off(sp)
```

The target keeps the result in `$v0` (`addu v0,v0,s2; sh v0`) because
`yOff` is only a source. A distinct local for the second request does
that:

```c
y  = obj->baseY - 3;
req.y  = y + yOff;
/* ... */
y2 = obj->baseY - 6;
req2.y = y2 + yOff;
```

`func_800CCA48` is the example. Reusing `y` stuck at 99.917%.

## Copy the incoming arg first, pin the hoisted constant to `$s2`

A prologue that needs `$s1` = incoming pointer, `$s2` = hoisted constant,
`$s3` = `&global` (with `lui v1; addiu s3, v1` after a still-live load in
`$v0`) is very sensitive to allocation order.

Assign the incoming pointer to a local first so it takes `$s1`. If that
local is not read until after the first branch, GCC still delays
`move s1, a0` into the branch delay slot:

```c
s16*         dest;
register s32 three asm("s2");
WipSysConfig* cfg;

dest  = arg0;
cfg   = &Wip_SysConfig;
mode  = Game_Session->field_128;
three = 3;
if (mode == three) {
    return;
}
```

Pin the constant to `$s2`. Without the pin it steals `$s1` and the incoming
arg lands in `$s3` (99.5%, only those two swapped). Pinning `&global` to
`$s3` instead emits `lui s3; addiu s3, s3` and copies `arg0` eagerly
(93.7%). Leave the global pointer unpinned so the address is computed
through `$v1` after `Game_Session` occupies `$v0`.

`func_800A7320` is the example.

## Wrap-around skip is `do { idx++; wrap; } while (empty && !flag)`

A circular walk that steps once, wraps, then skips empty table slots
needs increment-first do-while. `arg0++; while (1) { wrap; if (ok)
break; arg0++; }` dropped the continue increment (infinite loop in the
object) and left nops in the `blez` / `beqz` delay slots. `for (arg0++;
; arg0++)` did the same.

The target speculatively decrements in the `blez` delay slot, undoes it,
then increments, and puts `addiu ±1` / undo on the inner `beqz`:

```
blez  a1, back
addiu a0,a0,-1
addiu a0,a0,1
addiu a0,a0,1
...
beqz  v0, loop
addiu a0,a0,1
addiu a0,a0,-1
```

Increment, then wrap, then the combined empty test:

```c
do {
    arg0++;
    if (arg0 >= 0xC) {
        arg0 = 0;
    }
} while (table[arg0] == 0 && save->field_5C2 == 0);
```

`func_800A7BBC` is the example. The increment-outside / `while (1)`
shape stuck at 83.3%.

## Reuse the `$v1` temp for `(lo - abs)` so the LCG load can overwrite it

A packed-arg fade/shake that does `abs`, `lo - abs`, `packed >> 8`,
then `D_80070F60 = D_80070F60 * 5 + 0x71357911` wants

```
subu  v1, a2, v0
sra   v0, a0, 8
mult  v1, v0
lui   a0, 0x7135
lw    v1, rng        /* overwrites the subtract */
...
mflo  t0
addu  v0, v0, a0
srl   v1, v0, 16
mult  t0, v1
```

A fresh `scaled = lo - tmp` takes `$a1`/`$t0` and pushes the rng load
into `$v0`. Assign the subtract into the same `$v1` local that later
holds `(u32)D_80070F60 >> 16`:

```c
register s32 tmp asm("v0");
register s32 hi asm("v1");

tmp    = ABS(arg0->spawnArg1);
hi     = lo - tmp;
tmp    = packed >> 8; /* sra v0, a0, 8 — not in-place on packed */
scaled = hi * tmp;
D_80070F60 = D_80070F60 * 5 + 0x71357911;
hi     = (u32)D_80070F60 >> 16;
hi     = scaled * hi;
```

`tmp = packed >> 8` is required: `(lo - tmp) * (packed >> 8)` hoists
`sra a0, a0, 8` into the `lo < spawnArg1` delay slot (killing the
`lui %hi(D_80070F60)` hoist) and emits `subu v0` / `mult v0, a0`.

Take `val = hi >> 16` before the sign flip. Passing pinned `hi` to
`Display_ClampField126` (s8) becomes `sll v1, v1, 24; sra a0, v1, 24`.
The unpinned dest keeps `sll a0, v1, 24`. Odd path is `val = ABS(val)`;
even is `tmp = ABS(val); val = -tmp` so even stays copy-abs
(`move v0, v1; negu v0; negu v1, v0`).

`func_800E8938` is the example. A one-liner multiply stuck at 95% with
only that hoist and `mult` operand order different. Needs `--expand-div`
(scratch `build.sh` and the TU).

## Pin the hot object to `$s1` so a setup pointer can keep `$s0`

GCC 2.8.1 gives lower saved registers to higher spill-cost locals. A
`UiObject*` used throughout a handler outranks a `UiList*` that is only
read once, so the object takes `$s0` even if the list is assigned first.
The target instead keeps the list in `$s0` (later reused as the flags
word) and the object in `$s1`.

Pin the object, not the list. A `register UiObject* obj asm("s1")` is a
load (`lw s1, 0x20(s3)`), so it does not change the `lui` temp. Pinning
the list address instead emits `lui s0, %hi(list)` instead of the
target's `lui v0; addiu s0, v0, %lo(list)`. A second pin
`register s32 val asm("s2")` then leaves `&Wip_SysConfig` in `$s4`:

```c
UiList*            menu;
register UiObject* obj asm("s1");
register s32       val asm("s2");
WipSysConfig*      cfg;

menu = &D_8010E9A4;
obj  = arg0->spawnArg2;
cfg  = &Wip_SysConfig;
```

`func_800C8E10` is the example. Unconstrained allocation stuck at 93%
with `$s0`/`$s1` swapped; pinning the list address stuck at 97.4% with
only the prologue `lui` temps different.

## Pin dest + `$v0` shift temp for `x = arg - ((s16)arg >> 2)`

`amount = arg0; if (c) amount = arg0 - ((s16)arg0 >> 2)` copy-propagates.
GCC keeps one saved register, emits `sll s1,s0,16` / `sra s1,s1,18` /
`subu s1,s0,s1`, and cannot fill the `beqz` delay slot (`nop`).

The target uses `$s0` as the still-live original, `$v0` as the shift
temp, and `$s1` as the dest:

```
beqz  v0, skip
 sll   v0, s0, 16
sra   v0, v0, 18
subu  s1, s0, v0
```

Pin the dest so it cannot coalesce with `arg0`, and pin an explicit
shift temp to `$v0`:

```c
register s32 amount asm("s1");
register s32 tmp asm("v0");

amount = arg0;
if (func(0x40000) != 0) {
    tmp    = (s16)arg0 >> 2;
    amount = arg0 - tmp;
}
```

`func_8010A854` is the example. Unpinned dest stuck at 85% (one saved
reg, later `$s0`/`$s1`/`$s2` all shifted). Dest pin alone stuck at
98.7% with only that `nop` + `sll s1`.

## Duplicate TextDrawReq setup + `func_8002E53C` so GCC tail-merges one `jal`

When the target tests a flag first, then fills the same `TextDrawReq`
in **both** arms (x/y, `lh` drawOrder, string `lui`/`addiu` in `$a1`,
`addiu a0, sp, req`) and only then joins for `otIndex + 1` / `field_8`
/ the three `sb`s / one `jal func_8002E53C`, a shared-setup form
(`text = cond ? A : B;` then one fill + one call) CSEs the x/y stores
and emits a single fill after the string select (~75%).

Write the **entire** fill and the call in both arms:

```c
if (flags & 3) {
    req.x = ...;
    req.y = ...;
    req.otIndex = (s16)obj->drawOrder + 1;
    req.field_8 = prompt->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E = 1;
    func_8002E53C(&req, D_8010F1C4);
} else {
    req.x = ...;
    req.y = ...;
    req.otIndex = (s16)obj->drawOrder + 1;
    req.field_8 = prompt->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E = 1;
    func_8002E53C(&req, D_8010F1BC);
}
```

GCC tail-merges the common stores + `jal` into one call. Filling once
and then calling in both arms still CSEs the fill. `func_800D2224` is
the example.

## `SVECTOR` fill before `RotMatrixZYX`: write `vx`, `vy`, then `vz = 0`

Building `(-ang->vx, -ang->vy, 0)` and writing `vz` between the two
negated fields hoists `sh zero` into the first field's load-delay
(before `negu` of `vx`). The target stores `vz` after the second
field's `lhu` and the `move a1, mtx`:

```
lhu   v0, 0(ang)
addiu a0, sp, in
negu  v0, v0
sh    v0, vx(sp)
lhu   v0, 2(ang)
move  a1, mtx
sh    zero, vz(sp)
negu  v0, v0
jal   RotMatrixZYX
 sh   v0, vy(sp)
```

Assign in that store order so the independent zero cannot float earlier:

```c
in.vx = -arg1->vx;
in.vy = -arg1->vy;
in.vz = 0;
RotMatrixZYX(&in, &mtx);
```

Declare the two `SVECTOR`s before the local `MATRIX` so they sit at
`sp+0x10` / `sp+0x18` and `&in` rematerializes (`addiu rx, sp, 0x10`)
instead of taking a fifth saved register. `func_800B0FDC` is the
example. `vx, vz, vy` stuck at 99.3% with only that `sh zero` early.

## `s16 / 12` clamp: `u16` divide, signed compare, copy back so `$a0` is the call arg

An `s16` field loaded with `lhu`, divided by 12 (`multu` `0xAAAAAAAB` /
`srl 3`), then clamped to `{0,1,2}` for `func_800FDB18(3, ...)` needs
three pieces:

```
andi  v1, v0, 0xffff
slti  v0, v1, 3
beqz  v0, skip
 li   a0, 2
move  a0, v1
move  v1, a0
li    a0, 3
sll   v0, v1, 1
```

`(u16)field / 12` into a `u16` emits `sltiu`. Assign the divide to `s32`
through a `u16` cast so the mask stays and the compare is signed
`slti`. Then default-assign `2`, overwrite if `< 3`, and copy back into
the divide temp before the `* 0x60` stores:

```c
temp = (u16)((u16)inner->field_96E / 12);
idx  = 2;
if (temp < 3) {
    idx = temp;
}
temp            = idx;
params->field_4 = (temp * 0x60) + 0xC0;
params->field_6 = temp + 1;
func_800FDB18(3, coords, 0, params);
```

The copy-back is what yields `move v1, a0` / `li a0, 3` so the clamped
value lives in `$v1` while `$a0` is the call's first argument. Clamping
in place (`if (idx >= 3) idx = 2`) or using the clamp result directly
puts the value in `$a2` (~96–98%). `func_80109A1C` is the example.

## Name the signed `s16` compare so `lh` takes `$a0` and the `lhu` copy takes `$a1`

Comparing an `s16` field and then assigning that same field to another
`s16` emits `lh` (compare) plus `lhu` (halfword copy). Inlining both
uses swaps the registers:

```
lh    a1, 0x2e(v0)
lhu   a0, 0x2e(v0)
beq   a1, v1, store
```

The target wants the signed value in `$a0` (the now-dead firstChild /
`a0` temp) and the unsigned copy in `$a1`:

```
lh    a0, 0x2e(v0)
lhu   a1, 0x2e(v0)
beq   a0, v1, store
```

Assign the compare to an `s32` local, then reload for the store:

```c
flag = childObj->field_2E;
if ((flag == -1) || (flag == 6)) {
    obj->field_2E = childObj->field_2E;
}
```

`func_800D3FF0` is the example. The inlined compare stuck at 99.7%
with only those two registers swapped.

## `gte_rtps` is `0x4A180001`, not the DMPSX `.word 0x0000007f`

`gte_rtps()` in `inline_c.h` emits two nops plus the DMPSX placeholder
`.word 0x0000007f`. maspsx does not expand that token, so the object
gets `0x0000007f` instead of RTPS.

Use the real encoding, same pattern as `gte_rtv0_real`:

```c
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
```

`func_800DB004` is the example (scratch `SVECTOR` + `gte_SetRotMatrix` /
`gte_SetTransMatrix` / `gte_ldv0` / RTPS / `gte_stsxy` / `gte_stdp` /
`gte_stflg` / `gte_stszotz`).

## Add a table-base constant to the pointer, then add the scaled index

`(s32*)((arg0 << 2) + ((s32)slots + 0x4C0))` still folds as
`sll a0, a0, 2` / `addiu a0, a0, 0x4C0` / `addu s1, a0, a1`. The target
adds the constant to the live table pointer:

```
sll    a0,a0,0x2
addiu  v0,a1,0x4C0
addu   s1,a0,v0
```

Name both addends so the `+ 0x4C0` cannot sink into the shifted index:

```c
off4    = arg0 << 2;
counts  = (s32*)((s32)slots + 0x4C0);
counter = (s32*)(off4 + (s32)counts);
```

Shift `arg0 << 3` *before* mentioning `Mc_SaveData` so that shift takes
`$v1` and the `lui` of the table can follow it. `func_800B6DA4` is the
example.

## Take `&Mc_SaveData` only on the later path so it is not CSEd with `+0x1C8`

Two consume paths both touch `Mc_SaveData.field_5C2` / `field_5BC`. If
both write `Mc_SaveData.field_*` directly, GCC keeps `field_1C8` in
`$a1` and uses `lb 0x3FA(a1)` / `addiu a0, a1, 0x3F4` for both. The
later path reloads the object:

```
lui    v0,%hi(Mc_SaveData)
addiu  a0,v0,%lo(Mc_SaveData)
lb     v0,0x5C2(a0)
addiu  a0,a0,0x5BC
```

Assign `save = &Mc_SaveData` only in that path and use `save->field_5C2`
/ `&save->field_5BC`. Leave the first path as `Mc_SaveData.field_*` so
it still CSEs with the live `field_1C8` pointer. `func_800B6DA4` is the
example.

## `s16` layout field wins `$s0`; assign mask / lineY before the first call

A reloaded `UiObject` layout halfword that must live in `$s0` (so later
`y + 0x1A` is `addiu v0, s0, 0x1A`, not an in-place `addiu s0`) needs
to be `s16`, not `s32`. As `s32` it loses `$s0`/`$s1` to longer-lived
locals (`mask`, `lineY`) and the incoming `UiObject*` takes `$s2`
instead of `$s3`.

Assign `mask = arg1 & 3` and `lineY = y + 0xF` *before* the first call
so they take saved registers. GCC sinks `andi s1, s6, 3` into that
jal's delay slot and recomputes `lineY` after the `field_18` reload:

```c
s16 y;
s32 mask;
s32 lineY;

y     = arg0->field_18;
mask  = arg1 & 3;
lineY = y + 0xF;
text  = func_800B8EB0(arg1, 1, 1);
```

`req.y = (s16)(arg0->baseY - 6) + lineY` is `addiu v0, v0, -6` /
`addu v0, v0, s2`. Without the `s16` cast, `u16 baseY - 6` becomes
`li v1, 0xFFFA` / `addu`. `func_800D2E04` is the example.

## Relocate file offsets by adding the pointer itself, not a `base` local

A CAP-style relocator that does `off += (s32)file` must add the live
`file` pointer (`$s0` after `strncmp`). A separate `s32 base = (s32)file`
takes `$a3` and shifts the loop index / count / terminator out of
`$a0` / `$a1` / `$a2`:

```c
/* BAD — extra `move a3, s0`, index in $a1 */
base = (s32)file;
file->field_8 += base;

/* GOOD — `addu v1, v1, s0` */
file->field_8 += (s32)file;
```

Zero the loop index before `if (file->field_8 > 0)` so it sinks into
that `blez` delay slot as `move a0, zero`. A terminator compare of
`-1` should be a hoisted local (`flag = -1`) so it lives in `$a2`.
`func_800E40EC` is the example.

## Index a global table inside the `jal` so fail paths preload `$a0`

A cascade of `table[GetNibble(id)].field` lookups that share one tail
call must keep the outgoing flag in `$a0` (the same register as the
nibble ids). Two things fight that:

1. `table = D_xxx; bytes = table[GetNibble(id)].field_0;` assigns the
   table before the call, so the fail-path delay slot becomes
   `lui next_table` instead of `li a0, next_id`.
2. Assigning `flag = 0` before a later `GetNibble` forces `flag` into a
   saved register (the call clobbers `$a0`).

Index the global directly and only write `flag` on the success goto /
shared zero tail:

```c
bytes = D_80114198[GameFlag_GetNibble(0x4B)].field_0;
if (bytes != NULL) {
    if (D_80114198[GameFlag_GetNibble(0x4B)].field_4 == stage) {
        if (bytes[save->field_6 - 1] != 0) {
            flag = 1;
            goto done;
        }
    }
}
/* ...next table... */
flag = 0;
done:
Snd_SetModeFlag(flag);
```

The table address then lands in the `jal` delay slot (`addiu s1, %lo`)
and each fail branch preloads the next nibble id. Widen the cached
compare byte to `s32` (`stage = save->field_7`) so `lbu field_4` /
`bne v0, s2` does not emit `andi s2, 0xFF`. First `field_6` through
the `save` pointer, later ones as `Mc_SaveData.field_6` so they
rematerialize as `lui` / `lbu %lo`. `func_800ABCC8` is the example.

## Keep a live `-1` so `count + prev` is `addu`, not `addiu -1`

A vertex-cache loop that seeds `prev = -1` (first-arg `$a0` after
`ws = arg0`) and later compares the stream halfword against it also
wants that same register as the decrement:

```
bnez  v1, cont
 li    a0, -1
jr    ra
 move  v0, a2
addu  v0, v1, a0
blez  v1, end
 sw    v0, 0x1C(a1)
```

`field_1C = count - 1` or `field_1C--` becomes `addiu v0, v1, -1`.
`field_1C = count + prev` constant-folds the same way because GCC
still knows `prev` is `-1` at that point.

Hide the value after the early-out so the add must use the register,
then compare `u32` halfwords against that live `prev` (no extra
`andi 0xFFFF`):

```c
prev  = -1;
count = ws->field_1C;
if (count == 0) {
    return stream;
}
__asm__ volatile("" : "+r"(prev));
ws->field_1C = count + prev;
```

Reload the halfword as `*(u16*)stream` for the depth-table store so it
takes `$v0` and `>> 3` is `srl`. Reusing the first `idx` local leaves
it in `$v1` and emits `sra`. `func_8009EAA4` is the example.

## Assign an `s16` to `s32` before `& mask` so the load is `lh`

`dst = spawn->field_2 & 0xF` (field is `s16`) emits `lhu` because the
mask makes the sign bits dead. The target wants:

```
lh    v0, 0x36(s2)
nop
andi  v0, v0, 0xF
sh    v0, 0x2A(s0)
```

Widen to `s32` first so the load must sign-extend, then mask:

```c
s32 param;
param         = spawn->field_2;
mem->field_2A = param & 0xF;
```

An `s16` temporary still folds into `lhu`. Overlay `Task::spawnArg1` as
`u16` + `s16` rather than shifting the word (`lw` / `sra 16`).
`func_800F75BC` is the example.

## First overlay switch: own TU so its jtbl stays at the file-start hole

A 5-case switch in the first function of a gameplay TU emits a 0x14-byte
jtbl plus a 4-byte pad (`static const s32 s_jtbl_pad = 0`). That table
lives at the start of the TU's rodata (0x3FB8 here), *before* still-asm
jtbls. Putting the switch in the existing C file prepends the table onto
that file's later `.rodata` island and shifts everything after it.

Give the function its own `c` / `.rodata` pair in the overlay yaml
(`3FB8_75BC` at 0x3FB8 / 0x63DBC) and leave the remainder of the old
TU starting at the next function. Update
`fix_gameplay_linker_rodata_order` so splat cannot drop the new object
from `rodata_3FB8` / `3FB8.c` / `rodata_3FB8_2`. `func_800F75BC` is the
example.

## Capture the next color before `setlen`/`setcode` so they fill that delay

A POLY_GT4 fill that writes `r0`/`r1`/`r2` then `setlen`/`setcode` then
`r3` lets GCC slide the two `sb`s into `r2`'s load-delay:

```
lw    v0,0x10(a2)
sb    t1,-0xb(a1)
sb    t0,-0x7(a1)
sw    v0,0xe(a1)
lw    v0,0x14(a2)
nop
sw    v0,0x1a(a1)
```

The target finishes the `r2` store first (`lw`/`nop`/`sw`) and uses
`setlen`/`setcode` as the delay for the `r3` load. Pull that next word
into a temp before the byte stores:

```c
*(s32*)&poly->r2 = arg2[4];
color = arg2[5];
setlen(poly, 12);
setcode(poly, 0x3E);
*(s32*)&poly->r3 = color;
```

`func_8009F280` is the example. Writing `r3` from `arg2[5]` directly
stuck at 96.8%.

## Assign-in-`&&` so a later store keeps the compared byte

`if (p->flag == 0) { v = p->id; if (v == 1) { p->hp = v; } }` proves
`v == 1` and rematerializes the store as `li v0,1; sh v0`. The target
instead does

```
lb    v0, flag(a0)
nop
bnez  v0, skip
li    v0, 1
lb    a1, id(a0)
nop
bne   a1, v0, skip
...
sh    a1, hp(a0)
```

Assign the byte *inside* the `&&` so the load stays after the first
branch and `v` is not replaced by the constant:

```c
if (save->field_5C2 == 0 && (field13 = save->field_13) == 1) {
    save->field_6C8 -= actor->field_96E;
    if ((s16)save->field_6C8 <= 0 && Game_Session->field_1 != 0) {
        save->field_6C8 = field13; /* sh a1, not li 1 */
    }
}
```

Assigning `field13` before the `if` also avoids the fold but hoists
`lb id` above the `field_5C2` check. A nested `if` after an inner
assign folds. `func_8010B9A4` is the example.

## Scratch `head = head - K` then `+r`(vec) so VectorNormal takes `$s1`

A 0x10 scratch `VECTOR` whose first store is `sw v1, -K(head)` and whose
adjusted pointer later becomes both args of `VectorNormal(vec, vec)`
wants:

```
sw    v1,-0x10(v0)
addiu v0,v0,-0x10
move  s1,v0
lh    v0,2(s4)
lh    v1,2(s3)
move  a0,s1
```

Store through `(VECTOR*)(head - K)` then `vec = (VECTOR*)(head - K)`
computes `addiu s1, v0, -K` first. Reassigning `head = head - K;
vec = (VECTOR*)head` with `head` pinned to `$v0` emits the `addiu` /
`move`, but the upcoming `VectorNormal` first-arg copy steals it
(`move a0, v0` / later `move s1, a0`). Pin `vec` to `$s1` and lock the
copy before the next field store:

```c
register u8*     head asm("v0");
register VECTOR* vec asm("s1");

head = *scratch;
((VECTOR*)(head - 0x10))->vx = arg1->vx - arg0->vx;
head                         = head - 0x10;
vec                          = (VECTOR*)head;
__asm__ volatile("" : "+r"(vec) : "r"(head));
vec->vy  = arg1->vy - arg0->vy;
*scratch = vec;
vec->vz  = arg1->vz - arg0->vz;
VectorNormal(vec, vec);
```

`func_800E0308` is the example. This is the `func_80103DD4` store-first
alloc plus the `func_80103E7C` `+r` copy, needed when the scratch block
is also `$a0` of a later call.

## Reuse the id `$s0` as the lookup pointer; compare the next field first

An item id that later becomes the `func_800BAFE0` result wants to stay
in `$s0`. A separate `GpItemSlot* slot` takes `$s2` and parks the id
fields in `$s0`. Assign the pointer back into the same `s32`:

```c
item   = (s32)func_800BAFE0(item);
attach = ((GpItemSlot*)item)->field_0;
```

A second byte compared against a non-zero constant then assigned to the
same live callee-saved var loads directly into that register (`lbu s2`
/ `beq` / `nop`). The target compares first so the load sits in `$v1`
and the copy fills the delay slot:

```
lbu   v1, 2(s0)
li    v0, 0xFF
beq   v1, v0, skip
 move s2, v1
```

```c
if (slot->field_2 != 0xFF) {
    attach = slot->field_2;
    count  = slot->field_3;
}
```

`func_800C2140` is the example. Assign-then-compare stuck at 97.9%.

## Load `spawnArg2` before the `spawnType` test so it fills the `lbu` delay

A pointer used only inside `if (iter->spawnType == 1)` is rematerialized
next to its later dereference, leaving a `nop` after `lbu spawnType` and
an extra `lw` after `key->field_3`. Assign it *before* the flag test so
the scheduler fills that delay:

```
lbu   v0, 0x28(a3)    /* spawnType */
lw    t0, 0x20(a3)    /* spawnArg2 — delay of the lbu */
bne   v0, t2, skip
 lui  v0, %hi(Mc_SaveData)
```

```c
work = iter->spawnArg2;
if (iter->spawnType == 1) {
    /* ... */
    bytes = work->field_3C;
}
```

`func_800B56AC` is the example. The same load inside the `if` stuck at
97.9% (one extra instruction).

## Local 2-func table: use an initializer so the prologue saves come first

A stack jump table plus callee-saved addresses live across a `jalr` needs
the `$ra`/`$s*` stores immediately after `addiu $sp`. Separate assignments
let the scheduler emit the first `lui`/`addiu`/`sw` of the table *before*
those saves (96% — only the prologue order differs):

```c
void (*fns[2])(s32, s32);
fns[0] = D_8017DA78;
fns[1] = D_8017EF60;
```

An initializer is a distinct RTL block, so GCC emits the full prologue
then the two address stores — the same shape as `func_800AD5B8`:

```c
void (*fns[2])(s32, s32) = { D_8017DA78, D_8017EF60 };
```

`func_800AE53C` is the example. Index `(u16 >> 8) & 0x7F` is what produces
`srl 6` / `andi 0x1FC` (plain `>> 8` is `andi 0x3FC`).

## Split `i + idx * 4` so the add is `addu v0, i, off`

A byte walk `table[idx].field_1` at `table + i + (id - K) * 4` wants:

```
addiu  v0, a2, -K
sll    s6, v0, 2
addu   v0, s2, s6    /* i + off, i is first */
addu   v0, v0, s8    /* + table */
lbu    s1, 1(v0)
```

Inlining `i + idx * 4 + (s32)table` emits `addu v0, s6, s2` (off first).
Naming `off = idx * 4` then `i + off` flips the operands but coalesces
the subtract into `addiu s6` / `sll s6, s6, 2`. Split the addend:

```c
idx  = arg2 - 0x80;
temp = i + idx * 4;
item = ((GpItemQty*)(temp + (s32)table))->field_1;
```

Assign the table pointer *before* the subtract so `lui`/`addiu` of the
symbol precedes `addiu v0, a2, -K`. Use two table locals (one per loop)
so the second table can reuse the mode register after `mode != 1`.

`func_800CF4EC` is the example.

## Store then test `p->field` so the OR dest stays in `$v1`

A u8 flag OR'd with bits from a u16, stored, then tested for bit 0 wants
the computed value in `$v1` and the dest pointer in `$a0`:

```
lbu   v1, 4(a1)
andi  v0, v0, 0x100
or    v1, v1, v0
sh    v1, 4(a0)
...
andi  v1, v1, 1
bnez  v1, skip
```

Naming the OR result parks it in `$a0` and pushes the pointer to `$a1`:

```c
/* BAD — temp takes $a0, p takes $a1; 98.5% with only regs swapped */
temp = q->field_4 | (p->field_1A & 0x100);
p->field_4 = temp;
if (!(temp & 1)) {
    func();
}
```

Store first, then test the field. GCC keeps the halfword in `$v1` for the
`andi`/`bnez` and leaves `$a0` for `p`. Two bare reads of `p->field_1A`
(no saved copy) also emit the two `lhu`s with `sh zero` between them:

```c
p->field_4 = q->field_4 | (p->field_1A & 0x100);
p->field_E = q->field_4 | (p->field_1A & 0x180);
p->field_1A = 0;
if (!(p->field_4 & 1)) {
    func();
}
```

`func_800E9EFC` is the example.

## `s32` dest forces `lh`; duplicate the shared call for kill-before-alt

An `s16` field copied into another `s16` emits `lhu` (no sign extend). The
same source assigned to an `s32` temp is `lh`, and that temp can then be
stored and shifted without a second load:

```c
/* lhu / sh / sll 2 — wrong load */
mem->field_26 = arg->field_2;
mem->field_28 = mem->field_26 << 2;

/* lh / sh / sll 2 */
temp          = arg->field_2;
mem->field_26 = temp;
mem->field_28 = temp << 2;
```

A pause-flag early-out and a lifetime expiry that share a cleanup, with
the still-alive path doing extra work, want the cleanup *before* the extra
(`bnez extra; move a0, mem; jal cleanup; extra:`). One cleanup at the
function tail after `if (alive) { extra; return; }` lays the extra first.
Write the cleanup in both places so GCC merges them with the desired
order:

```c
if (flag >= 2) {
    if (flag < 4) {
        return;
    }
    cleanup(mem, task);
} else {
    /* ... */
    if (mem->field_22 >= mem->field_28) {
        cleanup(mem, task);
    } else {
        extra(mem);
    }
}
```

`if (flag < 2) { work } else if (flag < 4) return; cleanup;` inlines the
work (`beqz` over it). `flag >= 2` as the first test is what emits
`bnez` to the work block. `func_800FE41C` is the example.

## 0x10-stride 1-based record: `idx * sizeof + base`, then `rec-1` / `rec[-1]`

A 16-byte table indexed 1-based wants both the pre-decrement pointer
(`$v0 = base + idx*16`) and `tmp = v0 - 0x10` live at once:

```
sll    v0, v0, 4
addu   v0, v0, v1
addiu  v1, v0, -0x10
lw     a0, -0x10(v0)
lw     a1, 4(v1)
```

`rec = &arr[idx - 1]` (or `arr += idx - 1`) folds to `addiu v0, -0x10`
*before* the add, then loads from `0(v0)`. `rec = &arr[idx]; x = rec[-1].f`
keeps only one pointer and loads from `-0x10(v0)` / `-0xC(v0)`. Split the
address so the first field uses the original pointer and the rest use the
decremented one:

```c
rec  = (Rec*)(idx * sizeof(Rec) + (s32)arr); /* index + base → addu v0,v0,v1 */
arr  = rec - 1;
grid = rec[-1].field_0; /* lw -0x10(v0) */
a    = arr->field_4;    /* lw 4(v1) */
```

`arr + idx` / `&arr[idx]` emits `addu v0, v1, v0` (base + index). The
`(s32)arr` addend after the scaled index is what swaps the operands.
`func_800ACD2C` is the example.

## Pin jal return + empty `asm("")` so `sb` / `andi` / delay-slot `sb` stay in order

A clamp that the target writes as:

```
jal    foo
nop
lw     v1, field_10(list)
sb     v0, field_4(list)
andi   a0, v0, 0xff
slt    v1, a0, v1
beqz   v1, skip
 sb    v0, field_5(list)
sw     a0, field_10(list)
```

needs the raw return to stay in `$v0`, the loaded compare operand in `$v1`,
and the zero-extend in `$a0`. A plain `s32 count = foo()` copies out of
`$v0` (`move v1, v0`) and then loads `field_10` into `$v0`. Pin the three
temps:

```c
register s32 count asm("v0");
register s32 count8 asm("a0");
register s32 sel asm("v1");

count         = foo();
sel           = list->field_10;
list->field_4 = count;
asm("");
list->field_5 = count;
count8        = count & 0xFF;
if (count8 < sel) {
    list->field_10 = count8;
}
```

`asm("")` (no memory clobber) keeps `field_4` before the `andi`. Storing
`field_5` before the compare lets GCC sink it into the `beqz` delay slot
and leaves the next `jal`'s delay for `move a1, s1`. A `::: "memory"`
barrier or a volatile `field_4` store empties that delay (`nop`) and
parks `field_5` in the following `jal` delay instead.

`func_800C7844` is the example.

## Child-walk `beq` chain: compare named constants, not `switch`

A sibling walk that the target opens with

```
li    s5, -1
move  v1, v0
li    s4, 6
li    s3, 1
...
beq   v0, s5, case_m1
nop
beq   v0, s4, case_6
```

assigns `-1` / `6` / `1` in that order only when those locals are *used*
in the compares, in that order. `switch (flag) { case -1: ... case 6: }`
still emits the `beq` chain, but rematerializes the cases as `s5=1`,
`s4=-1`, `s3=6` because the store of `1` after `Ui_TeardownTree` wins
allocation. Write the dispatch as gotos so the named temps are the
compare operands:

```c
minusOne = -1;
child    = head;
six      = 6;
one      = 1;
do {
    flag = childObj->field_2E;
    next = child->nextSibling;
    if (flag == minusOne) {
        goto case_m1;
    }
    if (flag == six) {
        goto case_6;
    }
    goto loop_cont;
case_m1:
    obj->field_2E = flag;
    goto loop_cont;
case_6:
    Ui_TeardownTree(childObj, childObj->owner);
    obj->status = one;
loop_cont:
    ...
} while (child != head);
```

Pinning `one` to `$s3` (or `six` to `$s4`) coalesces the now-dead list
pointer into that same saved reg and rewrites the earlier `lui v0,
%hi(list)` / `addiu s0, v0, %lo` as `lui s0`. Leave the three constants
unpinned.

`func_800C7844` is the example.

## Widen a `u8` load to `s32` before an `s32` store

Copying a `u8` field into an `s32` after an independent immediate store:

```c
u8 loc = sess->field_4;
task->killCountdown = 2;
task->spawnArg1 = loc;
```

emits `lbu v0` / `andi v0, 0xff` and hoists `li v1, 2` into the previous
`beqz` delay slot. The target wants the pointer load first (nop in that
delay), `lbu` into a second register, then the immediate:

```
beqz  v0, skip
 nop
lw    v0, %lo(Game_Session)(s1)
nop
lbu   v1, 4(v0)
li    v0, 2
sh    v0, 0x2a(s0)
sw    v1, 0x34(s0)
```

Give the temp the destination width so the `lbu` is already the `s32`
value (`v1`) and the constant reuses the now-dead pointer register
(`v0`). That also blocks the hoist — `v1` is live as the loaded byte.

`GameSession.field_4` is `byte` (signed). Load it as `(u8)` or GCC
emits `lb` and the overlay will not match even when a stub `u8` field
did:

```c
s32 loc = (u8)Game_Session->field_4;
task->killCountdown = 2;
task->spawnArg1 = loc;
```

`func_800A8E8C` is the example.

## OT-link: name the masks and pin the index so `0xFFFFFF` does not steal `$t0`

An addPrim-style OT insert that also walks a sibling array wants this
preheader order:

```
la    t4, Display_State
lw    t2, Gpu_CurrentOt
lui   t1, 0xFF
ori   t1, t1, 0xFFFF
lui   t3, 0xFF00
addiu a2, a0, 0xC
```

`addPrim(...)` alone hoists the masks *after* `&elem->field_C` and gives
the `0xFFFFFF` constant `$t0`. Zeroing the loop index at entry (the
`func_800ACEBC` trick) is not enough: the constant is referenced more
often than the index, so it still wins `$t0`.

Assign named `mask = 0xFFFFFF` / `maskHi = 0xFF000000` *before* taking
the field address, expand the two tag stores with those locals (so they
stay live), and pin the index:

```c
register u32 i asm("t0");
u32          mask;
u32          maskHi;

i      = 0;
mask   = 0xFFFFFF;
maskHi = 0xFF000000;
/* then z = &elem->field_C, then the loop */
```

`register ... asm("t0")` is required here — unlike `func_800ACEBC` the
target increment is in-place `addiu t0, t0, 1`, so the pin does not
rewrite it. `func_800AD410` is the example.

## Repeat a table lookup in the compare and the store so `$v0` holds the value

Comparing a field against `table[idx]` and then writing that same entry
back wants the address computed first, the field loaded into `$v1`, and
the table word loaded into `$v0`:

```
addu  v0, v0, v1
lw    v1, 0x928(s0)
lw    v0, 0(v0)
nop
beq   v1, v0, skip
 ...
sw    v0, 0x928(s0)
```

A temp (`anim = table[idx]; if (p->field != anim) p->field = anim;`)
loads the table word first (`lw v1, 0(v0)` / `lw v0, field`), so the
store source flips to `$v1` and the later `addiu` of a sibling address
is scheduled before it.

Write the lookup twice:

```c
if (actor->field_928 != D_80113368[idx]) {
    actor->field_928 = D_80113368[idx];
}
```

GCC CSEs the address and keeps the table value in `$v0`. `func_8010C4F0`
is the example.

## Dummy `0` on an overlay helper so the `jal` delay slot is `move a2, zero`

A two-argument overlay helper (`func_800BB5BC(scan, idx)`) can still
need a literal third `0` at one call site so the compiler emits:

```
addu  a0, a2, a0
lw    a1, 0x10(v0)
jal   func_800BB5BC
 move  a2, zero
```

Omitting it schedules `addu a0, a2, a0` into the delay slot and drops
one instruction. Add an unused `s32 arg2` to the real prototype (it
does not change the callee) and pass `0`. Same rule as overlay imports
of main (`Ui_InsetLayout`). `func_800BDC80` is the example.

## Store the first vtable slot through the global, then take its address

`table = D_8010D67C; table[0] = A;` writes `sw A, 0(table)`. The target
stores through `%lo` first, then materializes the address for `table[1]`:

```
sw    A, %lo(D_8010D67C)(v0)
addiu v1, v0, %lo(D_8010D67C)
sw    B, 4(v1)
```

Write the first slot on the symbol, then assign the local:

```c
D_8010D67C[0] = func_A;
table         = D_8010D67C;
table[1]      = func_B;
```

`func_800BDC80` is the example. Pair with `count = 1` *before*
`(u32)(id - 0xA0) >= 0x20 || flags != 0` so `li a1, 1` sits in the
`sltiu` delay slot.

## Two typed pointers so `container_of` stays in `$v1` and copies to `$t0`

A list node recovered with `container_of` and then used both for a field
access and as `$a0` to a later `jal` wants:

```
beq   flags, 1, skip
 addiu v1, s0, -0x10
bnez  arg, calc
 move  t0, v1
lbu   v0, 0x4e(v1)
```

One pointer (`obj = container_of(...); obj->field |= 0x80;` plus
`func(obj, ...)`) allocates the address straight to `$t0` and leaves a
`nop` in the `beq` delay slot. Keep two typed views of the same object
so GCC emits the subtract into `$v1` and the copy into `$t0`:

```c
enemy = (GpEnemy*)((u8*)node - OFFSET_OF(GpEnemy, node));
obj54 = (GpObj54*)enemy;
if (arg0 == 0) {
    enemy->field_4E |= 0x80;
} else {
    func_800E1C58(obj54, payload);
}
```

`func_800A4904` is the example.

## Subtract globals in one expression so the pointer stays in `$a0`

Loading a stream pointer, subtracting a base, then using the same pointer
later:

```c
rec    = D_80114C38;
offset = (s32)rec - (s32)D_8005C374;
```

loads the two globals sequentially (`lw a2, ptr` then `lui`/`lw` of the
base) and parks the pointer in `$a2`. Write the subtract as one
expression so the two `%hi`/`lw` pairs interleave and CSE keeps the
pointer in `$a0`:

```c
offset = (s32)D_80114C38 - (s32)D_8005C374;
if (Display_State.field_12c == 0x10) {
    offset = (s32)D_80114C38 + 0x7F9FFF00;
}
/* later: D_80114C38->buttons */
```

```
lui   v0, %hi(D_80114C38)
lui   v1, %hi(D_8005C374)
lw    a0, %lo(D_80114C38)(v0)
lw    v0, %lo(D_8005C374)(v1)
subu  a2, a0, v0
```

`func_8009FD74` is the example.

## Pin the reload to `$a0` and barrier so increment store stays before `lhu`

Advancing a global pointer then reading the next record through the old
pointer wants:

```
lw    a0, ptr
li    v0, 0xffff
sh    v0, cached
addiu v0, a0, 4
sw    v0, ptr
lhu   v1, 4(a0)
```

A plain `rec = ptr; ptr = rec + 1; if (rec[1].field == SENTINEL)`
schedules `lhu` before the `addiu`/`sw`. Wrapping the increment in
`do { ptr = rec + 1; } while (0)` fixes the order but recolors: `%hi(ptr)`
lands in `$a0` and the value in `$v1`/`$a1`.

Pin the reload to `$a0` and put an empty `asm volatile("")` after the
increment store. Dead `$a1` stays as `%hi(ptr)` and the `lhu` waits:

```c
register Rec* rec asm("a0");

rec = ptr;
cached = 0xFFFF;
ptr    = rec + 1;
asm volatile("");
if (rec[1].field == 0xFFFF) {
```

`func_8009FD74` is the example.

## Assign an `s16` index to `s32` before `& mask` so the load stays `lh`

`s16 idx; table[idx & 3]` (or using the `s16` field directly as the
index) lets GCC 2.8.1 see that only the low bits matter and emit `lhu`.
The target still wants a signed load of the halfword:

```
lh    v0, 0x36(s2)
andi  v0, v0, 0x3
sll   v0, v0, 1
```

Assign the `s16` field to an `s32` first so the conversion needs sign
extension, then mask:

```c
s32 idx;

idx           = spawn->field_2; /* lh */
mem->field_28 = D_80112C6C[idx & 3];
```

`func_800FB67C` is the example.

## Two tail calls that differ by one constant: write both, do not phi the arg

A pair of `Ui_LayoutWithMode0(...)` (or any 6-arg helper) that share every
argument except a late color wants a *single* `jal` with the call setup
duplicated in each arm:

```
bnez  s3, work
 lui   v1, 0x10
ori   v1, v1, 0x2010
addiu a2, s2, -0xe
li    a3, 0xe
move  v0, a3
sw    v0, 0x10(sp)
j     jal
 sw    v1, 0x14(sp)
...
sw    zero, 0x14(sp)
jal   Ui_LayoutWithMode0
 nop
```

A `color` local assigned in both arms (`color = 0x102010` / `color = 0`)
lets GCC CSE `a2`/`a3`/`0x10(sp)` *after* the join and remaps saved args
(`arg3` steals `$s1`). Two full calls with an early `return` on the first
keeps `s0`..`s3 = a0`..`a3` and leaves only the `jal` shared:

```c
if (arg3 == 0) {
    Ui_LayoutWithMode0(arg0, (void*)arg1, (void*)(arg2 - 0xE),
                       (void*)0xE, (void*)0xE, (void*)0x102010);
    return;
}
/* ... work ... */
Ui_LayoutWithMode0(arg0, (void*)arg1, (void*)(arg2 - 0xE),
                   (void*)0xE, (void*)0xE, (void*)0);
```

`func_800CDA64` is the example.

## Split `tpage +=` / `tpage |=` with a named reload so `field_27` fills `lhu`

A dual-poly UV copy that adjusts the first prim from `TmdObject` bytes
(`field_26` added to `tpage`, `field_27 << 6` added to `clut`) and then
sets ABR (`tpage |= 0x20`) wants a store after the add, a reload of
`tpage`, and the `field_27` `lbu` in that `lhu` delay slot:

```
sh    v1, 0xc(a1)
lw    v1, 0x80(a0)
lhu   v0, 0xc(a1)
lbu   v1, 0x27(v1)
ori   v0, v0, 0x20
sh    v0, 0xc(a1)
lhu   v0, 0(a1)
sll   v1, v1, 0x18
sra   v1, v1, 0x12
```

`poly->tpage += (s8)obj->field_26; poly->tpage |= 0x20;` CSE's the add
into the OR (`addu` / `ori` / one `sh`). A `TmdObject*` for the second
load puts `field_80` in `$v0` and keeps the prior `tpage` in `$v1`.
Assign the clut addend first, then reload `tpage` into its own `s32`:

```c
poly->tpage += (s8)arg0->field_80->field_26;
tmp          = arg0->field_80->field_27;
tpage        = poly->tpage;
tpage       |= 0x20;
poly->tpage  = tpage;
poly->clut  += (s8)tmp << 6;
```

`func_8009FA24` is the example. Same split is needed for the
`field_0` / `POLY_GT4` siblings (`func_8009F56C`, `func_8009F708`,
`func_8009FB28`).

## Finish the 2D byte offset before adding the table base

`arr[row][col]` of a 4-byte cell (row stride 16) hoists `row << 4` into a
saved reg, but then adds the symbol base to `col << 2` first:

```
sll    v0, col, 2
addu   v0, v0, s4    /* + &arr */
addu   v0, s2, v0    /* + row<<4 */
```

The target wants the integer offset complete, then the base:

```
sll    v0, col, 2
addu   v0, v0, s2    /* + row<<4 */
addu   v0, v0, s4    /* + &arr */
```

Hoist `rowOff = row << 4`, add `col << 2`, then index the flattened first
element. `temp >> 2` cancels against the element size so no extra
`sra`/`sll` appear:

```c
rowOff = kind << 4;
temp   = (otherKind << 2) + rowOff;
rec    = &D_8010FA4C[0][0] + (temp >> 2);
```

Inlining `arr[row][col]` stuck at 99.8%. `func_800DB900` is the example
(same addressing in `func_800E0414`).

## Load both pair fields before the swap `if`

A `{u16 handler, u16 swap}` record that picks argument order must load
both halves before the branch. `if (rec->field_2 == 0) fn(a,b,rec->field_0)`
reloads `field_0` in each arm (`lhu a2, 0(v1)`). Locals keep one `lhu` of
each half and reuse `v0` for `andi a2, v0, 0xffff`:

```c
swap    = rec->field_2;
handler = rec->field_0;
if (swap == 0) {
    D_8010FA38[handler](node, other, handler);
} else {
    D_8010FA38[handler](other, node, handler);
}
```

`func_800DB900` is the example.

## Loop-invariant `0xFFFF` can steal `$a1` from a live location key

A `TaskDesc` walk that compares `setupArg` with a precomputed location
key (`stage * 10000 + room * 100`) wants that key in `$a1` after the
table pointer is consumed:

```
lw    a0, 0(a1)
move  a1, v1
move  a2, a0
...
bne   v0, a1, next
...
lhu   v1, 0(a2)
ori   v0, zero, 0xFFFF
```

GCC 2.8.1 hoists the terminator into the freed `$a1` instead
(`li a1, 0xffff`) and leaves the key in `$v1`. The `lhu` dest and the
`Game_SetPtrSlot(task, 7)` `a1=7` setup then shift with it.

`register s32 key asm("a1")` reserves `$a1` for the earlier `stage << 2`
multiply and wrecks the prefix. An early `term = 0xFFFF` takes `$s2`.
`func_800E31E8` stuck at 94.1% on this allocation.

## Overlay callback tables bundled with a function stay as inline asm

Splat often puts a stack-copied function-pointer table in the same
`.s` file as the function that copies it (`func_80106838` / `D_8009794C`).
`INCLUDE_RODATA` of a newly created `nonmatchings/.../D_*.s` looks like
the `D_80097940` pattern, but configure/splat regenerates that tree and
deletes the extra file. Emit the table with the same inline
`.section .rodata` block used for `D_800979F8` (`func_80108E40`). Keep
any trailing `0` word that sits before the next `.align 3` jump table.

## Copy a `VECTOR3` through the parent pointer; load the compared field first

Copying via a sub-pointer (`pos = &arg0->pos; out->vx = pos->vx`)
emits `addiu v1, s4, 0xC` and then `lw 4(v1)` / `lw 8(v1)`. Field
access through the original pointer keeps the loads on that register:

```c
out->vx = arg0->pos.vx;
out->vy = arg0->pos.vy;
out->vz = arg0->pos.vz;
```

When the same object’s pointer field is compared to a global, load
the field first so `lw` precedes `lui %hi(global)`:

```c
coord = arg0->coord;
world = &D_80070F10;
```

Reversing those two lines swaps the `lw` / `lui`. Keeping `world` also
makes `&world->workm` `addiu a0, s5, 0x24` instead of a fresh
`la D_80070F34`. `func_800DAE50` is the example.

A full `MATRIX` applied to a `VECTOR3` is `gte_SetRotMatrix` +
`gte_SetTransMatrix` + `gte_ldlvl` + `gte_rtirtr_real`
(`.word 0x4A498012`, MVMVA on IR with TR) + `gte_stlvl`.

## Assign `firstChild` onto the spawn-result pointer so it stays in `$a0`

A UI task that returns `Ui_SpawnFromDesc` into `spawned` colors that
pointer `$a0` (`move a0, v0` / `beqz a0`). A later child poll wants
the same coloring:

```
lw    a0, 0xc(s3)
beqz  a0, done
lw    v0, 0x20(a0)
lh    v1, 0x2e(v0)
```

A fresh `Task* child = arg0->firstChild` swaps the registers (`lw v0,
0xc` / `lw a0, 0x20(v0)`). Assign the child task onto `spawned` and
load `spawnArg2` into a new object pointer:

```c
spawned = (UiObject*)arg0->firstChild;
if (spawned != NULL) {
    childObj = ((Task*)spawned)->spawnArg2;
    if (childObj->field_2E == 6) {
```

`func_800CD39C` is the example. Same first-child cast as `func_800C010C`.

## Pin two stack args and assign them in the desired `lw` order

A sibling that only keeps one stack addend live across `jal` loads it
naturally (`func_800B4538`: `lw s0, 0x30(sp)`). Adding another live
pointer (`arg0` plus a last-arg table) makes GCC rematerialize the
addend after the call (`lhu` / `lw 0x38(sp)` / `addu`) and, if the
addend is then pinned, still emit the two pre-`jal` loads in first-use
order:

```
lw    s1, 0x44(sp)   /* last arg, used first after jal */
lw    s3, 0x38(sp)   /* addend, used later */
```

The target wants source order (`lw s3` then `lw s1`). Pin both and
assign them in that order *before* the call:

```c
register s32   extra asm("s3");
register void* sets asm("s1");

extra = arg4;
sets  = arg7;
func_800B3448(...);
if (sets != NULL) {
    ctx->field_0  = sets;
    slot->field_20 = sets;
}
idx = table[slot->field_15] + extra;
```

`func_800B47A8` is the example.

## Nested `for (i = 0; i < n; )` + `do { } while (++i < n)` for two delay slots

A count-guarded walk that also keeps a running byte pointer wants two
different schedules at once:

1. `move a2, zero` in the delay slot of the first `beqz n`
2. `addiu a2, a2, 1` in the load-delay of the loop-back `lhu` of `n`

A plain `if (n != 0) { i = 0; p = …; do { …; i++; } while (i < n); }`
leaves a `nop` in the `beqz` delay (`i = 0` is a dead store on the skip
path while `$a2` is still the live `%hi` of a nearby global). A
`for (i = 0; i < n; i++)` fills that delay but emits `i++` *before* the
reload of `n`. Putting `p` in the for-init hoists `addiu p, base, off`
above the `lhu` of `n`.

The for-init is only the `i = 0` / `i < n` peel; the real trip count and
the `p += stride` live in an inner do-while whose condition is `++i < n`:

```c
for (i = 0; i < rec->field_2; ) {
    p = &prim->field_F;
    do {
        /* … */
        p += 0x1C;
        prim++;
    } while (++i < rec->field_2);
}
```

`func_800AC960` is the example. Pin `prim` with `register … asm("a1")` so
the `%hi(D_8010CAE8)` reused for the later `D_8010CAE8[0]` NULL check
stays in `$a2`.

## `j = 0; if (j < n)` after `if (n > 0)` keeps both `blez` checks

A signed count that guards an inner walk wants two `blez $v0` in a row,
with `j = 0` in the second delay slot and the `start = base` /
`limit = n` copies after that:

```
blez  v0, skip
 nop
blez  v0, skip
 move v1, zero
move  a1, a3
move  a0, v0
```

`if (n > 0) { for (j = 0; j < n; j++) }` often puts `j = 0` *before* the
second `blez` and loads `n` straight into `$a0`. A plain
`if (n > 0) { do { } while (j < n); }` emits only one `blez`. Split the
for-loop's peel out so the second compare is `j < n` after `j = 0`:

```c
if (count > 0) {
    j = 0;
    if (j < count) {
        start = base;
        limit = count;
        do {
            /* … */
            j++;
        } while (j < limit);
    }
}
```

Assign `start` / `limit` *inside* the second `if` so they land after both
`blez`. Use `limit` for the do-while so the continuing `slt` uses `$a0`
while both initial checks still use `$v0`.

A running `base += 3` after the inner walk will steal the `start` copy
(`$a1` becomes the incrementing base) unless `start` stays live past the
loop. `register s32 start asm("a1")` plus `asm volatile("" :: "r"(start))`
after the do-while keeps the copy; `register s32 base asm("a3")` keeps
the increment on `$a3`. `func_800B7930` is the example.

## Keep `id + 0x80` as a live s32 so it does not CSE with `id - 0x80`

A wrap test written next to the usual 32-wide range check:

```c
if ((u8)(id + 0x80) < 0x20) {
    if ((u32)(id - 0x80) < 0x20U) {
```

is one `lbu` of a 0–255 id. GCC then proves `id + 128` and `id - 128`
are the same modulo 256 and emits a single `addiu -0x80`, reused for
both the `andi 0xFF` wrap and the later `sltiu 0x20`. The target wants
both adds:

```
beqz  s0, skip
 addiu v0, s0, 0x80
andi  v0, v0, 0xFF
sltiu v0, v0, 0x20
...
addiu v0, s0, -0x80
sltiu v0, v0, 0x20
```

Assign the sum to an `s32` and keep that 32-bit value live with an
empty asm. The full-width `+ 0x80` is no longer equivalent to
`- 0x80`, so both `addiu`s survive. `register … asm("s3")` on the
inlined “is equipped” flag then restores `$s1`/`$s2`/`$s3`:

```c
s32 wrap;

wrap = id + 0x80;
asm volatile("" ::"r"(wrap));
if ((u8)wrap < 0x20) {
    equipped = 0;
    if ((u32)(id - 0x80) < 0x20U) {
```

`func_800C5188` is the example. The same wrap vs range pair is in
`func_800B91C8`, which avoids the CSE by reloading `field_0` after a
store instead.

## Split the first-walk count so it can live in `$a2` then move to `$t1`

A function that counts occupied slots and then reuses `$a2` for a second
table pointer wants:

```
move  a2, a3        # occupied = i
...
move  t1, a2        # copy before the second walk
```

Keeping one `occupied` live through both walks allocates it to `$t1`
from the start (its final home) and puts the first-walk limit in `$a2`.
Copy to a new `used` after the first walk:

```c
occupied = i;
if (count != 0) {
    /* walk; occupied++ */
}
used = occupied;
if (arg1 >= 0x100) {
    return 1;
}
```

`occupied` dies at the copy, so it can take `$a2`; `used = occupied`
fills the `slti 0x100` delay as `move t1, a2`.

A shared `off` temp used in both walks stays live and steals `$a2`
(`sll a2, v0, 2` instead of reusing start's `$v0`). Inline each
`(start << 2) + (s32)table` (or use a walk-local off).

`table2 = (start2 << 2) + table2` is an in-place increment and commutes
to `addu a2, a2, v0`. A new walker coalesced with the table keeps the
operand order:

```c
walker = (GpItemRec*)((start2 << 2) + (s32)table2);
```

`addu a2, v0, a2`. `func_800B8988` is the example.

The quantity-aware sibling (`func_800B87F4`) is the same two walks plus a
live `arg2`, so every temp shifts one register (`table` `$t0`→`$t1`,
`occupied` `$a2`→`$a3`, `used` `$t1`→`$t2`). After the second table
select, `if (arg2 < 0) arg2 = D_8010E3B8[id-0xA0].field_0` parks the
`lui` in the `bgez` delay. The stack check wants

```
slt   v1, cap, qty+arg2
bnez  v1, out
 li   found, 2
j     out
 li   found, 1
```

`if (cap < qty + arg2) found = 2; else found = 1;` inverts to `beqz` /
`li 1` / `j` / `li 2`. Assign the delay-slot default first:

```c
found = 2;
if (cap->field_2 >= walker->field_2 + arg2) {
    found = 1;
}
```

`func_800B8988` keeps the inverted `sltu` / `beqz` / `li 2` / `j` / `li 1`
because it is `if (walker < cap) found = 1; else found = 2`.

## Hoist `id << elem_size` and share that temp with the other arm's `$v1`

A two-base table select that wants:

```
beqz  v0, else
 sll   v1, a0, 3
lui   v0, %hi(A)
j     join
 addiu v0, v0, %lo(A)
else:
lui   v0, %hi(B)
addiu v0, v0, %lo(B)
join:
addu  t0, v1, v0
```

`desc = &table[arg0]` after the if schedules the `sll` after both `la`s.
Compute the byte offset *before* the if so delayed-branch parks it in the
compare, then add the base:

```c
tmp = arg0 << 3;
if (arg0 < 0x100) {
    table = A;
} else {
    table = B;
}
desc = (GpItemDesc*)(tmp + (s32)table);
```

If a sibling arm already needs a `$v1` temp (`arg0 & 3`, then later
`tmp + 0xE`), reuse the **same** variable. A second local lets the
`(x * 3)` use `$v1` as scratch and pushes the offset into `$a3`.

`(product) + (tmp + K)` written inline folds to `addiu a0, a0, K` /
`addu a0, a0, v1`. Materialize the addend after the product:

```c
arg0 = (arg2 * 3 + arg0) * 3;
val  = tmp + 0xE;
str  = recurse(arg0 + val, ...);
```

`addiu v0, v1, 0xe` / `addu a0, a0, v0`. `func_800B8EB0` is the example.

## `gpf 1` is `0x4B98003D`; IR0 wants `$v0` then `$a2`

aspsx `gpf 1` assembles to `0x4B98003D` (bit 24 set), not the commonly
cited `0x4A98003D`. `gte_gpf12()` emits a DMPSX placeholder; use a
handwritten command:

```c
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")
```

`func_800D9794` then does `lh field_4A` / `move a2, v0` / `sw v0` /
`mtc2 a2, $8` / `gte_ldsv` / `gpf 1` / `gte_stsv`. Loading the scale
straight into a pinned `$a2` drops the copy. Keep two registers:

```c
register s32 val asm("v0");
register s32 scale asm("a2");

val          = light->field_4A;
scale        = val;
block->scale = val;
gte_lddp(scale);
```

The 0x1C scratch is the 0x18 light block plus `s32 scale` at +0x18.
`Gfx_NormalizeLightDir` and `gte_stsv` take `head - 0xC`; MATRIX row/column
stores read `block->dir` (`lhu 0x10(s0)`). Pin `block` to `$s0` so it does
not swap with the dir-matrix pointer.

When `arg2` is live (world position subtracted from `field_24.t` into
`block->in` before the normalize), `$a2` is taken and the IR0 copy must
be pinned to `$t0` instead:

```c
register s32 scale asm("t0");
```

`func_800D9A30` is the example. It also writes `-block->dir` into the
direction-matrix row (same as `Gfx_SetFlatLight`).

## Local `s32` prototype so `Display_SetFadeMax(0xFF)` can fill a delay slot

`display.h` declares `void Display_SetFadeMax(u8 arg0)`. Passing an `s32`
local then emits `andi a0, a0, 0xff` in the `jal` delay, which steals the
slot the target uses for `sh killCountdown`. A TU-local `s32` prototype
(rename the header declaration, then redeclare) lets

```c
register s32 fade asm("a0");
fade = 0xFF;
asm("" : "+r"(fade));
arg0->killCountdown = 0xC;
Display_SetFadeMax(fade);
```

emit `li a0, 0xff` / `li v0, 0xc` / `jal` / `sh v0, 0x2a(s5)`. Do not
change the main header. `func_800BF9FC` is the example.

## Per-arm 5-arg calls so `sw zero, 0x10(sp)` sinks and stays before `jal`

A single `Ui_SpawnFromDesc(..., 0)` at the join of an if/else chain puts
`sw zero, 0x10(sp)` in the `jal` delay slot. The target wants that store
as the first instruction of two skip-join arms *and* as a labeled
instruction immediately before `jal` / `nop` (so a third arm can jump to
the store). Write the call in each arm:

```c
if (arg == 0x45) {
    obj = Ui_SpawnFromDesc(desc, 1, 1, 2, 0);
} else if (arg == 0x44) {
    obj = Ui_SpawnFromDesc(desc, 0, 1, 1, 0);
} else if (arg == 0x43) {
    obj = Ui_SpawnFromDesc(desc, 0, 1, 8, 0);
} else {
    /* 0x42 / default also set field_122 */
    obj = Ui_SpawnFromDesc(desc, a1, a2, a3, 0);
}
```

GCC 2.8.1 merges the identical `jal`s and sinks the 5th-arg home.
A `parent` local instead homes to a register (`move v0, zero`) or a
new stack slot and grows the frame. `func_800BF9FC` is the example.

## Keep scratch `head` and `vec` both live so `$v1` is not coalesced into `$a2`

An 8-byte `G_SCRATCH_HEAD` alloc that later becomes the `SVECTOR*`
argument of a call wants the decremented pointer in `$v1`, then a
late `move a2, v1` in an earlier `slti` delay:

```
lw    v1, 0(v0)
addiu a3, t1, %lo(params)
addiu v1, v1, -8
sw    v1, 0(v0)
...
beqz  v0, skip
 move a2, v1
```

Writing `vec = (SVECTOR*)(head - 8)` (or assigning `vec` and never
using `head` again) coalesces them: the load lands in `$a2` and the
`params` lo16 is no longer in the `lw` delay. `register u8* head
asm("v1")` gets the register but also blocks that delay fill (`lw` /
`nop` / `addiu`).

Decrement `head` in place, copy to `vec`, then mention both in the
same empty asm as the `temp = idx` copy so they stay distinct and
the scheduler can still hoist:

```c
head     = *scratch;
params   = &D_80113358;
head    -= 8;
*scratch = head;
vec      = (SVECTOR*)head;
if (temp < 3) {
    idx = temp;
}
temp = idx;
__asm__ volatile("" : "+r"(temp) : "r"(head), "r"(vec));
```

`+r`(temp) also stops copy-prop of `temp = idx`, freeing `$a0` for
the switch's `li a0, 1` (reused as `field_95E = 1`). `func_80109844`
is the example.

## `gpl 1` is `0x4BA8003E`; keep an `s32` index after `lbu`

aspsx `gpl 1` assembles to `0x4BA8003E` (same bit-24 set as `gpf 1` /
`0x4B98003D`). `gte_gpl12()` emits a DMPSX placeholder; use a
handwritten command next to `gte_gpf12_real`:

```c
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")
#define gte_gpl12_real() __asm__ volatile("nop; nop; .word 0x4BA8003E")
```

`gte_LoadAverageShort12` is then `gte_lddp` / `gte_ldsv` / `gpf 1` /
`gte_lddp` / `gte_ldsv` / `gpl 1` / `gte_stsv`. Load/store helpers
match as-is.

A `u8` temp assigned from an already-`lbu`'d field, then used as an
array index, emits a redundant `andi rd, rd, 0xff` after the alloc
store. Hold the index in an `s32` so the `lbu` stands alone:

```c
s32 idx;
idx  = slot->field_14; /* lbu */
dest = &arr[idx];      /* sll / addu, no andi */
```

`func_800B43E0` is the example.

## Pin the switch-wide record pointer to `$s2` so the accumulator takes `$s1`

A dispatcher that loads one record, builds a short-lived id from two of its
bytes, then uses a per-case counter (`val` in one arm, reused as the loop
tally in another) wants:

```
s0 = flag id (then reused as the case-4 index)
s1 = val / count
s2 = record pointer
```

Leaving all three unpinned gives `rec` in `$s1` and `val` in `$s2` even
when `flagId` already landed in `$s0`. Pin only the pointer:

```c
register GpCapCmd* rec asm("s2");
```

Do not also pin `flagId` / `val`: that rewrites the `lbu` temps
(`field_7` into `$s0` instead of `$v0`) and drops the `sra` from the
`bnez count` delay slot. Reuse `val` as the case-4 tally (`i = 0; val = i`)
so the copy fills that `beqz field_6` delay slot. `func_800E34D8` is the
example.

## Force `i = 0` before an independent load so the delay slot stays `nop`

Zeroing a loop index and then loading an unrelated field:

```c
i    = 0;
item = rec->field_0;
off  = (item - 0x80) * 4;
```

lets `-fschedule-insns` fill the `lbu` delay with `move i, zero`. The target
wants the zero first and a real load delay:

```
move  a3, zero
lbu   v1, 0(t1)
nop
addiu v0, v1, -0x80
```

`rec[i].field_0` does not help — GCC folds `i == 0` and still moves the
zero. Pin the index after the store so the load cannot sneak in front:

```c
i = 0;
asm volatile("" ::"r"(i));
item = rec->field_0;
```

Same barrier already used in `func_800BAC8C`. A later copy of the same
setup can skip it when another independent move (`rec2 = rec`) is
available to fill that delay. `func_800B904C` is the example.

## Do not pin the switch result to `$a0` if the `la` must split through `$v0`

The item-table switch (`D_80114C20` / `D_80114D70` / `Mc_SaveData.field_1AC`)
wants the default hi in the `bne` delay slot and a split `la`:

```
bne   field, 2, default
 lui  v0, %hi(Mc_SaveData)
lui   v0, %hi(D_80114C20)
j     join
 addiu a0, v0, %lo(D_80114C20)
default:
addiu a0, v0, %lo(Mc_SaveData+0x1AC)
```

`register GpItemRec* tmp asm("a0")` fuses each `la` into `lui a0` /
`addiu a0, a0` and puts the case-2 hi in that delay slot instead. Leave
`tmp` unconstrained so GCC uses `$v0` as the address builder; `tmp`
still lands in `$a0` when the else path indexes it after the range
check. `func_800B8B00` is the example.

## Reassign the running pointer before a field-2 store to kill the IV

A search loop that both walks `walker++` and writes `walker->field_2 = 0`
lets GCC keep `&walker->field_2` as a second induction variable
(`addiu a2, a1, 2` / `addiu a2, a2, 4`). The target uses `lhu`/`sh` at
`2(a1)` only.

Copy the found row onto the table pointer (now dead in that arm) and
store through the copy:

```c
found          = walker;
found->field_0 = 0;
table          = found;
table->field_2 = 0;
```

`found->field_2 = 0` still builds the IV. `func_800B8B00` is the example.

## Occupied-slot `return dest` after a delayed `dest = jal` skips `move v0, s0`

`dest = func(...)` scheduled into a later `bnez` delay slot leaves `$v0`
holding the return value. `return dest` on that arm then jumps past the
shared `move v0, s0` (GCC sees `$v0` is still live). Sibling arms that
clobbered `$v0` still need that move, so the target shares one label.

`goto` the shared return instead of `return dest` on the occupied arm
so every path hits `move v0, s0`. `func_800B8B00` is the example.

## INCLUDE_ASM can hide a second function after the last `jr ra`

Splat merges the next function into the previous INCLUDE_ASM when that
next function has no symbol (no `jal` target, no named entry). After
the first function’s last `jr ra` comes a standard prologue
(`addiu $sp, $sp, -N`). Match both as separate C functions in source
order and add the second address to `sym.*.txt` so a later extract
splits them.

`func_80108A0C` / `func_80108AD4` is the example. The first function is
the `field_956 = 6` body of `func_80109290` (without the
`field_3 == -2` guard); the second is the `field_956 = 7` body inlined
in `func_80106838`.

## Duplicate the 1/0 call so the flag stays a branch, not `sltu`

`Ui_SetListScrollFlag(menu, (flags & 0x10) != 0)` and a `flag` temp:

```c
if (flags & 0x10) {
    flag = 1;
} else {
    flag = 0;
}
Ui_SetListScrollFlag(menu, flag);
```

both emit `andi` / `sltu` / one `jal`. The target wants the delay-slot
default plus override:

```
andi  v0, v0, 0x10
beqz  v0, zero
 move a0, menu
j     call
 li   a1, 1
zero:
move  a1, zero
call:
jal   Ui_SetListScrollFlag
```

Write the call in both arms. GCC merges them into one `jal` and keeps
the `li a1, 1` / `move a1, zero` phi.

```c
if (arg0->spawnArg1 & 0x10) {
    Ui_SetListScrollFlag(menu, 1);
} else {
    Ui_SetListScrollFlag(menu, 0);
}
```

`func_800CC4F4` is the example.

## Reuse the `field_22` temp so confirm copies `lh` / `sh` without a reload

`if (menu->field_22 == 6) { obj->field_2E = menu->field_22; }` reloads
the halfword for the store (`lhu a0` plus `li v1, 6`). The target
hoists `6` into the status `bne` delay as `li v0, 6`, loads once, and
stores that register:

```
bne   v1, v0, skip
 li   v0, 6
lh    v1, 0x22(menu)
nop
bne   v1, v0, skip
 nop
sh    v1, 0x2e(obj)
```

Keep the load in a temp and assign that temp:

```c
sel = menu->field_22;
if (sel == 6) {
    obj->field_2E = sel;
    obj->field_2C = menu->field_20;
}
```

`func_800CC4F4` is the example. `func_800CB188` already uses this
`sel = menu->field_22` form.

## s32 copies of s16 fields keep `lh` for compare-and-step

A signed halfword used both as a compare operand and as the base of
`±8` wants two `lh`s and then `addiu` from that register:

```
lh    v1, 0x24(a3)
lh    v0, 0x26(a3)
nop
bne   v1, v0, step
 slt  v0, v1, v0
...
bnez  v0, store
 addiu v0, v1, 8
addiu v0, v1, -8
store:
sh    v0, 0x24(a3)
```

`mem->field_24 += 8` / `-= 8` hoists an extra `lhu` and puts dest in
`$a0`. Copy both s16 fields into `s32` temps first, then write the
adjusted temp back:

```c
s32 cur;
s32 dest;

cur  = mem->field_24;
dest = mem->field_26;
if (cur == dest) {
    /* equal path */
} else if (cur < dest) {
    mem->field_24 = cur + 8;
} else {
    mem->field_24 = cur - 8;
}
```

`func_800EC47C` case 2 is the example.

## A memory barrier after `if (!flag) x = K; goto dest` splits the join

Two identical `if (!(flag)) state = 3; goto apply` tails are
cross-jumped into one. A blockage after the `if` keeps both copies
but makes the flag-set path jump to an intermediate `j apply`
instead of to `apply` itself:

```
bnez  skip
 li   v0, 3
sw    v0, state
skip:
j     apply
 addiu a0, sp, 0x10
```

The target wants the `bnez` to go straight to `apply` so the delay
slot can take `addiu a0` and the store sits in the `j apply` delay:

```
bnez  apply
 addiu a0, sp, 0x10
li    v0, 3
j     apply
 sw   v0, state
```

`asm volatile("" ::: "memory")` after the increment (to stop that
block sharing a later store) is fine. Do not put one between the
flag `if` and `goto apply`. `func_800EC47C` is the example; the
two flag checks still want a non-barrier way to stay unmerged.

## Scratch alloc that stores from `$v0` needs a separate head temp

`G_SCRATCH_HEAD` push that the target writes back from `$v0`:

```
lw    v0, 0(v1)
addiu v0, v0, -0x1C
move  s0, v0
jal   ...
 sw   v0, 0(v1)
```

Folding the decrement into the typed block pointer coalesces to
`addiu s0, v0, -N` / `sw s0`. Keep the raw head in `$v0`, copy it,
then store the head:

```c
register u8*   head asm("v0");
register Type* block asm("s0");

head     = *scratch;
head     = head - 0x1C;
block    = (Type*)head;
*scratch = head;
```

`func_800A6F38` is the example.

## Evaluate an inlined helper's args before its body

`func_800BF334(item, owner->parent->flags)` is written out in
`func_800BDAA8` (no `jal`). Computing `flags` only inside
`if (desc->field_3 & 1)` loads `owner` after the bit test, leaves
`flag` in `$a0`, and inserts nops. Evaluate the second argument
first so `owner` stays in `$a0` and `parent` / `flags` load before
the `andi`:

```c
owner = arg1->owner;
flags = owner->parent->flags;
flag  = 0;
if (D_8010D838[item].field_3 & 1) {
    flag = flags == 1;
}
```

Compare `Mc_SaveData.field_7` to the already-live `selected == 1`
temp (`$s2`), not a fresh `1`, so the `bne` reuses that register.

## Index the slot table in the loop so `lui` stays in `$a0`

Filling `D_8010E8F8[0] = item` / `[1] = [2] = -1` by taking the
address first:

```c
p = D_8010E8F8;
if (item != p[0]) {
    for (; i < 3; i++, p++) {
        if (i == 0) {
            D_8010E8F8[0] = item;
        } else {
            *p = minusOne;
        }
    }
}
```

CSEs the `%hi` into `$a1` (needed for the later `%lo` store) and
puts `addiu a0, a1, %lo` in the `beq` delay slot. The target loads
the global first (`lui a0` / `lw %lo(a0)`), keeps `i = 0` in that
delay slot, then copies hi and materializes the pointer:

```
lui    a0, %hi(D_8010E8F8)
lw     v0, %lo(D_8010E8F8)(a0)
beq    s0, v0, skip
 move  v1, zero
li     a2, -1
move   a1, a0
addiu  a0, a1, %lo(D_8010E8F8)
```

Write the else-stores as `D_8010E8F8[i]` and let GCC strength-reduce.
The comparison then owns `$a0` for the `lui`, and the copy/`addiu`
appear after the branch:

```c
if (item != D_8010E8F8[0]) {
    i        = 0;
    minusOne = -1;
    for (; i < 3; i++) {
        if (i == 0) {
            D_8010E8F8[0] = item;
        } else {
            D_8010E8F8[i] = minusOne;
        }
    }
}
```

`func_800C7590` is the example.

## Copy `&block->vec` after the scratch store so `ldv0` uses `$v0`

A 0x18-byte scratch alloc that then zeroes `block->vec` and `gte_ldv0`s it
wants the block pointer copied into `$v0` right after the stack-head store:

```
addiu  a1, v1, -0x18
sw     a1, 0(v0)
move   v0, a1
...
lwc2   $0, 0(v0)
lwc2   $1, 4(v0)
```

`gte_ldv0(&block->vec)` alone keeps the pointer in `$a1`. Take a second
local immediately after `*scratch = block`:

```c
*scratch = block;
vec      = &block->vec;
```

`func_800D937C` is the example.

## `if (flag >= 0) { work } else { ret = 0 }` keeps the jump / else block

Inverting that test (`if (flag < 0) ret = 0; else work`) lets GCC put
`move a0, zero` in the `bltz` delay slot and drop the else label. The
target wants a nop delay, a `j` after the work, and a standalone
`move a0, zero`. Write the work in the true arm:

```c
if (block->flag >= 0) {
    ret = -block->sx / 10;
} else {
    ret = 0;
}
```

`func_800D937C` is the example.

## Pin the record pointer; peel `field_0` with `volatile` so the loop skips the first load

A 4-byte record walked with `rec++` that also reads `field_3` both before
and after a `jal` makes GCC clone `rec + 3` into a second `$s` register
(`lbu -2(s0)` / extra increment / 0x38 frame). Pin the walker:

```c
register Rec* rec asm("s0");
```

The first `field_0` check uses `$a0`. After `rec = arg0` the body still
wants a reload from `$s0`, and the `do` / `while` must branch to the
`nop` / `sll` *after* that load (the end-of-loop `lbu` already left the
next index in `$v1`). CSE of `idx = rec->field_0` with the `$a0` check
drops the reload and aims the `bne` at the load itself. Force the peel:

```c
rec = arg0;
idx = *(volatile u8*)&rec->field_0;
do {
    tbl = tables[idx];
    ...
    rec++;
    idx = rec->field_0;
} while (idx != 0xFF);
```

`func_800AE62C` is the example.

## Default case sits between `apply = 1; goto join` and `join:`

A high-nibble filter that shares `apply = 1` with the `mask == 0` path
wants this layout:

```
bne   mask, expected, test
 move v0, apply
j     join
 li   apply, 1
default:
move  apply, zero
join:
move  v0, apply
test:
beqz  v0, skip
```

Put the default *after* the shared store, not next to the compare:

```c
if (mask == 0) {
    goto set_apply;
}
if (mode == 0 || mode == 2) {
    expected = 0x10;
    goto cmp;
}
if (mode == 1 || mode == 3) {
    expected = 0x20;
} else {
    goto set_zero;
}
cmp:
if (mask != expected) {
    cond = apply;
    goto test;
}
set_apply:
apply = 1;
goto join;
set_zero:
apply = 0;
join:
cond = apply;
test:
if (cond != 0) {
```

A `switch` on 0..3 emits `slti` range checks. An `if`/`else` that assigns
`apply = 0` in the default arm *before* the compare places that store
ahead of `bne mask, expected`. `func_800AE62C` is the example.

## Don't reuse a saved-reg local for a `max` temp; nest the early string load

A layout width that is `max(measure(name) + 0xB, measure(label))` wants
the second measure in `$v1`:

```
jal   Text_MeasureWidth
move  v1, v0
slt   v0, s0, v1
beqz  v0, skip
 nop
move  s0, v1
```

Reusing the `spawnArg1` local (`val`, already in `$s0`/`$s1` across the
equip block) for that second width pins the copy to a callee-saved
register (`move s1, v0`). Give the compare its own short-lived temp.

The same function also calls `func_800B8EB0` once to measure and again
to draw. Assigning both results to one `text` local makes that variable
interfere with `rec` (`$s1`) and `&Wip_SysConfig` (`$s2`), so the later
`color = 0x606060` / `text = func_800B8EB0(...)` pair swaps (`color` in
`$s1`, `text` in `$s2`). Nest the first call so `text` is only assigned
on the draw path:

```c
width = Text_MeasureWidth(func_800B8EB0(arg0->spawnArg1, 0, 0)) + 0xB;
other = Text_MeasureWidth(D_8010E494);
if (width < other) {
    width = other;
}
...
text  = func_800B8EB0(arg0->spawnArg1, 0, 0);
color = 0x606060;
```

`func_800CA838` is the example.

## Store the compared byte through an `s32`; pin jal-return + subtract copies

A switch arm that writes a default byte, then overrides it with the same
loaded value it just compared (`param[0] = field` when `field == 0xE`
or `0xF`) wants that load in `$v1` and `sb $v1` with no `andi 0xFF`:

```
li    v0, 0xd
sb    v0, 0x10(sp)
lbu   v1, field
li    v0, 0xe
bne   v1, v0, next
 li   v0, 0xf
sb    v1, 0x10(sp)
```

A `u8` local for that field emits `andi a0, v1, 0xff` and hoists the
`lbu` above the default store. Compare the field directly in the other
arms; only the "store the loaded value" arm needs an `s32` temp.

The same function's `jal` that returns a slot pointer wants
`move a0, v0` / `lbu v1, 2(a0)` and then `move v0, v1` /
`addiu v1, v0, -0x9F` after the `== 0xFF` check. GCC otherwise uses
`lbu 2(v0)` and in-place `addiu v1, v1, -0x9F` in the `beq` delay
slot. Pin the pointer to `$a0` and force the subtract source through a
temp:

```c
register GpItemSlot* slot asm("a0");

slot = func_800BAFE0(item + 0x7F);
asm volatile("" : "+r"(slot));
attach = slot->field_2;
if (attach != 0 && attach != 0xFF) {
    temp = attach;
    asm volatile("" : "+r"(temp));
    attach = temp - 0x9F;
}
```

`func_800A9310` is the example.

## Inline a helper with literal `0` so field loads use `$zero`

Copying `func_800BAD28` and writing `scan = NULL` / `((GpItemScan*)0)->field`
keeps a 0 in a GPR (`t1`) or CSEs that 0 with an earlier `state == 0` into
`$s2`, so the target's `lbu r, off($zero)` never appears. A
`static __inline` helper whose first argument is the scan pointer, called
as `helper(0, rec, 1)`, lets GCC 2.8.1 (`-finline`) substitute `$zero` for
every `arg0->field_*` load.

The third inlined arg then wants `$a3` (and `loop_end` `$a1`), matching
the non-inlined `func_800BAD28` shape after `$a0` is freed. Pin the
short-lived `table` / `end` / `newQty` temps onto the same `$v1` and
`loop_end` onto `$a1` so `i + count` overwrites the table pointer after
`base = table`:

```c
register GpItemRec* table asm("v1");
register s32        end asm("v1");
register s32        loop_end asm("a1");
register s32        newQty asm("v1");

base = table;
end  = i + count;
if (i < end) {
    loop_end = end;
```

Pinning only `loop_end` merges `end` into `$a1` and delays `base = table`.
`func_800B996C` is the example.

## Pass-through `$a3` so its save stays in the prologue pair

A 4-arg function that calls a callee which also reads `$a3`, then uses
that same pointer after the return, must pass the incoming `$a3`
through. Omitting it as "unused" lets GCC delay `move s4, a3` into a
later load-delay slot and emit `sw ra` before `sw s4`. Passing it
keeps the incoming-arg pair together:

```
move  a0, a1
sw    s4, 0x20(sp)
move  s4, a3
sw    ra, 0x24(sp)
```

`func_801011D0` is the example (`func_800E0FEC(..., arg3)`).

## `>= 0` ternary for `bltz` / `lui 0xffff` / `lui 1`

Stepping a 16.16 word away from zero when the low half is nonzero:

```
bltz  v1, join
 lui  v0, 0xffff
lui   v0, 1
join:
addu  v0, v1, v0
```

`if (val < 0) addend = -0x10000; else addend = 0x10000` inverts to
`bgez` with `0x1` in the delay. The flipped ternary puts the negative
constant in the `bltz` delay:

```c
if ((val & 0xFFFF) != 0) {
    dest = val + ((val >= 0) ? 0x10000 : -0x10000);
}
```

Use `-0x10000` (not `0xFFFF0000`) so the add stays signed. `func_801011D0`
is the example.

## Volatile store so `lw -N(head)` and `sw 0(block)` stay distinct

A scratch push that later reloads the first word through the original
head (`lw v1, -0x10(s1)`) but writes back through the block pointer
(`sw v0, 0(s0)`) CSEs to `sw v0, -0x10(s1)` if both sides use the
same typed pointer. Load via `head - N` and store through a volatile
view of the block:

```c
val = ((T*)(head - 0x10))->vx;
if ((val & 0xFFFF) != 0) {
    ((volatile T*)s)->vx = val + addend;
}
```

`func_801011D0` is the example.

## List identical switch cases separately, unique case last, so stores cross-jump

A child-flag switch whose `-1` and `9` arms both do `obj->field_2E = flag`,
with a different `6` arm (`Ui_TeardownTree` / `status = 1`), wants one
shared `j cont / sh flag` and only `6` hoisted into a callee-saved
register (`li s2, 6`).

Writing them as one body:

```c
switch (flag) {
    case 9:
    case -1:
        obj->field_2E = flag;
        break;
    case 6:
        Ui_TeardownTree(childObj, childObj->owner);
        obj->status = 1;
        break;
}
```

treats `-1` as a loop-invariant case-group value. GCC hoists it
(`li s2, -1` / `li s4, 6`), steals the register that should keep `1`
then `6`, and the `slti 7` delay slot loads `9` instead of `-1`.

Listing the identical arms as separate cases and putting the unique
arm last:

```c
switch (flag) {
    case 9:
        obj->field_2E = flag;
        break;
    case -1:
        obj->field_2E = flag;
        break;
    case 6:
        Ui_TeardownTree(childObj, childObj->owner);
        obj->status = 1;
        break;
}
```

emits two `j cont / sh` tails. GCC 2.8.1's cross-jump merges them into
the one store after the `bne 9` fall-through, while `-1` stays an
immediate in the `slti` delay slot. Reuse the `1` temp for the pivot
(`one = 6` before the loop) so that load overwrites `$s2` instead of
allocating `$s4`. `func_800C8B40` is the example.

## `i = count` after a count barrier fills the `beqz` delay slot

A zeroed counter used both as the loop index seed and as the unsigned
`slt` left-hand side:

```
move  t0, zero
...
slt   v0, t0, v0
beqz  v0, skip
 move t3, t0
```

wants `i = count` *after* a `asm volatile("" : "+r"(count))` barrier and
immediately before `if (count < limit)`. Putting `i = count` inside the
`if` lets the first `lui` of the loop-invariant tables steal the delay
slot. Putting it next to `count = 0` hoists `move t3, t0` to the top of
the prologue. The barrier keeps `slt t0, limit` from folding to `beqz
limit` and stops `i = count` from riding `count = 0`. `func_800C2B70`
is the example.

## Split-address inline asm for `lui v1; addiu dest, v1` of a second global

Two prologue address-of-globals (`Mc_SaveData.field_1AC` then
`Wip_SysConfig`) both want `$v0` as the `lui` temp once the first
`addiu` has freed it. The target instead uses `$v1` for the second:

```
lui   v0, %hi(Mc_SaveData)
addiu a2, v0, %lo(Mc_SaveData+0x1ac)
lui   v1, %hi(Wip_SysConfig)
addiu t4, v1, %lo(Wip_SysConfig)
```

`cfg = &Wip_SysConfig` emits `lui v0` / `addiu t4, v0`. Occupying `$v0`
with the upcoming `lbu` of a scan field spills other incoming args.
Pin `cfg` to the dest register and emit the split pair (same form as
`Text_ItoaHex` / `Fs_CopyWorkEntries`):

```c
register WipSysConfig* cfg asm("t4");
{
    register s32 hi asm("v1");
    asm volatile("lui %1, %%hi(Wip_SysConfig)\n\t"
                 "addiu %0, %1, %%lo(Wip_SysConfig)"
                 : "=r"(cfg), "=r"(hi));
}
```

`func_800C2B70` is the example.

## Assign the LCG back onto `D_80070F60`; split `t[1] +=` so `flg = 0` fills the load delay

Two in-block `D_80070F60 * 5 + 0x71357911` steps that both feed field
stores want the result written back to the global, not kept in temps:

```c
D_80070F60    = D_80070F60 * 5 + 0x71357911;
mem->field_12 = 0xFFF0 - (((u32)D_80070F60 >> 16) & 0x3F);
D_80070F60    = D_80070F60 * 5 + 0x71357911;
mem->field_24 = ((u32)D_80070F60 >> 16) & 0xFFF;
```

The first LCG stays in `$a0`, the constant in `$a2`, `&D_80070F60` in
`$a1`, and both `sw`s are delayed until after `field_24`. Temps
(`rng` / `rng2`) put the first result in `$t1` and DSE the first store.
The same `D_80070F60 = D_80070F60 * 5 + C` form at a later
`func_800EB2C8` call site lands the LCG in `$v0` so `$a3` can hold the
constant, then the `>> 16 & 0x1000` bit.

An independent `coord->flg = 0` next to `coord->coord.t[1] += step`
emits the `sw zero` before the `lh` / `lw`. Split the add so the zero
store fills the load delay and the `t[1]` store sits in the `jal`
delay slot:

```c
y                 = coord->coord.t[1] + mem->field_12;
coord->flg        = 0;
coord->coord.t[1] = y;
func_80098F58(coord);
```

`func_800FBEBC` is the example.

## Add a rotated scratch `SVECTOR` through the block, not the GTE pointer

After `gte_ldv0` / `gte_rtv0_real` / `gte_stsv` on `dir = head - 8`, adding
the source position through that same `dir` pointer emits `lhu -8(head)` /
`lhu 2(a0)` and delays `move a1, pos` until the last component. Adding
through the 0x10-byte scratch block keeps `8(s1)` / `0xa(s1)` / `0xc(s1)`
and lets GCC schedule the `func_800DE7CC` args and preload `pos.vy` /
`pos.vz` during the `vx` add:

```c
dir = (SVECTOR*)(head - 8);
gte_ldv0(dir);
gte_rtv0_real();
gte_stsv(dir);
block->dir.vx += ((GpRayScratch*)(head - 0x10))->pos.vx;
block->dir.vy += block->pos.vy;
block->dir.vz += block->pos.vz;
ret = func_800DE7CC(dir, &block->pos, dir, NULL);
```

`pos.vx` must be reloaded via `head - 0x10` so the add is `-0x10(v1)`
(the original scratch head, still live after GTE). `block->pos.vx` is
`0(s1)` and reshapes the whole add. `func_800EA02C` is the example.

## Invert `if (field == 0)` so the else reloads into `$a1`

A `u16` that is both the `!= 0` predicate and the call argument is CSEd
to `lhu v1` / `beqz` / `move a1, v1`. The target reloads at the else
site:

```
lhu   v1, field
bnez  v1, reload
 …
reload:
lhu   a1, field
j     join
 move a0, s4
```

Write the zero arm first so the nonzero arm is the `else` and must
emit a fresh `lhu a1`:

```c
if (actor->field_93C == 0) {
    mode = 2;
    if (actor->field_91C == NULL) {
        mode = 0x13;
    }
} else {
    mode = actor->field_93C;
}
```

`if (field != 0) { mode = field; } else { … }` stuck at 91% with only
that `move a1, v1`. `func_80107E1C` is the example.

## `dx = 1; dest = dx` after an ABS in `$v0`

`register s32 dx asm("v0")` plus `if (dx < 0) dx = -dx` is the
`lw v0` / `lw v1` / `subu v0, v0, v1` / `bgez` / `nop` / `negu`
sequence. A later `dest = 1` then uses the live switch constant in
`$a0` (`sb a0`) and leaves a `nop` in the first `beqz` delay.

Assign the 1 through the same `dx` so the fail join is `li v0, 1` /
`sb v0` and that `li` fills the first `slti` / `beqz` delay (harmless
on the close path, which immediately reloads `$v0`):

```c
dx = coord->coord.t[0];
dx -= actor->field_20;
if (dx < 0) {
    dx = -dx;
}
if (dx < 0x69) {
    /* second axis… */
} else {
    dx = 1;
    actor->field_973 = dx;
}
```

Split `dx = t[n]; dx -= other` so `t[n]` lands in `$v0` first.
`func_80107E1C` is the example.

## Inlined `return NULL` after `Task_Spawn` keeps `bnez` / `j` / `move a0, 0`

A local

```c
task = Task_Spawn(...);
if (task != NULL) {
    extra = task->extra;
    ...
}
```

becomes `beqz a0, join` and drops the redundant `task = NULL`. The target
wants the inlined-helper shape:

```
move    a0, v0
bnez    a0, setup
 nop
j       join
 move   a0, zero
```

`inline static` a small helper that `return NULL` on `item == 0` and on
`Task_Spawn` failure. The inlined `return` is `task = 0; goto join`, and
the 3rd helper arg (`item`) is allocated to `$t0` because `Task_Spawn`
needs `$a2`. `func_801034C0` is the example.

## Put the `if (call != NULL)` body in the gap between if/else arms

A shared `func_800EA478` tail with three id constants wants the then-block
*between* the `0x16` arm and the `0x19` arm, reached by a backward
`bnez`, plus `j join` / `lui` after the success work:

```
bne   a0, v1, check_19
 li   v0, 0x19
li    a0, 0x80060024
j     do_call
 move a2, zero
# success (only via bnez from after the call)
...
j     join
 lui  v1, %hi(table)
check_19:
```

Three separate calls, or a single call with temps, both emit
`beqz` and place the then-block *after* the jal. Write the layout with
gotos so the success label sits in that gap and the call's `if (eff !=
NULL) goto success`. `func_801034C0` is the example.

## Split SPRT `y` from text `y`; copy stack color so it takes `$s2`

A SPRT-then-number drawer that lives all five args across `Ui_InsertDrawTPage`
wants `$s1 = arg1`, `$s2 = color`, `$s3 = arg2`. Two temps that look harmless
break that coloring:

Reusing one `y` both before the call (`y = baseY; p->y0 = y + arg2 - 7`) and
after (`y = baseY - 3; req.y = y + arg2`) makes `y` live across the jal and
steals `$s1` from `arg1`. Then `arg1`/`arg2` flip to `$s3`/`$s1`.

Using `arg4` directly for `*(u32*)&p->r0` and `req.field_8` assigns
`$s2 = arg2`, `$s3 = arg4`. Copy the stack color into a named local *after*
the early `y = baseY` load (and before `arg2` is consumed) so `color` takes
`$s2` and `arg2` stays in `$s3`:

```c
p->x0 = arg0->baseX + arg1 + 0x6C;
y     = arg0->baseY;
color = arg4;
/* w/h/uv/clut/setlen */
*(u32*)&p->r0 = color;
setcode(p, 0x64);
p->y0 = y + arg2 - 7;
/* … call … */
textY          = arg0->baseY - 3;
req.y          = textY + arg2;
req.field_8    = color;
```

The early `y` load also frees `$v1` after `D_80071190 = p + 1`, which is
what stores the cursor bump before `p->x0`. `func_800C2538` is the example.
The `textY = baseY - 3` half is the same pattern as `func_800CDBEC`.

## Load a terminator key once so the record pointer stays in `$v1`

A 0xFFFF-terminated record walk that later passes the same key into a
call wants the record in `$v1` and the first `lhu` in `$a0`:

```
lw    v1, 4(v0)
lhu   a0, 0(v1)
beq   a0, s3, next
lhu   v0, -0xc(s1)
bne   a0, v0, incr
addiu a0, v1, 4
move  a1, zero
lhu   a2, 0(v1)
jal   callee
```

Writing `rec->key` in the terminator test, the match compare, *and* the
call argument lets GCC put the record in `$a2` (the call's third arg) and
emit an extra `lhu` before the compare. Load the key once into a `u16`,
use that temp for both tests, and re-read the field only as the call
arg:

```c
id = rec->key;
if (id != term) {
    do {
        if (id == want) {
            callee(&rec->payload, 0, rec->key, NULL);
            break;
        }
        rec++;
        id = rec->key;
    } while (id != term);
}
```

Passing `id` into the call becomes `move a2, v1` instead of `lhu a2, 0(v1)`.
`func_800B6B44` is the example.

## Scratch alloc in `$v0`/`$v1`, pin the block, name the 3-arg src

A downward `G_SCRATCH_HEAD` alloc that is then guarded by a NULL check
wants the head pointer in `$v0` and the subtracted pointer in `$v1`,
with the store *before* the branch and the saved block copy in the
`beqz` delay slot:

```
lui   v0, 0x1F80
ori   v0, v0, 0x3FC
lw    s2, 0(v0)
lw    s3, 0x1C(a0)
addiu v1, s2, -0x84
sw    v1, 0(v0)
lw    v0, 0x90C(s3)
nop
beqz  v0, cleanup
 move s1, v1
```

An unpinned `scratch` / `tmp` pair lands in `$v1`/`$a1` and sinks the
store into the delay slot. Pin them, then force the block copy so it
does not become the call's `$a1`:

```c
register void** scratch asm("v0");
register u8*    tmp asm("v1");

scratch  = (void**)G_SCRATCH_HEAD;
head     = *scratch;
actor    = arg0->actor;
tmp      = head - 0x84;
*scratch = tmp;
if (actor->field_90C != NULL) {
    block = (GpPitchScratch*)tmp;
    __asm__ volatile("" : "+r"(block));
```

Without the asm, GCC copies `tmp` into `$a1` (the upcoming
`func_801040A0` dest) and only then into `$s1`. Independent
`block->rot = 0` stores written *before* the field walk emit first and
leave `addiu a2, head, -0x14` in the jal delay. Name the first call
argument so those loads exist, then write the zeros after that load:

```c
src = (GsCOORDINATE2*)((GameActorExt*)actor->field_91C->extra)->field_8;
block->rot.vx = 0;
block->rot.vy = 0;
block->rot.vz = 0;
func_801040A0(src, (GsCOORDINATE2*)block, (SVECTOR*)(head - 0x14));
```

`-fschedule-insns` lifts the three `sh zero` into the `field_91C` /
`extra` / `field_8` load delays and puts the last one in the jal delay
slot. `func_80102F10` is the example.

## Nested `$v0` pin reloads a field used as both `if` and later index

A signed byte that is both an early `if (p->field == 0)` test and a later
array index wants the skip path to keep `$v0` from the first `lb` and the
then path to reload just before the join (`lb` after the then-block
stores, then `lh` of unrelated fields, then `sll` of `$v0`):

```
lb    v0, 8(s1)
bnez  v0, join
 ... clobbers $v0 ...
lb    v0, 8(s1)
join:
lh    a1, 0x18(s1)
```

A function-scope `register s32 mode asm("v0")` reserves `$v0` for the
whole function and steals it from a 16-byte prologue struct copy
(`lui v0, %hi(table)` / `addiu t3, v0, %lo(table)`). Reloading into a
nested pin *after* the `if` CSEs with the skip path and only emits the
then-path `lb`:

```c
if (arg0->field_8 == 0) {
    /* ... stores that clobber $v0 ... */
}
{
    register s32 mode asm("v0");
    mode = arg0->field_8;
    Text_DrawPrompt(..., texts.texts[mode], ...);
}
```

The 16-byte stack copy itself is `texts = D_80093DA0` of a 4-pointer
struct, not element-wise assignment. Pin the later item-walk locals
(`i` in `$s2`, scan pointer in `$s3`) so they do not swap. `func_800BE808`
is the example.

## D4 fade overlay: `D_80114C80`/`D_80114CA0` + split `0xE1000000 | 0x240`

`D_80114C80` is `TILE[2]` and `D_80114CA0` is `DR_TPAGE[2]`, indexed by
`Display_State.field_114` (16-byte / 8-byte stride). Several neighboring
D4 task states share this pair (`func_800AABB0` … `func_800AB828`).

On the leaf overlay (`func_800AB828`) the target hoists `0x64` and both
prim pointers before the `CdCmd_Queue.field_224` check. Keep that order
in C (`color = 0x64`, then `buf = ds->field_114`, then both `&arr[buf]`).

An `s8 yoff = ds->vramYOffset` local after `x0` is what places
`sb r/g/b` before `sh x0` and the `lbu 0x109` / `sll`/`sra` between `x0`
and `w`/`h`. Folding `y0 = -0x78 - ds->vramYOffset` with no local sinks
the RGB stores.

`dr->code[0] = 0xE1000240` (or an early `mode = 0xE1000240`) either
delays `lui t0,0xe100` into the TILE stores or emits it *before* the
`0xFFFFFF` mask. Write the command as two operands at the store site:

```c
setlen(dr, 1);
dr->code[0] = 0xE1000000 | 0x240;
addPrim(Gpu_CurrentOt - 0x10, dr);
```

That is the same split as `Title_MenuTask`; here it is required even
though the function is a leaf and both `addPrim`s already match.

When the overlay is the first thing in the function and the only queue
access is `field_224`, the target hoists `li a2,8` then
`lui a1,%hi(CdCmd_Queue)` (no `addiu` of the queue base) and wants:

```
addiu v0, v0, %lo(D_80114CA0)
lhu   a0, %lo(CdCmd_Queue+0x224)(a1)
nop
bnez  a0, skip
 addu t2, v1, v0
```

`if (CdCmd_Queue.field_224 == 0)` rematerialises `%hi` at the use site
and parks `buf` in `$a0`. Pin color to `$a2`, emit the hi with
non-volatile `asm("lui %0, %%hi(CdCmd_Queue)" : "=r"(qhi))`, then load
through that register as a C halfword (not an `asm lhu`). An `asm lhu`
is a scheduling barrier and leaves `addu t2` *before* the load. The C
load participates in delay-slot filling:

```c
register s32 color asm("a2");
register s32 qhi asm("a1");
register s32 queued asm("a0");

color = 8;
ds    = &Display_State;
asm("lui %0, %%hi(CdCmd_Queue)" : "=r"(qhi));
buf    = ds->field_114;
tile   = &D_80114C80[buf];
dr     = &D_80114CA0[buf];
queued = *(u16*)((s32)qhi + (s16)0x91C4); /* %lo(CdCmd_Queue+0x224) */
if (queued == 0) {
```

`0x91C4` is the signed 16-bit `%lo` of `CdCmd_Queue.field_224`
(`0x800691C4`). The object has an unpaired `R_MIPS_HI16` and a
hardcoded `lhu` offset; GNU ld still produces the same linked
instruction as `%hi/%lo`. `func_800AADDC` is the example.
`GameSession.field_78` is an `s16` cache of `field_7`; compare with
`lbu`/`lh` and write back with `lbu`/`sh`.

When idle-queue work runs *before* the overlay (`func_800AABB0`), assign
`color = 8` *after* the idle `if`, not before it. GCC copies that
assignment into the `beqz` delay slot on the skip path and keeps it after
`task->state++` on the taken path (`lw` / `nop` / `addiu` / `sw` /
`li a2,8`). Assigning color before the `if` (or rematerialising it at
the end of the body) schedules `li a2,8` into the `lw state` delay and
puts `lui Display_State` in the `beqz` slot instead.

## Don't pin `$s2` for `p = &global` if `la` must split around `jal` via `$v0`

`p = &Wip_SysConfig` allocated to a callee-saved register normally
expands as the two-register form, which `-fschedule-insns` can split
around a call:

```
lui   v0, %hi(Wip_SysConfig)
jal   func
 addiu s2, v0, %lo(Wip_SysConfig)
```

`register WipSysConfig* cfg asm("s2")` instead emits a same-register
`la $s2` (`lui s2` / `addiu s2, s2`). Split around the same `jal` that
becomes `lui s2` in the delay slot and `addiu s2, s2` after the return —
`$v0` is never the hi temp. Leave the pointer unpinned so GCC still
picks `$s2` (it has to survive the call) but keeps the `$v0` expansion.

To get `move a0, id` *before* `move s0, v0` after a prior `jal`, keep
the return in a `$v0`-pinned temp and copy it after reloading `$a0`:

```c
register GpItemSlot* ret asm("v0");
register GpItemSlot* slot asm("s0");
register s32         a0id asm("a0");
WipSysConfig*        cfg;

ret  = func_800BAFE0(id);
a0id = id;
slot = ret;
cfg  = &Wip_SysConfig;
func_800BB0CC(a0id);
```

Pinning `cfg` here, or writing `slot = func_800BAFE0(id)` directly,
restores `lui s2` / `move a0` in the delay slot. `func_800D2538` is the
example.

## Free scratch through `G_SCRATCH_HEAD`, not a live pointer, so it re-`lui`s `$a0`

A `scratch = (void**)G_SCRATCH_HEAD` that is still live at
`*scratch = (u8*)*scratch + N` stays in a callee-saved register (`$s1`).
That steals `$s1` from an argument and adds a saved `$s2`. The target of
a short function that only needs `$s0` (block) and `$s1` (out-arg)
reloads the constant into `$a0` after the work:

```
bne   a1, v0, restore
 lui  a0, 0x1F80
...
ori   a0, a0, 0x3FC
lw    v1, 0(a0)
addiu v1, v1, 0x10
sw    v1, 0(a0)
```

Write the free through the symbol:

```c
*(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
```

`func_800EA1A8` is the example. The longer sibling `func_800EA02C` keeps
`*scratch += 0x10` because extra calls already force more `$s` regs, so
the saved pointer is cheaper than a reload.

## Hardcoded `1 << 4` folds to `li 0x10`; pin + barrier keep `li 1` / `sll 4`

`val = 1 << 4` (or `one = 1; val = one << 4`) constant-folds to
`addiu v0, zero, 0x10`. The target wants the unfolded form used by
`arg4 << 4` in `func_800B4114`:

```
addiu v0, zero, 1
sll   v0, v0, 4
```

Pin the 1 to `$v0` and block const-prop with an empty `+r` barrier:

```c
register s32 one asm("v0");

one = 1;
asm volatile("" : "+r"(one));
val = one << 4;
```

`func_800B3910` is the example.

## Pin the 4th call arg's base in `$a3` so `+ off` stays in the `jal` delay

`func_800B3448(ctx, i, 0, field_8 + (i << 4))` wants `addu a3, a3, t0` in
the `jal` delay and `move a2, zero` in an earlier `lw` delay. A named
`call_a3 = field_8 + off` emits the add too early and leaves `a2 = 0` as
the `jal` delay.

Pin the base to `$a3` and add only at the call. A `register s32 raw
asm("a2")` plus `asm volatile("" : "+r"(raw))` after saving `arg2` keeps
the first `field_20` index as `sll v1, a2, 2` instead of the saved `$s2`
copy:

```c
register s32 raw asm("a2");
register s32 off asm("t0");
register s32 f8 asm("a3");

raw = arg2;
off = arg1 << 4;
f8  = (s32)arg0->field_8;
asm volatile("" : "+r"(raw));
func_800B3448(arg0, arg1, 0, f8 + off);
```

`func_800B3910` is the example. `slot->field_20[arg2]` loads `field_20`
first; `(arg2 << 2) + (s32)slot->field_20` is what puts the shift before
the load.

## Do not hoist `one = 1` across a toast if/else

A prompt with two text arms (error string vs item name) wants a literal
`1` in the short arm and a named `one` only in the arm that reuses it
for several `Text_DrawPrompt` calls:

```
bne   v1, v0, else
 move a0, s1
jal   Ui_LookupTable
 li   a1, 1
...
li    v0, 1
sw    v0, 0x14(sp)
j     join
...
else:
jal   Ui_LookupTable
 li   a1, 1
...
li    s0, 1
sw    s0, 0x14(sp)
```

Hoisting `one = 1` before the `if` parks `1` in `$s0` and steals the
`bne` delay slot (`li s0, 1`), turns `Ui_LookupTable(obj, 1)` into
`move a1, s0`, and makes `if (obj->status == 1)` compare against the
live `one` (`lw v0, 0(s1)` / `bne v0, s0`) instead of
`lw s0` / `li v0, 1` / `bne s0, v0`. That last `$s0` load is also what
the later `Game_Session->field_66 == 1` reuses.

Set `one = 1` only in the reuse arm; leave a bare `1` in the other.
`func_800C70F0` is the example.

## Pin switch `tmp` to `$v0` so `table = tmp` is `move a3, v0`

The item-table switch already wants `tmp` then `table = tmp`. If `table` is
pinned to `$a3` (or naturally lives there), GCC coalesces the two and writes
each case straight into `$a3`:

```
addiu a3, v0, %lo(D_80114C20)
lw    a3, %lo(D_80114D70)(v0)
```

The target keeps every case in `$v0` and copies once at the join
(`addiu v0, %lo(...)` / `lw v0` / `move a3, v0`). Pin `tmp` to `$v0`:

```c
register GpItemRec* tmp asm("v0");
register GpItemRec* table asm("a3");

switch (scan->field_2) {
case 2:
    tmp = D_80114C20;
    break;
case 1:
    tmp = D_80114D70;
    break;
default:
    tmp = Mc_SaveData.field_1AC;
    break;
}
table = tmp;
```

`func_800B83F0` is the example.

## Write the join dest through `off + table`, not a `rec` temp

Two arms that join on a 4-byte struct store compute the dest in `$v0`
(`sll v0, idx, 2` in the taken-arm delay slot, then `addu v0, v0, table`).
Assigning that address to a walk pointer first moves it to `$v1`:

```c
rec  = (GpItemRec*)(off + (s32)table);
*rec = saved; /* addu v1, v0, a3 / swl 3(v1) */
```

Write the store through the computed address so dest stays in `$v0`:

```c
*(GpItemRec*)((arg2 << 2) + (s32)table) = saved;
```

`func_800B83F0` is the example.

## Occupancy walk: goto, not `while`, so the ptr step stays in the continue delay

Walking toward a hole (`if (rec->field_0 == 0) break; i--; if (src < i) rec--;`)
as a `while (rec->field_0 != 0)` rotates: the `lbu` moves to the bottom and
the bound check becomes `beqz` plus a compensating `i++`. The target checks
at the top and only steps the pointer when continuing:

```
lbu   v0, 0(v1)
beqz  v0, join
 slt   v0, i, dest
addiu i, i, -1
slt   v0, src, i
bnez  v0, loop
 addiu v1, v1, -4
```

Keep the check-then-continue shape with a goto so `rec--` / `rec++` lands
in the `bnez` delay slot:

```c
loop:
    if (rec->field_0 != 0) {
        i--;
        if (src < i) {
            rec--;
            goto loop;
        }
    }
```

`func_800B83F0` is the example.

## Preload `coord->sub` and the LCG addend so they take `$v0` / `$a0`

Identity-matrix setup that also writes `coord->sub = mem->field_8` wants
the parent pointer loaded *before* `ONE` is stored:

```
lw    v0, 8(s0)
li    v1, 0x1000
sw    v1, 4(s1)
sw    v0, 0x4c(s1)
```

Writing `*(s32*)&coord->coord = ONE` first emits `sw` then `lw`/`nop`.
Keep the load in a temp:

```c
parent               = mem->field_8;
one                  = ONE;
*(s32*)&coord->coord = one;
coord->sub           = parent;
```

A two-step `D_80070F60 * 5 + 0x71357911` that *adds* a spawn-arg nibble
onto the first roll needs that addend in `$a0` before the multiply, so
the first LCG lands in `$v1` and the second in `$a0` (both `sw`s delayed
until after `field_26`). Inlining `(u16)arg0->spawnArg1 & 0xFFF` into
the field store puts the addend in `$v1` and swaps the LCG registers.
Extract it first:

```c
temp          = (u16)arg0->spawnArg1 & 0xFFF;
mem->field_2A = 0;
D_80070F60    = D_80070F60 * 5 + 0x71357911;
mem->field_24 = temp + (((u32)D_80070F60 >> 16) & 0xFF);
D_80070F60    = D_80070F60 * 5 + 0x71357911;
mem->field_26 = ((u32)D_80070F60 >> 16) & 0xFFF;
```

`func_801005D8` is the example.

## Literal `1` hoists into `$s1` among `%hi` `lui`s; a named `one` does not

A pinned `register s32 one asm("s1"); one = 1;` assigned before the loop
emits `li s1, 1` immediately after the other pre-loop `li` (e.g. the
`0xFFFF` terminator), *before* hoisted `lui %hi(global)`s. The target
wants that `li` between the second and third `lui`:

```
li    s8, 0xffff
lui   s6, %hi(D_80114DDC)
lui   s4, %hi(D_80114DD0)
li    s1, 1
lui   s5, %hi(D_80114DC8)
```

Write the stores as a literal `1` (`D_80114DD0 = 1; D_80114DC8 = 1;`)
and do **not** introduce a `one` temp. GCC CSE's the constant into `$s1`
and places `li s1, 1` in first-use order among the hoisted `%hi`s.

Duplicate `D_80114DC8 = 1` in each arm of the remap (not once after the
join) so `%hi(D_80114DC8)` hoists into `$s5`. GCC still CSE's the stores
back to a single `sh` after the join.

`func_800B63B8` is the example.

## Seed a later `$s1` result as `Mem_Calloc`'s heap flag

When the target copies `$s1` into `Mem_Calloc`'s second argument (`addu a1,
s1, zero`) and later reuses that same register as a table-lookup result,
`Mem_Calloc(size, 0)` emits `move a1, zero` and leaves `$s1` free for
something else. Write:

```c
s32 val;

val = 0;
p   = Mem_Calloc(size, val);
/* … */
val = table[idx];
```

The zero-init pins `$s1` for the whole function. `func_800E9CC8` is the
example.

## Overlay jtbl not contiguous with the TU's other `.rodata` needs its own C file

Gameplay `.rodata` is a sequence of C jtbl slices interleaved with splat
`rodata` blobs. A new `switch` in an existing TU appends its jtbl to that
TU's `.rodata` slice. If a named table sits between the old jtbl and the
new one (here `D_80097678` between `func_800E9BDC` and `func_800E9CC8`),
keep the function in a sibling C file (`3CD8_9CC8.c`) and add matching
`.rodata` / `c` yaml cuts so the jtbl lands after the blob. Same pattern
as `3CD8_34D8.c` / `3FB8_75BC.c`.

## Isolate `ret = 1` from `* 10` so the shift stays `sll`, not `sllv`

`x * 10` expands to `(x << 2) + x` then `<< 1`. A nearby live `1`
(`ret = 1`, or extra `+ 1` in the same formula) CSEs into a register and
turns those `sll`s into `sllv`. Finish the multiply first, then barrier
before the default:

```c
tmp = t * 10 + 1;
asm volatile("");
t   = (s8)(n - quot * 3);
val = tmp + t;
t   = (val << 2) + val;
val = t << 1;
asm volatile("");
ret = 1;
if (n < 0xC) { … }
```

Split `(s8)quot + 1` into a temp so `* 10 + 1` can accumulate in `$v1`
while the s32 quotient stays in `$a0` for the later `n - quot * 3`.
`func_800A1F64` is the example.

## `table += n; ret = *table` keeps the `addu` dest on the pointer

`ret = table[n]` with `n` in `$a1` reuses the index: `addu a1, v0, a1` /
`lbu v1, 0(a1)`. The target wants `addu v0, v0, a1` / `lbu v1, 0(v0)`.
Advance the pointer, then deref:

```c
table += n;
ret    = *table;
```

## `+r` on a delay-slot store constant so its `li` precedes the jal arg

`p->field_0 = val` (val in `$a0`) then `p->field_3 = -2` then `jal f(3)`
wants:

```
sh    a0, %lo(D)(v0)
li    v0, -2
li    a0, 3
jal   f
 sb   v0, 3(s0)
```

GCC reuses `$a0` immediately after the store (`li a0, 3` then `li v0, -2`).
Hold the store value in a temp and barrier it before the store/call:

```c
p->field_0 = val;
neg        = -2;
asm volatile("" : "+r"(neg));
p->field_3 = neg;
temp       = f(3);
```

The empty `+r` keeps `-2` live so its `li` is emitted before the call
arg, while the `sb` can still fill the `jal` delay. Don't `register asm("v0")`
that temp at function scope: GCC 2.8.1 reserves the hard register for the
whole function and steals `$v0` from the earlier math. `func_800A1F64` is
the example.

## Two scratch aliases: halfword `+r` first, then `+r` the block pointer

A 0x28 scratch that is both an `SVECTOR*` (field stores, `Gfx_ApplyMatrixNoSf`,
GTE) and the allocation pointer (`*scratch = p`, `VectorNormalSS`) wants:

```
lhu    v0, 8(s0)
lhu    v1, 0(a3)
lw     s4, 0(s5)
subu   v0, v0, v1
addiu  s3, s4, -0x28
move   s2, s3
sh     v0, -0x28(s4)
```

One pointer CSE's to `addiu s2`. Pinning both `$s2`/`$s3` hoists the
`lw` above the `lhu`s and sets `a0` from `$s3`. Load the two halfwords,
barrier them, *then* form the pair and keep the block pointer live:

```c
srcx = arg1->pos.vx;
dstx = arg0->vx;
asm("" : "+r"(srcx), "+r"(dstx));
head  = *scratch;
block = (SVECTOR*)(head - 0x28);
vec   = block;
((SVECTOR*)(head - 0x28))->vx = srcx - dstx;
asm("" : "+r"(block));
```

`register SVECTOR* vec asm("s2")` is enough; do not pin `block`. Write
the first component through `head - 0x28` (not `vec->vx`) so the `sh`
stays head-relative. `func_800B6118` is the example.

## Force `addiu $v0, $sp, N` after `gte_SetRotMatrix`

`SVECTOR tmp = *src; gte_SetRotMatrix(mtx); gte_ldv0(&tmp)` hoists
`addiu v0, sp, 0x10` into the previous `jal` delay, before the
`lwl`/`lwr` copy. `asm("" : "+r"(tmpp))` after `tmpp = &tmp` still
computes the address early. Emit the addiu in the GTE sequence:

```c
tmp = *(SVECTOR*)(head - 0x28);
gte_SetRotMatrix(mtx);
__asm__ volatile("addiu %0, $sp, 0x10" : "=r"(tmpp));
gte_ldv0(tmpp);
```

The offset is the stack slot of `tmp` (saved-reg frame: `0x10` when
`s0`–`s7`/`ra` start at `0x18`). `func_800B6118` is the example.

## Keep the loop result in `$a0` until the first `li a0, 1`

A scan that writes `found` only on a hit, then compares `status` against
`1`, wants the result in `$a0` through the loop and only then in `$s0`:

```
lw    v1, 0(s3)
move  s0, a0
li    a0, 1
sra   v0, v1, 16
beq   v0, a0, ...
```

Assigning `item = 0` before the loop parks it in `$s0` and drops the
`move`. Use two names: `found = i` (stays in `$a0`), then after
`status = obj->status` write `item = found` so the copy is the delay of
that `lw`. `func_800C9BE8` is the example.

## `do { } while (0)` + `goto` loop + `break` for the found path

A `for (i = 0; i < n; i++, p++)` after `if (n != 0)` still emits an
initial `slt` / `beqz`. A `do { ... i++; p++; } while (i < n)` unrolls
the first iteration and fills inner delay slots with `i++` / `i--`.

Wrap a `goto` loop in `do { } while (0)` and `break` out on the hit:

```c
if (count != 0) {
    n = count;
    do {
    loop:
        if (match) {
            found = id;
            break;
        }
        i++;
        p++;
        if (i < n) {
            goto loop;
        }
    } while (0);
}
```

No initial `slt`, no unroll, and `if (remaining < 0) break` stays
`bltz` to an out-of-line `move a0, t0` / `j` join. `func_800C9BE8` is
the example.

## Volatile `sll` so a scaled add can run even when the count is 0

`table = rec + idx` inside `if (count != 0)` puts `sll` in the `beqz`
delay and leaves a `nop` after `lbu count`. The target scales first,
then branches, then `addu` (the delay slot always runs):

```
lbu    v1, idx
lbu    t0, count
sll    v1, v1, 2
beqz   t0, skip
 addu  a2, v0, v1
```

Load `idx`, load `count`, emit the shift as `asm volatile`, then add
the already-scaled byte offset outside the `if`:

```c
asm("lbu %0, %%lo(scan)(%1)" : "=r"(idx) : "r"(hi));
count = scan->field_1;
asm volatile("sll %0, %0, 2" : "+r"(idx));
table = (volatile GpItemRec*)((s32)rec + idx);
if (count != 0) {
    n = count;
    ...
}
```

The volatile `sll` fills the `lbu count` delay and sits before `beqz`.
The integer add is the always-executed `addu` after the branch (same
bytes as a delay-slot add when nothing else claims that slot).
`func_800C9BE8` is the example.

## Assign an inlined helper result to a field, not a local across a call

`task = helper(); if (p->next) Kill(p->next); p->cur = task;` copies the
helper result into a callee-saved (`$s0`) so it survives `Kill`, then
stores later. The target keeps the result in `$v0` (`beqz s1` /
`move v0, zero`, then `move v0, s1`) and puts `sw v0, field` in the
next branch delay slot.

Assign the helper directly to the field. GCC 2.8.1 then treats `$v0` as
a value that must be stored before the clobbering call, and can lift
that store into the `beqz` delay slot of the following `if`:

```c
p->cur = helper(p, 0, a, b);
if (p->next != NULL) {
    Kill(p->next);
}
p->next = helper(p, 1, a, b);
```

`func_80103294` is the example.

## `register ... asm("v1")` steals anonymous `mflo`; assign the last square into that pin

A first-half pin such as `register s32 b asm("v1")` keeps `$v1` out of
anonymous temp allocation for the rest of the function. Three
`x * x` products then land in `$a2/$a1/$a0` as wanted, but the fourth
(`rsum * rsum`) goes to `$a3` instead of the target's `$v1`.

Compute the three squares into named dests first (`dx` in `$v0` as the
common source so each is `lw v0; mult v0,v0; mflo dest`), then write
the compare as `sum < (b = rsum * rsum)` so the last `mflo` reuses the
pin. A memory barrier after `delta.vx = dx` keeps that store *before*
`bgez` (not in the delay slot). On a hit, replay the call-arg and
field setup in target order (`a0`/`a1`, truncated `pos1`, `a2 = block`,
`rsum32`, `ret = 1`, zeros) so `li s5, 1` sits after `lhu a3`.
`func_800DBCAC` is the example.

## Split `done` / `ret` so fill jumps to `move $v0` and found jumps to `jr ra`

A leaf that returns a pointer kept in `$t2` wants two epilogue entry points:

```
bnez  t3, ret
 move  v0, t2
...
j     done
 sb    zero, 1(t2)
done:
move  v0, t2
ret:
jr    ra
 nop
```

`return dest` after `result = dest` becomes one `jr ra` / `move v0, t2` tail, and a second `if (found) return result` grows its own `jr ra`. Split the labels:

```c
result = dest;
if (found != 0) {
    goto ret;
}
...
goto done;
done:
    result = dest;
ret:
    return result;
```

`result = dest` before the branch fills the `bnez` delay; fill / empty-count `goto done` land on the `move`; found skips it.

A later `i = 0` then copy-props from the now-known-zero `found` (`move t0, t3`). Kill that equality with a non-volatile empty asm so the constant 0 uses `$zero`:

```c
asm("" : "+r"(found));
i = 0;
```

`volatile` parks a `nop` in the `beqz count` delay instead of `move t0, zero`. Put a `goto fill` target *above* the non-stacking loop so the empty-slot body is a backward `beqz` between an early `jr ra` and the loop setup. `func_800B8CAC` is the example.




## Index `(&global)[i]` so the dest `la` hoists with other loop invariants

A named dest pointer assigned before the loop is scheduled *before* the
loop's hoisted `%hi`s:

```
lui    v0, %hi(table)
addiu  t1, v0, %lo(table)
lui    v0, %hi(dest)      /* too early */
addiu  a1, v0, %lo(dest)
lui    t0, %hi(halfword)
lui    a3, %hi(out)
lui    a2, %hi(other)
```

The target wants the dest address *after* those `%hi`s, grouped with
the other loop-invariant addresses, then incremented in the branch
delay (`addiu a1, a1, 4`).

Don't keep a dest pointer. Write the store as an index of the global
so the address is a loop invariant, not an early named assignment.
`-O2` strength-reduces it back to a pointer increment:

```c
scans = D_8010D550;
do {
    if (i == 0) {
        item       = D_80114DDC;
        src        = scans[item & 0xFF];
        D_80114D7C = item;
    } else {
        src = &Mc_SaveData.field_5BC;
    }
    (&D_8010D628)[i] = *src;
    i++;
} while (i < 2);
```

`func_800BCC44` is the example. `dest = &D_8010D628; *dest = *src;
dest++` stuck at 98.6% with only that `la` two instructions early.

## Pin fail `-1` and shared `0x34` through `$v0` so the store is a phi

A fail path that writes `field_2E = -1` then shares `field_2C = 0x34`
with the success tail wants:

```
li    v0, -1
sh    v0, 0x2e(s3)
li    v0, 0x34
j     store
 sw    zero, 0(s3)
...
li    v0, 0x34
store:
sh    v0, 0x2c(s3)
```

A named `s32 code` without a pin takes `$v1` and is hoisted *before*
the `-1` store. `obj->field_2C = 0x34` on both arms does not share the
`sh`. Route both constants through one `$v0` pin so the fail path
overwrites the same register after the first store:

```c
register s32 code asm("v0");

if (mem == NULL) {
    code          = -1;
    obj->field_2E = code;
    code          = 0x34;
    obj->status   = 0;
    goto end;
}
...
code = 0x34;
end:
obj->field_2C = code;
```

`func_800BCC44` is the example. Unpinned `code` stuck at 98.8% (`li v1,
0x34` first, then `li v0, -1`).

## Materialize `x * 4 - C` before adding a second index so `-C` stays on `$v0`

`table[x * 4 - 5 + y]` reassociates as `x * 4 + (y - 5)` (`addiu v1, v1,
-5`) or folds `-5 * sizeof(ptr)` into the load (`lw v0, -0x14(v0)`).
The target wants the subtract on the scaled `x`:

```
sll    v0, v0, 2
addiu  v0, v0, -5
addu   v0, v0, v1
```

Load `x` and `y` first, then split the math so `-5` cannot move onto `y`:

```c
idx  = (s8)actor->field_987;
temp = Wip_SysConfig.field_26;
idx  = idx * 4 - 5;
idx  = idx + temp;
img  = table[idx][(s8)actor->field_989];
```

Also reuse the scratch-allocation temp (`register s32 temp asm("v1")`)
as `y`. Leaving that register bound to the allocated `RECT*` lets copy
prop pass `$v1` as the call's third arg (`move a2, v1`) and steals `$v1`
from the `Wip_SysConfig` / frame-index loads. Overwriting it with `y`
kills the RECT copy and frees `$v1` for `lb` / `lbu`. Pin the table `la`
to `$a0` so it is not colored as the eventual `img` in `$a1`.
`func_801030CC` is the example.

## Unpack RGB555 through stored `u16` g/b so reloads are `lhu` / `srl`

A CLUT lerp that splits a 16-bit color into three 5-bit channels
shifted left 7 wants the green/blue extracts to reload the stored
copies, not reuse the original color register:

```
lhu    v0, 0(a0)
sh     v0, 4(dst)          # b = color
sh     v0, 2(dst)          # g = color
andi   v0, v0, 0x1F
sll    v0, v0, 7
sh     v0, 0(dst)          # r = (color & 0x1F) << 7
lhu    v0, 2(dst)
lhu    v1, 4(dst)
sll    v0, v0, 2
andi   v0, v0, 0xF80       # g = (g << 2) & 0xF80
srl    v1, v1, 3
andi   v1, v1, 0xF80       # b = (b >> 3) & 0xF80
```

`SVECTOR` (`s16`) reloads as `lh` / `sra`. Use a `u16` r/g/b struct.
Store `color` into `g` and `b` first so the later `g << 2` / `b >> 3`
are independent loads that `-fschedule-insns` can interleave. Pack
the same way so `out->r` is read after `v0` has been reused:

```c
color = *src;
dst->b = color;
dst->g = color;
dst->r = (color & 0x1F) << 7;
dst->g = (dst->g << 2) & 0xF80;
dst->b = (dst->b >> 3) & 0xF80;
...
packed = ((out->b >> 2) & 0x3E0) | ((out->g >> 7) & 0x1F);
packed = (packed << 5) | ((out->r >> 7) & 0x1F);
```

The STP bit is `if ((s16)*src0 < 0 || (s16)*src1 < 0) *dst = packed |
0x8000;` — GCC puts `ori packed, 0x8000` in both compare delay slots.
`func_800B2088` is the example.

## Memory barrier after stores so `li` fills the `beqz` delay, not `sh`

A default-in-delay-slot compare after an increment:

```
lhu    v0, field
lhu    v1, kind
sh     zero, other
addiu  v0, v0, 1
sh     v0, field
beqz   v1, join
 li    a1, DEFAULT
```

`mode = DEFAULT; if (kind != 0) mode = OTHER` after the stores lets
`-fschedule-insns` hoist `li a1` above `sh` and put the store in the
delay slot instead. An empty `asm volatile("" ::: "memory")` between
the store and the default assignment keeps `sh` before the branch so
`li` fills the delay. Pin the increment load to `$v0` and `kind` to
`$v1` so the two `lhu`s stay in that order. `func_8010747C` is the
example.

## Put a shared switch tail after later cases so `j` / `bnez` land on it

A 6-case jump table that places case 1 *after* case 3 (case 0 `j`s
there, case 3 `bnez`es there) wants case 1 later in the source:

```c
case 0:
    ...
    goto shared;
case 2:
    ... /* fallthrough */
case 3:
    if (cond) {
        goto shared;
    }
    ...
    break;
case 1:
shared:
    func_800DB500(2);
    break;
```

Fallthrough from case 0 into case 1 in source order glues the tail
onto case 0 (`jal` then `j epilogue`) instead of a `j` to a later
block. `func_8010747C` is the example.

## 3-way if/else-if stores keep `bnez; li K; j join`

When the target does:

```
lhu    v1, mode
nop
bnez   v1, else
 li    v0, 3
j      join
 li    v0, 0x78
else:
bne    v1, v0, join
 li    v0, 0x3C
li     v0, 0x14
join:
sb     v0, field
```

a shared temp (`val = 0x78 / 0x14 / 0x3C; field = val`) inverts to
`beqz` and drops the `j`. Store in each arm instead:

```c
mode = (u16)actor->field_958;
if (mode == 0) {
    actor->field_98D = 0x78;
} else if (mode == 3) {
    actor->field_98D = 0x14;
} else {
    actor->field_98D = 0x3C;
}
```

GCC coalesces the three stores back into one `sb` and keeps `bnez` +
`j`. `func_80109FC4` is the example.

## Jump-thread deletes `move v0, zero` when obj lives in `$v0`

An inner `if (obj != NULL) { flag = (obj->field_1 & 4) != 0; } else { flag = 0; }`
with `obj` loaded into `$v0` wants:

```
lw     v0, 4(v0)
beqz   v0, else
 nop
lbu    v0, 1(v0)
andi   v0, v0, 4
j      join
 sltu   v0, zero, v0
else:
move   v0, zero
join:
```

GCC 2.8.1 jump-threads the `beqz` to `join` and deletes `move v0, zero`
because a NULL pointer is already 0. The then-path `j` then also
disappears. Force the else to materialize with the same zero-in-register
constraint used in `mc.c`:

```c
} else {
    asm volatile("" : "=r"(flag) : "0"(0));
}
```

`func_800AE9B0` is the example.

## Split `la` of a function pointer so `%hi` lands in `$v1` while `$v0` holds a `lhu`

Taking `func = Tmd_AllocBuffers` with `func` pinned to `$s2` emits the
same-register form (`lui s2` / `addiu s2, s2`). The target wants the
two-register form so `%hi` can sit in `$v1` while `$v0` stays live as
the `lhu` of a nearby `u16` field:

```
lui    v1, %hi(Tmd_AllocBuffers)
lhu    v0, field_C(s1)
addiu  s2, v1, %lo(Tmd_AllocBuffers)
ori    v0, v0, 0x80
```

Dest `$s2` would normally use `$v0` as the `lui` temp, but `$v0` is
busy, so GCC falls back to `lui s2`. Split with a non-volatile pair and
read the field between them:

```c
register s32 hi asm("v1");
register void (*func)(TmdObject*) asm("s2");
u16 flags;

asm("lui %0, %%hi(Tmd_AllocBuffers)" : "=r"(hi));
flags = extra->field_C;
asm("addiu %0, %1, %%lo(Tmd_AllocBuffers)" : "=r"(func) : "r"(hi));
extra->field_C = (flags | 0x80) & 0xFFFB;
```

`func_80104684` is the example.

## Copy into `$v0` before `<< 16` so the shift is `move` / `sll`, not `sll` from `$a2`

A 0/1 flag in `$a2` that is then tested as `(s16)flag` (i.e. `sll 16` /
`blez`) wants:

```
move   v0, a2
sll    v0, v0, 0x10
blez   v0, fail
```

`shifted = found << 16` with `shifted` pinned to `$v0` emits `sll v0, a2,
0x10`. Copy first, then a `+r` barrier so `-fschedule-insns` cannot fold
the copy into the shift:

```c
register s32 shifted asm("v0");

shifted = found;
asm volatile("" : "+r"(shifted));
shifted = shifted << 16;
if (shifted > 0) {
```

A `for (i = 0; i < 3; i++)` over `((u8*)((s32)row + i))[1]` keeps two
long-lived pointers (`arg2`, a table) in `$s3`/`$s4`. The equivalent
`goto` loop swaps those two s-registers. `func_800B6EE0` is the example.

## Leave `p = &global` unpinned so `%hi` is `$v0` and fills the incoming `bne`

A merge reached from `bne key, C, increment` plus a fall-through from a
nested range check wants:

```
bne    s2, v0, inc
 lui    v0, %hi(Mc_SaveData)
lw     v0, Game_Session
...
bnez   in_range, skip
 lui    s0, %hi(scan)
lui    v0, %hi(Mc_SaveData)
addiu  a0, v0, %lo(Mc_SaveData)
```

`register McSaveData* save asm("a0"); save = &Mc_SaveData;` delay-fills
with `lui a0` / `addiu a0, a0`. An unpinned local plus the skip-goto
CFG lets `-fdelayed-branch` copy `lui v0, %hi` into the `bne` delay
and keep `addiu a0, v0, %lo` at the merge:

```c
if (key == 0x50B0000 || key == 0x51D0000) {
    if (Game_Session->field_9 - 1 < 3U) {
        goto skip_count;
    }
}
save = &Mc_SaveData;
if (save->field_6CC < 0x270FU) {
    save->field_6CC++;
}
skip_count:
```

A later straight-line `&Mc_SaveData` (no incoming `bne` to fill) still
needs the split `lui $v0` / `addiu $a0` pair from the D_8010D230 note,
so `flags = 0` can sit between `addiu` and `lhu`. Pinning one `UiObject*`
across two states also merges them into `$s0`; the state-3 path wants
the pointer already in `$a0` for `Ui_TeardownTree`. `func_800A110C` is
the example.

## Finish `idx * sizeof` before loading the array base so the last `sll` sits above `lw` / `nop`

`dest = &arr[idx]` with `sizeof == 0x50` is `sll 2` / `addu` / `sll 4` /
`lw base` / `addu`. `-fschedule-insns` hoists the independent `lw` between
the last two shifts (`lw v1` / `sll v0, 4` / `addu`, no load-delay `nop`).
The target keeps the multiply intact:

```
sll    v0, v1, 2
addu   v0, v0, v1
sll    v0, v0, 4
lw     v1, 4(a0)
nop
addu   s0, v1, v0
```

Materialize the byte offset, then an input-only empty asm, then the
array index (CSE reuses the offset):

```c
off = idx * 0x50;
asm volatile("" :: "r"(off));
dest = &((GpAnimMtxRec*)arg0->field_4)[idx];
```

`+r`(off) instead of `"r"(off)` copies `v0` to `v1` so the `lw` can take
`$v0`. `func_800B4248` is the example.

## Pin scratch `head` to `$t1` and the `head-N` block to `$t0` when both stay live

A downward scratch alloc that later reads both the original head
(`lh -0x10(t1)`, `addiu a0, t1, -8`) and the allocated block (`lh 2(t0)`)
wants:

```
lw     t1, 0(s1)
addiu  t0, t1, -0x10
sw     t0, 0(s1)
```

Unpinned, `head` takes `$t0` and the alloc takes `$t1`, so the field
reads fold as `lh -0xE(t0)` / `lh -0xC(t0)` instead of `lh 2(t0)` /
`lh 4(t0)`. Pin both:

```c
register void*    head asm("t1");
register SVECTOR* trans asm("t0");

head  = *scratch;
trans = (SVECTOR*)((u8*)head - 0x10);
*scratch = trans;
```

`func_800B4248` is the example.

## `gte_stsv` dest in `$a0` copy-props into later field reads; split the SSA

`gte_stsv(st)` with `st = trans` in the `bne` delay (`move a0, t0`)
makes later `trans->vy` / `trans->vz` use `$a0` (`lh 2(a0)`) even when
`trans` is pinned to `$t0`. The target uses the original alloc pointer
(`lh 2(t0)` / `lh 4(t0)`); `vx` still folds as `lh -0x10(t1)`.

Empty asm that rewrites `trans` after the `vx` store blocks the
copy-prop and emits no instruction when in/out stay in `$t0`:

```c
register SVECTOR* st asm("a0");
register SVECTOR* trans asm("t0");

st = trans;
gte_stsv(st);
dest->mtx.t[0] = trans->vx;
asm volatile("" : "=r"(trans) : "r"(trans));
dest->mtx.t[1] = trans->vy;
dest->mtx.t[2] = trans->vz;
```

`func_800B4248` is the example.

## Combined `buf[0x20]` + `TextDrawReq` so later stores go through `$a0`

A second text-draw that prints into a stack buffer wants:

```
addiu  a0, sp, 0x28
addiu  v0, v0, 1
sw     v0, 0x4C(sp)      # req.otIndex via $sp
li     v0, 5
sb     v0, 0x2C(a0)      # req.glyphTable via buf
sw     v1, 0x28(a0)      # req.field_8
sb     v0, 0x2D(a0)      # req.centerMode
jal    Text_ItoaSigned
 sb    zero, 0x56(sp)    # req.field_E via $sp
```

Separate `u8 buf[0x20]; TextDrawReq req2;` emits every later field as
`0x5x(sp)`. Put them in one struct, pin the struct pointer to `$a0`,
and mix named vs pointer stores: `draw.req.otIndex` / `draw.req.field_E`
stay `$sp`-relative, `d->req.glyphTable` / `field_8` / `centerMode` use
`$a0+0x28/0x2C/0x2D`.

Keep the item id live (`asm volatile("" :: "r"(item))`) so a later
`(u32)(item - 0xA0) < 0x20U` is `addiu v0, s3, -0xA0` / `sltiu v0`
instead of clobbering `$s3` in place. Split the last-block `0x606060`
into a new `$v1` temp so `lui v1, 0x60` fills that `beqz` delay.
`func_800CCEEC` is the example.

## D4 overlay: hold `lhu` until after `tile` when `$a1` is reused

The fade-overlay `lui a1, %hi(CdCmd_Queue)` / C `lhu` pair from
`func_800AADDC` hoists the load past the TILE / `DR_TPAGE` address math
when the next block also needs `$a1` (another `lui a1` for a BSS
halfword). The C load then consumes `$a0` too early (`sll a1` instead
of `sll a0`, and `addu t2` can no longer fill the `bnez` delay).

Depend the load on the finished `tile` pointer with a non-volatile
empty asm so `lhu` sits after `addiu %lo(D_80114CA0)` and `addu t2`
stays in the delay slot:

```c
tile = &D_80114C80[buf];
asm("" : : "r"(qhi), "r"(tile));
dr     = &D_80114CA0[buf];
queued = *(u16*)((s32)qhi + (s16)0x91C4);
```

Depending on `dr` as well emits `addu t2` *before* the load. Input
constraints on the `lui` (`"r"(color), "r"(ds)`) keep `li a2, 8` /
`la Display_State` ahead of that `lui`.

A later 0/1 flag that is consumed as `if (x & 0xFFFF)` becomes `sltu`
if written `if (func()) x = 1; else x = 0`. Assign the masked return
into a `$v0`-pinned temp, keep the success `x = 1; break;`, and fall
through to the shared `x = 0` (same label as the previous switch arm):

```c
register s32 done asm("v0");

case 2:
    done = func() & 0xFFFF;
    if (done) {
        done = 1;
        break;
    }
default:
    done = 0;
    break;
}
if (done & 0xFFFF) {
```

`func_800AB5F4` is the example.

## Fill CdCmd_Enqueue arg `addiu`s between session-field `lbu`s

A 0x21 enqueue that copies `GameSession.field_7/6/5` into a stack
payload wants `&param1` / `&param2` in `$a1` / `$a2` *between* each
load and store:

```
lw     v1, Game_Session
li     a0, 0x21
lbu    v0, 7(v1)
addiu  a1, sp, 0x10
sb     v0, 0x13(sp)
lbu    v0, 6(v1)
addiu  a2, sp, 0x18
sb     v0, 0x12(sp)
lbu    v1, 5(v1)
li     v0, 1
...
jal    CdCmd_Enqueue
 sb    v1, 0x11(sp)
```

Assign the call args as named temps (`cmd = 0x21; p1 = param1;`)
immediately after each field load so those `addiu`s fill the load
delay. The last field must overwrite the session pointer in `$v1`
(`lbu v1, 5(v1)`) and stay there until the `jal` delay: pin both the
session pointer and that byte to `$v1`. Loading it into `$v0` instead
stores `param1[2]` late and moves `&param2` after the zeroing stores.

`func_800AB1C8` is the example.

## `idx + table` so GCC emits `addu dest, dest, base`

`*(s32*)(table + (idx & 0xFFFC))` canonicalizes to `addu v0, a0, v0`
(base + offset). The TMD SZ-table loads want the offset register as
both dest and first operand (`andi v0, 0xFFFC` / `addu v0, v0, a0`).
Stage the mask into a named `idx` and add the table onto it:

```c
idx = rec[0] & 0xFFFC;
sz  = *(s32*)(idx + (s32)szTable);
```

`func_80099B94` is the example.

## Keep `&poly->x2` as a live `POLY_F3*` so both cursors `addiu 0x14`

A pre-transformed POLY_F3 OT insert loads SXY via the SXYP FIFO from
`x0`/`x1`/`x2` (`lw -8/-4/0` / `mtc2 $15`). Taking `xy = (POLY_F3*)&poly->x2`
once before the loop and incrementing both `xy` and `poly` keeps

```
addiu  a3, a3, 0x14
addiu  t0, t0, 0x14
```

Assigning `xy` only once without incrementing it (or recomputing
`&poly->x2` each iteration) either freezes the SXY pointer or emits
`addiu a3, t0, 0x10`. Pin `poly` to `$t0` and `xy` to `$a3`.
`func_80099B94` is the example.

## POLY_F4 SXY FIFO from `&x3` plus `goto draw` for the 4th-vertex nclip

The quad sibling of the POLY_F3 OT insert (`func_80099B94`) points `xy`
at `&poly->x3` so the first nclip loads `x0`/`x1`/`x2` as `lw -12/-8/-4`
/ `mtc2 $15`. A 4th `lw 0(xy)` / nclip only runs when the first OPZ
is `<= 0`; `if (opz > 0) goto draw;` then `if (opz < 0) { draw: ... }`
emits `bgtz` to the SZ/OT body and `bgez` over it. Pin `opz` to `$t1`
and `mask` to `$t2` (swapped vs the F3 handler). Do not name the SZ3
load `gte_ldsz3` — PsyQ already uses that for the 3-arg SZ1/SZ2/SZ3
macro. `func_80099994` is the example.

## Delay-slot copy plus in-`if` `asm("" : "+r"(n))` so `i = n` is not CSE'd

`n = count; if (cond) { i = n; }` wants `n = count` in the `beq` delay
(`move s3, s5`) and `i = n` as `move s2, s3` after the following
`lw a2, stack`. Copy-propagating `i = n` to `count` emits `move s2, s5`.
A volatile `"+r"(n)` *before* the `if` forces the copy early and leaves
the delay for a hoisted `li v0, 1`. Put a non-volatile empty asm *inside*
the `if` so the delay-slot copy stands and `i = n` uses `$s3`:

```c
n = count;
if (mode != 2) {
    asm("" : "+r"(n));
    i = n;
    off = (arg1 - 0x80) * 4;
```

`func_800C942C` is the example.

## Pin a late `addiu` to a just-loaded value so it cannot hoist

`y = temp + 0x58` is independent of the third `TextDrawReq` setup, so
`-fschedule-insns` computes it in the first delay after the previous
call (`addiu s3, s0, 0x58` before `lhu baseX`). The target wants that
add in the `lh drawOrder` delay, with `lui`/`%lo` of the string filling
the earlier loads.

Load `drawOrder` into a named temp first, then emit the add as a
non-volatile asm that lists `draw` as an input. The data dependence
keeps the `addiu` after the `lh` without a `volatile` barrier, so `la`
can still split across `req.x` / `req.y`:

```c
draw = (s16)obj->drawOrder;
asm("addiu %0, %1, 0x58" : "=r"(y) : "r"(temp), "r"(draw));
req.otIndex = draw + 1;
```

`y = temp + 0x58` plus `asm("" : "+r"(temp) : "r"(draw))` still hoists,
or else pulls an earlier `lh field_18` out of its `lhu baseX` delay.
`asm volatile` parks the add correctly but inserts `nop`s and delays
`la`. `func_800D3D98` is the example.

## Two spilled offset locals for two `i + off + table` loops

A pair of loops that both compute `(id - 0x80) * 4` then
`((GpItemQty*)(i + off + (s32)table))->field_1` will keep `off` in `$s8`
(clobbering a live scan pointer) or reuse one stack slot if they share
one local. Separate `off` / `off2` gives `sw 0x10(sp)` / `sw 0x14(sp)`
and the `lw a2` / `nop` / `addu v0, s2, a2` form. `func_800C942C` is the
example.

## Split `lui`/`lbu` of a global byte around an `lhu` from a live `$v0`

After `addr = &table[idx]` in `$v0`, the next global byte wants

```
addu   v0, v0, a0
lui    a0, %hi(D_byte)
lhu    v1, 0(v0)
lbu    a0, %lo(D_byte)(a0)
sll    v1, v1, 1
```

A C `row = D_byte` after the `lhu` emits `lhu` first, then `lui v0` (the
address is dead so `$v0` is the lui temp). A C `row = D_byte` *before*
the `lhu` either clobbers the table pointer in `$a0` or puts `lbu`
before `lhu`.

Split the byte load so `%hi` depends on `addr` (keeps `$v0` live, dest
`$a0`) and `%lo` depends on the halfword (so `lhu` sits between them).
`col <<= 1` is independent of `$a0` and wants to fill the `lbu` delay;
`-fschedule-insns` will still start `row * 20` first and insert a `nop`.
An empty `asm volatile("")` after the shift parks it:

```c
addr = (s32)&cols[hp / 10];
asm("lui %0, %%hi(D_8011541B)" : "=r"(row) : "r"(addr));
col = *(u16*)addr;
asm("lbu %0, %%lo(D_8011541B)(%1)" : "=r"(row) : "r"(row), "r"(col));
col <<= 1;
asm volatile("");
col += row * 20;
```

`func_800E2438` is the example.

## Force unsigned `/ 100` `mfhi` into `$v1` with early `mflo`

`(u32)x / 100` is `multu` / `mfhi` / `srl 5`. When `$v1` is reserved by
`register ... asm("v1")` (or the dest is a different pinned reg such as
`$a1`), the umulh temp becomes `$t2`:

```
mult   a1, v0
mflo   t2
lui    v0, 0x51eb
ori    v0, v0, 0x851f
multu  t2, v0
mfhi   t2
srl    a1, t2, 5
```

The target reuses `$v1` for `mflo` and `mfhi`. Assign the product to the
`$v1` pin, force `mflo` before the magic `lui` with `asm volatile("" :
"+r"(prod))`, load `0x51EB851F` into `$v0`, then emit `multu`/`mfhi`
with asm so `mfhi` clobbers `$v1`. `(u32)hi >> 5` is `srl`; a signed
`>> 5` is `sra`:

```c
col = val * scale;
asm volatile("" : "+r"(col));
mag = 0x51EB851F;
asm volatile("multu %0, %1" : : "r"(col), "r"(mag));
asm volatile("mfhi %0" : "=r"(col));
val = (u32)col >> 5;
```

`func_800E2438` is the example.

## Keep a 2D `/3` index from folding, and schedule signed-`/3` magic before the dividend

`unknown_850[i % 3 + (i / 3) * 3]` is algebraically `unknown_850[i]`. GCC
2.8.1 folds it and emits only one signed `/ 3` (`0x55555556`). Expanding
the remainder off a copy of the second quotient, then blocking the fold
with a volatile asm, restores the target's two-divide form:

```
mult   a1, v0          /* n / 3 */
...
mult   v1, v0          /* (n / 3) / 3 */
move   a0, a2
subu   a2, v1, a0*3    /* (n / 3) % 3 */
subu   a1, a1, v1*3
addiu  a1, a1, 1       /* n % 3 + 1 */
...
addu   v0, a2, a0*3    /* reconstructed index */
```

```c
register s32 n asm("a1");
register s32 i asm("v1");
register s32 row;
register s32 col;

i    = (arg2 - 0xF) / 3;
n    = arg2 - 0xF;
col  = i / 3;
row  = col;
col  = i - row * 3;
save = &Mc_SaveData;
asm volatile("" : "+r"(row), "+r"(col), "+r"(i), "+r"(n));
n = n - i * 3 + 1;
if (save->unknown_850[col + row * 3] < n) {
    save->unknown_850[col + row * 3] = n;
}
```

`i = (arg2 - 0xF) / 3` *before* `n = arg2 - 0xF` (n pinned to `$a1`) CSEs
the dividend into `$a1` and emits `lui`/`ori` of the `/3` magic *then*
`addiu a1, s1, -0xF`. Assigning `n` first puts the `addiu` above the
magic. `save = &Mc_SaveData` in the same block as the second `/3` fills
that `mult`'s latency with `addiu s0, s0, -0x5BC` (from
`&Mc_SaveData.field_5BC` back to the struct base). A volatile asm after
the `% 3` of `i` keeps `n % 3 + 1` from sliding into that same slot.

`func_800CC15C` is the example.

## `x += shift; store x` so `addu` writes the addend, not the sum temp

`dest = x + (prod >> 12)` with `x` in `$v1` and the shift in `$v0` emits
`addu v0, v1, v0` / `sh v0`. The target keeps the sum in `$v1`:

```
sra    v0, t2, 12
addu   v1, v1, v0
sh     v1, dest
```

Update the addend, then store it:

```c
x += prod >> 12;
block->src[0].vx = x;
```

`func_800DDC2C` is the example.

## Pin a loop `%hi` with volatile `lui` so it sits between `i = 0` and the other inits

A loop that reloads `D_sym` wants:

```
move   t0, zero
lui    t1, %hi(D_sym)
move   a2, s1
li     a3, 0x20
...
lw     a1, %lo(D_sym)(t1)
```

`p = D_sym` inside the loop hoists `%hi` to the preheader *after* every
C init (`out` / `off`). A second `p = D_sym` also emits a second `lui`.
Split the address: emit `%hi` after `i = 0` with a fake dep on `i`, then
load through that register and an independent field so the `%lo` delay
is filled (not `nop`):

```c
i = 0;
asm volatile("lui %0, %%hi(D_80115448)" : "=r"(hi) : "r"(i) : "memory");
out = block->pos;
off = 0x20;
...
asm("lw %0, %%lo(D_80115448)(%2)\n\tlw %1, 68(%3)"
    : "=r"(p), "=r"(t) : "r"(hi), "r"(block));
```

The paired `lw 68(block)` is `block->mat.t[0]` (offset 0x44). Volatile
`lui` plus `memory` keeps it from sinking below `out` / `off`.
`func_800DDC2C` is the example.

## Hoist `&field_24` and AND `0x80000000` for TMD FLAG clip (not `>= 0`)

`func_8009D388` (POLY_FT3) tests FLAG with `if (ws->field_24 >= 0)` and
computes `&ws->field_24` inside the loop (`addiu v0, a3, 0x24` after
RTPT). The POLY_F3 sibling wants:

```
addiu  t5, a3, 0x24
lui    t8, 0x8000
addiu  t3, a3, 0x28
```

then `and v0, v0, t8` / `bnez` after each `gte_stflg`. A signed compare
emits `bltz` and leaves `&field_24` unhoisted. Name both the flag pointer
and the mask, and store FLAG a second time after `gte_stsxy3_f3`:

```c
flg      = &ws->field_24;
clipMask = 0x80000000;
opz      = &ws->field_28;
...
gte_stflg(flg);
if ((ws->field_24 & clipMask) == 0) {
    gte_nclip_real();
    gte_stopz(opz);
    if (ws->field_28 > 0) {
        gte_stsxy3_f3(poly);
        gte_stflg(flg);
        if ((ws->field_24 & clipMask) == 0) {
```

`setlen` 4 / `setcode` 0x20 / poly size 0x14. `func_8009DB00` is the
example. Same prologue is used by `func_8009D518` / `func_8009D718` /
`func_8009D900`.

## Quad TMD: shared 4th-vertex draw, `gte_avsz4_real`, pin `mask` to `$t1`

`func_8009D718` is the POLY_GT4 sibling of `func_8009DB00`. After the
hoisted FLAG/clip/`opz` prologue it RTPT-clips the first three verts,
`nclip`s, stores SXY into the GT4 (`gte_stsxy3_gt4`) unconditionally,
then RTPS-clips vertex 3. The first `nclip` result is reused: if
`field_28 > 0` draw, else `nclip` again on (v0,v1,v3) and draw only when
that MAC0 is `< 0`. Both paths share one `gte_stsxy2(&poly->x3)` /
`gte_avsz4_real` / OT block; a `goto` into the `< 0` body emits
`bgtz` / delay-slot `addiu v0, t0, 0x2C` like the target. No
`setlen`/`setcode` (another handler fills the packet).

`gte_avsz4_real` is `nop; nop; .word 0x4B68002E` (cf. avsz3
`0x4B58002D`). Poly stride 0x34.

Without the extra `t1 = poly+7` that the setcode siblings keep, GCC
colors `opz` as `$t1` and `mask` as `$t2`. Pin the mask so `opz` falls
into `$t2`:

```c
register u32 mask asm("t1");
...
if (ws->field_28 > 0) {
    goto draw;
}
gte_nclip_real();
gte_stopz(opz);
if (ws->field_28 < 0) {
draw:
    gte_stsxy2(&poly->x3);
    gte_avsz4_real();
    gte_stotz(opz);
    /* OT link */
}
```

`func_8009D518` (POLY_FT4) / `func_8009D900` (POLY_F4) are the same
shape with different SXY offsets and a live `poly+7`. Those
`setlen`/`setcode` siblings already occupy `$t1` with `poly+7`, so
unpinned coloring puts `opz` in `$t2` and `mask` in `$t3`. Pin the
mask to `$t2` instead so `opz` falls into `$t3`:

```c
register u32 mask asm("t2");
```

`func_8009D900` is POLY_F4: `gte_stsxy3_f4`, `setlen` 5 / `setcode`
0x28, stride 0x18, `gte_stsxy2` at `&poly->x3` (offset 0x14).

## Relative matrix: reuse `$a0` as 0x30 scratch, pin after the overwrite

`func_800A8864` is `arg2 = inverse(arg0) * arg1` for rigid transforms:
transpose the parent rotation into scratch (same `t4`/`t5`/`t6` halfword
pattern as `Gfx_TransposeRot`), `gte_MulMatrix0_real` into `arg2`, then
`ApplyMatrixLV` of `child.t - parent.t`. Splat tags it "Handwritten"
because of COP2; the C is still GCC 2.8.1.

The target copies the incoming parent out of `$a0` immediately:

```
move   a3, a0
addiu  a0, t0, -0x30
sw     a0, 0(s0)
lhu    t4, 0(a3)     /* not 0(a0) */
```

`src = arg0; tmp = (Scratch*)(head - 0x30)` without a barrier keeps the
old `$a0` live for the first `lhu` and parks the new pointer in `$t1`.
Pin all three after the store so the overwrite sticks:

```c
register MATRIX*          src asm("a3");
register GpRelMatScratch* tmp asm("a0");

src      = arg0;
tmp      = (GpRelMatScratch*)(head - 0x30);
*scratch = tmp;
__asm__ volatile("" : "+r"(tmp), "+r"(src), "+r"(head));
```

Type that scratch as `MATRIX` + `VECTOR` (0x30) and keep `tmp` itself
pinned to `$a0`. A second `Scratch*` copy of `tmp` emits `move t1, a0`
and `sw 0x20(t1)` for the translation delta. Pass the vec to
`ApplyMatrixLV` as `(VECTOR*)(head - 0x10)` (not `&tmp->vec`) so the
call is `addiu a1, t0, -0x10`; dest is `(VECTOR*)arg2->t` (`addiu a2,
a2, 0x14` in the second subtract's load delay).

## Pin array base and the loaded byte both to `$v0` so stores beat `sll 24` / `slti`

`n = arr[i].field_1; menu->field_4 = n; menu->field_5 = n; if ((s8)n >= 0xB)`
with `n` in `$v1` sign-extends first (`sll` / `sra` / `slti`) and parks the
two `sb`s in the `slti` delay. The target wants the address in `$v0`, the
index in `$v1`, `lbu v0, 1(v1)`, then both stores, then the sign-extend.
Give the pointer and the byte the same hard register so the `lbu`
clobbers the base and the stores must happen before `sll` reuses `$v0`:

```c
{
    register s32         val asm("v0");
    register GpItemScan* s asm("v0");

    s             = &D_8010D628;
    val           = s[arg0->spawnArg1].field_1;
    menu->field_4 = val;
    menu->field_5 = val;
    if ((s8)val >= 0xB) {
        menu->field_5 = 0xA;
    }
}
```

`func_800BD2FC` is the example.

## Three `Ui_DrawText` calls CSE to one `jal` with `lui a1`; a `char*` temp uses `$v0`

A 3-way title pick compiled as

```c
if (arg0->spawnArg1 == 0) {
    if (arg0->flags == 1) {
        Ui_DrawText((UiPanel*)obj, D_80093D70);
    } else {
        Ui_DrawText((UiPanel*)obj, D_80093D80);
    }
} else {
    Ui_DrawText((UiPanel*)obj, D_80093D8C);
}
```

shares one `jal Ui_DrawText` and loads each string with `lui a1` /
`addiu a1, a1, %lo(...)`. Assigning through `char* text` first emits
`lui v0` / `addiu a1, v0` and hoists the else-string into the
`bnez spawnArg1` delay instead of `move a0, s2`. `func_800BD2FC` is the
example.

## `count = count < func()` emits `slt s0, s0, v0` / `beqz s0`

`if (count < func_800BAF5C(scan))` keeps the compare in `$v0`
(`slt v0, s0, v0` / `beqz v0`). Reusing the saved count for the boolean
writes the `slt` onto `$s0`:

```c
count = scan->field_1;
count = count < func_800BAF5C(scan);
if (count != 0) {
    obj->field_4 |= 0x20000;
} else {
    obj->field_4 &= ~0x20000;
}
```

`func_800BD2FC` is the example.

## Inline a recalc helper so `cfg` stays in `$a1` and the old current stays in `$a3`

`func_800B996C` `jal`s `func_800B7930` then `cfg->field_1c = cfg->field_1e`,
so `cfg`/`save` live in `$s0`/`$s1`. The HP sibling must write the
`func_800BC0C0` body inline (no `jal`): `cfg` stays in `$a1`, `save` in
`$t0`. Cache original `field_18` in an `s32` so the `D_80114BE8` store is
`lh a3` / `sw a3` and the clamp is `slt v0, v0, a3`:

```c
hp                 = cfg->field_18;
D_80114BE8.field_0 = hp;
...
if (cfg->field_1a < hp) {
    cfg->field_18 = cfg->field_1a;
}
cfg->field_18 = cfg->field_1a;
```

Calling `func_800BC0C0()` instead emits a `jal` and reallocates those
pointers into callee-saved regs. `func_800B9B40` is the example.

## Split an unaligned 4-byte copy so `lui` / `li a1` / `addiu $t4` match the jal args

`save->field_5BC = D_8010D520; func_800B8CAC(&save->field_5BC, 0x6C, 1)`
schedules `li a1, 0x6C` *before* `lui %hi(D_8010D520)`. The target wants:

```
addiu  a0, v0, 0x5BC
lui    v1, %hi(D_8010D520)
li     a1, 0x6C
addiu  t4, v1, %lo(D_8010D520)
lwl/lwr/swl/swr
jal    func_800B8CAC
 li    a2, 1
```

Pin the save pointer to `$v0` and the copy source to `$t4`. Emit the `la`
as a volatile `lui` into a live `$v1` temp, materialize the `0x6C` (pinned
`$a1`) *before* the `%lo` `addiu`, then keep that `$v1` live so the `%lo`
cannot reuse it:

```c
register McSaveData* save asm("v0");
register GpItemScan* src asm("t4");
register GpItemRec*  table asm("v1");

save = &Mc_SaveData;
dest = &save->field_5BC;
asm volatile("lui %0, %%hi(D_8010D520)" : "=r"(table));
item = 0x6C;
asm volatile("" ::"r"(item));
asm volatile("addiu %0, %1, %%lo(D_8010D520)" : "=r"(src) : "r"(table));
asm volatile("" ::"r"(table));
*dest = *src;
func_800B8CAC(dest, item, 1);
```

A 4x3 byte-clear of `D_80114BF0` then wants `addu v0, v0, a2` (index first).
`levels[col + i] = 0` is `addu v0, a2, v0`. Write
`*(u8*)((col + i) + (s32)levels) = 0` inside `for (; item < 4; item++, i += 3)`.
`func_800BA538` is the example.

## Don't pin a post-call dest to `$a3` when `$a3` is also the last call arg

`f(arg0, arg1, arg2, scratch)` then `dest = obj->field_8; if (dest)` wants
the call setup:

```
lw    v0, 0(v1)
move  a0, s1
...
lw    v0, 0(v1)
move  a3, s0
jal   f
```

`register T* dest asm("a3")` for that later NULL-check makes GCC prepare
`$a3` (4th arg) before `$a0`, swapping the two delay-slot fills (99.8%).
Leave `dest` unpinned; GCC still loads it into `$a3` after the return.
`func_800B3108` is the example.

## Reuse `$v1` across scratch head, dividend, and bitfield pointer

A 3-arg function that early-outs on `arg2->field_E`, allocates an 0x80-byte
scratch, then walks 11-10-11 bitfields wants `$v1` for three sequential
lifetimes and the subtract in `$v0`:

```c
register void** scratch asm("v1");
register T*     tmp asm("v0");
register s32    blend asm("v1");
register Packed* p asm("v1");

scratch  = (void**)G_SCRATCH_HEAD;
tmp      = *scratch;
tmp -= 1;
*scratch = tmp;
```

`tmp -= 1` on the 0x80-byte struct is `addiu v0, v0, -0x80` / `sw v0, 0(v1)`;
the copy into `$s0` fills the `beq field_0, field_4` delay. Pin `blend` so
`lh v1, field_C` / `div v1, v0` / `mflo v1` and the invBlend phi stay in
`$v0`. Pin `p` so each bitfield extract is `lw v0, 0(v1)` instead of
reloading the pointer from `$s1`. `func_800B3108` is the example.

## Full-screen POLY_F4 + `setDrawTPage`: extents first, `setSemiTrans` after `addPrim`

A leaf overlay that allocates a `POLY_F4` then a `DR_TPAGE` from `D_80071190`
wants the screen extents and ABR mask live before the prim cursor load:

```c
arg1 &= 3;
x0   = -0xA0;
x1   = 0xA0;
yTop = -0x78;
yBot = 0x78;

p          = (POLY_F4*)D_80071190;
D_80071190 = (DR_TPAGE*)(p + 1);
setPolyF4(p);
setRGB0(p, arg0[0], arg0[1], arg0[2]);
p->x0 = x0;
p->y0 = yTop - Display_State.vramYOffset;
```

`arg1 &= 3` at the top is `andi a1, a1, 3` during the y1 stores; the later
`arg1 << 5` is `sll a1, a1, 5` inside the first `addPrim`. Folding
`(arg1 & 3) << 5` at the `setDrawTPage` site delays the `andi`.

`setSemiTrans(p, 1)` **after** the POLY `addPrim` (not before, as in
`func_800EC888` / `func_800EC914`) lets `lbu code / ori 2 / sb` fill the
`lui`/`ori 0xE100020A` window. Then:

```c
setDrawTPage(dr, 0, 1, 0xA | (arg1 << 5));
```

which is `0xE100020A | (abr << 5)`. Same OT slot as `func_800EC914` with
`z = 0x10`. `func_800EA858` is the example.

## Keep `+ K` on a sign-extended `s8` with `* -1 + u16`, not `u16 - (s8 + K)`

`y - (vramYOffset + 7)` with `y` a `u16` reassociates to `(y - 7) - vramYOffset`
(`addiu y, -7` then `subu`). The target wants the add on the byte after
`lbu` / `sll 24` / `sra 24`:

```
lbu    v0, vramYOffset
sll    v0, v0, 24
sra    v0, v0, 24
addiu  v0, v0, 7
subu   v0, y, v0
```

`(vramYOffset + 7) * -1 + y` keeps that form, but a plain `s8` as the left
operand of `+ 7` is `lb`. Load through a volatile byte so the extend stays
the long form:

```c
p->y1 = ((s8)*(volatile u8*)&Display_State.vramYOffset + 7) * -1 + y;
```

`addPrim`'s `0xFFFFFF` wants `lui a3, 0xFF` in an earlier load delay and
`ori 0xFFFF` in a later one. A full `mask = 0xFFFFFF` at the `addPrim` site
emits both halves together. Split it: comma-assign `mask = 0xFF0000` into an
independent store so `lui` fills the first delay, barrier across the next
prim stores, then `mask |= 0xFFFF` before the last vertex:

```c
p->x1 = (mask = 0xFF0000, x);
asm("" : : "m"(p->y0));
p->y1 = ((s8)*(volatile u8*)&Display_State.vramYOffset + 7) * -1 + y;
p->x2 = x + 7;
asm volatile("" : "+r"(mask) : "m"(p->y1));
mask |= 0xFFFF;
p->y2 = ((s8)*(volatile u8*)&Display_State.vramYOffset + 7) * -1 + y;
```

Use that `mask` in a handwritten `addPrim` (same shape as `Ui_DrawCaret`).
`func_800E6608` is the example.

## `s16` `>=` as `x > y - 1` so GCC emits `addiu -1` / `slt` / `beqz`

`if (++x >= y)` with both sides `s16` emits `slt v0, x, y` / `bnez` (skip the
body). When the true path is the call and the false path is the epilogue, the
target wants the inverted form:

```
addiu  v1, y, -1
slt    v1, v1, x
beqz   v1, skip
```

Write the compare as `if (x > y - 1)` after the increment. `func_800F1364` is
the example.

## Dummy `case 0` plus a `u16` temp so switch is `lhu v0` / `andi v1`

A 1..3 dispatcher on a `u16` table field wants:

```
lhu    v0, field(v0)
j      join
andi   v1, v0, 0xffff
...
lhu    v0, field(v0)
nop
andi   v1, v0, 0xffff
li     v0, 1
beq    v1, v0, case1
slti   v0, v1, 2
```

`switch (kind)` with only cases 1..3 pivots at 2 (`lhu v1` / `li v0, 2` in
the jump delay). An empty `case 0` pivots at 1 and gives `slti < 2`.

Assigning the `u16` load straight to an `s32` still folds to `lhu v1`.
Keep a `u16` temp, barrier it, then widen:

```c
if ((id & 0x8000) == 0) {
    raw = D_80113390[id & 0x7F].field_4;
    asm volatile("" : "+r"(raw));
    kind = raw;
} else {
    raw = D_8011398C[id & 0x7F].field[5];
    asm volatile("" : "+r"(raw));
    kind = raw;
}
switch (kind) {
    case 0:
        break;
    case 1:
        /* ... */
```

`kind = raw & 0xFFFF` also matches; `s32 raw` without the mask becomes
`move` and drops the `andi`. `func_800E2A24` is the example.

## Unsigned `>> 16` in the `if (x & 0xF0000)` arm so the delay slot is `srl`

```c
if (x & 0xF0000) {
    y = x >> 16;
} else {
    y = K;
}
```

with a signed `s32` `>>` is `sra` *before* the branch. Put `(u32)x >> 16` in
the taken arm so GCC fills the `bnez` delay with `srl v0, v1, 0x10` and only
emits `li v0, K` on the fall-through. `func_800F1364` is the example.

## Pinned `== 1` locals steal `li`, 1 from `$v0`

After `if (x == 1)`, a `register ... asm("sN")` on `x` makes later `1`s copy
from `$sN` (`move a1, s5`, `move extra, s5`, `move a2, s7`) instead of
`li ..., 1` / `move ..., v0`. Unpin the flag so Pad's jal delay is `li a1, 1`
and `Ui_SpawnFromDesc(..., one, one, ...)` is `move a2, v0` / `move a3, v0`.
Pinning the flag to get the other `$s` coloring is what produces
`move a3, a2`. `func_800BD6DC` is the example.

## Barrier a copied src so `subu` keeps the original register

`lum = inner` then `temp - inner` CSEs to `subu v0, v0, v1` because `lum`
is the fresh copy in `$v1`. The target saved inner with `move v1, a0` and
still subtracted `$a0`. Split the first XYZ load from its shift so both
`lw`s issue before `sra`, then:

```c
temp = block->outerSq;
asm volatile("" : "+r"(temp));
lum  = inner;
asm volatile("" : "+r"(lum));
block->outerSq = temp - inner;
block->distSq -= lum;
```

The first barrier forces `lw` before `move` (move fills the load delay).
The second redefines `lum` so it is not CSE-equivalent to `inner`, and
`temp - inner` stays `subu ..., a0`. `func_800D9138` is the example.

## Pin a `$v0` temp so `lbu` does not land in the saved dest

`item = cfg->field_21 + 0x7F` with `item` in `$s2` emits `lbu s2` /
`addiu s2, s2, 0x7F`. The target loads to `$v0` then adds into `$s2`.
A nested `register s32 t asm("v0"); t = cfg->field_21; item = t + 0x7F;`
restores `lbu v0` / `addiu s2, v0, 0x7F`. Same for `field_23 + 0x5F`.
`func_800C8368` is the example.

## Color a 3-store loop so the table pointer is `$a2` with split `la`

`i` in `$a0`, compare-index in `$a1`, `minusOne` in `$t0` leaves `$a2` for
`table = D_8010E8F8` as `lui v0, %hi` / `addiu a2, v0, %lo` / `lw v0, 0(a2)`.
Without those pins the table absorbs `$v1` (or `$a0`) and the `%lo` load
folds. Keep `table` and `p = table` distinct with `asm volatile("" :
"+r"(table), "+r"(p))` so the loop walks `$v1` (`move v1, a2` / `p++`).
Reuse a later `flags` local as the early `idx` copy so `move a3, s8` fills
the `*stored != item` delay. `func_800C8368` is the example.

## `$v1`-pinned `sel` so `andi` delay-slots then copies before `p` reuses `$v1`

`slot = flags & 0xFF` writes `andi a1, a3, 0xff` directly. The target
computes into `$v1` in the `beq` delay, copies to `$a1`, then `p = table`
reuses `$v1`. A `register s32 sel asm("v1")` with `i = 0` between `sel =
flags & 0xFF` and `slot = sel`, plus `asm volatile("" : "+r"(sel))` so
`sel` does not coalesce with `slot`, emits:

```
andi   v1, a3, 0xff
move   a0, zero
move   a1, v1
li     t0, -1
move   v1, a2
```

`func_800C8368` is the example.

## Copy a live-across-call arg inside the calling branch, not at entry

A function with `if (flag == 1) { call(); use(arg3); } else { if (arg3 == 0)
arg3 = 1; use(arg3); }` that mentions `arg3` after the call makes GCC copy
it at function entry:

```
sw     s1, 0x14(sp)
move   s1, a3
lui    v0, %hi(flag)
...
bne    v0, v1, else
```

Both branches then use `$s1`, so the else-path's `bnez a3` / `negu a3`
becomes `bnez s1` / `negu s1`. Copy to a local *inside* the calling
branch (`setIdx = arg3` before the jal, then only `setIdx` after it).
`$a3` stays the else-path's register, and `move s1, a3` fills the first
`lbu field_15` delay on the taken path. `func_800B3AA4` is the example.

## Same-value extra store hoists `%hi`; keep CSE `1` as a literal

A global stored late (`D = 1` after a long init) still wants
`lui s0, %hi(D)` in an earlier fill slot. A second, earlier `D = 1` is
DSE'd (same value) but starts the `%hi` live range, so the `lui` fills
that slot.

The matching `li s2, 1` then has to come *after* that `lui` (setup for
the delayed store). `register s32 one asm("s2"); one = 1;` emits
`li s2, 1` at the assignment, which is too early if `one = 1` is written
before the extra `D = 1`. Leave the constant as a literal `1` at every
store; GCC CSE's it into `$s2` and schedules `lui %hi` then `li s2, 1`.

The same extra-store trick on a later global (`mode` copied to a halfword
before `other = 0` *and* again inside `if (mode != 0)`) hoists that
`%hi` into the previous `lui`/`sh` gap so the real `sh` can fill `beqz`
(which forces `sll v0, s3, 16` instead of clobbering `$s3`).

`func_800E41F4` is the example.

## Double `asm volatile("" : "+r"(off))` so `<< 4` then `+ 4` stays `addiu`

`off = idx * 16 + 4` after `sll 4` becomes `ori v0, v0, 4` (combine
turns PLUS into IOR because the shift proves the low 4 bits are 0). An
empty `+r` barrier after the shift hides that alignment, so GCC emits
PLUS 4 — then folds it into `lhu 4(base)`.

Barrier before *and* after the `+ 4` keeps a standalone `addiu v0, v0, 4`
between `sll 4` and `addu` of the table base:

```c
off <<= 4;
asm volatile("" : "+r"(off));
off += 4;
asm volatile("" : "+r"(off));
off += (s32)recs;
val = *(u16*)off;
```

```
sll    v0, v0, 4
addiu  v0, v0, 4
addu   v0, v0, v1
lhu    v0, 0(v0)
```

`func_800A1CD0` is the example.

## `goto` the spawn body so a shared kill is fall-through between `slt` and `jal`

An effect task that kills on `flag >= 4` *and* on `count >= limit`, else
spawns, wants kill as the join of both compares, with spawn as the
`bnez` target:

```
slti   v0, flag, 4
beqz   v0, kill
...
slt    v0, count, limit
bnez   v0, spawn
 lui    a0, 0x6
move   a0, mem
kill:
jal    func_800EC7E4
move   a1, task
j      epilogue
nop
spawn:
ori    a0, a0, ...
```

Nested

```c
if (flag < 4) {
    /* init / draw */
    if (count < limit) {
        spawn();
        return;
    }
}
kill();
```

inverts the inner compare (`beqz` skip-to-kill) and emits spawn first,
then a `j epilogue` over a trailing kill. Jump *to* the spawn block so
kill stays the `flag >= 4` / `count >= limit` fall-through:

```c
if (flag < 4) {
    /* init / draw */
    if (count < limit) {
        goto spawn;
    }
}
kill();
return;
spawn:
    spawn();
```

`func_800F9474` is the example.

## Unsigned `* 3u >> 4` reuses `state = 1` as `sllv` then `srl`

`field = (u16)spawnArg1 * 3 / 16` as signed int emits the signed-div
bias (`sra` + add). After `arg0->state = 1` (`li v0, 1` / `sw`), write
`((u16)spawnArg1 * 3u) >> 4` so that 1 is the `sllv` shift amount:

```
li     v0, 1
sw     v0, state
lhu    v1, spawnArg1
sllv   v0, v1, v0      # << 1
addu   v0, v0, v1      # * 3
srl    v0, v0, 4
```

`func_800F9474` is the example.

## `"+m"` clobber after an s16 store so later field reads stay `lh`

Storing `(u16 % 10U) - 1` to an s16 field leaves a register that is already
a valid sign-extended s16 (unsigned wrap of 0 − 1 is `0xFFFFFFFF`). GCC
2.8.1 CSEs that register into later `field << 7` / `field << 8`, emitting
`sll` / `lhu` instead of the target's `sh` / `lh` / `sh` / `lh` pair with
both values live for the two shifts.

Clobber the field memory, then snapshot it into two block-scoped s32
locals around the neighboring store:

```c
mem->field_20 = (D_80114C08.field_0 % 10U) - 1;
__asm__ volatile("" : "+m"(mem->field_20));
x             = mem->field_20; /* lh */
mem->field_26 = 0x20;
y             = mem->field_20; /* lh */
mem->field_28 = (x << 7) + 0x180;
mem->field_2A = (y << 8) + 0x400;
```

Keep the s32 temps inside the init block so they do not steal `s0`/`s1`
from `mem`/`coord`.

`func_800FB7E4` is the example.

## Copy `MATRIX.t` longs into an `SVECTOR` with `*(u16*)&t[i]`

`tmp.vx = arg0->coord.t[0]` (s32 → s16) emits `lw` / `sh`. The target
loads only the low half (`lhu` / `nop` / `sh`) because the translation is
a 16-bit value stored in a `long`. Pun through `u16`:

```c
tmp.vx = *(u16*)&arg0->coord.t[0];
tmp.vy = *(u16*)&arg0->coord.t[1];
tmp.vz = *(u16*)&arg0->coord.t[2];
```

`func_800B1D00` is the example.

## Repeat a global load so `%hi` stays in `$a1`; don't stash it in a local

`u16 val = D_80114C08.field_0` then `val / 10U` / `val / 100U` reuses the
`%hi` register as the load dest (`lhu a1, %lo(a1)`). The target keeps
`$a1` as `%hi` for every `field_0` access (`lhu a2, %lo(D_80114C08)(a1)`).
Write `D_80114C08.field_0` at each use so the CSE temp is `$a2`.

## `x - 1 + y` reassociates onto `y`; a live `-1` variable or a split add does not

`(a + b - 1) + y` with `y` a `(u16)` modulo result becomes `a + b + (y - 1)`
(`andi` / `addiu -1` / `addu`). The target wants `addiu -1` on the
accumulator before computing `y`:

```
addu   v1, v1, v0      # a + b
addiu  v1, v1, -1
sll    v0, a3, 2       # start y = val % 10
```

Two ways to keep that order:

1. In a branch where a live local is already `-1`, add that local instead
   of the literal (`... * 3 + kind + (u16)(val % 10U)` when `kind == -1`).
   GCC folds it to `addiu -1` but will not slide it onto `y`.
2. Split the add (`idx = a + b - 1; idx += (u16)y`). Needed when no such
   local exists, but it also stops `la table` from sinking into a multiply
   stall. Use (1) when the index is inlined as `table[expr]` so the `la`
   can fill `multu` delay (`lui t0` / `addiu t0, %lo` during `tens / 10`).

`func_800FA7CC` is the example (kind-add on the `D_80112A50` arm, split
add on the `D_80112978` arm).

## Empty `asm volatile("")` pins `if (x <= 0) return` as `bgtz` + `j; li -1`

A large success body after `if (shifted > 0) { huge; return x; } return -1`
inverts to `blez epilogue`. The target wants the fail physically between
the compare and the success block, shared with earlier range `goto fail`:

```
bgtz   success
 sll    v0, s0, 2     # first insn of success in delay
j      epilogue
 li     v0, -1
success:
```

Invert the test and put an empty volatile (a scheduling barrier, not a
nop) on the fail label so jump-opt will not move that return later and
merge it with other `return -1`s:

```c
if ((u32)(arg2 - 0xA0) >= 0x20U) {
    goto fail;
}
if ((u32)(arg1 - 0x80) >= 0x20U) {
    goto fail;
}
if (shifted <= 0) {
fail:
    asm volatile("");
    return -1;
}
/* huge success; later `if (have <= 0) return -1` stays `blez epilogue` */
```

`asm volatile("" : "=r"(arg3)); arg3 = 0;` in the `arg3 == 0` else forces
the redundant `move s2, zero` that CSE would drop. `func_800B715C` is
the example. Also `asm volatile("" : "+r"(qtyTable))` after `la` so
`*(u8*)((s32)qtyTable + off + 0x200)` emits `addu v0, v1, v0` rather
than the commuted `addu v0, v0, v1`.

## Reuse an unpinned temp for `$v0`; do not pin `$v1` across `sltu`

A distance-attenuation helper that wants `mflo v1` / `sra v0, v1, 2` /
`lw v1, distSq` / `sltu v0, v0, v1` cannot pin the outer-radius temp
with `register s32 outer asm("v0")`. That reserve steals `$v0` from the
earlier `vx²+vy²+vz²` dest (`addu a0, a0, a2` instead of `addu v0, a0,
a2`) and from the epilogue scratch pointer (`$a2` instead of `$a0`).
Reuse one unpinned `s32 sq` for the distance sum, then the outer radius,
then the `>> 2` result so GCC keeps the chain in `$v0`:

```c
sq  = vx * vx + vy * vy + vz * vz;
block->distSq = sq;
sq  = obj->field_5C;
lum = sq * sq;
sq  = lum >> 2;
lum = block->distSq;
tooFar = (u32)sq < (u32)lum;
```

`register s32 lum asm("v1")` then coalesces the compare into
`sltu v1, v0, v1`. Leave `lum` unpinned so the dest stays `$v0`.
`func_800D70E4` is the example.

## `three = 3` hoists into `$v1` and steals the `lhu` id; idx/off coalesce

A merged `func_800BB470` + `func_800BAC34` body wants:

```
lhu    v1, 0(s1)          # id
lw     v0, Game_Session
sra    a0, v1, 4          # idx
sll    a3, a0, 2          # off, idx stays in $a0
andi   v1, v1, 0xf
lbu    v0, 7(v0)
sll    a1, v1, 1          # shift
...
lw     v0, 4(v0)          # flags
li     v1, 3              # delay slot; $v1 reused after id dies
addu   v0, v0, a3
lw     v0, 0(v0)
sllv   a2, v1, a1         # mask
```

`s32 three = 3` (or a literal `3 << shift` computed after the flags load)
is independent, so `-fschedule-insns` lifts `li v1, 3` to the top of the
block. That occupies `$v1` for the whole block, so `id` lands in `$a0`
and `idx`/`off` coalesce (`sra a1` / `sll a1, a1, 2`). Reassigning
`id = 3` after `shift = id << 1` delays the `li` (same pseudo) but the
long live range loses `$v1` to `mask` (`id` in `$a1`, `sllv` for
`field_7 << 3` instead of `sll 3`).

`asm volatile("" :: "r"(idx))` after `off = idx << 2` stops the
coalesce and can recover `shift` in `$a1` / `mask` in `$a2` / delayed
`li v1, 3`, but the extra live range of `idx` overlaps `sess` and the
`UiObject*` from `spawnArg2`, swapping `$t0`/`$a3` (obj vs off) and
`$v0`/`$v1` (Game_Session vs `D_8010D230`). Nested
`register ... asm("a0")` (etc.) is function-wide in 2.8.1 and breaks
the earlier `Ui_SpawnFromDesc` `xori a1, 1`. `func_800B65B0` is the
example; not fully matched.

## `i << 2` so the IV init is `move t3, t4` after the hoists

A selection-sort that walks `table[start + i]` wants a running byte
offset `$t3` initialized *after* the loop-invariant `li` / `lui`s:

```
move   t4, zero          # i = 0, blez delay
li     s3, 1
...                      # hoisted table / remap bases
move   t3, t4            # off = i*4, i is 0
loop:
```

A source-level `off = i` in the preheader is scheduled *before* those
hoists. Write the index as `i << 2` (or `i * 4`) in the address and let
strength reduction create the IV; the init lands after the movables.

## Switch then `if (x != 1)` so the second table pick keeps delay slots

Two copies of the 1 / 2 / default table select: a `switch` (case 2, 1,
default) for the first copy hoists `s3=1`, `s2=2`, `s1=C20`, `s0=D70`,
`t9=Mc_SaveData`. Repeating that `switch` for the second copy, with
`minKey = key` in the `== 1` delay, expands to extra `j` / `nop` and
moves default out of the `bne` delay. Write the second copy as:

```c
if (scan->field_2 != 1) {
    table = Mc_SaveData.field_1AC;
    if (scan->field_2 == 2) {
        table = D_80114C20;
    }
} else {
    table = D_80114D70;
}
```

The first `switch` still owns the s-register assignment; the `if != 1`
form keeps `addiu default` in the `bne` delay with `minKey` in the
`beq == 1` delay. `func_800B8588` is the example.

## Unconditional `p += next` so the add fills the following `beqz` delay

`other = base; next = off + 4; if (j < n) other += next` sinks
`addiu v1, t3, 4` into the `beqz` delay and leaves `addu a3, a3, v1`
after the branch. The target executes that add even on the skip path
(delay-slot speculation). Compute it before the compare:

```c
other = (T*)((s32)table + (start << 2));
next  = (i << 2) + 4;
other = (T*)((s32)other + next);
if (j < n) {
    /* walk other */
}
```

`addiu v1, t3, 4` stays after `addu a3, v1, v0`; `addu a3, a3, v1`
fills the `beqz`. `func_800B8588` is the example.

## Shared `index = arg0` tail so type 6/3/2 emit `j` / `li` into `move s2, a0`

Classifying an id into `(type, index)` wants types 6, 3, and 2 to share
`index = arg0`. Writing `index = arg0` in each arm emits `li type` /
`move index, a0` / `j join`. Jump to a shared assignment with `li type`
in the delay, and skip that assignment when index is already packed:

```c
if (arg0 >= 0x500) {
    type = 6;
    goto set_index;
}
if (arg0 >= 0x300) {
    type  = 7;
    index = packed;
    goto after_index;
}
if ((u32)(arg0 - 1) < 0x5FU) {
    type = 3;
    goto set_index;
}
type = 2;
set_index:
index = arg0;
after_index:
```

`func_800C5C2C` is the example.

## `idx = (x & 0xFF) << 2` before `p = arr` so `sll` precedes `addiu p`

`p[slot] = -1` after a loop whose exit delay already has `andi v0, arg, 0xFF`
wants `sll v0, 2` / `addiu s0, sp, flags` / `addu v0, s0, v0` / `jal` delay
`sw -1`. Assigning `p = arr` first emits `addiu` then `sll`. Compute the
byte offset first so the already-live `$v0` is shifted immediately:

```c
idx = (arg1 & 0xFF) << 2;
p   = blk.flags;
*(s32*)((s32)p + idx) = -1;
CdCmd_DropPending();
```

`func_800C5C2C` is the example.

## Pin `off` to `$v1` as three adds so `base + i*8 + 0x10` is not strength-reduced

`saved[i]` as `base + i*8 + 0x10` in a counted do-while becomes a walking
pointer (`sll v0` / `addu a0, s4, v0` / `addiu v1, a0, 0x10` / `lbu 0x10(a0)`).
Three assignments into a `$v1` temp keep `sll v1, s1, 3` in the `beq == -1`
delay, then `addu v1, s4` / `addiu v1, 0x10`:

```c
register s32 off asm("v1");
off = i * 8;
off = off + (s32)base;
off = off + 0x10;
cur = (CdCmdEntry*)off;
```

`func_800C5C2C` is the example.

## Join `SetHolderParam` text in `$a0` and kill REG_EQUAL on the 0s

`Ui_SetHolderParam(text, 0, 0)` immediately followed by `func_800CDE80(item, 0)`
with `item` in `$s0` copy-props both 0s: `move a1, zero` / `jal` /
`move a2, zero`. The target reuses the first 0 as `move a2, a1` in the delay
and still has `li a1, 1` in the `bnez item` delay of the empty-slot path.

Inlining `SetHolderParam(func_800B8EB0(item, 1, 0), 0, 0)` keeps that `li a1, 1`
but still copy-props `$a2`. Split the join so the text lives in `$a0` and `$a1`
is 1 then 0, and kill REG_EQUAL on the 0:

```c
register s32 t asm("a0");
register s32 a1v asm("a1");
a1v = 1;
if (item == 0) {
    t = (s32)emptyStr;
} else {
    t = (s32)func_800B8EB0(item, a1v, 0);
}
a1v = 0;
asm("" : "+r"(a1v));
Ui_SetHolderParam(t, a1v, a1v);
func_800CDE80(item, 0);
```

`func_800C26B8` is the example.

## C `lui` of a shifted 16-bit constant fills a branch delay; `asm("lui")` does not

`asm("lui %0, %%hi(sym)")` will not fill a delay slot. The first statement of
`if (state == 0)` was that asm, so `bnez state, skip` got a `nop` and the
`lui` sat after it. A C assignment of a value that is only a high 16-bit
immediate emits `lui` that *can* fill the delay:

```c
hi = 0x8007 << 16;
asm("addiu %0, %1, %%lo(Mc_SaveData+0x5BC)" : "=r"(scan) : "r"(hi));
```

```
bnez   v0, skip
 lui    s0, 0x8007          # delay; linked bytes == %hi(Mc_SaveData+0x5BC)
addiu  s1, s0, %lo(...)
```

Keep the `%lo` addiu as asm so it retains its reloc. `0x8007` is
`%hi(Mc_SaveData+0x5BC)` (`Mc_SaveData` is `0x80072168`). `func_800C9E94`
is the example.

## Store scratch `head - N` through `$v1`, barrier, then copy to the s-reg

A GTE helper that keeps the allocated scratch in `$s0` across a later call
wants the new head stored from `$v1` and `move s0, v1` in the next
independent load delay (`lw field_8` of `&coord->workm`):

```
addiu  v1, a2, -0x28
sw     v1, 0(v0)
lw     v0, 8(a0)
move   s0, v1
addiu  v0, v0, 0x24
```

`block = *scratch = (head - N)` computes into `$s0` (`addiu s0` / `sw s0`).
Assigning `block` after `gte_SetRotMatrix` cannot hoist that copy above the
volatile GTE asm. Store first, memory-barrier, then copy so the move can
sit in the `&workm` address load:

```c
*scratch = (void*)(head - 0x28);
__asm__ volatile("" ::: "memory");
block = (GpNearScratch*)(head - 0x28);
gte_SetRotMatrix(&((GsCOORDINATE2*)arg0->field_8)->workm);
```

Pin scratch to `$v0` with the existing `lui`/`ori` fake dependency so the
alloc temp lands in `$v1` and `head` stays in `$a2`. `func_800E1380` is
the example.

## `n = id < K; if (n) goto store` so slti dest is the count, plus `asm("")` to keep `bnez`

A small integer result computed as `1` / `2` / `3` from `id < 0x80`,
`id == 0x92`, and a `u8 != 0xFF` test wants the count in `$v0` sharing the
`slti` dest:

```
move   v1, v0          /* save func() return */
slti   v0, s0, 0x80
bnez   v0, store
 li    v0, 1
li     v0, 0x92
bne    s0, v0, check
 li    v0, 0xFF
j      store
 li    v0, 1
check:
lbu    v1, 2(v1)
nop
bne    v1, v0, store
 li    v0, 3
li     v0, 2
store:
sb     v0, 4(s5)
```

Pin the `func()` return to `$v1` and the count to `$v0`, save the return
before `slti` with `asm volatile("" : "+r"(slot))`, then assign the compare
into the count so they share `$v0`:

```c
register GpItemSlot* slot asm("v1");
register s32         n asm("v0");

slot = func(id);
asm volatile("" : "+r"(slot));
n = id < 0x80;
if (n) {
    n = 1;
    goto store;
}
n = 0x92;
if (id == n) {
    n = 1;
    goto store;
}
n = 0xFF;
if (slot->field_2 != n) {
    n = 3;
    goto store;
}
n = 2;
store:
menu->field_4 = n;
```

`if (id < 0x80) { n = 1; }` without `n = id < 0x80` puts `slti` in `$a0`
because `$v0` is reserved for `n`. Skipping the `+r` barrier lets `slti`
sink above `move v1, v0` into the `jal` delay.

A later copy of the same chain whose next call is `Ui_ComputeVisibleRows`
(no extra store after `sb`) inverts `bnez` to `beqz` and fills the delay
with `li v0, 0x92`. An empty `asm("")` between the `if (n) goto` and
`n = 0x92` keeps `bnez` / `li v0, 1`. The `Ui_InitList` copy did not need
it because `menu->field_10 = 0` already sat after the store.

`func_800C3CE0` is the example.

## Empty `case` so a 2-node switch subtree still emits `sltu` to default

GCC 2.8.1 `emit_case_nodes` for a node with only a left child that is a
simple single-value leaf skips the `> pivot` test and just does
`beq left; j default`. The target wants the 3-node shape (`beq pivot;
sltu; bnez default; beq low`):

```
beq    s0, v0, case_10C     # pivot
sltu   v0, v0, s0
bnez   v0, default          # > pivot
lui    v0, %hi(low)
beq    s0, v0, case_low
j      default
```

That extra `sltu` appears when the pivot also has a right child. An empty
`case` for the missing in-between value (same `break` as default) adds
that right child without emitting a body:

```c
switch (mapId) {
    case 0x1060000:
        /* play start/end sound */
        break;
    case 0x10C0000:
        /* play */
        break;
    case 0x21B0000:
        break; /* no sound on this path; keeps the 10C pivot */
    case 0x31B0000:
        /* play */
        break;
}
```

State 1 of the same function has a real `0x21B0000` body, so the tree
already has the right child. State 3 must keep the empty case or the
lower half collapses to two sequential `beq`s. `func_800BEBE4` is the
example.

A `(s8)func()` result that must be `sll s0, v0, 24` / `sra s0, s0, 24` in
the next `jal` delay needs its own block-scope temp (see “Scope a `| k`
temp”). Reusing the switch index (`mapId = (s8)func()`) emits
`sll v0, v0, 24` / `sra s0, v0, 24` instead.
