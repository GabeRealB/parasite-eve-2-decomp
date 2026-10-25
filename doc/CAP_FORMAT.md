# CAP dialogue (`cap`)

| | |
|---|---|
| Magic | `"CAP2"` (loaders compare 3 bytes) |
| Asset type | `cap`, `raw/cap/<room>_dialogue_<i>.cap`; decoded `cap/<name>.json` |
| Container | data resource of a room bundle ([`BUNDLE_FORMAT.md`](BUNDLE_FORMAT.md)) |
| Companion asset | its glyph atlas, `font_atlas` (§9) |
| Code | `src/gameplay/cap_reloc.c`, `cap_commands.c`, `captions.c`; `include/gameplay/cap.h` |
| Tools | `tools/peassets/cap_decode.py`, `cap_dialogue.py`, `cap_text.py`, `cap_font.py` |

A room runs command *n* (`capRunCommand`); the command chooses a key, and the
records with that key play: lines in the caption box, and scene steps around them.

## 1. File

| Order | Part | Size |
|---|---|---|
| 1 | header | `0x14` |
| 2 | glyph cells | 4 × (highest cell used + 1) |
| 3 | text streams | one per record, in record order; optionally one more, empty |
| 4 | sequence table | 4 + 12 × slots |
| 5 | command index | 4 + 4 × entries |

No padding between parts. All offsets are file-relative until relocation (§8).

## 2. Header (`CapFile`)

| Offset | Type | Field | |
|---|---|---|---|
| `0x00` | `char[4]` | `magic` | `"CAP2"` |
| `0x04` | `s32` | `field_4` | 8; unread |
| `0x08` | `u32` | `glyphs` | offset of §3; always `0x14` |
| `0x0C` | `u32` | `sequences` | offset of §5 |
| `0x10` | `u32` | `commands` | offset of §8 |

## 3. Glyph cells (`TextGlyphCell`, 4 bytes)

| Offset | Type | |
|---|---|---|
| `0x0` | `u8 u, v` | position in the atlas |
| `0x2` | `u8 w, h` | size; text advances `w − 1`, lines `h + 2` |

| Cells | |
|---|---|
| 0..24 | speaker plates, indexed by speaker number (AYA 0, PIERCE 3, BALDWIN 8; `???` stands for several); only the speakers shown are filled, the rest are 0 |
| 25.. | glyphs, numbered by first use in the text streams |

A cell is a picture, not a character: the same index is a different character in each dialogue.

## 4. Text streams

`u16` codes, ending in `0xFFFF`.

| Code | Meaning |
|---|---|
| `FFFF` | end |
| `FFFE` | line break; ends an open choice |
| `FFFD` | spacer, 3 pixels |
| `8000 \| v` (mask `9F00`) | view change while drawn: `v` a view file, or a logical view with `0x2000`; `0x4000` hides player and companion when playing in place |
| `81xx` / `82xx` / `83xx` | choice: its reply sets the key to `xx`; confirm sound `SOUND_SYSTEM_CONFIRM` / none / `SOUND_SYSTEM_CURSOR` |
| `84xx` | inline icon `xx` of `gCapInlineIconCells` (4 cells, tpage `0x1E`, CLUT `0x3C00`) |
| below `8000` | glyph: bits 0..9 cell, 10..11 palette (CLUT `0x3D50` + palette), 12..14 reveal delay |

## 5. Sequence table (`CapSequenceTable`)

| Offset | Type | Field | |
|---|---|---|---|
| `0x0` | `s16` | `count` | records, terminators included |
| `0x2` | `s16` | `slotSize` | 12; unread |
| `0x4` | slots | | per command: its command (§6), its records (§7), a terminator |

A terminator is a record with `textRef` = `-1` and every other byte 0.
An empty dialogue has `count` 0 and no slots.

## 6. Command (`CapCommand`, 12 bytes)

| Offset | Field | Decoded | Read by |
|---|---|---|---|
| `0x0` | `opcode` | `opcode` | all |
| `0x1` | `flags`: `0x01` wrap, `0x02` persist, `0x04` branch | `wraps`, `persists`, `branches` | counter (all three), tally (branch) |
| `0x2` | `counterLimit` | `limit` | counter |
| `0x3` | `flagIndexLo` | `game_flag` (low) | counter with persist, flag |
| `0x4` | `counter` | `counter` | counter without persist; 0 in retail |
| `0x5` | `bitFlagIndex` | `tally_first` | tally |
| `0x6` | `bitFlagCount` | `tally_count` | tally |
| `0x7` | `flagIndexHi` | `game_flag` (high) | as `0x3` |
| `0x8` | `nextIndex` | `next` | counter, tally, with branch |
| `0x9` | 3 bytes | `tail_0`..`tail_2` | none; 0 in retail |

| Opcode | Starting key | Then |
|---|---|---|
| 0 plain | 0 | play |
| 1 counter | the counter: the game-flag nibble with persist, else `counter` | branch and value > limit: run `next` instead. Else play, then value + 1 while below the limit or with branch; at the limit, 0 with wrap, else unchanged |
| 2 flag | the game-flag nibble | play |
| 3 room | — | send `ROOM_MESSAGE_COMMAND` (`0x13F0`) with the command's number to the room; nothing plays |
| 4 tally | how many of the `tally_count` stage objects from `tally_first` have state 0, 1 or 3 | branch and 0: run `next` instead. Else play |

A trigger with action `WORLD_COLLISION_TRIGGER_ACTION_CAP` runs command
`parameter0`, or room command `parameter0` when `parameter1` is
`WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE` (`0xFF`).

## 7. Records (`CapSequenceRecord`, 12 bytes)

Playback runs a command's records in order, skipping those whose `key` (byte 5)
is not the current key. A choice, or a declined action with a fallback key,
changes the key. Flag `0x80` makes a record a scene step; otherwise it is a line.

### 7.1 Line

| Offset | Field | Decoded | |
|---|---|---|---|
| `0x0` | `title` / `fallbackKey` | `speaker` / `fallback_key` | speaker number + 1 (0 none); with an action, the key a decline selects |
| `0x1` | `flags` | `left_aligned` (`0x02`), `caret` (`0x04`) | |
| `0x2` | `displayFrames` | `show_frames` | frames showing the completed text |
| `0x3` | `pauseFrames` | `pause_frames` | empty frames after; 255 waits to be resumed; both 0: the player confirms |
| `0x4` | `soundAndTextFlags` | `instant` (bit 0), `room_sound` (bits 1..7) | instant reveal; `ROOM_MESSAGE_SOUND` with the sound, once |
| `0x5` | `key` | `key` | |
| `0x6` | `actionId` | `action` | see below |
| `0x7` | `minDisplayFrames` | `minimum_frames` | holds a confirmation; nonzero turns off instant |
| `0x8` | `textRef` | `text` | |

| `actionId` | Action |
|---|---|
| 0 | none |
| 1..100 | placed object *n* of the stage (its two-bit state, its `AreaObjectPlace` entry). State 2: passes as accepted. Else `itemPickupPublishPlacedObject` prompts for the item or key item (accepted: collected, unless replenishing), the save point (telephone and save menu) or the transfer screen; declined or unavailable: key ← `fallbackKey` if nonzero |
| 101..255 | scene child *n* − 100 (`SCENE_MESSAGE_FIND_OTHER_CHILD`, children outside type 9) receives `CAP_ACTION_MESSAGE_REQUEST` |

Not read on a line: flags `0x01`, `0x08`, `0x10`, `0x20`, `0x40`.

### 7.2 Scene step

| Offset | Field | Decoded | |
|---|---|---|---|
| `0x0` | `view` | `view` | view file to change to (logical with EVS opcode 41 `SET_CAP_DIRECT_VIEW_IDS`); 0 keeps the view; applied as the next line starts |
| `0x1` | `flags` | `send_message` (`0x08`), `scene_sync` (`0x20`), `wait_for_cue` (`0x40`) | |
| `0x2` | `messageValue` | `message` | with `0x08` |
| `0x3` | `messageDelayFrames` | `message_delay` | with `0x08` |
| `0x4` | `messageRecipient` | `message_to` | with `0x08`: 0 player, 1 companion, 2..17 placed actor 0..15 |
| `0x5` | `key` | `key` | |
| `0x6`..`0xB` | | | not read |

| Flag | Effect |
|---|---|
| `0x08` | after `message_delay` frames: player or companion get `GAME_ACTOR_MESSAGE_SET_TEXTURE_SEQUENCE` with `message` (mouth and expression); a placed actor gets an eye mode (`capDelayedTextureMessageTask`) |
| `0x20` | after the view change, sync with the caption control task (`CAP_CONTROL_MESSAGE_BEGIN_SCENE_SYNC`) |
| `0x40` | hold until an event script waits for a cue (`CAP_CONTROL_EVENT_SCRIPT_WAITING_FOR_CUE`), then consume it |

A scene step advances in the same tick: its text is never drawn, and the byte 4
and `minDisplayFrames` loaded for it are replaced by the next record's.

## 8. Command index and relocation

| Offset | Type | |
|---|---|---|
| `0x0` | `s32 count` | highest command number + 1 (1 when empty) |
| `0x4` | `u32 entries[count]` | offset of command *n*'s slot, 0 for none; entry 0 is always 0 |

`capRelocateFile`: adds the file base to the three header offsets, to every
record's `textRef` except `-1` (a terminator also skips the next slot, the
next command), and to every nonzero index entry; publishes `gCapGlyphCells`
and `gCapCommandEntries`. Nothing is rebased when `glyphs` is not positive, so
a second call only republishes.

## 9. Glyph atlas (`font_atlas`)

| | |
|---|---|
| Stored | a 4-bit image chunk, extracted like a `pe2img` |
| Asset | `<dialogue>_atlas`, `generated_by: <dialogue>` |
| Where | the room's disc file (room dialogue) or a view's file (cutscene) |
| Texture page | (384, 0); the room cutscene task sets `capTPageX`/`Y`, or (960, 0) when `capTPageX` is 0. Some atlases are uploaded elsewhere: (384, 256) for an actor's caption (`dryfield_night_garage_dialogue_1`), (832, 0) for the heliport's two, and the shooting gallery's two side by side in one image at (704, 0) and (768, 0), the room choosing the page by command range (`generated_by` lists them in page order) |
| Without one | a dialogue drawing no glyph and no plate has none; one generating none draws on its folder's (another dialogue's), checked on its cells' pixels |

Layout rule: walking the text streams in order, each new cell is placed when
first used, a speaker plate after the glyphs of the first line showing it;
left to right, a new row 14 pixels down when an item would pass x = 256.
All other pixels are 0.

A second layout tool wrote two dialogues, MIST room 21 (an earlier take of the parking lot's talk with Pierce) and
Dryfield r04. It numbers glyph cells from 0 instead of 25 and has no speaker plates: a title draws the glyph cell it
names, as `_capDrawTextStream` always does. Its glyphs are a bolder face (`dialogue/font_bold`, with r04's in
Japanese), and its atlas carries a ▼ at (242, 242) that no cell reaches. A decoded dialogue states this in `atlas`:
`face`, `first_glyph`, and `marks` (glyphs of the face by position). Dryfield r04's cells were laid out for other
text over a Japanese page that was never redrawn, so they slice across its glyphs; the game draws those slices,
and the decode keeps them as `stored_cells`, its text referring to them as `{cell n}`. Its page is an image of its
own (`dryfield_r04_glyph_page`), not one the dialogue generates.

## 10. Retail invariants

Over the 166 distinct dialogues (213 in bundles). Each makes a field derivable.

| | |
|---|---|
| `field_4` = 8, `slotSize` = 12, `glyphs` = `0x14` | all |
| command slots in number order | all; command number = list position |
| one text stream per record, in record order, none shared | all |
| `limit` = highest key among the command's records | 1328 of 1333 commands |
| a tally's `tally_count` = highest key | 69 of 72 |
| unread `tally_count`: 1 in counter, flag and room commands, 0 in plain | most |
| unread scene `message_to` without `0x08`: 1 | 2089 of 2111 |
| glyph cells numbered by first use | all |
| atlas follows the layout rule, the cell table holds nothing else | 105 of 162 decoded |
| a cast speaker's cell (a speaker no record shows) has its plate's size | 627 of 637 (the rest: numbers no dialogue shows) |

## 11. Decoded form

`cap_decode.py decode <dialogue|bundle>` (and the extraction, for all) → `assets/USA/cap/<dialogue>.json`;
`encode` rebuilds the dialogue and its atlas. Nothing is written unless both
rebuild exactly.

```json
{"commands": [null, {"opcode": "counter", "records": […]}, …,
  {"opcode": "counter", "persists": true, "game_flag": "0x004", "unread": {"tally_first": 1},
   "records": [
    {"key": 0, "type": "line", "text": "{view 12}A SWAT ammunition box...\nIt's filled with 9mm rounds.\n", "room_sound": 5},
    {"key": 0, "type": "line", "action": {"placed_object": 1}, "instant": true}]}],
 "trailing_empty_text": true}
```

| Element | |
|---|---|
| `commands` | position = command number; `null` for none; ends at the last command |
| command, record fields | the decoded names of §6, §7; written only where they differ from §10's defaults |
| `unread` | bytes the command's opcode or the record's type does not read |
| `text` | characters; markup `{view N [logical] [hide]}`, `{choice confirm\|silent\|cursor K}`, `{icon N}`, `{spacer}`, `{palette N}`, `{delay N}`, `{glyph name}`, `{code XXXX}` |
| `speaker` | plate name; `name#N` when the plate stands for several numbers |
| `trailing_empty_text` | the extra empty stream (§1) |
| `atlas` | what the layout rule does not imply: `placed`, cells the atlas holds somewhere else than the rule puts them (`{cell: [x, y]}`; 349 cells in 18 dialogues); `cast`, the cells of speakers no record shows, `[x, y, w, h]` as stored (715 in 41 dialogues; none is drawn); `drawn`, those whose plate is on the atlas; `cells`, a table longer than its cells need (4) |

Pictures go to the dialogue font, two UFO 3 fonts (`dialogue/font` and
`dialogue/speakers`, `font` records of `layout = "images"`): a glyph per
character, named and mapped to it, and a glyph per speaker plate, named for the
speaker with its numbers in its lib. A glyph's image is its picture, 4-bit
indices read back by index (0 is clear), written in the first text palette
(256, 245) or the plates' (304, 246); it advances by its width and stands on the
baseline, as `captions.c` draws it. The extraction decodes every dialogue after
writing stages.json (`extract.decode_dialogues`), so the font holds every
picture the decodable dialogues show: 83 glyphs, 25 plates. First recognition: `cap_glyphs.tsv`,
`cap_plates.tsv` (hashes, no pixels; a plate's speaker numbers in its third
column, so names do not depend on decode order); characters drawn alike share a
line, and the dialogue's `glyphs` field in `asset_data.py` (`{cell: "I"}`)
chooses. Without one, `I` against `l` is read from the cell's uses: a lowercase
letter before it makes it `l`, capitals beside it, standing alone or before an
apostrophe `I`; at a word's start a vowel after it `l` (lever), a consonant `I`
(It, Ice). Two cells of one dialogue sharing the picture are one of each. Every
glyph and plate of the 162 decodable dialogues is in the tables (81 glyphs, 25
plates).
`<dialogue>.warnings.json`: per cell, an unsettled ambiguity (with the guess),
an unseen glyph or plate, or two cells with one character.

## 12. Open

| | |
|---|---|
| `field_4` | always 8; role unknown |
| flag `0x01` | no reader |
| flag `0x10` (`CAP_SEQUENCE_TITLE_BANK`) | on 96 lines, 92 without a speaker; passed to both caption renderers, ignored by both |
| atlas layout | the cast: a cell table lays out a plate for every speaker of some cast, at positions on its glyphs and on each other, not where the atlas was drawn - the layout that placed them is not known, so `cast` keeps them as stored; 18 atlases place cells off the rule (`placed`) |
