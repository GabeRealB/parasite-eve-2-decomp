# CAP dialogue payloads in `.pe2cap2` bundles

The dialogue payload has magic `"CAP2"` (the loader compares 3 bytes).
Extracted `.pe2cap2` files are CDF resource bundles: they can contain CAP
payloads, images, STF credits and MDEC bitstreams. Their resource directory is
described in §6; the sections below describe the CAP payload itself.

A CAP file is not a dialogue blob. It is a **small state machine that chooses
which line plays**, over counters that persist in save flags, plus the text those
lines point at. That is why a room does nothing more than
`Gp_RunCapCmd1(0xC)` and lets the file decide "first time say A, then B, then B
forever".

Everything below is read off the matched interpreter in
`src/gameplay/cap_commands.c`, `src/gameplay/cap_reloc.c`,
`src/gameplay/captions.c`, and the types in
`include/gameplay/cap.h`.

---

## 1. File header — `CapFile` (0x14)

```
0x00  char magic[4]      "CAP2" on disc; loaders compare 3 bytes
0x04  s32  field_4       constant 8 in every retail payload; unread; role unproven
0x08  glyphs.offset      file-relative glyph-cell offset; TextGlyphCell* cells after relocation
0x0C  sequences.offset   file-relative sequence-table offset; CapSequenceTable* table after relocation
0x10  commands.offset    file-relative command-index offset; CapCommandTable* table after relocation
```

The three offsets are **file-relative on disc** and rebased in place by
`capRelocateFile`: a wrong three-byte "CAP" prefix returns 0 without changing
the file or active tables. No offset or table-bound validation is performed.
Nothing is relocated when `glyphs.offset <= 0`. A relocated KSEG0 pointer is
negative as a signed word, so repeated calls skip rebasing but still publish
the glyph and command tables and return 1.
Retail payloads store glyph offset `0x14`, the first byte after this header.

## 2. Relocation

`capRelocateFile` adds the file base to `glyphs.offset`, `sequences.offset`
and `commands.offset`, then walks both tables:

- **Sequence table** — `CapSequenceTable`:

  ```
  0x00  s16 count              sequence records the walk visits, including terminators
  0x02  s16 slotSize           shared command and sequence-record size; 12 in every retail payload; unread
  0x04  CapCommand firstCommand  slot zero of the first sequence; absent when count is 0
  0x10  CapSequenceRecord records[]  walk start
  ```

  The checked payload in §6 begins `41 00 0C 00`: count `0x41`, then slot size
  `0x0C`. Each visited record's `textRef.offset` is rebased **unless it is
  `CAP_TEXT_REF_END` (`-1`)**, in which case the walk skips the next slot.
  An earlier terminator's skipped slot is the next sequence's command, so the
  walk leaves that command unrelocated. The last visited record is a terminator
  on every retail payload with a nonzero count, and that final skip covers the
  `CapCommandTable`'s first 12 bytes. Four payloads have
  `count == 0` and no sequence slots; their `CapCommandTable` starts at
  `firstCommand` instead, and the sequence walk stops before it.
- **Command index** — `CapCommandTable`:

  ```
  0x00  s32 count                command references the relocation walk visits
  0x04  CapCommandRef entries[]  one word per command index; zero names no sequence
  ```

  Every nonzero word is a file-relative offset of a sequence command, slot zero
  of that sequence, and is rebased. Zero stays null and names no sequence.
  `Gp_CapCmds` is the published `entries` pointer. Playback indexes those words
  by the caller's command index. `count` is the relocation bound.

  On every retail payload with a nonzero sequence count the index is packed at
  the first byte after the sequence slots. The sequence walk's last record is a
  terminator, so its extra skip advances across this index's first 12 bytes —
  the count and the first two entries — and only advances the record pointer. The command
  walk then rebases the nonzero entries, including an entry that can sit inside
  those 12 bytes. Entry 0 is zero in every retail payload. Other zero words are
  command indices that name no sequence. Four payloads have a sequence count of
  0. The index then starts at `sequences.offset + 4`, the sequence table's
  first-command slot, and holds one zero entry.

Then:

```c
Gp_CapGlyphs = file->glyphs.cells;
Gp_CapCmds   = file->commands.table->entries;
```

`Gp_CapCmds[i].command` is the `CapCommand*` in slot zero, and
`Gp_CapCmds[i].sequence` is that same address indexed as `CapSequenceRecord`
values. Playback starts at slot one.

## 3. Event records — `CapSequenceRecord` (0xC)

```
0x0  u8  control.text.title          title glyph + 1; 0 means no title
0x1  u8  control.text.flags          CAP_SEQUENCE_* flags
0x2  u8  control.text.displayFrames  frames displaying completed text
0x3  u8  control.text.pauseFrames    following frames without text; 255 waits for resume
0x4  u8  trigger.soundAndTextFlags   bit 0 instant text; bits 1-7 room sound ID
0x5  u8  key                         variant matched against Gp_CapEventKey
0x6  u8  actionId                    0 none; 1-100 flag/item action; >100 non-type-9 scene child ID + 100
0x7  u8  minDisplayFrames            minimum elapsed frames before advancing after confirmation
0x8  CapTextRef textRef              file-relative byte offset, then const u16* text; -1 ends the run
```

The flag byte selects additional interpretations of the control bytes.
`CAP_SEQUENCE_VIEW_CONTROL` (`0x80`) selects `control.scene`: byte 0 is
`view`, byte 2 is `messageValue`, byte 3 is `messageDelayFrames`, and byte 4
is `trigger.messageRecipient` (0 player, 1 companion, otherwise placed-actor index +
2). `CAP_SEQUENCE_DELAYED_MESSAGE` (`0x08`) enables that delayed message.
An action ID above 100 is decoded by subtracting `CAP_SEQUENCE_CHILD_ACTION_BASE`
and sending `SCENE_MESSAGE_FIND_OTHER_CHILD`, which matches a byte ID on children
outside type 9. This differs from the placed-actor lookup used by delayed messages.
For an action record, byte 0 is `control.action.fallbackKey`: declining an
item action or finding it unavailable can select that nonzero variant key.

For text, `CAP_SEQUENCE_LEFT_ALIGN` (`0x02`) disables centering,
`CAP_SEQUENCE_FORCE_CARET` (`0x04`) requests the caret, and
`CAP_SEQUENCE_TITLE_BANK` (`0x10`) adds 256 to the title selector. Scene flags
`0x20` and `0x40` request the control handshake and select its phase. Bit 0 of
the flag byte has no observed playback consumer. Both timing bytes zero select
manual confirmation. A nonzero `minDisplayFrames` disables instant text; if
confirmation comes early, playback waits the remaining minimum before
processing the next record. Shared captions use the same instant-text bit to
suppress their caret.

`CapTextRef.offset` holds a file-relative byte offset before relocation. Adding
the CAP file base to that word makes `CapTextRef.text` a borrowed `const u16*`
code stream; the containing file must stay loaded during playback. Text codes
are u16 elements, with 0xFFFF ending the stream. The separate table-end sentinel
`CAP_TEXT_REF_END` (`-1`) is never relocated or dereferenced.

`Gp_FindCapEvt(start)` scans forward from `start` through `Gp_CapTable` and
stops at the first record whose `textRef.offset == CAP_TEXT_REF_END` **or** whose `key` equals
the current `Gp_CapEventKey`, returning the index. So an event slot is a run of
records terminated by `-1`, and the key selects a variant within the run.

## 4. Command records — `CapCommand` (0xC)

```
0x0  u8 opcode         CAP_COMMAND_PLAIN, COUNTER, FLAG, ROOM or TALLY
0x1  u8 flags          CAP_COMMAND_WRAP, CAP_COMMAND_PERSIST, CAP_COMMAND_BRANCH
0x2  u8 counterLimit   highest counter value that still plays
0x3  u8 flagIndexLo    low byte of the game-flag nibble index
0x4  u8 counter        live file counter when PERSIST is clear; 0 in every shipped command
0x5  u8 bitFlagIndex   first current-stage two-bit flag (TALLY)
0x6  u8 bitFlagCount   how many consecutive two-bit flags TALLY reads
0x7  u8 flagIndexHi    high byte of the game-flag nibble index
0x8  u8 nextIndex      command-table index taken when BRANCH skips playback
0x9  u8 slotTail[3]    unread; zero in every shipped command
```

The record is one sequence slot wide, the same 12 bytes as a
`CapSequenceRecord`. Bytes 9..11 have no command reader. The game-flag nibble
index is `flagIndexLo | (flagIndexHi << 8)`.

`Gp_RunCapCmd(index, mode)` walks the command table in a loop. `nextIndex`
replaces the index, so a branch chains without recursion.

## 5. Opcodes

`CAP_COMMAND_PLAIN`, `CAP_COMMAND_COUNTER`, `CAP_COMMAND_FLAG` and
`CAP_COMMAND_TALLY` finish by starting playback at this command's sequence.
`CAP_COMMAND_ROOM` gives the command index to the room and does not start
playback itself. The playing opcodes differ in how the variant key is chosen
and whether state advances.

### 0 — plain

```c
Gp_StartCapSlot(index, mode, 0);
```
Always variant 0. One unconditional line.

### 1 — counter

The only opcode that mutates state.

```c
val = (flags & CAP_COMMAND_PERSIST) ? gameFlagGetNibble(flagId) : command->counter;
if ((flags & CAP_COMMAND_BRANCH) && command->counterLimit < val)  goto nextIndex;
Gp_StartCapSlot(index, mode, val);
if (val < command->counterLimit || (flags & CAP_COMMAND_BRANCH)) val++;
else if (flags & CAP_COMMAND_WRAP)                               val = 0;
(flags & CAP_COMMAND_PERSIST) ? gameFlagSetNibble(flagId, val) : (command->counter = val);
```

- `CAP_COMMAND_PERSIST` decides **where the counter lives**: a save-game nibble
  (persistent across rooms and saves) or `counter` in the record itself
  (resets when the file reloads).
- `CAP_COMMAND_BRANCH` turns the limit into a **branch condition** rather than a
  clamp — once past it, control jumps to `nextIndex` instead of speaking.
- `CAP_COMMAND_WRAP` stores 0 at the limit; without it, and without BRANCH, the
  counter sticks.

That triple covers "say it once", "cycle through N lines", "say N times then
something else".

### 2 — flag-indexed

```c
Gp_StartCapSlot(index, mode, gameFlagGetNibble(flagId));
```
Variant is read straight from a game flag. No mutation — the line follows story
state that something else owns.

### 3 — delegate

```c
taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), 0x13F0, index, 0);
```
Hands the decision to slot 7's task with message `0x13F0`. The room decides;
the file only marks the hand-off point.

### 4 — tally 2-bit flags

```c
val = 0;
for (i = 0; i < command->bitFlagCount; i++)
    if (areaGetCurrentObjectState(command->bitFlagIndex + i) == 0 ||
        areaGetCurrentObjectState(command->bitFlagIndex + i) == 1 ||
        areaGetCurrentObjectState(command->bitFlagIndex + i) == 3)
        val++;
if ((flags & CAP_COMMAND_BRANCH) && val == 0) goto nextIndex;
Gp_StartCapSlot(index, mode, val);
```
Variant is how many of a run of current-stage two-bit flags have value 0, 1 or
3. With `CAP_COMMAND_BRANCH`, a zero tally continues at `nextIndex` instead of
playing.

## 6. Checked against a real file

`assets/USA/raw/pe2cap2/pe2cap2_4.pe2cap2`. `.pe2cap2` payloads are stored
opaque (§3.6 of `ASSET_FORMATS.md` — no LZSS), so the extracted bytes are what
the loader sees.

**The magic on disc is `"CAP2"`, four characters.** `capRelocateFile` compares
only three, so any `CAP*` passes. Do not write a 4-byte comparison.

**One raw chunk is a resource bundle containing images and data blobs.** This
file has CAP2 headers at `0x1AA0` and `0x29F0`. Its first `0x320` bytes contain
fifty 16-byte `_FsCdfResourceEntry` descriptors; the rest of the first `0x7F0`
payload bytes is zero. Each descriptor holds a resource kind (0 empty, 2 image,
3 untyped data), an unread byte that is zero in every retail bundle, the count
of payload sectors written before a redirect, the resource's byte length, an
absolute RAM destination, and an optional later write pointer. Retail bundles
leave the redirect pair zero, and destinations are 16-byte aligned. `Fs_ProcessChunkHeader` publishes
only the kind and destination as `FsResourceSlot` entries in `D_8006C338`,
before streaming the subsequent sectors into RAM. These resource kinds are
distinct from the outer CDF chunk opcodes.

Here slots 2..7 describe six image resources starting at raw offset `0x7F0`,
with RAM destinations beginning at `0x80188920`. Slots 14 and 15 describe the
two CAP2 blobs, at `0x80189BD0` and `0x8018AB20`. Caption selection counts
kind-3 resources in directory order; that kind can also hold STF credits or
MDEC bitstreams in other bundles, so it does not by itself identify a CAP file.

Header at `0x1AA0`, matching §1 exactly:

```
"CAP2"  field_4=0x8  glyph=+0x14  sequences=+0xB30  commands=+0xF00
```

**The command index addresses sequence commands.** Count 18. Entry 0 is zero,
index 6 is zero, and the other entries are file-relative offsets `0xB34`,
`0xB64`, `0xB88`, `0xBA0`, …. `0xB34` is exactly `sequences + 4`,
`firstCommand`. Gaps between nonzero entries are whole multiples of 12
(`0xB64-0xB34 = 4 records`, `0xB88-0xB64 = 3`, `0xBA0-0xB88 = 2`).

Each sequence starts with a **command header in slot zero**, followed by
`CapSequenceRecord` playback records from slot one. `Gp_RunCapCmd` reads the
header as `CapCommand`; `Gp_StartCap` stores the same base pointer as
`Gp_CapTable` but initializes its record index to one. The command and playback
records have different meanings. Relocation starts at `sequences + 0x10`
(`records`), after the first command, and its extra step after a terminator
skips the next sequence's command.

Run termination confirmed: within the first run the fourth record has
`textRef.offset == CAP_TEXT_REF_END`.

**Glyph cells and text codes are separate records.** `glyphs.cells` points to
four-byte `TextGlyphCell` entries: unsigned texture U, texture V, width and
height. CAP quads use the extents as both screen-space and texture-space corner
deltas; ordinary text advances by `width - 1`, and line height uses `height + 2`.
Title labels index the same table; inline icons use the separate four-cell
`D_8010FB70` table. Texture-page and palette selection are outside the cell.

The `+0x14 .. +0xB30` region includes the glyph table and text referenced by
events. Interpreting that entire region as halfwords yields 1422 `u16` values,
107 distinct, 1241 below `0x100`, and 108 at least `0x8000`; those measurements
mix cell bytes with text and do not establish a character encoding. Rendering
uses a text code's low ten bits as its glyph-cell index after handling controls.

## 7. What is still open

- **Glyph index -> character.** The values are indices into `Gp_CapGlyphs`;
  turning them into readable text needs that table plus the font image. The
  mapping is not alphabetical by inspection (`0x21` as `A` does not produce
  words).
- **Control codes.** Values `>= 0x8000` are mostly still undecoded. A dialogue
  choice is the exception: a code whose high byte is `0x81`, `0x82` or `0x83`.
  Its low byte is the variant key copied into `Gp_CapEventKey` on confirm, and
  bits 8-11 select the confirm sound (`1` plays `SOUND_SYSTEM_CONFIRM`, `2`
  plays nothing, `3` plays `SOUND_SYSTEM_CURSOR`). Retail text uses `1` and
  `2` only. The renderer records the pen and those fields in a `CapChoice`
  (`src/gameplay/cap.h`).
- **`CapFile.field_4`** is `8` in all 213 retail CAP payloads. No loader reads
  it, and the role is unproven.
- **Message `0x13F0`** (opcode 3) - the payload contract with slot 7's task.
- **What `mode` selects.** `Gp_StartCap` sets a text-box geometry
  (`0x30`, `0xC0`, `0x140`, `7`) but the per-mode differences are untraced.

## 8. Why this matters beyond extraction

CAP is the closest thing the game has to a room scripting language, and it is
deliberately narrow: five opcodes over counters, game-flag nibbles and 2-bit
progress flags. It selects dialogue; it does not spawn, move or branch the
world.

Anything designing a room DSL should read that as a boundary. CAP is a good
target for the "which line, how many times" part of a room and a bad target for
room logic, which retail keeps in the overlay's C.
