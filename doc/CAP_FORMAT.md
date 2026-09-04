# CAP dialogue files (`.pe2cap2`)

The per-room dialogue container, magic `"CAP2"` (the loader compares 3 bytes).
172 files on disc, one per room
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
0x00  char magic[4]    "CAP2" on disc; strncmp checks 3 bytes only
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

## 6. Checked against a real file

`assets/USA/raw/pe2cap2/pe2cap2_4.pe2cap2`. `.pe2cap2` payloads are stored
opaque (§3.6 of `ASSET_FORMATS.md` — no LZSS), so the extracted bytes are what
the loader sees.

**The magic on disc is `"CAP2"`, four characters.** `Gp_RelocCapFile` compares
only three, so any `CAP*` passes. Do not write a 4-byte comparison.

**One raw chunk holds several CAP2 blobs.** This file has headers at `0x1AA0`
and `0x29F0`. The leading region before the first magic is a different
structure containing already-absolute pointers (`0x80188920`, matching the
per-room cap2 RAM address in `OVERLAYS.md` §2). What that leading region is has
not been identified.

Header at `0x1AA0`, matching §1 exactly:

```
"CAP2"  field_4=0x8  glyph=+0x14  evt=+0xB30  ptr=+0xF00
```

**The pointer table indexes into the event table.** Count 18; entries are
file-relative offsets `0xB34`, `0xB64`, `0xB88`, `0xBA0`, … and `0xB34` is
exactly `evt + 4`, the first `GpEvt12`. Entry gaps are whole multiples of 12
(`0xB64-0xB34 = 4 records`, `0xB88-0xB64 = 3`, `0xBA0-0xB88 = 2`).

That settles the aliasing question: **`GpCapCmd` and `GpEvt12` are the same
records seen two ways.** `Gp_RunCapCmd` reads `Gp_CapCmds[i]` as a 9-byte
command; `Gp_StartCap` stores the same pointer as `Gp_CapTable` and
`Gp_FindCapEvt` walks it as 12-byte events. The command record is the head of
its own event run.

Run termination confirmed: within the first run the fourth record has
`field_8 == -1`.

**Text is glyph indices, not a character encoding.** Over `+0x14 .. +0xB30`:
1422 `u16` values, only **107 distinct**, 1241 of them below `0x100`, most
frequent `0x001B` (165x), then `0x0021` (104x), `0x0023` (80x). A dense small
alphabet with a skewed frequency profile — indices into the glyph table at
`field_8` (`Gp_CapGlyphs`), not ASCII or Shift-JIS. 108 values are `>= 0x8000`
and are presumably control codes; `0xFFFE` / `0xFFFF` appear as terminators.

## 7. What is still open

- **Glyph index -> character.** The values are indices into `Gp_CapGlyphs`;
  turning them into readable text needs that table plus the font image. The
  mapping is not alphabetical by inspection (`0x21` as `A` does not produce
  words).
- **Control codes.** The 108 values `>= 0x8000` are undecoded.
- **The event-table count word does not parse as `s32`.** At `evt` the bytes
  are `41 00 0C 00`. `Gp_RelocCapFile` reads that as `s32 count` = `0x000C0041`
  = 786497, which would walk far past the file. Read as `{u16 count = 0x41;
  u16 stride = 0x0C}` it is sensible: 65 records of 12 bytes. The interpreter
  is matched, so the ROM really does load a word there. **Unresolved** - either
  `GpCapEvtTable` is mistyped in `include/gameplay/3CD8.h`, or the relocation
  path is not reached for these files, or the base used here is not the base
  the loader uses. Do not build a packer on the current struct until this is
  settled.
- **`GpEvt12` fields 0-3 and 6.** Only `field_4`, `field_5`, `field_7`,
  `field_8` have known roles.
- **`GpCapFile.field_4`** is `8` in this file and never read by the loader.
- **Message `0x13F0`** (opcode 3) - the payload contract with slot 7's task.
- **What `mode` selects.** `Gp_StartCap` sets a text-box geometry
  (`0x30`, `0xC0`, `0x140`, `7`) but the per-mode differences are untraced.
- **The leading pre-magic region** of the raw chunk.

## 8. Why this matters beyond extraction

CAP is the closest thing the game has to a room scripting language, and it is
deliberately narrow: five opcodes over counters, game-flag nibbles and 2-bit
progress flags. It selects dialogue; it does not spawn, move or branch the
world.

Anything designing a room DSL should read that as a boundary. CAP is a good
target for the "which line, how many times" part of a room and a bad target for
room logic, which retail keeps in the overlay's C.
