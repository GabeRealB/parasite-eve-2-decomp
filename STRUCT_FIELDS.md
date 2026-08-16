# Known struct fields

Catalog of **proven** field roles. Members are renamed in C when the role is
safe; remaining `field_XX` names stay until then. Sources: usage in matched C,
existing header comments, and `DECOMPILATION_LEARNINGS.md`.

Convention: only list fields with evidence. Unlisted `field_*` / `unknown_*` /
`pad_*` remain unknown.

---

## Task system (`task.h`)

### `Task` (0x48)
| Off | Member | Role |
|-----|--------|------|
| 0x00 | `node` | Intrusive list links (`next` / `prev`) |
| 0x08 | `parent` | Parent task (NULL if root); children clear this on kill |
| 0x0C | `firstChild` | First child (NULL if none); sibling ring via `nextSibling` |
| 0x10 | `nextSibling` | Next sibling in parent’s child ring (self if only child) |
| 0x14 | `callback` | Per-frame callback (`TaskFunc`) |
| 0x18 | `exitCallback` | Exit / kill callback (often `Task_Kill`) |
| 0x1C | `idMap` | Optional `TaskIdMap*` (freed on kill) |
| 0x20 | `spawnArg2` | Spawn arg2 — often `UiObject*` / UI context |
| 0x24 | `field_24` | Pointer to `GpMsgEntry` id/handler table (`D_8010D208` via `func_800B6358`, `D_8010FB90` via `func_800E71B0`); walked by `func_800AC464` |
| 0x28 | `spawnType` | Spawn type (desc flags low byte: 0 bare, 1/2 overlay) |
| 0x29 | `priority` | List priority (lower runs earlier) |
| 0x2A | `killCountdown` | Deferred-kill countdown / state |
| 0x2C | `extra` | Spawn extra (`GameActorExt*`, overlay object, …) |
| 0x30 | `state` | Generic state word (handlers / kill path) |
| 0x34 | `spawnArg1` | Spawn arg1 — menu/ctx pointer, mode, …; `func_800E73E8` writes 1 here on `D_801156B8` instead of `Task_Kill` |
| 0x38 | `flags` | Small flag byte |
| 0x3C | `extraState` | Extra state word |

### `TaskDesc` (0xC)
| Off | Member | Role |
|-----|--------|------|
| 0x0 | `flags` | Low byte spawn type; bit 0x100 type-1 setup arg |
| 0x2 | `priority` | Low byte → `Task::priority` |
| 0x4 | `callback` | Per-frame entry |
| 0x8 | `setupArg` | Extra arg to type-1 setup (`func_80099170`) |

### `TaskIdMap` (0x8)
| Off | Member | Role |
|-----|--------|------|
| 0x0 | `index` | Index into `table` |
| 0x4 | `table` | `TaskIdPair*` id/type table |

### `TaskIdPair` (0x2)
| Off | Member | Role |
|-----|--------|------|
| 0x0 | `id` | Id |
| 0x1 | `type` | Type |

---

## Title overlay (`title.h`)

### `TitleWork` (0x18) — stored in `Task::idMap` slot on title menu task
| Off | Member | Role |
|-----|--------|------|
| 0x00 | `timer` | Frame / phase counter (intro window, fade timing) |
| 0x04 | `selection` | Menu cursor index (spawn id via `Title_MenuSpawnIds`) |
| 0x08 | `fadeTileEnable` | Non-zero → draw fullscreen fade TILE |
| 0x0C | `logoFade` | Intro logo alpha ramp 0..0x80 |
| 0x10 | `menuFade` | Menu chrome alpha ramp 0..0x80 |
| 0x14 | `menuCount` | Entry count (wrap / skip logic) |

---

## Display (`display.h`)

### `DisplayState` (0x138)
| Off | Member | Role |
|-----|--------|------|
| 0x18 | `width` | Display width |
| 0x1A | `height` | Display height |
| 0x1C | `interlace` | Interlace enable |
| 0x20 | `dispEnv[2]` | Dual `DISPENV` |
| 0x48 | `drawEnv[2]` | Dual `DRAWENV` |
| 0x108 | `vsyncFlag` | VSync-facing volatile flag |
| 0x109 | `vramYOffset` | VRAM Y offset for image transfer |
| 0x118 | `frameMode` | Frame / mode word used by stage flow |
| 0x123 | `skipTeardown` | Kill-path gate (`Task_Kill` skips overlay teardown if set) |
| 0x124 | `region` | Region / rate select (1 → PAL paths, CdStream sector count) |
| 0x128 | `field_128` | Drawn into scratch by model path |

---

## Pad (`pad.h`)

### `PadState` (0x5C)
| Off | Member | Role |
|-----|--------|------|
| 0x00 | `status` | Pad status halfword (`0xFF` init; `0x73` analog path) |
| 0x02 | `eventIdx` | Ring index into `events` banks |
| 0x03 | `initialized` | Init flag (set 1 in `Pad_Init`) |
| 0x04 | `buttons` | Current buttons |
| 0x06 | `prevButtons` | Previous / held mask |
| 0x08 | `triggered` | Triggered / edge mask |
| 0x0A | `cooldown` | Cooldown counter |
| 0x0B | `autoRepeat` | Auto-repeat timer |
| 0x10 | `events[2][8]` | Two banks of `PadEvent` |
| 0x50–0x56 | `field_50`… | Analog stick related |
| 0x5A/5B | `field_5A/5B` | Cleared at init |

### `PadEvent` (0x4)
| Off | Member | Role |
|-----|--------|------|
| 0x0–0x2 | `field_0/1/2` | Event payload (cleared as sb/sb/sh) |

### `PadScratch` (0x6)
| Off | Member | Role |
|-----|--------|------|
| 0x0 | `buttons` | Current inverted buttons |
| 0x2 | `prevButtons` | Previous frame |
| 0x4/5 | `rawLo` / `rawHi` | Raw port bytes |

### `PadRawPort` (0x24)
| Off | Member | Role |
|-----|--------|------|
| 0x2/3 | `field_2/3` | Big-endian button halfword source |

### `PadRemapState` (0x1C)
| Off | Member | Role |
|-----|--------|------|
| 0x3 | `field_3` | Cleared by `func_800AC164` teardown |
| 0x8 | `field_8` | Pad input remap mode (nonzero → overlay remap path) |

---

## Memcard (`mc.h`)

### `McWork` (large)
| Off | Member | Role |
|-----|--------|------|
| 0x10/14 | `field_10/14` | `MemCardSync` cmd/result outs |
| 0x18 | `field_18` | Source buffer / write address |
| 0x1C | `field_1C` | Sector/offset (<<7 for write) |
| 0x20 | `field_20` | Byte count |
| 0x30 | `field_30[15][0x28]` | Directory entries |
| 0x288 | `field_288` | Directory entry count |
| 0x28C | `field_28C` | Free-block count |
| 0x290 | `field_290` | `Mc_FileName` match index |
| 0x294 | `field_294[15][0x20]` | Per-slot 0x80 read buffers |
| 0xA14 | `field_A14` | Selected slot index |
| 0xA1C/A1E | checksum pair | Over 0x200 bytes of buffer |
| 0xA20 | `field_A20` | Flag; high byte first map slot |
| 0xA24 | `field_A24[]` | Block→direntry map |

### `McSaveData`
| Off | Member | Role |
|-----|--------|------|
| 0x4–0x9 | `field_4`… | Header region (checksummed from 0x4, size 0x38) |
| 0x10 | `field_10` | Init bitmask; bit 0 = global init done (`func_800AB980`). Per-slot bit is `field_7` via the `GameSessionFrom4` overlay |
| 0x12 | `field_12` | Slot index 1..16 |
| 0x13 | `field_13` | 1-based index into `D_80113360` (`func_800E3D24`); also `D_8007217B` |
| 0x5C7 | `field_5C7` | signed addend for the `D_80113360` lookup (`func_800E3D24`); also `D_8007272F` |
| 0x5C8 | `field_5C8[]` | 32 `McItemSlot`s; `func_800BBE54` clears each and sets `field_2` to 0xFF except index 0x1A |
| 0x1C/1E | checksum pair | Save header sum / ones-complement |
| 0x6C8/6CA | `field_6C8/6CA` | Halfword pair; `func_800AB980` inits both to 100 on first visit |
| 0x6D0 | `field_6D0[]` | 96-word bit flags; `func_800BB7C0` sets (`arg1 != 0`) / clears (`arg1 == 0`) bit `id` for `id < 0x180`; `func_800BC06C` tests (else returns 1); `func_800BBF84` clears all 96 words |
| 0x888 | `field_888[]` | 1-based `s32` counters; increment capped at 0x1869E (`func_80106518`) |
| 0x908 | `field_908[]` | 32 signed addends for item ids 0x60–0x7F (`func_800BC324`) |
| 0x93C | `field_93C` | Save-data checksum halfword |
| 0x940/942 | checksum pair | Over first byte of buffer slots 1..8 |

### `McChecksumBlock`
| Off | Member | Role |
|-----|--------|------|
| 0x0/2 | `field_0/2` | Sum / ones-complement |
| 0x4 | `field_4[]` | Payload |

### `McBufferSlot` (0xC)
| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | `McChecksumBlock*` buffer |
| 0x4 | `field_4` | Buffer size |
| 0x8 | `field_8` | Extra word |

### `McPromptPair` (0x8)
| Off | Member | Role |
|-----|--------|------|
| 0x0/4 | `field_0/4` | Prompt / dialog data pointers |

---

## CD / FS (`fs.h`)

### `CdCmdEntry` (0x8)
| Off | Member | Role |
|-----|--------|------|
| 0x0–3 | `idB0`…`idB3` | Packed file id digits |
| 0x4 | `cmd` | Opcode (0x21 load, 0x54 stage, 0x55 HED, …) |
| 0x5–7 | `param0`…`param2` | Command params |

### `CdCmdQueue` (selected)
| Off | Member | Role |
|-----|--------|------|
| 0x00 | `entries[8]` | Ring of commands |
| 0x1C8 | `writeIdx` | Enqueue index |
| 0x1CA | `readIdx` | Execute index |
| 0x1D0 | `step` | Sub-state of current command |
| 0x48 | `field_48` | Last `CdPosToInt` |
| 0x58 | `field_58[5]` | Stream decode slots |
| 0x190 | `field_190` | `CdCmd190*` stream descriptor |
| 0x1A8 | `field_1A8` | Copied to `D_80070F60` by `func_800B00C4` |
| 0x1AC | `field_1AC` | `srand` seed restored by `func_800B00C4` |
| 0x210 | `field_210` | u16 flag; `func_800A9010` sets 1 after `Stream_FindSlot` succeeds and `func_800B62D4`, clears it after `func_800B56AC` |

### `CdCmd58Entry` / `CdCmd190`
See inline comments in `fs.h` (buffer offsets, stream id, VLC slots).

### `FsCdfChunkHeader` / streams / files
See named members in `fs.h` (`type`, `endFlag`, `size`, `loadAddr`, `id`, `offset`).

---

## CD→SPU stream (`cdstream.h`)

### `CdStreamState` (0x58)
| Off | Member | Role |
|-----|--------|------|
| 0x00–03 | `flags0`…`phase` | Status bitfields / stream phase |
| 0x06 | `readySlot` | 1-based `CdReady` index |
| 0x08/0C/10 | `doneCb` / `startCb` / `voiceFreeCb` | Callbacks |
| 0x24 | `spuAddr` | Current SPU write address |
| 0x28 | `startSector` | CD start sector |
| 0x3C | `spuBase` | SPU base address |
| 0x40 | `sectorsPerChunk` | 0x18 NTSC / 0x14 PAL |
| 0x42 | `ringHalf` | 0x2770 ring half size |
| 0x44 | `countdown` | Countdown timer |
| 0x46/47 | `mtsPeriod` / `mtsParam` | MTS write cadence |
| 0x48 | `sector` | `MtsSector*` buffer |
| 0x4E | `remaining` | Remaining transfer count |
| 0x50/51 | `voiceL` / `voiceR` | SPU voice indices |
| 0x52 | `mode` | Stream mode |
| 0x53 | `flags` | bit1 = linked L/R pitch |
| 0x54 | `pending` | Pending flag |
| 0x56 | `settleCounter` | Disc init settle |

### `CdStreamChannel` (0x40)
| Off | Member | Role |
|-----|--------|------|
| 0x00 | `voiceMask` | `1 << voice` |
| 0x04 | `attr` | SPU voice attr bitfield |
| 0x08/0A | `pitch` / `pitchAlt` | Pitch pair |
| 0x14 | `field_14` | Often 0x1000 |
| 0x1C/20 | `spuAddr` / `spuAddr2` | Dual SPU addresses |

### `CdReadyEntry` / `CdReadyQueue`
See named members (`flags`, `sectorPos`, `pollFn`, `doneFn`, `errorFn`, ring indices).

### `CdStreamParams` (0x20)
| Off | Member | Role |
|-----|--------|------|
| 0x00 | `startSector` | CD sector |
| 0x04 | `spuBase` | SPU base |
| 0x08 | `sectorBuf` | Sector buffer |
| 0x0C | `doneCb` | Done callback |
| 0x10/14 | `startCb` / `voiceFreeCb` | Stored into state |
| 0x18 | `pitch` | Pitch |
| 0x1A/1B | `voiceL` / `voiceR` | Voices |
| 0x1C | `mode` | Mode |

### `MtsSector`
| Off | Member | Role |
|-----|--------|------|
| 0x08 | `magic` | High 3 bytes `"MTS"`, low byte channel count |

### `CdAudioPhase` (0x6) — phase machine in `cdaudio.c`
| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | Primary phase (`switch` driver) |
| 0x1–4 | `field_1`…`field_4` | Sub-phase words |
| 0x5 | `field_5` | Control / abort flag |

### `CdAudioLoc` (0x10)
| Off | Member | Role |
|-----|--------|------|
| 0x0/1 | `field_0/1` | Status / ready flags |
| 0x2 | `field_2` | Pitch-related halfword |
| 0x4 | `field_4` | Sector / position int |
| 0x8 | `field_8` | SPU / buffer param |
| 0xC | `field_C` | Sector position int |

### `CdAudioTbl` (0x18)
| Off | Member | Role |
|-----|--------|------|
| 0x2 | `field_2` | Index into `CdAudio_TblEntries` |
| 0xC | `field_C` | `u16*` halfword table base |
| 0x10 | `field_10` | Transfer / table param |

### `CdAudioCtl` (0x14)
| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | Busy / retry counter |
| 0x4 | `field_4` | Secondary counter |
| 0x8 | `field_8` | Phase mirror |
| 0xA | `field_A` | Error code (`-1`/`-2`) |
| 0xC | `field_C` | Countdown |
| 0x10 | `field_10` | Flag / control |

### `CdAudioTblEntry` (0x4)
| Off | Member | Role |
|-----|--------|------|
| 0x3 | `field_3` | Compared across adjacent entries for span length |

### `CdAudioLocEx` / `CdAudioVoices`
Overlays of `CdAudio_Loc` region: voice indices at +0x3E/+0x3F; sector `CdlLOC` at +0x10.

---

## Audio tick list

### `AudioTickNode` (0x18)
| Off | Member | Role |
|-----|--------|------|
| 0x0 | `poll` | `s32 (*)(s32 arg)` — returns −1 to remove |
| 0x4 | `onRemove` | Remove callback `void (*)(void)` |
| 0x8 | `id` | Node id (match key for remove) |
| 0xC | `arg` | Arg passed to poll |
| 0x10 | `prev` | Prev link |
| 0x14 | `next` | Next link |

---

## SPU / sound bank

### `SpuReverbConfig` (0x24)
| Off | Member | Role |
|-----|--------|------|
| 0x0/4 | `enableVoices` / `disableVoices` | Voice bitmasks |
| 0x8 | `reverbMode` | Mode |
| 0xC | `isDirty` | Needs apply |
| 0x10 | `attr` | `SpuReverbAttr` |

### `SpuVoiceState` (0x1D4)
| Off | Member | Role |
|-----|--------|------|
| 0x0 | `reverbVoiceStatus` | Reverb voice status |
| 0x4 | `field_4[24]` | Per-voice age / score |
| 0x64 | `field_64[24]` | Per-voice state (0/3 free-ish) |
| 0x7C | `field_7c[24]` | Key-on staging (set 5 on note-on) |
| 0x94 | `field_94[24]` | Occupied (0 free, 1 busy) |
| 0xAC | `field_ac[24]` | Alloc priority |
| 0x10C/16C | `field_10c/16c[24]` | Cleared on voice release |
| 0x1D0 | `field_1d0` | Key-on related mask |

### `SpuLVoiceTable` (0x67C)
| Off | Member | Role |
|-----|--------|------|
| 0x0 | `count` | Active attr count |
| 0x2 | `attrs[24]` | `SpuLVoiceAttr` array |
| 0x664 | `field_664[24]` | Per-voice flags |

### `SpuVoiceRange` (0x4)
| Off | Member | Role |
|-----|--------|------|
| 0x0 | `first` | First voice index |
| 0x2 | `count` | Count of voices in range |

### `SpuVoiceRef` (0x8)
| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | Voice index |
| 0x4 | `field_4` | `SpuVoiceAttr*` |

### `SndNote` (0x14)
| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | Reverb enable |
| 0x1 | `field_1` | Pan |
| 0x3 | `field_3` | Volume |
| 0x4/5 | `field_4/5` | Root key / fine |
| 0x6 | `field_6` | Priority |
| 0x8/9 | `field_8/9` | MIDI key min/max |
| 0xA/B | `field_A/B` | Bend ranges |
| 0xC/E | `field_C/E` | ADSR words |
| 0x10 | `field_10` | SPU waveform address |

### `SndBankGroup` (0x4)
| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | Group size (note count) |
| 0x2/3 | `field_2/3` | Volume / pan |

### `SndBank` (0x20)
| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | `SndBankGroup*` |
| 0x4 | `field_4` | `SndNote*` |
| 0x8 | `field_8` | Bank id (`0xFxxx` free) |
| 0xB | `field_B` | Group count |
| 0x10 | `field_10` | `u16*` group index table |
| 0x1C | `field_1C` | Heap allocation |

### `SndBankInitEntry` (0xC)
| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | Slot map index into `D_800680AC` |
| 0x2 | `field_2` | Bank id written to slot/bank |
| 0x4/6 | `field_4/6` | `SndHeap_Malloc` sizes |
| 0x8 | `field_8` | Stored to `SndBankSlot.field_C` |

### `SndBankSlot` (0x10)
| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | `SndBankHdr*` / heap ptr |
| 0x4 | `field_4` | `SndBank*` |
| 0x8 | `field_8` | Bank id |
| 0xC | `field_C` | Extra pointer from init table |

### `SndBankHdr` (0x8+)
| Off | Member | Role |
|-----|--------|------|
| 0x4 | `field_4` | Bank id |
| 0x6 | `field_6` | Entry count |
| 0x8+ | offsets | `u16` table (`SndBankHdrOff`) |

### `LinInterp` (0x10)
| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | Current value (0..0xFFFF) |
| 0x4 | `field_4` | Target value |
| 0x8 | `field_8` | Step per tick |
| 0xC | `field_C` | Direction (−1/1) |
| 0xE | `field_E` | Active flag |

---

## Snd script / voice

### `SndScript` (0x60) — see header comments
Key: `field_0` id, `field_16` status flags, ramp triples `field_10`…`field_15`,
loop stack `field_17`/`field_18`/`field_20`, `field_40` voice list head,
`field_44` ctx, `field_48` script cursor, `field_4C` voice params, `field_50` volume interp.

### `SndVoice` (0x40)
| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | SPU voice index |
| 0x4 | `field_4` | Countdown / timer |
| 0xC | `field_C` | Current oneV/script command |
| 0x10/12 | `field_10/12` | FX gates (alias `SndVoiceFx`) |
| 0x34 | `field_34` | `SndVoiceOwner*` parent |
| 0x38/3C | `field_38/3C` | Prev/next list links |

### `SndVoiceFx` / `SndOneE` / `SndOneV` / `SndScriptCmd` / `SndVoiceParams` / `SndVoicePick`
See detailed comments on each struct in `sound.h`.

---

## SndEvt message queue

### `SndEvt` (0x1C)
| Off | Member | Role |
|-----|--------|------|
| 0x0 | `allocated` | Allocated flag (0 free, 1 in use) |
| 0x2 | `handlerIdx` | Handler index into `SndEvt_Handlers` |
| 0x4–0x10 | `field_4`… | Handler payload (args vary by opcode) |
| 0x14 | `prev` | Queue prev |
| 0x18 | `next` | Queue next |

### `SndEvtFrom4`
Overlay of payload starting at `SndEvt.field_4` (includes `SndVoiceParams*` at +0xC).

---

## MIDI

### `MidiSong` / `MidiTrack` / `MidiNoteSlot` / `MidiOpcodeSlot`
See comments in `sound.h` (track cursor, delta time, voice slots, opcode table at 0x484).

---

## UI

### `UiObject` (0x30)
| Off | Member | Role |
|-----|--------|------|
| 0x00 | `status` | Status flag |
| 0x04 | `field_4` | From desc / mode seed |
| 0x08 | `mode` | Mode (5 = skip draw; 3 = torn down) |
| 0x0C–12 | `field_C`… | Layout RECT-like halfwords |
| 0x14 | `drawOrder` | Text draw priority/order |
| 0x16 | `timer` | Timer/counter |
| 0x18/1A | `field_18/1A` | Layout offsets |
| 0x1C | `field_1C` | Position (+2 for text draw) |
| 0x1E/20/22 | `field_1E` / `baseX` / `baseY` | Placement |
| 0x24 | `callback` | Callback |
| 0x28 | `owner` | Owning `Task*` |
| 0x2C/2E | `field_2C/2E` | Teardown / choice state |

### `UiObjectDesc` / `UiPanel` / `UiList` / `UiMiniObj` / `TextBlockDesc` / `TextLineNode`
See header comments (layout, list cursor, line count, …).

### `TextDrawReq` / `PrimDrawParams` / `TextStream` / `FontGlyph` / `GlyphUvwh`
See comments in `text.h` (x/y, OT, glyph table, SPRT/TILE RGB, stream cursor).

---

## Stage / session / TMD

### `StageCtx` (0x38)
| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | `TaskDesc*` table |
| 0x04/08 | `field_4/8` | Spawn args for `Task_SpawnFromTable` |
| 0x17/18/19 | flags | Flow gates |
| 0x1C | `field_1c` | Flag word (bit clears) |
| 0x24 | `field_24` | Last `Display_State.frameMode` |
| 0x28 | `field_28` | Step counter |
| 0x2C/34 | arrays | CDF load params |

### `GameSession` (0x13C)
| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | s8 flag; nonzero makes `func_800AC058` kill its task immediately |
| 0x1 | `field_1` | s8 flag; nonzero skips the `func_800E6CE0` / spawn path in `func_800AF0AC`, `func_800AF180`, and `func_800AE45C` |
| 0x2 | `field_2` | Soft state flag |
| 0x4 | `field_4` | Byte used by CD/display helpers; address taken as a 4-byte location key. Also 1-based index into the innermost `D_8010CB54` byte table (`func_800AD284`) |
| 0x5 | `field_5` | u8; 1-based index into `D_8010CB40` / `D_8010CBA4` innermost tables (`func_800AEEFC`, `func_800ACEBC`, `func_800D9C64`). Second-innermost for `D_8010CB54` |
| 0x6 | `field_6` | u8 room index (1-based for `D_8010CB40` / `D_8010CB2C` / `D_8010CBA4`) |
| 0x7 | `field_7` | u8 stage index (1-based for `D_8010CB40` / `D_8010CB2C` / `D_8010CBA4`) |
| 0xC | `field_C[16]` | Pointer table (cleared by `Game_ClearPtrSlots`) |
| 0x4C | `field_4C` | Init flag |
| 0x4E | `field_4E` | Set by `Fs_LoadFile` for category-8 |
| 0x52 | `field_52` | s16 cleared by `func_800A9730` when `spawnArg1 == 0` |
| 0x58 | `field_58` | u16 current pad/button mask (`lhu`); `func_80103804` copies it onto `GameActor.field_962` |
| 0x5F | `field_5F` | u8 flag; nonzero makes `func_800E74EC` skip overlay-wait timer setup |
| 0x64 | `field_64` | u8 flag; nonzero makes `func_800AD5B8` / `func_800AD50C` skip their state dispatch |
| 0x66 | `field_66` | u8; 1 makes `func_800CE3B4` spawn `D_8010EB94` and scale with `Ui_Scale15(2)` (else `D_8010EAD0` / scale 1) |
| 0x68 | `field_68` | u8 flag; set to 1 by `func_800E7378` and cleared by `func_800E73E8` / `func_800E7434` when `D_8007218B != 9` |
| 0x69 | `field_69` | u8 flags; bit 0x2 skips `SndEvt_EnqueueType2(0, 0xB4)` when the last `GpStateF0.field_6` ref is released |
| 0x76 | `field_76` | s16 flag; nonzero makes `func_800AD378` rebuild via `func_800ACD2C` then clear it |
| 0x7C–80 | halfwords | Counters / mode |
| 0x126 | `field_126` | u8 flag; nonzero skips `func_800A7A64` display-mode init |
| 0x128 | `field_128` | u8; `0xFF` sentinel (`func_800B0748` / `func_800B082C`) |
| 0x129 | `field_129` | u8; last `CdCmd_Enqueue(0x21)` `param1[0]` written by `func_800B065C` (no-op if unchanged) |
| 0x12C | `field_12C` | u8 flag; 0 runs extra `func_800E06AC` pass in `func_800DB72C` |
| 0x12D | `field_12D` | s8 countdown (`lb`/`sb`); `0x7F` sentinel in `func_800A7320` |
| 0x12E | `field_12E` | u8; `func_800A76A4` copies it as `s8` into `D_80114BD8.field_2` |
| 0x12F | `field_12F` | u8; `func_800AAF70` writes `0x1E` |
| 0x139 | `field_139` | u8; `func_800E8888` writes `killCountdown * 2`, or 0 when that task kills itself |
| 0x13A | `field_13A` | u8; cleared by `func_800AE45C` when `D_80114CDC` is 0 |
| 0x13B | `field_13B` | u8 flags; cleared by `func_800E8F68` with `Pad_ClearEvents(0)`. Bit 0 is set by `func_800E93D4` while its `spawnArg1` countdown runs and cleared when that task kills itself. Bit 0x80 lets `func_800E93D4` proceed when `D_801153F4` is set. |

### `GameActor` / `GameActorExt`
Sparse: `field_17C`/`field_930` addresses for overlay setup; `field_C` kill flag bit 0x80.
`field_17C` is an 18-entry `GpRec18` table (`func_800E1A6C` walks it from
`&field_17C` until `field_0` bit 0x2; `func_801041B4` returns 1 if any
`field_4 & 0x100100 == 0x100000`).
`field_40`/`field_44`/`field_48` are three s32s (`VECTOR3` vx/vy/vz) copied from the
argument of `func_80105B74` onto the slot-3 actor.
`field_50`/`field_52`/`field_54` are an `SVECTOR` (vx/vy/vz) written by `func_80104D68`
from its pose argument and passed to `RotMatrix`; `field_52` is also the facing
angle (`lh`) used by `func_8010BCF4` / `func_80103E7C`.
`field_58`/`field_5C`/`field_60`/`field_64`/`field_68`/`field_6A`/`field_70` are s16s
cleared together by `func_8010C46C` / `func_8010C4F0` / `func_8010C75C` (store order
0x60, 0x58, 0x64, 0x5C, 0x6A, 0x68, 0x70). `field_6A` is the 0x6A adjust skipped
when `field_954` is nonzero in `func_80109720`.
`field_12A` is a u16 flags halfword at +0x1E of the `field_10C` list node (`lhu`/`sh`);
`func_8010C46C` / `func_80106350` AND it with `0x3FFF` (clear bits 14–15).
`field_424` is a 0x14-byte `GpAnimCtx` overlay; `func_80103AC0` passes it to
`func_800B4514` for each slot `i = 1 .. field_938-1`. `func_801038F8` does the
same walk through `func_800B3FA8(..., i, arg1)`, then copies `field_985` onto
the slid overlay at `field_441`.
`pad_438` is the base of a 0x28-byte `GpAnimSlot` array (same stride as
`GameActorSlot`, but starting 0x10 before `field_448`); `func_80105B0C`
passes `&pad_438[i]` to `func_800B3DF4` for `i = 1 .. field_938-1`.
`field_7A8` is the address `func_80103874` (and the same-shaped helpers
`func_801034C0` / `func_8010BFCC` / …) passes as `func_800B3F84` arg3
(`GpAnimCtx.field_8`).
`field_93A` is a u16 anim-table index (`sh` / `lhu`); `func_80103874` writes
`D_80112D68[Mc_SaveData.field_22 - 1] + Wip_SysConfig.field_21` (same sum as
`func_800E3BBC` / `func_800E3CEC`). `func_8010BFCC` writes
`D_80113360[Mc_SaveData.field_13 - 1] + Mc_SaveData.field_5C7` (same sum as
`func_800E3D24`). `func_80104CAC` writes `0x7FFF` (same
inactive sentinel as `GpAnimSlot.field_0`) when installing `GpAnimArg.field_0`.
`field_928` is the pointer at `D_80112D6C[field_93A]` (`func_80103874`) or
`D_80113368[field_93A]` (`func_8010BFCC`), passed as
`func_800B3F84` arg1 (`GpAnimCtx.field_0`). `func_80104CAC` copies
`GpAnimArg.field_0` here instead of indexing those tables.
`field_448` is a 19-entry table of `GameActorSlot` (0x28 each; flags halfword at +0x00).
`func_80100B78` stores count `0x13` at `field_938`; `func_80105894` returns
`(slot[arg1].field_0 & 0x102) == 0`. `func_80101848` case 8 reads
`field_448[1].field_0` and continues when bit 0 or bit 1 is set (same halfword
as `GpAnimSlot.field_10` on the `pad_438` overlay).
`func_801058BC` clamps `arg2` to 1..0x7F, writes it through the slid-actor overlay
at `field_441` for `i = 1 .. field_938-1`, then stores the same byte at `field_985`.
`GameActorExt.field_1C` / `field_20` are `MATRIX*` defaults (`D_80114E98` / `D_80114EB8`)
written by `func_800D9D18` onto the slot-3 extra and onto `field_920`/`field_924` extras.
`GameActorExt.field_8` is a `GsCOORDINATE2*` (`flg` cleared to 0 by `Display_SpawnFromMode`
and `func_800B57EC`; `sub` is the parent link, same convention as `Gfx_InitCoordinateTrees`).
Offset 0x18 / 0x20 are the low 16 bits of `coord.t[0]` / `coord.t[2]` (world X/Z);
`func_800AEE28` loads them as `u16` (`GpCoordXZ`).
Offset 0x44 (`param` in libgs) is loaded as an s16 flag by `func_8010B590` (`GpCoordExt`).
Offset 0x4C (`sub` in libgs) is the parent `GameActorExt.field_8` pointer written by
`func_80104364` (with `field_44 = 1`) and `func_80104258` (with `field_44 = 0`).
`field_AC`/`field_CC`/`field_EC`/`field_10C`/`field_12C` are 0x20-byte list nodes
unlinked by `func_800E1638` during actor teardown (`func_80101408`).
`field_124` is a u32 packed word at +0x18 of the `field_10C` node;
`func_801061F0` writes `0x20000 | (WipSysConfig.field_21 << 8) | field_22` from
slot 3; `func_80106238` replaces bits 14–15 with `(arg1 << 1) | arg2`.
`field_90C` is a `GpLinkNode*` (same object as `func_800DAB38` unlinks); `func_80103B5C` clears `node->field_5` then nulls the slot. `func_800DB0D8` walks both `D_80115760[]` slots and clears `field_5` without nulling the pointer. `func_800DAC54` returns a 2-bit mask of those slots whose `field_90C` equals the given node. `func_800DACAC` assigns the node onto `D_80115760[0]`'s actor, clearing the previous node's `field_5` and this node's `field_4` bit 0. `func_800DACF8` is the inverse: it nulls any `D_80115760[]` slot whose `field_90C` is this node, clears `field_5`, and sets `field_4` bit 0.
`field_910` is a `GpActorD4*` (0xD4-byte block from `func_8010BAC8`); `func_8010BF7C` writes `field_C4` as `arg1 + (arg2 & func_80037164())`.
`field_914`..`field_924` are child `Task*` slots killed (if non-NULL) on that path.
`field_944`..`field_950` / `field_970` are s16 timers written to `0x258` by `func_8010A42C`;
`field_954` is a u16 (`lhu`/`sh`) cleared with the 0x954–0x95E cluster (nonzero skips the 0x6A adjust in `func_80109720`);
`field_93C` is a u16 (`lhu`) mode override; `func_80108CC4` passes it to `func_80103A18` when nonzero (else 4);
`field_958` is an s16 mode written to 1 or 3 by `func_80105A8C` (third arg zero / nonzero);
`field_95A`/`field_95C` are u16s in that same cluster (`func_80109818` writes `field_95C = 5`);
`field_95E` is a u16 phase (`lhu`/`sh`); `func_8010ABD4` only runs the `func_8010AB70` body when it is 1;
`field_960` is a u16 (`sh`) previous `field_956` saved by `func_80109290`;
`field_962` is a u16 button mask (`lhu`); `func_80103804` copies `GameSession.field_58` here after saving the previous value to `field_964`; `func_80109250` maps D-pad up/down (`0x5000` / `0x4000`) onto `field_973` as `+1`/`-1`/`0`;
`field_964` is a u16 previous `field_962`; `func_80103804` writes newly pressed bits to `field_966` (`field_962 & ~field_964`) and released bits to `field_968` (`field_964 & ~field_962`);
`field_966` is a u16 newly-pressed mask (`lhu`); `func_80104A4C` sets `WipSysConfig.field_24` when bit `0x20` is set and `field_954`/`field_956` are idle;
`field_968` is a u16 newly-released mask;
`field_96C`/`field_96E` are s16s cleared with `field_972` by `func_8010B210` (called from `func_8010A42C` case 2);
`func_8010A9D0` compares `field_96C` as `u16` (`lhu`) against 1 and passes `0x10` or `0x11` to `func_80103A18`;
`field_973`/`field_974` and `field_975`/`field_976` are signed-byte pairs compared by `func_80108568` (first mismatch → `func_80108770(..., 4)`; second mismatch only when `field_973 == 0` → `func_80108684`);
`func_80103804` snapshots each pair (`974=973`, `976=975`, `978=977`) before overwriting `field_977` with bit 6 of the new `field_962`;
`field_97C` is a signed flag byte (`lb`/`sb`); `func_80108FD4` / `func_80108458` clear it before `func_80108E0C` or `func_80103B5C`;
`field_97D` is a flag byte (`lbu`/`sb`); `func_80109374` writes 1 when `field_962`
bit 0x80 is set, `D_80114C08.field_3 == 0`, `Wip_SysConfig.field_21 != 0`, and
`field_991 == 0`, else 2. Callers test bit 0x1 (`func_801085D0` / `func_801090E8`)
or bit 0x2 (`func_80108458` / `func_80108FD4`). Bit 0x4 selects `func_801055D4`
vs `func_80108770` in `func_80106550`;
`field_97E` is a flag byte set to 1 by `func_80103B5C` / `func_80103F70` (no `field_90C`, or `field_4` bit 0 set). `func_80103F70` compares it as signed (`lb`) against 2 before the `field_21 == 0x17` helpers;
`field_97F` is a signed result byte (`lb`/`sb`); `func_801060E0` writes 1 / 2 / 0 from button bits (mask 0x8/0x2, or 0x80/0x10 when `field_954` and `D_80072310` are both 2);
`field_981` is a u8 counter cleared with the 0x954–0x95E cluster; `func_801041FC` increments it from 0 (`lb`/`lbu`);
`field_983` is a u8 flag byte (`lbu`/`sb`); `func_8010AA28` ORs in `0x18` after `func_80103B5C`;
`func_80104CAC` writes 7 or `0x38` from `GpAnimArg.field_10` (nonzero / zero);
`field_985` is a u8 default copied onto slid `field_441` by `func_801038F8`;
`func_80104CAC` writes `0x10` before that walk;
`field_987`/`field_988`/`field_989` and `field_98A`/`field_98B`/`field_98C` are two u8 groups written by command `0x401` (`func_80105AB0`): arg 0 sets them to (1,0,0) and (2,0,0); args 1–3 write the first group as `(arg+1, 0, 0)`; args ≥4 write the second as `(arg-3, 0, 0)` (and `field_98C`);
`field_98D`/`field_98E`/`field_990` are companion bytes (case 10 also stores `rand() & 0x1F + 0xA`).
`field_991` is a signed flag byte (`lb`); `func_80109374` requires it 0 to write `field_97D = 1`.
`field_993` is a u8 (`sb`) copy of the third arg of `func_8010B2D4` / `func_8010B348`, written only when `field_96C` is 0.
`func_8010B348` switches on `(u16)GpIdRec.field_4`: 2/4 write `field_972 = 5`, 3 writes `field_972 = 0`, then stores `func_800E2CD4(field_4, 0)` to `field_96E`.

### `GpActorD4`
0xD4-byte block at `GameActor.field_910`, allocated and `Mem_Set(..., 0xD4)` by `func_8010BAC8`.
| Off | Member | Role |
|-----|--------|------|
| 0xC4 | `field_C4` | s16; `func_8010BF7C` stores `arg1 + (arg2 & rand)` |

### `WipSysConfig`
`field_21` is a u8 packed into `GameActor.field_124` bits 8–15 by `func_801061F0`.
It stores selected item id − 0x7F (`func_800CF448`); 0 means none selected.
`field_23` is a u8 storing selected item id − 0x5F for ids in 0x60–0x7F (`func_800CEC5C`).
`field_22` is a u8 packed into `GameActor.field_124` bits 0–7 by `func_801061F0`.
`func_800BBF1C` sets it to `D_80072330[field_21 + 0x7F].field_0 + 0x61` when a
slot is occupied, else 0, then calls `func_801061F0`.
`field_24` is a u8 cleared by `func_801053A0`; `func_8010C81C` saves and restores it around that call.
`field_25` is an OR mask of PE/status bits set by `func_8010A42C` (same bit as the `func_800ECA10` arg).

### `TmdObject` / `TmdSource`
| Off | Member | Role |
|-----|--------|------|
| TmdObject.0x0C | `field_C` | Flags; bit 2 (`0x4`) skips buffer (re)alloc (`Tmd_AllocMissingBuffers`) and is set by `func_800B62D4` before `Tmd_FreeBuffers` |
| TmdObject.0x10 | `field_10` | `TmdSource*` |
| TmdObject.0x18 | `field_18` | Aux buffer (`Tmd_AllocBuffers`) |
| TmdObject.0x24 | `field_24` | Stream param; `Tmd_ProcessStream` copies to scratch `field_70` as `s8` |
| TmdObject.0x25 | `field_25` | Stream param; `Tmd_ProcessStream` copies to scratch `field_72` as `(s8)<<6` |
| TmdSource.0x0 | `field_0` | Init flag |
| TmdSource.0x4 | `field_4` | Byte count for calloc×2 |
| TmdSource.0x20 | `field_20` | Command/data stream |

---

## Stream (stream)

### `StreamSlot` (0x28)
| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | Status (1 = active in polls) |
| 0x4 | `field_4` | Nonzero → active work |
| 0xE | `field_E` | Counter (compared to 0x64) |
| 0x1A | `field_1A` | Returned by accessors |

---

## Gameplay overlay (`include/gameplay/`)

### `GpActorWork` — `3FB8.h`
Task overlay: `actor` is `Task::idMap` at 0x1C; `extra` is `Task::extra` at
0x2C (`GameActorExt*`). `func_8010B120` passes `extra->field_8` to
`func_800D937C` / `func_800D9340` as a `GpObj38*`.

### `GpStateC08` (0x10) — `gameplay.h`
Global at `D_80114C08`. Splat also emits per-byte labels (`D_80114C0A` / `D_80114C0B` / …)
for the same block.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | u16 loaded by many helpers |
| 0x03 | `field_3` | s8 state (`lb`); `func_80109290` compares to -2; `func_80109374` requires 0; `func_800A7DE0` writes 2 when `field_A >= 2` |
| 0x06 | `field_6` | Flags; bit 0 gates `func_800A7DB8` writing `field_E` |
| 0x07 | `field_7` | Cleared by `func_800A7DE0` |
| 0x08 | `field_8` | Cleared by `func_800A7DE0` |
| 0x0A | `field_A` | s8 (`lb`); `func_800A7DE0` sets `field_3 = 2` when >= 2, then clears it |
| 0x0E | `field_E` | Written by `func_800A7DB8` when `field_6` bit 0 is clear; cleared by `func_800A7DE0` |

### `GpStateBD8` (0x4) — `gameplay.h`
Global at `D_80114BD8`. Filled by `func_800A76A4` and passed as `Task_Spawn`
bank 1 type `0x31` `spawnArg2`.

| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | u8; cleared before spawn |
| 0x1 | `field_1` | u8; cleared before spawn |
| 0x2 | `field_2` | s16; `(s8)GameSession.field_12E` |

### `GpCb2CRec` (0x24) — `gameplay.h`
Element of tables pointed to by `D_8010CB2C`. Stage index is
`GameSession.field_7 - 1`, room index is `GameSession.field_6 - 1`,
record index is `func_800AD284() - 1`. Passed as `Task_Spawn` type-0xF
`spawnArg1` (`func_800A8B6C` / `func_800A8DC0`).

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `mtx` | `MATRIX`; rotation copied to `D_80070E44`, translation to `D_80070F28` (`func_800A8724`) |
| 0x20 | `field_20` | Extra word; `lhu` into `Display_State+0x110`, `lw` into GTE OFC (`func_800A8724`) |

### `GpCbA4Vec` (0x8) — `3A34.h`
Nested table entry at `GpCbA4Rec.field_4`. Entry 0's `field_0` is the
max valid index. `func_800D957C` returns `&table[GameSessionFrom4.field_0]`
when in range, else the default at `D_8010F9E4`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Max index (entry 0) / signed X minimum (data entries) |
| 0x02 | `field_2` | Signed Y minimum (`func_800D7A9C`) |
| 0x04 | `field_4` | Signed Z minimum (`func_800D7A9C`) |
| 0x06 | `field_6` | Unused by `func_800D957C` / `func_800D7A9C` |

### `GpCbA4Rec` (0x8) — `3A34.h`
Element of tables pointed to by `D_8010CBA4`. Stage index is
`GameSessionFrom4.field_3 - 1`, room index is `field_2 - 1`,
record index is `field_1 - 1`. `func_800D9C64` returns the record
or NULL if a table pointer is missing.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Word returned by `func_800D9654` |
| 0x04 | `field_4` | `GpCbA4Vec*` nested table walked by `func_800D957C` |

### `GpLinkNode` (0x8) — `3A34.h`
| Off | Member | Role |
|-----|--------|------|
| 0x00 | `next` | Singly-linked list (`D_80115268`) |
| 0x04 | `field_4` | Flag byte (bit 0 cleared on link) |
| 0x05 | `field_5` | Flag byte (cleared on link/unlink) |
| 0x06 | `field_6` | 1 = on list; 0 = unlinked |

Embedded at `GpEnemy.node` (+0x10). `func_800DAB38` also clears `GameActor+0x90C` slots that point at the node.

### `GpSlot70` (0xC) — `3A34.h`
32-entry table at `D_80115270`. `func_800DAF98` zeros `field_0` / `field_4` / `field_6` only.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Object pointer (NULL = free; `func_800DA7B8` may store 4 as a sentinel) |
| 0x04 | `field_4` | Signed value (`lh`/`bgez`; incremented by `func_800DA6E8`) |
| 0x06 | `field_6` | Countdown timer (set to 0x14; decremented by `func_800DA7B8`) |
| 0x08 | `field_8` | Projected screen X (`swc2 SXY2` / `lh`) |
| 0x0A | `field_A` | Projected screen Y |

### `GpRec18` (0x18) — `session.h` (`GameActor.field_17C[18]`)
Array element cleared by `func_800E18E0` (`Mem_Set` of `count * 0x18`).
Walked by `func_800E1A1C`, which counts occupied slots whose `field_4`
high 16 bits match the argument. `func_800E1A6C` walks until `field_0`
bit 0x2 and, for each occupied slot, keeps only that bit and zeros the
payload fields (not 0xE / 0x16). `func_801041B4` scans all 18 actor
slots for `field_4 & 0x100100 == 0x100000`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | u16 flags: bit 0x1 = occupied; bit 0x2 = last (`func_800E18E0` writes 2 on the last element; `func_800E1A6C` does `field_0 &= 2`) |
| 0x02 | `field_2` | s16; cleared by `func_800E1A6C` |
| 0x04 | `field_4` | Packed word; `func_800E1A1C` compares `field_4 & 0xFFFF0000` to its argument; cleared by `func_800E1A6C` |
| 0x08 | `field_8` | s16; cleared by `func_800E1A6C` |
| 0x0A | `field_A` | s16; cleared by `func_800E1A6C` |
| 0x0C | `field_C` | s16; cleared by `func_800E1A6C` |
| 0x0E | `pad_E` | unused by `func_800E1A6C` |
| 0x10 | `field_10` | s16; cleared by `func_800E1A6C` |
| 0x12 | `field_12` | s16; cleared by `func_800E1A6C` |
| 0x14 | `field_14` | s16; cleared by `func_800E1A6C` (GCC hoists `&field_14` as the store base) |
| 0x16 | `pad_16` | unused by `func_800E1A6C` |

### `GpStateF0` (0x18) — `3A34.h`
Global at `D_801153F0`. Full object may still be larger.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | state byte (set to 1 by `func_800DB4E0` if 0; set to 2 when refcount hits 0 in `func_800DB558` / `func_800DB630` / `func_800DB6B4`) |
| 0x01 | `field_1` | alternate-active flag (`lbu`; `func_800A7508` / `func_800A7CB0` / `func_800A7CF4` / `func_800A7D54` OR it with `field_0 == 1 && field_6`); last-ref release sets 0x3C |
| 0x02 | `field_2` | bitset (`func_800DB500` ORs `1 << (arg0 - 1)` when `arg0 != 0`); cleared on last-ref release |
| 0x03 | `field_3` | cleared with `field_2` on last-ref release; also written as `D_801153F3` by `func_800DB530` |
| 0x04 | `field_4` | u8; also `D_801153F4` |
| 0x05 | `field_5` | u8 count of claimed `GpSlot18`s; incremented by `func_800E1C58` |
| 0x06 | `field_6` | u16 refcount (inc: `func_800DB53C`; dec: `func_800DB558` / `func_800DB630` / `func_800DB6B4`) |
| 0x08 | `field_8` | s32 word; cleared by `func_800DB630` on last-ref release |
| 0x0C | `field_C` | s32 word; cleared by `func_800DB630` on last-ref release |
| 0x10 | `field_10` | s32 word; cleared by `func_800DB630` on last-ref release |
| 0x14 | `field_14` | s32 accumulator; `func_800E2C78` adds `min_u(arg0->field_40, arg2)` when `(arg1 & 0x7F)` is 0x19..0x1B |

### `GpObj` (0x20 header) — `3A34.h`
Doubly-linked node unlinked by `func_800E1638` and linked onto
`D_8010FA8C[index]` by `func_800E15AC`. Embedded as 0x20-byte slots in
`GameActor` (`field_AC` / `field_CC` / `field_EC` / `field_10C` /
`field_12C`). Other list users may be larger; 0x20 is the header.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `next` | Intrusive next; NULL-terminated |
| 0x04 | `prev` | Previous node, or the list-head object when first |
| 0x08 | `field_8` | Pointer stored by `func_8010C980` (first arg) |
| 0x0C | `field_C` | `GpRec18*` table wiped by `func_800E18E0` |
| 0x10 | `field_10` | s16; cleared with `field_12` / `field_14` by `func_8010C980` |
| 0x12 | `field_12` | s16 |
| 0x14 | `field_14` | s16 |
| 0x18 | `field_18` | s32 flags/mode word (`func_8010C980` ORs `0x30000`) |
| 0x1C | `field_1C` | s16 stored by `func_8010C980` (last arg) |
| 0x1E | `flags` | u16 flags: bit 0x8 = on list; bits 0x7 kept on unlink (type / kind); `func_8010C980` sets 1 then ORs 0x8000 |

### `GpObj20` (0x24) — `3A34.h`
Sparse overlay. Full object size is not known yet.

| Off | Member | Role |
|-----|--------|------|
| 0x20 | `field_20` | `MATRIX*` whose `t[0]/t[1]/t[2]` are set by `func_800D9550` |

### `GpObj44` (0x56) — `3A34.h`
Sparse overlay of the same light object as `GpObj38`. Full object size is
not known yet; 0x56 is the minimum covering known fields.

| Off | Member | Role |
|-----|--------|------|
| 0x44 | `field_44` | s16 room-id filter (`lh`); 0 = any room. `func_800D9718` compares it to `(u8)Game_Session->field_4` |
| 0x4A | `field_4A` | s16 GTE IR0 scale; `func_800D9718` writes `0x1000` (ONE). Nearby handwritten light helpers `lh` it into IR0 |
| 0x50 | `field_50` | s16; first of three color/luminance components (`lh`) |
| 0x52 | `field_52` | s16; `func_800D9718` weights this by 6 |
| 0x54 | `field_54` | s16; `func_800D9718` weights this by 2. Return is `((8*50 + 6*52 + 2*54) >> 8) + 0xF00` |

### `GpObj40` (0x42) — `3A34.h`
Sparse overlay. Full object size is not known yet.

| Off | Member | Role |
|-----|--------|------|
| 0x40 | `field_40` | s16; `func_800E2C78` unsigned-clamps it against `arg2` and adds the result to `GpStateF0.field_14` |

### `GpSlot18` (0x18) — `3A34.h`
Table element pointed to by `GpObj54.field_54`. Occupied when the first
word's low 2 bits equal 1.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | u16 flags; occupancy is `(first word & 3) == 1`. `func_800E1C58` ORs bit 0 |
| 0x02 | `field_2` | s16; cleared when the slot is claimed |
| 0x04 | `field_4` | payload pointer stored by `func_800E1C58` |
| 0x08 | `field_8` | s16; cleared when claimed |
| 0x0A | `field_A` | s16; cleared when claimed |
| 0x0C | `field_C` | s16; cleared when claimed |
| 0x10 | `field_10` | s16; cleared when claimed |
| 0x12 | `field_12` | s16; cleared when claimed |
| 0x14 | `field_14` | s16; cleared when claimed |

### `GpObj54` (0x58) — `3A34.h`
Sparse overlay. Same object family as `GpObj50` / `GpObj4C`.

| Off | Member | Role |
|-----|--------|------|
| 0x54 | `field_54` | `GpSlot18*` table; NULL means `func_800E1C58` is a no-op |

### `GpObj4A` (0x4C) — `3A34.h`
Array element linked onto `D_8010FAB0[index]` by `func_800E1688` and unlinked
by `func_800E1708`. Walked at +0x4C until `field_4A` bit 0x80.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `next` | Intrusive next; NULL-terminated |
| 0x04 | `prev` | Previous node, or the list-head object when first |
| 0x08 | (pad) | Pointer slot; insert callers store `D_80070F10` |
| 0x4A | `field_4A` | Flag byte: 0x20 = on list, 0x40 set after insert, 0x80 = last in array. Unlink keeps bits 0x87 |

### `GpObj3A` (0x3C) — `3A34.h`
List node linked onto `D_8010FAB8[index]` by `func_800E17B4` and unlinked
by `func_800E1834`. `func_800E1884` empties the whole list (clears the
head, then each node's prev / next / `0x20` flag). Same next/prev/`0x20`
flag protocol as `GpObj4A`. Full object size is not known yet; 0x3C is the
minimum covering known fields.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `next` | Intrusive next; NULL-terminated |
| 0x04 | `prev` | Previous node, or the list-head object when first |
| 0x3A | `field_3A` | Flag byte: 0x20 = on list. Unlink keeps bits 0x87 |

### `GpRec4` (0x4) — `3A34.h`
Element of `D_80114028`. `func_800E2CD4(idx, 0)` returns `field_0`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | u16 value returned by `func_800E2CD4` for table 0 |
| 0x02 | `field_2` | u16 companion word (unread by `func_800E2CD4`) |

### `GpRec6` (0x6) — `3A34.h`
Element of `D_80114054`. `func_800E2CD4(idx, 1)` returns `field_0`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | u16 value returned by `func_800E2CD4` for table 1 |
| 0x02 | `field_2` | u16 companion word (unread by `func_800E2CD4`) |
| 0x04 | `field_4` | u16 companion word (unread by `func_800E2CD4`) |

### `GpRec10` (0xA) — `3A34.h`
Element of `D_80113390`. Selected when an id's 0x8000 bit is clear;
index is `id & 0x7F`. Sibling accessors `func_800E2D3C` / `func_800E2D90`
/ `func_800E3194` return `field_4` / `field_6` / `field_8`.

| Off | Member | Role |
|-----|--------|------|
| 0x04 | `field_4` | u16 returned by `func_800E2D3C` |
| 0x06 | `field_6` | u16 returned by `func_800E2D90` |
| 0x08 | `field_8` | u16 returned by `func_800E3194` |

### `GpRec12` (0xC) — `3A34.h`
4-entry ranked table walked by `func_800D9B9C` from `arg4` toward 0.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | s32 payload stored from `func_800D9B9C` arg2 |
| 0x04 | `field_4` | s32 descending sort key (`func_800D9B9C` arg1; ignored if <= 0) |
| 0x08 | `field_8` | s32 payload stored from `func_800D9B9C` arg3 |

### `GpRec16` (0x10) — `3A34.h`
Element of `D_8011398C`. Selected when an id's 0x8000 bit is set;
index is `id & 0x7F`. Sibling accessors `func_800E2D3C` / `func_800E2D90`
/ `func_800E3194` return `field_A` / `field_C` / `field_E`.

| Off | Member | Role |
|-----|--------|------|
| 0x0A | `field_A` | u16 returned by `func_800E2D3C` |
| 0x0C | `field_C` | u16 returned by `func_800E2D90` |
| 0x0E | `field_E` | u16 returned by `func_800E3194` |

### `GpObj4C` (0x50) — `3A34.h`
Sparse overlay. Full object size is not known yet.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `next` | Intrusive list link; `D_8011556C` head walked by `func_800E0B08` / `func_800E1BF0` |
| 0x46 | `field_46` | u16 copied to `func_800E1BF0` arg0 when `field_4B` is pending |
| 0x48 | `field_48` | u8 copied to `func_800E1BF0` arg1; `func_800E1B80` matches it against `Game_Session->field_4` |
| 0x49 | `field_49` | u8 copied to `func_800E1BF0` arg2; `func_800E1B80` copies it to `Mc_SaveData.field_4` |
| 0x4B | `field_4B` | Signed pending flag; `func_800E0B08` stores 0 when non-zero; `func_800E1BF0` / `func_800E1B80` consume it |
| 0x4C | `field_4C` | Flag byte; `func_800E3008` ORs bit 0; nearby helpers test bits 0x1 / 0x2 / 0x4 |
| 0x4E | `field_4E` | Packed modes + flag: bits 0-1 current, bits 2-3 previous, high nibble (incl. 0x80) preserved by `func_800D930C` |
| 0x4F | `field_4F` | Blend/transition timer; set to 0x10 by `func_800D930C` when the current mode changes |

### `GpEnemy` — `1BC.h`
0x60-byte work object (`Mem_Calloc` in `func_800B0494`). Stored in `Task::spawnArg2`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `task` | Owning `Task*` |
| 0x0C | `field_C` | Countdown word; `func_800B0544` sets 0x78, `func_800B0560` decrements and advances `Task::state` at 0 |
| 0x10 | `node` | `GpLinkNode` unlinked by `func_800DAB38` |
| 0x18 | `field_18` | Set to `&D_80070F10` by `func_800B0494` |

### `GpAnimObj` — `1BC.h`
Source object for `func_800B3CCC` / `func_800B3F60`. Full size unknown.

| Off | Member | Role |
|-----|--------|------|
| 0x30 | `field_30` | Copied into `GpAnimCtx.field_10` |
| 0x34 | `field_34` | Address stored as `GpAnimCtx.field_4` (0x50-byte record base) |

### `GpAnimRec` (0x4) — `1BC.h`
Element of the table at `GpAnimSet.field_0`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Halfword compared with `GpAnimSlot.field_6` in `func_800B3AA4` |
| 0x02 | `field_2` | Scaled `<< 4` into `GpAnimSlot.field_C` / `field_E` (`func_800B3E74`) |
| 0x03 | `field_3` | Opcode-like byte (`func_800B3AA4` tests `< 0xC0` / sign; `func_800B3FA8` stores `& 0xF` in `field_B`) |

### `GpAnimSet` — `1BC.h`
Object behind each pointer in `GpAnimSlot.field_20` / `GpAnimCtx.field_0`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | `GpAnimRec*` table; `func_800B4668` returns `field_0 + slot->field_2` |

### `GpAnimSlot` (0x28) — `1BC.h`
Element of `GpAnimCtx.field_C`. Initialized by `func_800B3CE8` /
`func_800B3FA8`; advanced by `func_800B3448`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Set index into `field_20`; `0x7FFF` = inactive (`func_800B4668`) |
| 0x02 | `field_2` | Record index within `field_20[field_0]->field_0` |
| 0x06 | `field_6` | Index into `field_20[set]->field_0` (`func_800B3E74`) |
| 0x0C | `field_C` | Timing value; `func_800B3E74` sets `field_2 << 4` |
| 0x0E | `field_E` | Timing value; `func_800B3E74` sets the same `field_2 << 4` |
| 0x15 | `field_15` | This slot's index in the `field_C` array |
| 0x20 | `field_20` | `GpAnimSet**` table (copy of `GpAnimCtx.field_0`) |

### `GpAnimCtx` (0x14) — `1BC.h`
Context filled by `func_800B3CCC` / `func_800B3F60`. Used as arg0 by the
`func_800B3448` cluster.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Pointer table (arg1); copied onto 0x28-byte slots at +0x20 |
| 0x04 | `field_4` | `&src->field_34`; 0x50-byte records in `func_800B3448` |
| 0x08 | `field_8` | Pointer (arg3); 0x10-byte stride in `func_800B3448` |
| 0x0C | `field_C` | `GpAnimSlot*` array (`func_800B3F60` writes it; `func_800B3CCC` does not) |
| 0x10 | `field_10` | Copy of `src->field_30` |

### `GpAreaObj` — `1BC.h`
Object behind `GpAreaRec.field_4`. Full size unknown.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Signed id; compared with `GpAreaKey.field_5` |
| 0x01 | `field_1` | Flags (bits 0/1/2/4 in nearby 1BC / 1A8 helpers; `func_800B59A8` returns bit 1) |

### `GpAreaRec` (0x8) — `1BC.h`
Element of tables pointed to by `D_8010CBCC`. Indexed by `GpAreaKey.field_2`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Nested `GpAreaRec*` table (`func_800B5C88`) |
| 0x04 | `field_4` | `GpAreaObj*`; returned by `func_800B5A08` |

### `GpAreaKey` — `1BC.h`
Location key for `D_8010CBCC`. Same 4-byte prefix as `GameSessionFrom4` /
`D_8007216C`. Full size unknown.

| Off | Member | Role |
|-----|--------|------|
| 0x02 | `field_2` | Record index (`* 8`) into `D_8010CBCC[field_3]` |
| 0x03 | `field_3` | Table index into `D_8010CBCC` |
| 0x05 | `field_5` | Nested-record index (`func_800B5C88`); compared with `GpAreaObj.field_0` |

### `GpWorkObj` — `1BC.h`
Overlay of `Task::spawnArg2` for sibling walkers. Full size unknown.

| Off | Member | Role |
|-----|--------|------|
| 0x08 | `field_8` | Id compared with the search key (`as_u16` in `func_800B5E08`, `as_u8` in `func_800B5E78`) |
| 0x0A | `field_A` | u16; high byte is the work type (`func_800B5E08` / `func_800B5EE8` match 9; `func_800B5E78` skips 9) |

### `GpSndMaskRec` (0x8) — `1BC.h`
Element of the 0-terminated table `D_8010D1C4`. Walked by `func_800AFF90`
(disable matching banks) and `func_800B0034` (enable matching banks).

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `mask` | Bit tested against the `u16` argument |
| 0x04 | `flags` | `SndEvt_EnqueueType7` / `SndBank_SetEnableFlags` bank id |

### `GpItemSlot` (0x8) — `268.h`
Indexed as `D_80072330[idx]` by `func_800BAFE0`. Two (id, count) pairs.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Item id (first pair) |
| 0x01 | `field_1` | Count for `field_0` |
| 0x02 | `field_2` | Item id (second pair) |
| 0x03 | `field_3` | Count for `field_2` |

### `GpItemMap` (0x4) — `268.h`
Indexed as `D_8010D2F8[idx]` by `func_800BBDC8`. 8-entry map from item id
to a `GpItemSlot` pair.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Which `GpItemSlot` pair to write (0 = first, else second) |
| 0x01 | `field_1` | Item id / `D_80072330` index |
| 0x02 | `field_2` | Mapped item id written into that pair |

### `GpItemQty` (0x4) — `268.h`
32-entry tables at `D_8010E238` (first `GpItemSlot` pair) and `D_8010D278`
(second pair), indexed by `itemId - 0x80`. Looked up by `func_800BB938`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Count written into the matching `GpItemSlot` pair |
| 0x01 | `field_1` | Related item id (first of three) |
| 0x02 | `field_2` | Related item id |
| 0x03 | `field_3` | Related item id |

### `GpItemScan` (0x4) — `268.h`
Scan descriptor for `func_800BB6FC`. Built on the stack by `func_800BBEC0`
(memset 0, then `field_1 = 0xFF`). Same layout as `Mc_SaveData` at 0x5BC
(`D_80072724`), which other callers pass as `func_800BB6FC`'s first arg.
`D_80072724` is also `D_80072330 + 0x3F4` (`GpItemBlock.scan`); `func_800BB418`
passes it that way so the slot base stays in `$v0`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Start index into a 4-byte item table |
| 0x01 | `field_1` | Number of table entries to scan |
| 0x02 | `field_2` | Table select (1 = `D_80114D70`, 2 = `D_80114C20`, else `D_80072314`) |

### `GpItemRec` (0x4) — `268.h`
Row in the item tables selected by `GpItemScan` (`D_80072314` /
`D_80114C20` / `*D_80114D70`). `func_800BB5BC(scan, idx)` returns
`&table[scan->field_0 + idx]`. Also returned by `func_800D6910`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Item id |
| 0x01 | `field_1` | Count (compared as signed by `func_800CF448` / `func_800B91C8`) |
| 0x02 | `field_2` | u16 quantity accumulated by `func_800BB6FC` |

### `GpItemAttr` (0x8) — `268.h`
Indexed as `D_8010DFB8[itemId]` by `func_800BC324`. Item ids 0x60–0x7F
map onto the 32-entry slice at `D_8010E2B8` (`D_8010DFB8 + 0x60 * 8`).

| Off | Member | Role |
|-----|--------|------|
| 0x05 | `field_5` | Unsigned base added to `Mc_SaveData.field_908[itemId-0x60]`; result clamped to 10 |

### `GpBit2Bank` (0x8) — `268.h`
Per-stage table at `D_8010D230`, indexed by `GameSession.field_7` /
`GameSessionFrom4.field_3` / `Mc_SaveData.field_7`. `func_800BB974` /
`func_800BB470` extract a 2-bit field; `func_800BB8E8` / `func_800BAC34`
write one.

| Off | Member | Role |
|-----|--------|------|
| 0x04 | `field_4` | `u32*` packed 2-bit flags (16 fields per word; index `>> 4` / `& 0xF`) |

### `GpItemDesc` (0x8) — `4CC.h`
Indexed as `D_8010D838[itemId]` by `func_800BF334`. `field_3` bit 0
selects whether the function returns `arg1 == 1` (else 0), before the
`D_80114D7C == 0x703` / item `0x81` / `D_8007216F == 1` override.

| Off | Member | Role |
|-----|--------|------|
| 0x03 | `field_3` | Flags; bit 0 tested by `func_800BF334` |
| 0x04 | `field_4` | Pointer (name string in `func_800B8EB0`) |

### `GpItemObj8` — `4CC.h`
Object at `Task::spawnArg2` for `func_800BF5CC` / `func_800BF624` /
`func_800CE094`. Full size unknown.

| Off | Member | Role |
|-----|--------|------|
| 0x08 | `field_8` | Packed item id passed to `func_800BB470` |
| 0x0A | `field_A` | Item/location halfword; copied to `D_80114DDC` by `func_800BF624`, cleared on cancel by `func_800CE094` |

### `GpItemObj2` — `4CC.h`
Object stored in `Task::extraState` by `func_800BF5CC`. Full size unknown.

| Off | Member | Role |
|-----|--------|------|
| 0x02 | `field_2` | Set to 1 when `func_800BB470` returns 2 |

### `GpEvt12` (0xC) — `3CD8.h`
Current sequence table at `D_801155A8` (`func_800E41F4` stores its first arg
there). `func_800E6EA0` walks from a start index until `field_8 == -1` or
`field_5` equals `D_80115668` (saved from `func_800E41F4`'s third arg).

| Off | Member | Role |
|-----|--------|------|
| 0x4 | `field_4` | u8 flags copied to `D_80115670` by `func_800E6E50`; bit 0 is cleared when `field_7 != 0` |
| 0x5 | `field_5` | u8 key compared with `D_80115668` |
| 0x7 | `field_7` | u8 copied to `D_80115678` (countdown) by `func_800E6E50` |
| 0x8 | `field_8` | s32; `-1` terminator, else payload/id passed to later sequence helpers |

### `GpState34` (0x34) — `3CD8.h`
Allocated by `func_800E8758` (`Mem_Calloc(0x34, 0)`); stored at `Task::idMap`.
Dual script interpreter: A uses `field_E`/`field_10`/`field_14`, B uses
`field_F`/`field_11`/`field_15`. `func_800E92C4` / `func_800E9350` decrement
the delay counters and advance via `func_800E8A90` / `func_800E8BB0`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Script table pointer (from `Task::spawnArg2`) |
| 0x04 | `field_4` | Secondary table pointer |
| 0x0A | `field_A` | Current command A (low byte = opcode) |
| 0x0C | `field_C` | Current command B (low byte = opcode) |
| 0x0E | `field_E` | Script A program counter |
| 0x0F | `field_F` | Script B program counter |
| 0x10 | `field_10` | Delay A; `func_800E92C4` decrements |
| 0x11 | `field_11` | Delay B; `func_800E9350` decrements |
| 0x14 | `field_14` | Loop counter A |
| 0x15 | `field_15` | Loop counter B |

### `GpState18` (0x18) — `3CD8.h`
Allocated by `func_800E8FB0` / `func_800E9188` (`Mem_Calloc(0x18)`); stored at
`Task::idMap` of the bank-2 type 0xD task. `func_800E9070` indexes two
dispatch tables with `field_A` and `field_C`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | s32; first spawn arg (`func_800E8FB0` / `func_800E9188`) |
| 0x04 | `field_4` | s32; second spawn arg |
| 0x08 | `field_8` | s16; cleared by `func_800E8FB0`, third arg of `func_800E9188` |
| 0x0A | `field_A` | u8 dispatch index A (`func_800E9070`) |
| 0x0C | `field_C` | u8 dispatch index B (`func_800E9070`) |

### `GpState1C` (0x1C) — `3CD8.h`
Allocated by `func_800E9CC8` (`func_8002D978(0x1C)`); pointed to by `D_80115740`.
Most members are s16. `func_800EA3A0` writes `arg0 + 1` to `field_C`.

| Off | Member | Role |
|-----|--------|------|
| 0x10 | `field_10` | s16 flags; `func_800EC9C8` tests bit 0 before `func_800EA478(0x800600E8, …)` |
| 0x18 | `field_18` | s16 PE/status bit written by `func_800ECA10` (low byte of arg; same bits as `WipSysConfig.field_25`) |
| 0x1A | `field_1A` | u16 flags; `func_800FC6C0` ORs in `0x80`, `func_800EC868` ORs in `0x100` |

### `GpCoord64` (0x64) — `3CD8.h`
Eight-slot table at `D_80114F30`. `func_800EA3EC` inits every slot;
`func_800EA3B4` decrements `field_0` when non-zero. `func_800D9618`
returns the number of slots whose `field_0` is non-zero.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | s32 refcount; cleared by `func_800EA3EC`, decremented by `func_800EA3B4`, counted by `func_800D9618` |
| 0x04 | `coord` | Embedded `GsCOORDINATE2`; `coord.sub` is parented to `&D_80070F10` |

### `GpMapRec` (0xE) — `3688.h`
Per-room record in tables pointed to by `D_8010F0F4`. Index is
`GameSession.field_7 - 1` then `GameSession.field_6`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | s16 X coord (`func_800D02A4`) |
| 0x02 | `field_2` | s16 Y coord (`func_800D02A4`) |
| 0x04 | `field_4` | u16 X extent (`func_800D02A4`) |
| 0x06 | `field_6` | u16 Y extent (`func_800D02A4`) |
| 0x08 | `field_8` | s16 X scale (`func_800D02A4`) |
| 0x0A | `field_A` | s16 Y scale (`func_800D02A4`) |
| 0x0C | `field_C` | u8 room id stored in `D_80114DF0` (`func_800D1FD4`); also `lhu` / `0xFFFF` sentinel (`func_800D02A4` / `func_800D1434`) |

---

## WIP (provisional)

### `WipSysFlags` — `field_4` soft-reset flag; `field_6` boot + stream TU gate  
### `WipSysConfig` — four s16s init 100; `field_40[0x40]` 0xFF fill  
### `WipUiHolder` / `WipUiChild` — `field_28` → child; `field_34` written by UI  
### `WipSelectMenuExt` — `field_290` seeds `UiList` cursor  

---

## How to extend

1. Prefer adding a one-line role in this file **and** an end-of-line `//` on the
   struct member in the header.
2. Rename the member only when the role is proven and all call sites are updated
   (see `NAMING.md`).
3. Do not invent roles for unlisted fields.
