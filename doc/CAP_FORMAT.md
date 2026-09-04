# CAP dialogue files (`.pe2cap2`)

The per-room dialogue container, magic `"CAP"`. 172 files on disc, one per room
that speaks.

A CAP file is not a dialogue blob. It is a **small state machine that chooses
which line plays**, over counters that persist in save flags, plus the text those
lines point at. That is why a room does nothing more than
`Gp_RunCapCmd1(0xC)` and lets the file decide "first time say A, then B, then B
forever".

Everything below is read off the matched interpreter in
`src/gameplay/3CD8.c` and `src/gameplay/3CD8_34D8.c`, and the types in
`include/gameplay/3CD8.h`.

---

## 1. File header — `GpCapFile` (0x14)

```
0x00  char magic[4]    "CAP" (strncmp, 3 bytes)
0x04  s32  field_4     unread by the loader
0x08  s32  → glyph table   published as Gp_CapGlyphs (GlyphUvwh*)
0x0C  s32  → event table   GpCapEvtTable*
0x10  s32  → pointer table GpCapPtrTable*
```

The three offsets are **file-relative on disc** and rebased in place by
`Gp_RelocCapFile`, which is also the format validator: wrong magic returns 0,
and nothing is relocated when `field_8 <= 0`.

## 2. Relocation

`Gp_RelocCapFile` adds the file base to `field_8` / `field_C` / `field_10`, then
walks both tables:

- **Event table** — `GpCapEvtTable { s32 count; }` followed by `count`
  `GpEvt12` records. Each record's `field_8` is rebased **unless it is `-1`**,
  in which case the walk skips an extra record. `-1` is the terminator sentinel,
  so a terminator consumes a slot without owning text.
- **Pointer table** — `GpCapPtrTable { s32 count; }` followed by `count` words.
  Every nonzero word is rebased.

Then:

```c
Gp_CapGlyphs = (GlyphUvwh*)file->field_8;
Gp_CapCmds   = (s32*)((GpCapPtrTable*)file->field_10 + 1);
```

So **the pointer table is the command index**: `Gp_CapCmds[i]` is a
`GpCapCmd*`, one per event slot.

## 3. Event records — `GpEvt12` (0xC)

```
0x0  u8  field_0
0x1  u8  field_1
0x2  u8  field_2
0x3  u8  field_3
0x4  u8  field_4    flags copied to D_80115670; bit 0 cleared if field_7
0x5  u8  field_5    matched against Gp_CapEventKey
0x6  u8  field_6
0x7  u8  field_7    copied to D_80115678
0x8  s32 field_8    -1 terminator, else relocated u16* text
```

Text is `u16*`, not bytes.

`Gp_FindCapEvt(start)` scans forward from `start` through `Gp_CapTable` and
stops at the first record whose `field_8 == -1` **or** whose `field_5` equals
the current `Gp_CapEventKey`, returning the index. So an event slot is a run of
records terminated by `-1`, and the key selects a variant within the run.

## 4. Command records — `GpCapCmd` (9 bytes)

```
0x0  u8 opcode        0..4
0x1  u8 flags         bit0 wrap, bit1 persist counter in a flag, bit2 compare/branch
0x2  u8 limit         counter limit
0x3  u8 flagId lo
0x4  u8 counter       live in-memory counter (opcode 1, non-persistent)
0x5  u8 bitSlot       first 2-bit flag slot (opcode 4)
0x6  u8 bitCount      slot count (opcode 4)
0x7  u8 flagId hi
0x8  u8 next          next command index for the branch
```

Flag id is `field_3 | (field_7 << 8)` — a `GameFlag_*Nibble` id.

`Gp_RunCapCmd(index, mode)` is a `for(;;)` over the command table; `next` makes
it a jump, so commands chain without recursion.

## 5. Opcodes

Every opcode ends by calling `Gp_StartCapSlot(index, mode, variant)`. The
opcodes differ only in **how `variant` is chosen** and whether state advances.

### 0 — plain

```c
Gp_StartCapSlot(index, mode, 0);
```
Always variant 0. One unconditional line.

### 1 — counter

The only opcode that mutates state.

```c
val = (flags & 2) ? GameFlag_GetNibble(flagId) : rec->counter;
if ((flags & 4) && limit < val)  goto next;          // past the limit, branch away
Gp_StartCapSlot(index, mode, val);
if (val < limit || (flags & 4)) val++;
else if (flags & 1)             val = 0;             // wrap
(flags & 2) ? GameFlag_SetNibble(flagId, val) : (rec->counter = val);
```

- `flags & 2` decides **where the counter lives**: a save-game nibble
  (persistent across rooms and saves) or `field_4` in the record itself
  (resets when the file reloads).
- `flags & 4` turns the limit into a **branch condition** rather than a clamp —
  once past it, control jumps to `next` instead of speaking.
- `flags & 1` wraps back to 0 at the limit; without it the counter sticks.

That triple covers "say it once", "cycle through N lines", "say N times then
something else".

### 2 — flag-indexed

```c
Gp_StartCapSlot(index, mode, GameFlag_GetNibble(flagId));
```
Variant is read straight from a game flag. No mutation — the line follows story
state that something else owns.

### 3 — delegate

```c
Gp_DispatchMsg(Game_GetPtrSlot(7), 0x13F0, index, 0);
```
Hands the decision to slot 7's task with message `0x13F0`. The room decides;
the file only marks the hand-off point.

### 4 — tally 2-bit flags

```c
val = 0;
for (i = 0; i < bitCount; i++)
    if (Gp_GetCurBit2Flag(bitSlot + i) != 2) val++;   // counts states 0, 1, 3
if ((flags & 4) && val == 0) goto next;
Gp_StartCapSlot(index, mode, val);
```
Variant is **how many** of a run of 2-bit flags are not in state 2 — a progress
tally ("how many of these have you not finished"). With `flags & 4`, a zero
tally branches instead of speaking.

## 6. What is still open

- **Text encoding.** `field_8` is a relocated `u16*`. Whether those are glyph
  indices into `Gp_CapGlyphs`, a codepage, or contain inline control codes is
  not traced.
- **`GpEvt12` fields 0-3 and 6.** Only `field_4`, `field_5`, `field_7` and
  `field_8` have known roles.
- **`GpCapFile.field_4`** is never read by the loader.
- **Message `0x13F0`** (opcode 3) — the payload contract with slot 7's task is
  not documented.
- **`Gp_StartCapSlot`** itself: how `variant` selects among the records in a
  slot's run, and what `mode` does.

## 7. Why this matters beyond extraction

CAP is the closest thing the game has to a room scripting language, and it is
deliberately narrow: five opcodes over counters, game-flag nibbles and 2-bit
progress flags. It selects dialogue; it does not spawn, move or branch the
world.

Anything designing a room DSL should read that as a boundary. CAP is a good
target for the "which line, how many times" part of a room and a bad target for
room logic, which retail keeps in the overlay's C.
