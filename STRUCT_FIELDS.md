# Known struct fields

Catalog of **proven** field roles. Members are renamed in C when the role is
safe; remaining `field_XX` names stay until then. Sources: usage in matched C,
existing header comments, and `DECOMPILATION_LEARNINGS.md`.

Convention: only list fields with evidence. Unlisted `field_*` / `unknown_*` /
`pad_*` remain unknown.

---

## Task system (`task.h`)

Actor-model overview, spawn/kill, and the `Task_DescBanks` catalog:
[`doc/TASKS.md`](doc/TASKS.md).

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
| 0x24 | `field_24` | Pointer to `GpMsgEntry` id/handler table (`Gp_Slot4MsgTable` via `Gp_BindSlot4`, `D_8010FB90` via `func_800E71B0`, `Gp_PlayerMsgTable` via `Gp_InitPlayerWork`); walked by `Gp_DispatchMsg` |
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
| 0x8 | `setupArg` | Extra arg to type-1 setup (`Gp_AttachTmdFlags`) |

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
| 0x110 | `field_110` | Projection plane H; copied from `GpViewRec.field_20` (`Gp_ApplyView`) |
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
| 0x8 | `field_8` | Pad input remap mode (nonzero → overlay remap path). `Gp_InitPlayClock` writes -1 when `Display_State.field_12c != 0`. `Gp_ApplyPadReplay` plays the `Gp_ReplayCursor` stream when this is not 1, and clears it at end-of-stream / overflow |
| 0x9 | `field_9` | s8 (`lb`); `Gp_InitPlayClock` calls `func_80715198` when this is 1 and `Display_State.field_12c` is 0 |

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
| 0x0C | `field_C` | u16 play time in seconds (`lhu`). `Gp_InitPlayClock` splits it into minutes (`/ 60`) and seconds (`% 60`) on the play-clock idMap |
| 0x0E | `field_E` | Signed scale flag; if > 0 and `func_800D50D4` arg1 is 0, table value is `* 2 / 5` (unless `field_F` applies) |
| 0x0F | `field_F` | Signed scale flag; if > 0 and `func_800D50D4` arg1 is 0, table value is `* 4 / 5` (takes priority over `field_E`). Also indexes `Gp_StatRows` (`Gp_RecalcMaxHp` / `Gp_UiBoostHp`) |
| 0x26 | `field_26` | Unsigned addend for `Wip_SysConfig.field_1a` (`Gp_RecalcMaxHp`). `Gp_UiBoostHp` adds 5 when the value is below 250 |
| 0x27 | `field_27` | Unsigned addend for `Wip_SysConfig.field_1e` (`Gp_RecalcMaxMp`). `Gp_UiBoostMp` adds 1 when the value is below 250 |
| 0x28 | `field_28[32]` | `McPosRec` saved object poses (`Gp_SaveEnemyPose`). `field_3 == 0` empty; `field_A` is `GpEnemy.field_8`; `field_4/6/8` world XYZ; `field_0/1/2` packed euler |
| 0x10 | `field_10` | Init bitmask; bit 0 = global init done (`Gp_InitStageVisit`). Per-slot bit is `field_7` via the `GameSessionFrom4` overlay |
| 0x12 | `field_12` | Slot index 1..16 |
| 0x13 | `field_13` | 1-based index into `Gp_AllyIdBase` (`Gp_AllyAnimId`); also `D_8007217B` |
| 0x5C7 | `field_5C7` | signed addend for the `Gp_AllyIdBase` lookup (`Gp_AllyAnimId`); also `D_8007272F` |
| 0x5C8 | `field_5C8[]` | 32 `McItemSlot`s; `Gp_ResetAuxSlots` clears each and sets `field_2` to 0xFF except index 0x1A |
| 0x1C/1E | checksum pair | Save header sum / ones-complement |
| 0x6C8/6CA | `field_6C8/6CA` | Halfword pair; `Gp_InitStageVisit` inits both to 100 on first visit |
| 0x6CC | `field_6CC` | u16 counter capped at 9999; `func_800A110C` increments it when `spawnArg1 == 0` |
| 0x6CE | `field_6CE` | u16 counter capped at 9999; `func_800A110C` increments it when `spawnArg1 != 0` |
| 0x6D0 | `field_6D0[]` | 96-word bit flags; `Gp_SetItemSeenBit` sets (`arg1 != 0`) / clears (`arg1 == 0`) bit `id` for `id < 0x180`; `Gp_HasItemSeenBit` tests (else returns 1); `Gp_InitItemSeenBits` clears all 96 words |
| 0x888 | `field_888[]` | 1-based `s32` counters; increment capped at 0x1869E (`func_80106518`) |
| 0x908 | `field_908[]` | 32 signed addends for item ids 0x60–0x7F (`Gp_GetModLevel`) |
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
| 0x1A8 | `field_1A8` | Copied to `Gp_LcgState` by `Gp_RestoreStreamRng` |
| 0x1AC | `field_1AC` | `srand` seed restored by `Gp_RestoreStreamRng` |
| 0x210 | `field_210` | u16 flag; `func_800A9010` sets 1 after `Stream_FindSlot` succeeds and `Gp_FreeSlot4TmdBuffers`, clears it after `Gp_ApplyAreaTmdFlags` |

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
Gameplay overlay `Gp_CapGlyphs` is a `GlyphUvwh*` font table (`Gp_RelocCapFile`
stores the relocated CAP file `field_8` there). `Gp_CapTextTopY` / `Gp_CapTextHeight`
read `h + 2` as line height; `Gp_CapCenterX` / `Gp_CapCenterXLine` read `w`.

---

## Stage / session / TMD

### `TmdScratchModelBlock` (0x88) — model-path scratch; draw handlers share the same low offsets
| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Packet dest (`POLY_FT3*` etc.); advanced by the handler |
| 0x04 | `field_4` | Packet dest for per-vertex XY/UV/RGB (`func_8009AA5C` / `func_8009EAA4`) |
| 0x08 | `field_8` | Vertex array base; TMD indices are `& 0xFFF8` byte offsets |
| 0x0C | `field_C` | Normal array base; TMD indices are `& 0xFFF8` byte offsets |
| 0x10 | `field_10` | Per-vertex SZ table (`s32*`); indexed `vtxIdx >> 3` |
| 0x14 | `field_14` | OT base (`u_long*`); same slot as `TmdScratchDrawBlock.field_14` |
| 0x18 | `field_18` | Stream stride in words |
| 0x1C | `field_1C` | Remaining primitive count |
| 0x24 | `field_24` | GTE FLAG (`gte_stflg`) |
| 0x28 | `field_28` | NCLIP MAC0 / AVSZ3 OTZ; high bit set when FLAG bit 31 is set |
| 0x74 | `field_74`/`field_76`/`field_78` | Rotated normal via `gte_stsv` (IR1/IR2/IR3) |
| 0x7C | `field_7C`/`field_7E` | Packed SXY, then env-map UV (`(s16)sxy >> 4 + 0x20` plus `IR >> 8`) |

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
| 0x0 | `field_0` | s8 disk/scenario id (1 or 2). `Gp_TickPlayClock` rolls it from `Gp_LcgState` on player/companion death, sets 1 when `field_128 == 0xFF`, and uses `lbu` as `CdCmd_EnqueueLoadFile` index. Nonzero makes `func_800AC058` kill its task immediately |
| 0x1 | `field_1` | s8 flag; nonzero is an invuln/skip in `Gp_TickPlayClock` (restores `Wip_SysConfig.field_18` or `Mc_SaveData.field_6C8` to 1 and returns). Also skips the `Gp_CapBusy` / spawn path in `Gp_PostMsg13EF`, `Gp_SpawnEvt1IfCapIdle`, and `Gp_PostDirIfCapIdle` |
| 0x2 | `field_2` | Soft state flag |
| 0x4 | `field_4` | Byte used by CD/display helpers; address taken as a 4-byte location key. Also 1-based index into the innermost `Gp_ViewIndexTables` byte table (`Gp_GetViewIndex`). That byte then 1-based-indexes `Gp_SprtTables` records (`Gp_GetViewSprtExtra` returns `field_8`; `Gp_ViewSprtCmdEmpty` reads `field_4->field_2 == 0`) |
| 0x5 | `field_5` | u8; 1-based index into `Gp_ViewCountTables` / `Gp_RoomCoordTables` / `Gp_RoomObjTables` innermost tables (`Gp_GetViewCountLo`, `Gp_FindViewIndex`, `Gp_GetRoomCoordRec`, `Gp_LinkRoomObjects`). Second-innermost for `Gp_ViewIndexTables` |
| 0x6 | `field_6` | u8 room index (1-based for `Gp_ViewCountTables` / `Gp_ViewTables` / `Gp_RoomCoordTables` / `Gp_RoomParamTables` / `Gp_ViewIndexTables` / `Gp_SprtTables` / `Gp_RoomObjTables`) |
| 0x7 | `field_7` | u8 stage index (1-based for `Gp_ViewCountTables` / `Gp_ViewTables` / `Gp_RoomCoordTables` / `Gp_RoomParamTables` / `Gp_ViewIndexTables` / `Gp_SprtTables` / `Gp_RoomObjTables`). `Gp_GrantLocationItems` also uses it as the high byte of a packed location key and as the index into `D_8010F9F4` / `D_8010FA0C` |
| 0x9 | `field_9` | u8 location sub-index (`lbu`). `Gp_GrantLocationItems` packs it as the low byte of `(field_7 << 24) \| (field_6 << 16) \| (field_9 << 8)` |
| 0xC | `field_C[16]` | Pointer table (cleared by `Game_ClearPtrSlots`) |
| 0x4C | `field_4C` | Init flag |
| 0x4E | `field_4E` | Set by `Fs_LoadFile` for category-8 |
| 0x52 | `field_52` | s16 cleared by `Gp_FinishLoadWait` when `spawnArg1 == 0` |
| 0x58 | `field_58` | u16 current pad/button mask (`lhu`); `Gp_CaptureActorPad` copies it onto `GameActor.field_962` |
| 0x5E | `field_5E` | u8; set to 1 by `Gp_InitPlayClock` before allocating the play-clock idMap |
| 0x5F | `field_5F` | u8 flag; nonzero makes `func_800E74EC` skip overlay-wait timer setup |
| 0x64 | `field_64` | u8 flag; nonzero makes `func_800AD5B8` / `func_800AD50C` skip their state dispatch |
| 0x65 | `field_65` | u8; 1 makes `Gp_UpdateActorColor` skip the color-matrix rebuild unless `GameActorExt.field_18` is set (and `field_C` bit 0x80 is clear) |
| 0x66 | `field_66` | u8; 1 makes `func_800CE3B4` spawn `D_8010EB94` and scale with `Ui_Scale15(2)` (else `D_8010EAD0` / scale 1) |
| 0x68 | `field_68` | u8 flag; set to 1 by `func_800E7378` and cleared by `func_800E73E8` / `func_800E7434` when `D_8007218B != 9` |
| 0x69 | `field_69` | u8 flags; bit 0x1 skips the bank-load spawn in `func_800A0718` (sets `D_80062734 = 0xFF` instead); bit 0x2 skips `SndEvt_EnqueueType2(0, 0xB4)` when the last `GpStateF0.field_6` ref is released; bit 0x4 selects spawn arg 3 vs 2 from `D_80062774` |
| 0x76 | `field_76` | s16 flag; nonzero makes `func_800AD378` rebuild via `Gp_LinkRoomObjects` then clear it |
| 0x7C–80 | halfwords | Counters / mode |
| 0x11C | `field_11C` | s16 cache of `Mc_SaveData.field_22` (`lh`/`sh`). `Gp_LoadWaitBoot` refreshes it (and `field_11E` from `Wip_SysConfig.field_26`) when either is stale. `Gp_InitStarterInv` writes -1 to force that refresh after an inventory rebuild |
| 0x11E | `field_11E` | s16 cache of `Wip_SysConfig.field_26` (`lh`/`sh`). Refreshed with `field_11C` by `Gp_LoadWaitBoot` |
| 0x124 | `field_124` | u8 companion type 1/2/3. `Gp_PickCompanion` compares it to the selected type and returns 0 if already set; the caller stores a nonzero return here. Cleared when no companion table hits |
| 0x125 | `field_125` | u8; written with `Mc_SaveData.field_5C7` (`Gp_EnqueueCompanionCd` / `Gp_PickCompanion` table-2 high nibble) |
| 0x126 | `field_126` | u8 flag; nonzero skips `func_800A7A64` display-mode init |
| 0x127 | `field_127` | u8 (`lbu`); 0 runs the HP / companion-down checks in `Gp_TickPlayClock` |
| 0x128 | `field_128` | u8; `0xFF` sentinel (`func_800B0748` / `func_800B082C`). `Gp_TickPlayClock` writes 1 when companion type is 1, 4 when type is 3 |
| 0x129 | `field_129` | u8; last `CdCmd_Enqueue(0x21)` `param1[0]` written by `Gp_EnqueueSndCd` (no-op if unchanged) |
| 0x12C | `field_12C` | u8 flag; 0 runs extra `func_800E06AC` pass in `func_800DB72C` |
| 0x12D | `field_12D` | s8 countdown (`lb`/`sb`); `0x7F` sentinel in `func_800A7320` |
| 0x12E | `field_12E` | u8; `func_800A76A4` copies it as `s8` into `D_80114BD8.field_2` |
| 0x12F | `field_12F` | u8; `func_800AAF70` writes `0x1E`. `Gp_TickPlayClock` copies it into `Task::killCountdown` on the death path |
| 0x139 | `field_139` | u8; `func_800E8888` writes `killCountdown * 2`, or 0 when that task kills itself |
| 0x13A | `field_13A` | u8; cleared by `Gp_PostDirIfCapIdle` when `D_80114CDC` is 0 |
| 0x13B | `field_13B` | u8 flags; cleared by `Gp_HaltPadScripts` with `Pad_ClearEvents(0)`. Bit 0 is set by `Gp_PadHoldTask` while its `spawnArg1` countdown runs and cleared when that task kills itself. Bit 0x80 lets `Gp_PadHoldTask` proceed when `D_801153F4` is set. |

### `GameActor` / `GameActorExt`
Sparse: `field_17C`/`field_930` addresses for overlay setup; `field_C` kill flag bit 0x80.
`field_930` is an s32 (`sw`); `func_800AE1F0` writes a lookup byte from `D_801149FC`
(or `(Gp_DirByte & 0x70) >> 4`) here. `Display_SpawnFromMode` still takes its address
for `func_801011D0`.
`GameActorExt.field_C` bit 0x8 is written by `Gp_WaitItemFlag2` on first run and
cleared before `Task_CallExit` when the 2-bit bank value is 2.
`field_17C` is an 18-entry `GpRec18` table (`Gp_ClearRec18Occupied` walks it from
`&field_17C` until `field_0` bit 0x2; `func_801041B4` returns 1 if any
`field_4 & 0x100100 == 0x100000`). `Gp_InitPlayerWork` stores `&field_17C` at
`field_90` / `field_9C` / `field_A8` and takes the address of `field_88` /
`field_94` / `field_A0` as `GpObj.field_C` for the `field_AC` / `field_CC` /
`field_EC` nodes.
`field_40`/`field_44`/`field_48` are three s32s (`VECTOR3` vx/vy/vz) copied from the
argument of `func_80105B74` onto the slot-3 actor.
`field_50`/`field_52`/`field_54` are an `SVECTOR` (vx/vy/vz) written by `func_80104D68`
from its pose argument and passed to `RotMatrix`; `field_52` is also the facing
angle (`lh`) used by `func_8010BCF4` / `func_80103E7C`.
`field_80` is an s16 sibling of `field_82`. `func_80104F5C` copies the low 16 bits
of its `GpFacingArg.field_0` here (and `field_4` onto `field_82`).
`field_82` is an s16 target facing angle. `func_80108BD8` compares it to `field_52`
(including the `tgt - 0x1000` wrap) and either snaps `field_52` to it or steps
toward it by a `func_80103E7C` delta clamped to `[-0x40, 0x40]`.
`field_58`/`field_5C`/`field_60`/`field_64`/`field_68`/`field_6A`/`field_70` are s16s
cleared together by `func_8010C46C` / `func_8010C4F0` / `func_8010C75C` (store order
0x60, 0x58, 0x64, 0x5C, 0x6A, 0x68, 0x70). `field_70` is a pitch-like s16: when
`field_90C` is set and the XZ lock distance exceeds `(s16)arg2`, `func_80102D20`
adds a `ratan2(-delta.vy, dist) - field_70` step clamped to `[-0x30, 0x30]`
(ignored if `|step| < 0x20`) while `|field_70 + step| < 0x281`. `field_6A` is the look/aim yaw offset. `func_80109720` (after the `field_954`
dispatcher) clears `extra->field_8[4].flg`, then if `field_962` has D-pad
left/right (`0xA000`) and `field_954` is 0, steps it by ±0x20 (left `-0x20`)
while `|field_6A + step| < 0x1A1`; otherwise decays it by `field_6A >> 3`
(minimum step ±0x40) and snaps to 0 when `|field_6A| < 0x41`. `func_8010BE5C`
turns it toward `ratan2(delta) - field_52` (clamped `[-0x20, 0x20]` per frame,
`|field_6A| < 0x1A0`).
`field_12A` is a u16 flags halfword at +0x1E of the `field_10C` list node (`lhu`/`sh`);
`func_8010C46C` / `func_80106350` AND it with `0x3FFF` (clear bits 14–15).
`field_424` is a 0x14-byte `GpAnimCtx` overlay; `Gp_AnimTickChildSlots` passes it to
`Gp_AnimTickIndex` for each slot `i = 1 .. field_938-1`. `Gp_AnimResetChildSlots` does the
same walk through `Gp_AnimResetSlot(..., i, arg1)`, then copies `field_985` onto
the slid overlay at `field_441`.
`pad_438` is the base of a 0x28-byte `GpAnimSlot` array (same stride as
`GameActorSlot`, but starting 0x10 before `field_448`); `func_80105B0C`
passes `&pad_438[i]` to `Gp_AnimTickSlot2` for `i = 1 .. field_938-1`.
`field_7A8` is the address `Gp_BindActorAnim` (and the same-shaped helpers
`func_801034C0` / `func_8010BFCC` / …) passes as `func_800B3F84` arg3
(`GpAnimCtx.field_8`).
`field_93A` is a u16 anim-table index (`sh` / `lhu`); `Gp_BindActorAnim` writes
`Gp_WeaponIdBase[Mc_SaveData.field_22 - 1] + Wip_SysConfig.field_21` (same sum as
`Gp_MsgPlayerWeapon` / `Gp_PlayerWeaponId`). `func_8010BFCC` writes
`Gp_AllyIdBase[Mc_SaveData.field_13 - 1] + Mc_SaveData.field_5C7` (same sum as
`Gp_AllyAnimId`). `func_80104CAC` writes `0x7FFF` (same
inactive sentinel as `GpAnimSlot.field_0`) when installing `GpAnimArg.field_0`.
`func_8010C4F0` writes `GpAnimArg.field_0` (low 16 bits) here when
`Gp_AnimBlkTbl[field_0]` differs from `field_928`.
`field_928` is the pointer at `Gp_PlayerAnimBlkTbl[field_93A]` (`Gp_BindActorAnim`) or
`Gp_AnimBlkTbl[field_93A]` (`func_8010BFCC`), passed as
`func_800B3F84` arg1 (`GpAnimCtx.field_0`). `func_80104CAC` copies
`GpAnimArg.field_0` here instead of indexing those tables. `func_8010C4F0`
assigns `Gp_AnimBlkTbl[arg2->field_0]` only when it differs from the current
pointer.
`field_448` is a 19-entry table of `GameActorSlot` (0x28 each; flags halfword at +0x00).
`Gp_InitPlayerWork` stores count `0x13` at `field_938`; `func_80105894` returns
`(slot[arg1].field_0 & 0x102) == 0`. `func_80101848` case 8 reads
`field_448[1].field_0` and continues when bit 0 or bit 1 is set (same halfword
as `GpAnimSlot.field_10` on the `pad_438` overlay).
`func_801058BC` clamps `arg2` to 1..0x7F, writes it through the slid-actor overlay
at `field_441` for `i = 1 .. field_938-1`, then stores the same byte at `field_985`.
`GameActorExt.field_18` is a pointer; a non-NULL value lets `Gp_UpdateActorColor` rebuild the
color matrix even when `Game_Session->field_65 == 1`, unless `field_C` bit 0x80 is set.
`GameActorExt.field_1C` / `field_20` are `MATRIX*` defaults (`Gp_DefaultMtx` / `Gp_DefaultMtx2`)
written by `Gp_BindDefaultMtx` onto the slot-3 extra and onto `field_920`/`field_924` extras.
`GameActorExt.field_8` is a `GsCOORDINATE2*` (`flg` cleared to 0 by `Display_SpawnFromMode`
and `Gp_ReparentCoord`; `sub` is the parent link, same convention as `Gfx_InitCoordinateTrees`).
Offset 0x18 / 0x20 are the low 16 bits of `coord.t[0]` / `coord.t[2]` (world X/Z);
`Gp_YawToPosXZ` loads them as `u16` (`GpCoordXZ`).
Offset 0x44 (`param` in libgs) is loaded as an s16 flag by `func_8010B590` (`GpCoordExt`).
Offset 0x46 is a signed yaw halfword written by `Gp_SpawnAtPlace` (`GpCoordPlace`);
a non-zero value is passed to `Gfx_RotMatrixY` on `&coord`.
Offset 0x4C (`sub` in libgs) is the parent `GameActorExt.field_8` pointer written by
`func_80104364` (with `field_44 = 1`) and `func_80104258` (with `field_44 = 0`).
`field_AC`/`field_CC`/`field_EC`/`field_10C`/`field_12C` are 0x20-byte list nodes
unlinked by `Gp_UnlinkObj` during actor teardown (`Gp_TeardownSlot0`).
`field_14C` is a `GpActorD4Rec` filled by `func_80100FCC` from `D_80112FA4[arg1]`
(low 16 bits of `vx`/`vy`/`vz` into `field_8`/`field_A`/`field_C`, mirrored to
`field_0`/`field_2`, `field_4 = field_C + D_80112F60[arg1]`) and linked from
`field_10C` as `GpObj.field_C`. `field_14` points at `field_32C`.
`field_32C` is a 6-entry `GpRec18` array wiped by `Gp_InitRec18Table(..., 6, 0)`.
`field_3D4` is a `GsCOORDINATE2` copied from the companion extra (`field_91C`)
coordinate; `func_80100FCC` then runs `Gfx_RotMatrixX` on `workm` (angle 0x400)
and zeros the three s16s at 0x418 (`param` as vx/vy/vz, `GpActorSvec`).
`field_124` is a u32 packed word at +0x18 of the `field_10C` node;
`func_801061F0` writes `0x20000 | (WipSysConfig.field_21 << 8) | field_22` from
slot 3; `func_80106238` replaces bits 14–15 with `(arg1 << 1) | arg2`;
`func_8010B79C` ORs in `0x80` on the slot-0xA companion.
`field_90C` is a `GpLinkNode*` (same object as `Gp_UnlinkNode` unlinks); `Gp_DetachLinkNode` clears `node->field_5` then nulls the slot. `Gp_ClearSlotNodeFlags` walks both `Gp_ActorSlots[]` slots and clears `field_5` without nulling the pointer. `Gp_NodeSlotMask` returns a 2-bit mask of those slots whose `field_90C` equals the given node. `Gp_AssignNodeSlot0` assigns the node onto `Gp_ActorSlots[0]`'s actor, clearing the previous node's `field_5` and this node's `field_4` bit 0. `Gp_ClearNodeSlots` is the inverse: it nulls any `Gp_ActorSlots[]` slot whose `field_90C` is this node, clears `field_5`, and sets `field_4` bit 0.
`field_910` is a `GpActorD4*` (0xD4-byte block from `Gp_SpawnAlly`); `func_8010BF7C` writes `field_C4` as `arg1 + (arg2 & func_80037164())`.
`func_80104258` tests it as a NULL check on the parent actor: non-NULL writes
`TmdObject.field_24/field_25 = 4/6` on the spawned extra, else `6/0`, then
runs `Tmd_ProcessStream` twice.
`field_914`..`field_924` are child `Task*` slots killed (if non-NULL) on that path.
`field_944`..`field_950` / `field_970` are s16 timers written to `0x258` by `func_8010A42C`;
`func_80109FC4` decrements `field_944`/`946`/`948`/`94A`/`94C`/`94E`/`950` (via `lhu`)
while the matching `WipSysConfig.field_25` bit is set and clears that bit when the
timer is `<= 0`; bit 4 also ticks `field_98D` and reloads it from `field_958`
(0 → `0x78`, 3 → `0x14`, else `0x3C`) after `func_8010A854(1)`; bit `0x80` is
skipped when `Gp_StateC08.field_A` is 2 or 3.
`field_954` is a u16 (`lhu`/`sh`) cleared with the 0x954–0x95E cluster (nonzero skips the 0x6A adjust in `func_80109720`);
`field_10` / `field_14` / `field_18` are s32s (`lw`/`sw`); `func_8010C30C` copies
`GsCOORDINATE2.coord.t[0..2]` from `GameActorExt.field_8` here after folding
`coord[1].coord.t` X/Z through `ApplyMatrixLV`.
`field_20` / `field_24` / `field_28` are s32s (`lw`/`sw`); `Gp_SetActorDest` copies
`GpVecArg.field_0` / `field_4` / `field_8` here. Optional `GpOverrideArg` (NULL zeros
both) copies `field_0` / `field_4` onto `field_93C` / `field_93E` and sets `field_956 = 4`.
`field_30` / `field_34` / `field_38` are u16s at 4-byte stride (`lhu`); `func_8010154C`
copies them onto a scratch `SVECTOR` when `field_986` is set, otherwise fills that
vector from `GsCOORDINATE2.workm` column 2 scaled by `field_973`.
`field_93C` is a u16 (`lhu`) mode override; `func_80108CC4` passes it to `Gp_AnimPlayChildSlotsEx` when nonzero (else 4);
`field_958` is an s16 mode written to 1 or 3 by `func_80105A8C` (third arg zero / nonzero);
`field_95A`/`field_95C` are u16s in that same cluster (`func_80109818` writes `field_95C = 5`);
`field_95A` indexes `D_80112E20` (`u16` facing steps): `func_80101F58` adds
`D_80112E20[field_95A] * field_975` onto `field_52` (masked `0xFFF`) when nonzero.
When `field_97E == 1`, `func_80101F58` decays `field_58`/`field_5C`/`field_60`/`field_64`/`field_70`
by `value >> 3` (minimum step `±0x20`) and snaps to 0 when `|value| < 0x21`, then
clears `field_97E` if all five were already 0;
`field_934` is an s32 (`lw`/`sw`); `func_8010AC54` treats it as a frame delay (decrement when nonzero; reload 5 after each `field_93E` step); `func_8010C75C` copies `GpDelayArg.field_14` here, writes `field_956 = 6`, and clears `field_93E`;
`field_93E` is an s16 step (`lhu`/`lh`/`sh`); `func_8010AC54` increments it when the delay expires, inlines the `func_8010AB70` body at 3, and indexes `extra->field_8` as `GsCOORDINATE2[4 - field_93E]` for `func_800EA478(0x600E0, …, 0x320, 0)`; `func_801088D4` writes `arg2` here (0 / 1 / 2);
`field_95E` is a u16 phase (`lhu`/`sh`); `func_8010ABD4` only runs the `func_8010AB70` body when it is 1; `func_8010AC54` writes 1 on first entry; `func_801088D4` writes `0x3E8` to abort the item-use path when `D_80112F1C[field_21][0]` is zero;
`field_960` is a u16 (`sh`) previous `field_956` saved by `func_80109290`; `func_801088D4` writes `arg1` here (item-use flags);
`field_962` is a u16 button mask (`lhu`); `Gp_CaptureActorPad` / `func_8010154C` copy `GameSession.field_58` here after saving the previous value to `field_964`; `func_80109250` maps D-pad up/down (`0x5000` / `0x4000`) onto `field_973` as `+1`/`-1`/`0`;
`Gp_ApplyDirArg` writes the same `+1`/`-1` when `GpDirArg.field_10 == 7` and the
XZ direction is non-zero: actor yaw is `ratan2(-coord.m[2][0], coord.m[2][2])`,
target yaw is `ratan2(field_0, field_8)`, and `+1` means the wrapped delta is
within 90° (`0x400`);
`field_964` is a u16 previous `field_962`; `Gp_CaptureActorPad` writes newly pressed bits to `field_966` (`field_962 & ~field_964`) and released bits to `field_968` (`field_964 & ~field_962`);
`field_966` is a u16 newly-pressed mask (`lhu`); `func_80104A4C` sets `WipSysConfig.field_24` when bit `0x20` is set and `field_954`/`field_956` are idle;
`field_968` is a u16 newly-released mask;
`field_96C`/`field_96E` are s16s cleared with `field_972` by `func_8010B210` (called from `func_8010A42C` case 2);
`func_8010A9D0` compares `field_96C` as `u16` (`lhu`) against 1 and passes `0x10` or `0x11` to `Gp_AnimPlayChildSlotsEx`;
`field_973`/`field_974` and `field_975`/`field_976` are signed-byte pairs compared by `func_80108568` (first mismatch → `func_80108770(..., 4)`; second mismatch only when `field_973 == 0` → `func_80108684`);
`Gp_CaptureActorPad` snapshots each pair (`974=973`, `976=975`, `978=977`) before overwriting `field_977` with bit 6 of the new `field_962`;
`field_97C` is a signed flag byte (`lb`/`sb`); `func_80108FD4` / `func_80108458` clear it before `func_80108E0C` or `Gp_DetachLinkNode`;
`field_97D` is a flag byte (`lbu`/`sb`); `func_80109374` writes 1 when `field_962`
bit 0x80 is set, `Gp_StateC08.field_3 == 0`, `Wip_SysConfig.field_21 != 0`, and
`field_991 == 0`, else 2. Callers test bit 0x1 (`func_801085D0` / `func_801090E8`)
or bit 0x2 (`func_80108458` / `func_80108FD4`). Bit 0x4 selects `func_801055D4`
vs `func_80108770` in `func_80106550`;
`field_97E` is a flag byte set to 1 by `Gp_DetachLinkNode` / `func_80103F70` (no `field_90C`, or `field_4` bit 0 set). `func_80103F70` compares it as signed (`lb`) against 2 before the `field_21 == 0x17` helpers;
`field_97F` is a signed result byte (`lb`/`sb`); `func_801060E0` writes 1 / 2 / 0 from button bits (mask 0x8/0x2, or 0x80/0x10 when `field_954` and `D_80072310` are both 2);
`field_981` is a u8 counter cleared with the 0x954–0x95E cluster; `func_801041FC` increments it from 0 (`lb`/`lbu`);
`field_983` is a u8 flag byte (`lbu`/`sb`); `func_8010AA28` ORs in `0x18` after `Gp_DetachLinkNode`;
`func_80104CAC` writes 7 or `0x38` from `GpAnimArg.field_10` (nonzero / zero);
`field_985` is a u8 default copied onto slid `field_441` by `Gp_AnimResetChildSlots`;
`func_80104CAC` writes `0x10` before that walk;
`field_987`/`field_988`/`field_989` and `field_98A`/`field_98B`/`field_98C` are two u8 groups written by command `0x401` (`func_80105AB0`): arg 0 sets them to (1,0,0) and (2,0,0); args 1–3 write the first group as `(arg+1, 0, 0)`; args ≥4 write the second as `(arg-3, 0, 0)` (and `field_98C`);
`field_98D`/`field_98E`/`field_990` are companion bytes (case 10 also stores `rand() & 0x1F + 0xA`).
`field_991` is a signed flag byte (`lb`); `func_80109374` requires it 0 to write `field_97D = 1`.
`field_993` is a u8 (`sb`) copy of the third arg of `func_8010B2D4` / `func_8010B348`, written only when `field_96C` is 0.
`func_8010AD64` loads it as `s8` (`lb`) and indexes the 0x20-byte `GpObj` nodes at `field_AC`; `field_8` of that node is the `GsCOORDINATE2*` passed to `func_800FDB18`. When the index is 0 the scratch `SVECTOR.vy` is `-0x190`, else 0.
`func_8010B348` switches on `(u16)GpIdRec.field_4`: 2/4 write `field_972 = 5`, 3 writes `field_972 = 0`, then stores `Gp_LookupIdField(field_4, 0)` to `field_96E`.

### `GpActorD4`
0xD4-byte block at `GameActor.field_910`, allocated and `Mem_Set(..., 0xD4)` by `Gp_SpawnAlly`.
| Off | Member | Role |
|-----|--------|------|
| 0x18 | `field_18` | `GsCOORDINATE2`; `Gp_BindActorD4` copies `GameActorExt.field_8` here and points the `GpObj` at 0x68 at it |
| 0x68 | `field_68` | 0x20-byte `GpObj`; `Gp_BindActorD4` links it via `Gp_LinkObj(1, …)` and ORs `flags` with 0xC800 |
| 0x88 | `field_88` | `GpActorD4Rec`; pose / id payload plus `field_14` → `field_A0` |
| 0xA0 | `field_A0` | `GpRec18` table wiped by `Gp_InitRec18Table(..., 1, 0)` |
| 0xC4 | `field_C4` | s16; `func_8010BF7C` stores `arg1 + (arg2 & rand)` |
| 0xCD | `field_CD` | u8; `func_8010B79C` copies `D_80167230[Mc_SaveData.field_5C7]` |

### `WipSysConfig`
`field_4` is a `MATRIX*` (`GsCOORDINATE2.coord`) stored by `Gp_InitPlayerWork`.
`field_21` is a u8 packed into `GameActor.field_124` bits 8–15 by `func_801061F0`.
It stores selected item id − 0x7F (`Gp_EquipHeld`); 0 means none selected.
`field_23` is a u8 storing selected item id − 0x5F for ids in 0x60–0x7F (`func_800CEC5C`).
`field_22` is a u8 packed into `GameActor.field_124` bits 0–7 by `func_801061F0`.
`Gp_SyncHeldRelated` sets it to `D_80072330[field_21 + 0x7F].field_0 + 0x61` when a
slot is occupied, else 0, then calls `func_801061F0`.
`field_24` is a u8 cleared by `Gp_MoveActorBy`; `Gp_MoveActorByKeep` saves and restores it around that call.
`field_25` is an OR mask of PE/status bits set by `func_8010A42C` (same bit as the `Gp_SetState1CPe` arg).
`func_80109FC4` ticks those bits each frame and writes the updated mask back.
`field_1a` is an s16 max recomputed by `Gp_RecalcMaxHp` (and inlined by
`Gp_UiBoostHp`) from `Gp_StatRows[field_F].field_0`
plus `Mc_SaveData.field_26` plus optional `Gp_ModStatAttrs[field_23-1].field_4`, then
clamped to 250. `field_18` is the matching current; `Gp_RecalcMaxHp` copies `field_1a`
down into it when current exceeds the new max. `Gp_UiBoostHp` then heals
`field_18` to `field_1a`.
`field_1e` is the matching max recomputed by `Gp_RecalcMaxMp`: signed per-slot
levels in `Mc_SaveData.unknown_850[0..11]` index `Gp_IdParamHi[base+j+1].field[1]`
(base steps by 3), plus optional `Gp_ModStatAttrs[field_23-1].field_6`, plus
`Gp_StatRows[field_F].field_4`, plus `Mc_SaveData.field_27`, then clamped to 250.
`field_1c` is the matching current; `Gp_RecalcMaxMp` copies `field_1e` down into
it when current exceeds the new max.

### `TmdObject` / `TmdSource`
| Off | Member | Role |
|-----|--------|------|
| TmdObject.0x0C | `field_C` | Flags; bit 2 (`0x4`) skips buffer (re)alloc (`Tmd_AllocMissingBuffers`). `Gp_FreeSlot4TmdBuffers` sets it before `Tmd_FreeBuffers`; `Gp_ApplyAreaTmdFlags` clears it when the matching `GpAreaTmdRec.field_8` is 1 and sets it when that halfword is 0x101 |
| TmdObject.0x10 | `field_10` | `TmdSource*` |
| TmdObject.0x18 | `field_18` | Aux buffer (`Tmd_AllocBuffers`) |
| TmdObject.0x24 | `field_24` | Tpage addend; `Tmd_ProcessStream` copies to scratch `field_70` as `s8`; `func_8009EB84` adds it to `POLY_GT3.tpage` |
| TmdObject.0x25 | `field_25` | Clut addend; `Tmd_ProcessStream` copies to scratch `field_72` as `(s8)<<6`; `func_8009EB84` adds it to `POLY_GT3.clut` |
| TmdSource.0x0 | `field_0` | Init flag |
| TmdSource.0x4 | `field_4` | Byte count for calloc×2 |
| TmdSource.0x20 | `field_20` | Command/data stream |

---

## Stream (stream)

### `StreamSlot` (0x28)
| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | Status (1 = active in polls; 2 = overlay/search match in `Gp_FindStreamSlot`) |
| 0x4 | `field_4` | Nonzero → active work |
| 0x8 | `field_8` | Copied into `CdCmdQueue.field_188` (malloc size) by `Gp_FindStreamSlot` |
| 0xE | `field_E` | Counter (compared to 0x64) |
| 0x1A | `field_1A` | Returned by accessors |

---

## Gameplay overlay (`include/gameplay/`)

### `GpActorWork` — `3FB8.h`
Task overlay: `actor` is `Task::idMap` at 0x1C; `extra` is `Task::extra` at
0x2C (`GameActorExt*`). `func_8010B120` passes `extra->field_8` to
`Gp_GetObjPan` / `Gp_GetObjDepth` as a `GpObj38*`.

### `GpEffWork` (0x2C) — `3FB8.h`
`Task::spawnArg2` for `func_800F1A9C` / `func_800F5184` / `func_800F75BC` /
`func_800F77F8` / `func_800F9474` / `func_800FB67C` / `func_800FB7E4` /
`func_800FBEBC` / `func_800FE41C`. Allocated by `func_800EA478` (`Mem_Calloc(0x2C)`).

| Off | Member | Role |
|-----|--------|------|
| 0x08 | `field_8` | Parent `GsCOORDINATE2*`; copied to `coord->sub` on first run |
| 0x10 | `field_10` | 3-halfword overlay passed to `func_800EA478` (`&field_10`); zeroed by `func_800FBEBC`; `SVECTOR` dest of `VectorNormalSS` / GPF source in `func_800F9474` |
| 0x12 | `field_12` | Per-frame Y step; `func_800FBEBC` inits `0xFFF0 - (LCG >> 16 & 0x3F)` and adds it to `coord.t[1]` |
| 0x14 | `field_14` | Zeroed with `field_10` by `func_800FBEBC` |
| 0x18/1A/1C | `field_18/1A/1C` | s16 translation; sign-extended into `coord.t[]`; `SVECTOR` source of `VectorNormalSS` in `func_800F9474` (LCG-filled if all zero) |
| 0x20 | `field_20` | Spawn-wave count (`func_800FC74C`); draw-step counter (`func_800FBEBC`, +1 every 4 `field_22` ticks, kill at 8); `(Gp_StateC08.field_0 % 10) - 1` (`func_800FB7E4`) |
| 0x22 | `field_22` | Step counter (`func_800F1A9C` / `func_800F5184` / `func_800F9474` / `func_800FB7E4` / `func_800FBEBC` / `func_800FE41C`) |
| 0x24 | `field_24` | Scale / packed `spawnArg1` lo / `(spawnArg1_lo * 3) >> 4` (`func_800F9474`) / 0x10 decay-by-2 (`func_800F5184`) / 0x80 decay-by-8 (`func_800FB67C`) / LCG draw param (`func_800FBEBC`) |
| 0x26 | `field_26` | Target scale / `spawnArg1` hi / 0x20 plus `field_2A` (`func_800F5184`) / 0x100 plus 0x80 (`func_800FB67C`) / `spawnArg1 & 0xFFF` (`func_800FBEBC`) / 0x20 (`func_800FB7E4`) |
| 0x28 | `field_28` | Size / lifetime (`field_26 << 2` in `func_800F9474` / `func_800FE41C`) / `D_8011291C[].field_0` draw param (`func_800F5184`) / packed RGB from `D_80112C6C` (`func_800FB67C`) / `spawnArg1 & 0xF000` (`func_800FBEBC`, bit `0x8000` selects LCG `| 0x1000`) / `(field_20 << 7) + 0x180` (`func_800FB7E4`) |
| 0x2A | `field_2A` | Packed draw param for `func_800F7AD4`, or `D_8011291C[].field_2` step, or `(field_20 << 8) + 0x400` (`func_800FB7E4`) |

### `GpPickScratch` (0x68) — `3FB8.h`
Scratch from `G_SCRATCH_HEAD` for `func_80105BC4`. Picks the nearest
occupied `GpRec18` (bit `0x100000` in `field_4`, `GpRoomParamRec.field_2`
nonzero) and spawns via `func_800EA478`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `pad_0` | `GpDeltaScratch` overlay passed to `func_800E0FEC` |
| 0x10 | `flg` | `GsCOORDINATE2.flg` (written as 1); address passed to `func_800EA478` |
| 0x48 | `t[3]` | `GsCOORDINATE2.workm.t[]` from the chosen `GpRec18` |
| 0x5C | `sub` | `GsCOORDINATE2.sub` (cleared) |
| 0x60 | `offset` | Three `rand() & 7` halfwords; `func_800EA478` last arg; added onto `arg2->workm.t[]` |

### `GpPadReplay` (0x4) — `gameplay.h`
Recorded pad pair in the demo/replay stream at `Gp_ReplayCursor`. `Gp_ApplyPadReplay`
plays these into `PadScratch->buttons`. `Gp_ReplayButtons` caches the current
buttons; `Gp_ReplayFramesLeft` is the remaining frame count. Stream start is
`D_8005C374 + 0xD4C`, or `0x80600E4C` when `Display_State.field_12c == 0x10`.

| Off | Member | Role |
|-----|--------|------|
| 0x0 | `buttons` | Replay button mask copied to `PadScratch` |
| 0x2 | `duration` | Frames to hold this mask; `0xFFFF` buttons ends the stream |

### `GpStateC08` (0x18) — `gameplay.h`
Global at `Gp_StateC08`. Splat also emits per-byte labels (`D_80114C0A` / `D_80114C0B` / …)
for the same block.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | u16 loaded by many helpers |
| 0x02 | `field_2` | s8 (`lb`/`sb` as splat `D_80114C0A`); `func_800A1F64` writes the low byte of `func_800A1558(3)`, or 1 if that value is <= 0 |
| 0x03 | `field_3` | s8 state (`lb`); `func_80109290` compares to -2; `func_80109374` requires 0; `func_800A7DE0` writes 2 when `field_A >= 2`; `func_800A1F64` writes -2 |
| 0x05 | `field_5` | s8 category index (`lb` as splat `D_80114C0D`); `func_800A1558` reads a `Gp_IdParamHi` row from it when `< 0xC` |
| 0x06 | `field_6` | Flags; bit 0 gates `func_800A7DB8` writing `field_E`; bit 1 cleared by `Gp_ResetHudFx` |
| 0x07 | `field_7` | Cleared by `func_800A7DE0` |
| 0x08 | `field_8` | Cleared by `func_800A7DE0`; set to 1 by `func_800A1F64` |
| 0x09 | `field_9` | Cleared by `func_800A1F64` |
| 0x0A | `field_A` | s8 (`lb`, splat `D_80114C12`); `func_800A7DE0` sets `field_3 = 2` when >= 2, then clears it; cleared by `Gp_ResetHudFx`; set to 2 by `func_800A1F64`; `func_80109FC4` loads it `lbu` and skips the `field_25` bit `0x80` timer when the value is 2 or 3 |
| 0x0B | `field_B` | s8 category index (`lb`); `func_800A1634` uses this unless its first arg is 1, in which case it uses `field_5` |
| 0x0C | `field_C` | s8 (`lb` as `D_80114C14`); cleared by `Gp_ResetHudFx`; `Gp_ScaleDamage` uses it as a signed index into `D_80113CFC` (`((x/16)-1)*2 + (x%16)`) when non-zero |
| 0x0D | `field_D` | Cleared by `Gp_ResetHudFx` |
| 0x0E | `field_E` | Written by `func_800A7DB8` when `field_6` bit 0 is clear; cleared by `func_800A7DE0` / `Gp_ResetHudFx` |
| 0x0F | `field_F` | Cleared by `Gp_ResetHudFx` |
| 0x10 | `field_10` | s16 (`lh` as `D_80114C18`); cleared by `Gp_ResetHudFx` |
| 0x12 | `field_12` | s16 (`lh` as `D_80114C1A`); cleared by `Gp_ResetHudFx`; nonzero keep-alive gate in `func_800FB7E4` |
| 0x14 | `field_14` | s16; cleared by `Gp_ResetHudFx` |
| 0x16 | `field_16` | s8 (`lb`/`sb` as `D_80114C1E`); cleared by `Gp_ResetHudFx` |
| 0x17 | `field_17` | Cleared by `Gp_ResetHudFx` |

### `GpStateBE8` (0x8) — `gameplay.h`
Global at `Gp_HpMpWork`. Working copies of two `Wip_SysConfig` halfwords.

| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | s32; sign-extended `Wip_SysConfig.field_18` (`Gp_ResetHudFx` / `Gp_UiBoostMp` / `Gp_UiBoostHp`) |
| 0x4 | `field_4` | s32; sign-extended `Wip_SysConfig.field_1c` (splat label `D_80114BEC`) |

### `GpIdMapC` (0x1A) — `gameplay.h`
+0xC overlay of the 0x30-byte `Mem_Calloc` record stored at `Task::idMap` by
`Gp_InitPlayClock`. Nested as `GpIdMap30.extra`.

| Off | Member | Role |
|-----|--------|------|
| 0x16 | `field_16` | s8; set to -1 by `Gp_ResetHudFx` |
| 0x18 | `field_18` | s16; cleared by `Gp_ResetHudFx` |

### `GpIdMap30` (0x30) — `gameplay.h`
Play-clock work allocated by `Gp_InitPlayClock` and stored at `Task::idMap`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | s32 minutes; `Mc_SaveData.field_C / 60` |
| 0x04 | `field_4` | s32 seconds; `Mc_SaveData.field_C % 60` |
| 0x08 | `field_8` | s32 snapshot of `Display_State.field_4` |
| 0x0C | `extra` | `GpIdMapC` overlay passed to `Gp_ResetHudFx` |

### `GpStateBD8` (0x4) — `gameplay.h`
Global at `D_80114BD8`. Filled by `func_800A76A4` and passed as `Task_Spawn`
bank 1 type `0x31` `spawnArg2`.

| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | u8; cleared before spawn |
| 0x1 | `field_1` | u8; cleared before spawn |
| 0x2 | `field_2` | s16; `(s8)GameSession.field_12E` |

### `GpSprtCmd` (8) — `D4.h`
0xFFFF-terminated command list at `GpSprtRec.field_4`. `Gp_LinkViewSprts`
walks it; `Gp_LinkSprtCmd` / `Gp_EmitSprts` consume one record.

| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | u16 start index into `GpSprtRec.field_0` |
| 0x2 | `field_2` | u16 count of `GpSprtElem` / `GpSprtPrim` slots to process |
| 0x4 | `field_4` | u8; nonzero skips OT-linking that record's prims |
| 0x5 | `field_5` | u8; nonzero skips `Gp_LinkSprtCmd` and `Gp_SetSprtShadeBits` |

### `GpSprtElem` (0x14) — `D4.h`
Array at `GpSprtRec.field_0`. `Gp_LinkSprtCmd` / `Gp_EmitSprts` index from
`GpSprtCmd.field_0` for `field_2` entries. `Gp_EmitSprts` copies these
fields into a merged `DR_TPAGE`+`SPRT`.

| Off | Member | Role |
|-----|--------|------|
| 0x0 | `tpage` | u16 texture page; `0xE1000000 \| (tpage & 0x9FF)` |
| 0x2 | `clut` | u16 CLUT copied to the SPRT |
| 0x4 | `w` / `h` | s16 sprite size (copied as a word) |
| 0x8 | `x0` / `y0` | s16 sprite position (copied as a word) |
| 0xC | `otz` | u16 OT depth; shifted by `Display_State.field_128` then `>> 2` and `& 0xFFC` |
| 0xE | `u0` / `v0` | u8 texcoords (copied as a halfword) |
| 0x10 | `r0` / `g0` / `b0` | u8 RGB; skipped when `flags` bit 0 is set |
| 0x13 | `flags` | u8; bit 0 = shade-tex (skip RGB); OR'd into SPRT code |

### `GpTpageSprt` (0x1C) — `D4.h`
Merged `DR_TPAGE` + `SPRT` packet in the `D_80071190` primitive buffer.
`Gp_EmitSprts` writes one per `GpSprtElem` and `MargePrim`s them.

### `GpSprtRec` (0xC) — `D4.h`
Per-view record in tables pointed to by `Gp_SprtTables`.

| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | `GpSprtElem*` array (`Gp_LinkSprtCmd` arg0) |
| 0x4 | `field_4` | `GpSprtCmd*` command list |
| 0x8 | `field_8` | Returned by `Gp_GetViewSprtExtra` |

### `GpSprtPrim` (0x1C) — `D4.h`
Primitive slot in the `Gp_SprtLists` dual-buffer lists. `Gp_SprtCursor` is
the cursor; `Gp_LinkSprtCmd` OT-links `tag` and advances one slot.
`Gp_SetSprtShadeBits` sets or clears bit 0 of `field_F` for `field_2` slots.

| Off | Member | Role |
|-----|--------|------|
| 0x0 | `tag` | OT link word (`0xFF000000` length / `0xFFFFFF` address) |
| 0xF | `field_F` | u8; bit 0 set when `Gp_SetSprtShadeBits` arg is nonzero, cleared when 0 |

### `GpRoomObjRec` (0x10) — `D4.h`
Element of tables pointed to by `Gp_RoomObjTables`. Stage index is
`GameSession.field_7 - 1`, area index is `GameSession.field_6 - 1`,
record index is `GameSession.field_5 - 1`. `Gp_LinkRoomObjects` rebuilds
the current room's grid params and object lists from this record.

| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | `GpGridParams*`; parented to `&D_80070F10` and stored in `Gp_GridParams` |
| 0x4 | `field_4` | `GpObj4A*` array; linked onto `Gp_Obj4ALists[1]` (`Gp_LinkObj4A(1, …)`) |
| 0x8 | `field_8` | `GpObj4A*` array; linked onto `Gp_Obj4ALists[0]` (`Gp_LinkObj4A(0, …)`) |
| 0xC | `field_C` | `GpObj3A*` array; linked onto `Gp_Obj3ALists[0]` (`Gp_LinkObj3A(0, …)`) |

### `GpViewRec` (0x24) — `gameplay.h`
Element of tables pointed to by `Gp_ViewTables`. Stage index is
`GameSession.field_7 - 1`, room index is `GameSession.field_6 - 1`,
record index is `Gp_GetViewIndex() - 1`. Passed as `Task_Spawn` type-0xF
`spawnArg1` (`Gp_SpawnViewTasks` / `Gp_SpawnCurView`).

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `mtx` | `MATRIX`; rotation copied to `D_80070E44`, translation to `D_80070F28` (`Gp_LoadStageView` / `Gp_ApplyView`) |
| 0x20 | `field_20` | Extra word; `lhu` into `Display_State.field_110`, `lw` into GTE H / `gte_SetGeomScreen` (`Gp_LoadStageView` / `Gp_ApplyView`) |

### `GpDisp2dCoord` (0x50) — `gameplay.h`
`GsCOORDINATE2` overlay embedded in `GpDisp2d`. `Gp_AttachDisp2d` writes an
identity `mtx`, zeros `rot` as three halfwords at 0x44/0x46/0x48 (libgs
`param` / first half of `super`), and parents `sub` to `&D_80070F10`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `flg` | Coordinate dirty flag; cleared on init |
| 0x04 | `mtx` | Local `MATRIX`; identity (diagonal `ONE`) |
| 0x24 | `workm` | Work matrix (left zero from `Mem_Calloc`) |
| 0x44 | `rot` | `SVECTOR` overlay of libgs `param`/`super` |
| 0x4C | `sub` | Parent coordinate (`&D_80070F10`) |

### `GpDisp2d` (0x60) — `gameplay.h`
spawnType-2 task extra allocated by `Gp_AttachDisp2d` (`"new_disp_2d"`).
Linked onto `Tmd_ListAlt`. Freed by `Gp_FreeDisp2d`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `next` | Intrusive `Tmd_ListAlt` next |
| 0x04 | `prev` | Intrusive `Tmd_ListAlt` prev |
| 0x08 | `field_8` | Pointer to embedded `coord` |
| 0x0C | `field_C` | Word flag; set to 1 on create |
| 0x10 | `coord` | Embedded `GpDisp2dCoord` |

### `GpRoomBoundVec` (0x8) — `3A34.h`
Nested table entry at `GpRoomCoordRec.field_4`. Entry 0's `field_0` is the
max valid index. `Gp_GetRoomBound` returns `&table[GameSessionFrom4.field_0]`
when in range, else the default at `Gp_RoomBoundDefault`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Max index (entry 0) / signed X minimum (data entries) |
| 0x02 | `field_2` | Signed Y minimum (`func_800D7A9C`) |
| 0x04 | `field_4` | Signed Z minimum (`func_800D7A9C`) |
| 0x06 | `field_6` | Unused by `Gp_GetRoomBound` / `func_800D7A9C` |

### `GpRoomCoordRec` (0x8) — `3A34.h`
Element of tables pointed to by `Gp_RoomCoordTables`. Stage index is
`GameSessionFrom4.field_3 - 1`, room index is `field_2 - 1`,
record index is `field_1 - 1`. `Gp_GetRoomCoordRec` returns the record
or NULL if a table pointer is missing.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Word returned by `Gp_GetRoomCoordSet` |
| 0x04 | `field_4` | `GpRoomBoundVec*` nested table walked by `Gp_GetRoomBound` |

### `GpRoomParamRec` (0x4+) — `3A34.h`
Element of 8-entry pointer arrays selected by `Gp_RoomParamTables`.
Stage index is `GameSession.field_7 - 1`, room index is
`field_6 - 1`. `Gp_LoadRoomParams` copies each `field_3` into
`Gp_RoomParams[]`. Full object size is not known yet (`func_80105ED4`
loads a pointer at +0x4).

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Unused by `Gp_LoadRoomParams` |
| 0x01 | `field_1` | Flag; skip when nonzero (`func_800DDDF8`, `func_800DE7CC`) |
| 0x02 | `field_2` | Nonzero: slot is a valid spawn candidate (`func_80105BC4`) |
| 0x03 | `field_3` | Word copied into `Gp_RoomParams[i]` by `Gp_LoadRoomParams` |

### `GpLinkNode` (0x8) — `3A34.h`
| Off | Member | Role |
|-----|--------|------|
| 0x00 | `next` | Singly-linked list (`Gp_LinkList`) |
| 0x04 | `field_4` | Flag byte (bit 0 cleared on link) |
| 0x05 | `field_5` | Flag byte (cleared on link/unlink) |
| 0x06 | `field_6` | 1 = on list; 0 = unlinked |

Embedded at `GpEnemy.node` (+0x10). `Gp_UnlinkNode` also clears `GameActor+0x90C` slots that point at the node.

### `GpSlot70` (0xC) — `3A34.h`
32-entry table at `Gp_LockSlots`. `Gp_ClearLockSlots` zeros `field_0` / `field_4` / `field_6` only.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Object pointer (NULL = free; `func_800DA7B8` may store 4 as a sentinel) |
| 0x04 | `field_4` | Signed value (`lh`/`bgez`; incremented by `func_800DA6E8`) |
| 0x06 | `field_6` | Countdown timer (set to 0x14; decremented by `func_800DA7B8`) |
| 0x08 | `field_8` | Projected screen X (`swc2 SXY2` / `lh`) |
| 0x0A | `field_A` | Projected screen Y |

### `GpRec18` (0x18) — `session.h` (`GameActor.field_17C[18]`)
Array element cleared by `Gp_InitRec18Table` (`Mem_Set` of `count * 0x18`).
Walked by `Gp_CountRec18Hi`, which counts occupied slots whose `field_4`
high 16 bits match the argument. `Gp_ClearRec18Occupied` walks until `field_0`
bit 0x2 and, for each occupied slot, keeps only that bit and zeros the
payload fields (not 0xE / 0x16). `func_801041B4` scans all 18 actor
slots for `field_4 & 0x100100 == 0x100000`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | u16 flags: bit 0x1 = occupied; bit 0x2 = last (`Gp_InitRec18Table` writes 2 on the last element; `Gp_ClearRec18Occupied` does `field_0 &= 2`) |
| 0x02 | `field_2` | s16; cleared by `Gp_ClearRec18Occupied` |
| 0x04 | `field_4` | Packed word; `Gp_CountRec18Hi` compares `field_4 & 0xFFFF0000` to its argument; cleared by `Gp_ClearRec18Occupied` |
| 0x08 | `field_8` | s16; cleared by `Gp_ClearRec18Occupied` |
| 0x0A | `field_A` | s16; cleared by `Gp_ClearRec18Occupied` |
| 0x0C | `field_C` | s16; cleared by `Gp_ClearRec18Occupied` |
| 0x0E | `pad_E` | unused by `Gp_ClearRec18Occupied` |
| 0x10 | `field_10` | s16; cleared by `Gp_ClearRec18Occupied` |
| 0x12 | `field_12` | s16; cleared by `Gp_ClearRec18Occupied` |
| 0x14 | `field_14` | s16; cleared by `Gp_ClearRec18Occupied` (GCC hoists `&field_14` as the store base) |
| 0x16 | `pad_16` | unused by `Gp_ClearRec18Occupied` |

### `GpStateF0` (0x2C) — `3A34.h`
Global at `Gp_StateF0`. Full object may still be larger (`Gp_PendingObj4CFlag` is a
separate symbol at +0x34). `Gp_InitStateF0` zeros `field_0`..`field_2A`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | state byte (set to 1 by `Gp_ArmStateF0` if 0; set to 2 when refcount hits 0 in `Gp_ReleaseStateF0Add` / `Gp_ReleaseStateF0Clear` / `Gp_ReleaseStateF0`) |
| 0x01 | `field_1` | alternate-active flag (`lbu`; `Gp_IsStateF0Active` / `func_800A7CB0` / `Gp_EnqueueSndCdIfF0` / `Gp_CdIdleIfF0Active` OR it with `field_0 == 1 && field_6`); last-ref release sets 0x3C |
| 0x02 | `field_2` | bitset (`Gp_SetStateF0Bit` ORs `1 << (arg0 - 1)` when `arg0 != 0`); cleared on last-ref release |
| 0x03 | `field_3` | cleared with `field_2` on last-ref release; also written as `D_801153F3` by `Gp_SetStateF0Byte3` |
| 0x04 | `field_4` | u8; also `D_801153F4` |
| 0x05 | `field_5` | u8 count of claimed `GpSlot18`s; incremented by `Gp_ClaimSlot18` |
| 0x06 | `field_6` | u16 refcount (inc: `Gp_IncStateF0Ref`; dec: `Gp_ReleaseStateF0Add` / `Gp_ReleaseStateF0Clear` / `Gp_ReleaseStateF0`) |
| 0x08 | `field_8` | s32 accumulator; `Gp_ReleaseStateF0Add` adds `arg0->field_20->field_50->field_6`; cleared by `Gp_ReleaseStateF0Clear` on last-ref release |
| 0x0C | `field_C` | s32 accumulator; `Gp_ReleaseStateF0Add` adds `arg0->field_20->field_50->field_8`; cleared by `Gp_ReleaseStateF0Clear` on last-ref release |
| 0x10 | `field_10` | s32 accumulator; `Gp_ReleaseStateF0Add` adds `arg0->field_20->field_50->field_A`; cleared by `Gp_ReleaseStateF0Clear` on last-ref release |
| 0x14 | `field_14` | s32 accumulator; `func_800E2C78` adds `min_u(arg0->field_40, arg2)` when `(arg1 & 0x7F)` is 0x19..0x1B |
| 0x18–0x2A | `field_18`..`field_2A` | unknown u8s; cleared by `Gp_InitStateF0` |
| 0x2B | `field_2B` | u8 copy of `Mc_SaveData.field_F`; 4 when `field_F == 0` and `Mc_SaveData.field_E != 0`; 0 when `Gp_IsDebugAttachRoom() == 1` |

### `GpObj` (0x20 header) — `3A34.h`
Doubly-linked node unlinked by `Gp_UnlinkObj` and linked onto
`Gp_ObjLists[index]` by `Gp_LinkObj`. Embedded as 0x20-byte slots in
`GameActor` (`field_AC` / `field_CC` / `field_EC` / `field_10C` /
`field_12C`). Other list users may be larger; 0x20 is the header.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `next` | Intrusive next; NULL-terminated |
| 0x04 | `prev` | Previous node, or the list-head object when first |
| 0x08 | `field_8` | `GsCOORDINATE2*` stored by `func_8010C980` (first arg). `Gp_ObjWorldPos` applies `workm` to the 0x10 SVECTOR and adds `workm.t` |
| 0x0C | `field_C` | `GpRec18*` table wiped by `Gp_InitRec18Table`. `Gp_FindNearestSlot` / `func_800DEC80` treat it as a `GpActorD4Rec*` (`field_14` → `GpRec18` table; first 16 bytes as `SVECTOR[2]`) |
| 0x10 | `field_10` | s16; SVECTOR.vx (with `field_12` / `field_14`). Cleared by `func_8010C980`; loaded into GTE V0 by `Gp_ObjWorldPos` |
| 0x12 | `field_12` | s16; SVECTOR.vy |
| 0x14 | `field_14` | s16; SVECTOR.vz |
| 0x18 | `field_18` | s32 flags/mode word (`func_8010C980` ORs `0x30000`) |
| 0x1C | `field_1C` | s16 stored by `func_8010C980` (last arg) |
| 0x1E | `flags` | u16 flags: bit 0x8 = on list; bits 0x7 kept on unlink (type / kind); `func_8010C980` sets 1 then ORs 0x8000. Bit 0x800: `func_800DEC80` copies the first occupied `GpRec18` when `arg3 == 0`. Bit 0x400: copies the occupied slot whose `field_4` high 16 bits equal `0x10` |

### `GpGridParams` (0x24 overlay) — `3A34.h`
Sparse overlay pointed to by `Gp_GridParams`. Full object size is not known
yet; 0x24 is the minimum covering known fields (u16 at 0x20 plus
alignment pad).

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | `GsCOORDINATE2*` (`Gp_LinkRoomObjects` stores `&D_80070F10`). `Gp_LocalToGrid` applies `workm` via `ApplyTransposeMatrixLV` and subtracts `coord.t[0]` / `coord.t[2]` from transformed X / Z |
| 0x14 | `field_14` | s32 X origin added to world X before dividing by `field_20` |
| 0x18 | `field_18` | s32 Z origin added to world Z before dividing by `field_20` |
| 0x20 | `field_20` | u16 cell size; divisor for world-to-grid (`lhu`) |

### `GpObj38` (0x44) — `3A34.h`
Sparse overlay. When `GameActorExt.field_8` is passed in, this is a
`GsCOORDINATE2` and `field_24` is `workm`. Light helpers treat
`field_24.t` as a `VECTOR*`.

| Off | Member | Role |
|-----|--------|------|
| 0x24 | `field_24` | `MATRIX`; `Gp_GetObjPan` loads rot+trans and RTPS of the origin |
| 0x38 | `field_24.t[0]` | returned by `Gp_GetObjTransX`; light dir/pos X. `func_800D9A30` subtracts this from a world `VECTOR` |
| 0x3C | `field_24.t[1]` | light dir/pos Y |
| 0x40 | `field_24.t[2]` | `Gp_GetObjDepth` subtracts `Display_State.field_110` |

### `GpObj20` (0x24) — `3A34.h`
Sparse overlay. Full object size is not known yet.

| Off | Member | Role |
|-----|--------|------|
| 0x20 | `field_20` | `MATRIX*` whose `t[0]/t[1]/t[2]` are set by `Gp_SetObjTrans` |

### `GpObj44` (0x56) — `3A34.h`
Sparse overlay of the same light object as `GpObj38`. Full object size is
not known yet; 0x56 is the minimum covering known fields.

| Off | Member | Role |
|-----|--------|------|
| 0x44 | `field_44` | s16 room-id filter (`lh`); 0 = any room. `Gp_GetObjLuma` compares it to `(u8)Game_Session->field_4` |
| 0x4A | `field_4A` | s16 GTE IR0 scale; `Gp_GetObjLuma` writes `0x1000` (ONE). Nearby handwritten light helpers `lh` it into IR0 |
| 0x50 | `field_50` | s16; first of three color/luminance components (`lh`) |
| 0x52 | `field_52` | s16; `Gp_GetObjLuma` weights this by 6 |
| 0x54 | `field_54` | s16; `Gp_GetObjLuma` weights this by 2. Return is `((8*50 + 6*52 + 2*54) >> 8) + 0xF00` |

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
| 0x00 | `field_0` | u16 flags; occupancy is `(first word & 3) == 1`. `Gp_ClaimSlot18` ORs bit 0 |
| 0x02 | `field_2` | s16; cleared when the slot is claimed |
| 0x04 | `field_4` | payload pointer stored by `Gp_ClaimSlot18` |
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
| 0x54 | `field_54` | `GpSlot18*` table; NULL means `Gp_ClaimSlot18` is a no-op |

### `GpObj4A` (0x4C) — `3A34.h`
Array element linked onto `Gp_Obj4ALists[index]` by `Gp_LinkObj4A` and unlinked
by `Gp_UnlinkObj4A`. Walked at +0x4C until `field_4A` bit 0x80.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `next` | Intrusive next; NULL-terminated |
| 0x04 | `prev` | Previous node, or the list-head object when first |
| 0x08 | `field_8` | `GsCOORDINATE2*`; `Gp_LinkRoomObjects` stores `&D_80070F10` |
| 0x4A | `field_4A` | Flag byte: 0x20 = on list, 0x40 set after insert, 0x80 = last in array. Unlink keeps bits 0x87 |

### `GpObj3A` (0x3C) — `3A34.h`
List node linked onto `Gp_Obj3ALists[index]` by `Gp_LinkObj3A` and unlinked
by `Gp_UnlinkObj3A`. `Gp_ClearObj3AList` empties the whole list (clears the
head, then each node's prev / next / `0x20` flag). Same next/prev/`0x20`
flag protocol as `GpObj4A`. Full object size is not known yet; 0x3C is the
minimum covering known fields.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `next` | Intrusive next; NULL-terminated |
| 0x04 | `prev` | Previous node, or the list-head object when first |
| 0x3A | `field_3A` | Flag byte: 0x20 = on list, 0x40 = active (`func_800E0308`), 0x80 = last in array (`Gp_LinkRoomObjects`). Unlink keeps bits 0x87 |

### `GpRec4` (0x4) — `3A34.h`
Element of `Gp_IdField0`. `Gp_LookupIdField(idx, 0)` returns `field_0`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | u16 value returned by `Gp_LookupIdField` for table 0 |
| 0x02 | `field_2` | u16 companion word (unread by `Gp_LookupIdField`) |

### `GpRec6` (0x6) — `3A34.h`
Element of `Gp_IdField1`. `Gp_LookupIdField(idx, 1)` returns `field_0`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | u16 value returned by `Gp_LookupIdField` for table 1 |
| 0x02 | `field_2` | u16 companion word (unread by `Gp_LookupIdField`) |
| 0x04 | `field_4` | u16 companion word (unread by `Gp_LookupIdField`) |

### `GpRec10` (0xA) — `3A34.h`
Element of `Gp_IdParamLo`. Selected when an id's 0x8000 bit is clear;
index is `id & 0x7F`. Sibling accessors `Gp_GetIdParam0` / `Gp_GetIdParam1`
/ `Gp_GetIdParam2` return `field_4` / `field_6` / `field_8`.

| Off | Member | Role |
|-----|--------|------|
| 0x04 | `field_4` | u16 returned by `Gp_GetIdParam0` |
| 0x06 | `field_6` | u16 returned by `Gp_GetIdParam1` |
| 0x08 | `field_8` | u16 returned by `Gp_GetIdParam2` |

### `GpRec12` (0xC) — `3A34.h`
4-entry ranked table walked by `Gp_InsertRankedSlot` from `arg4` toward 0.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | s32 payload stored from `Gp_InsertRankedSlot` arg2 |
| 0x04 | `field_4` | s32 descending sort key (`Gp_InsertRankedSlot` arg1; ignored if <= 0) |
| 0x08 | `field_8` | s32 payload stored from `Gp_InsertRankedSlot` arg3 |

### `GpGiveRec` (0xC) — `3A34.h`
Location-keyed grant record walked by `Gp_GrantLocationItems`. Lists live at
`D_8010F9F4` (`Mc_SaveData.field_F` 0 or 2) and `D_8010FA0C` (otherwise),
indexed by `GameSession.field_7`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | s32 packed location key, or `-1` terminator |
| 0x04 | `items[4]` | u16 item ids; 0 is empty. Slot 3 also requires `func_800B9D80(0x80000)` |

### `GpRec16` (0x10) — `3A34.h`
Element of `Gp_IdParamHi`. Selected when an id's 0x8000 bit is set;
index is `id & 0x7F`. Sibling accessors `Gp_GetIdParam0` / `Gp_GetIdParam1`
/ `Gp_GetIdParam2` return `field_A` / `field_C` / `field_E`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field[8]` | 8 u16s. `func_800D50D4` indexes by arg1 after remapping id as `((id>>4&3)*3 + (id>>2&3))*3 + (id&3)` |
| 0x0A | `field[5]` | u16 returned by `Gp_GetIdParam0` |
| 0x0C | `field[6]` | u16 returned by `Gp_GetIdParam1` |
| 0x0E | `field[7]` | u16 returned by `Gp_GetIdParam2` |

### `GpObj4C` (0x50) — `3A34.h`
Sparse overlay. Full object size is not known yet.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `next` | Intrusive list link; `Gp_PendingObj4C` head walked by `Gp_ClearPendingObj4C` / `Gp_TakePendingObj4C` |
| 0x46 | `field_46` | u16 copied to `Gp_TakePendingObj4C` arg0 when `field_4B` is pending |
| 0x48 | `field_48` | u8 copied to `Gp_TakePendingObj4C` arg1; `Gp_CommitObj4CSave` matches it against `Game_Session->field_4` |
| 0x49 | `field_49` | u8 copied to `Gp_TakePendingObj4C` arg2; `Gp_CommitObj4CSave` copies it to `Mc_SaveData.field_4` |
| 0x4B | `field_4B` | Signed pending flag; `Gp_ClearPendingObj4C` stores 0 when non-zero; `Gp_TakePendingObj4C` / `Gp_CommitObj4CSave` consume it |
| 0x4C | `field_4C` | Flag byte; `Gp_SetObjFlag1` ORs bit 0; nearby helpers test bits 0x1 / 0x2 / 0x4; `Gp_RemapActorColor` default mode remaps the color matrix when bits 0xC are set |
| 0x4E | `field_4E` | Packed modes + flag: bits 0-1 current, bits 2-3 previous, high nibble (incl. 0x80) preserved by `Gp_SetLightMode`. `Gp_RemapActorColor` uses bits 0-1 as the lighting mode; bit 0x80 with `field_4B == 0` is a sine flicker that clears the bit |
| 0x4F | `field_4F` | Blend/transition timer; set to 0x10 by `Gp_SetLightMode` when the current mode changes |

### `GpPairSrcE` (0x10) — `3A34.h`
Wider view of the object pointed to by `GpObj50.field_50` /
`GpObj5C.field_50`. Full size is not known yet; 0x10 includes alignment
pad after `field_E`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | `GpU16Pair*` table packed by `Gp_PackObjPair` |
| 0x04 | `field_4` | u16 scale; `Gp_TickObjFlag4` multiplies it by `D_80113D38[field_5C]` / 100 |
| 0x06 | `field_6` | u16; `Gp_ReleaseStateF0Add` adds it into `Gp_StateF0.field_8` |
| 0x08 | `field_8` | u16; `Gp_ReleaseStateF0Add` adds it into `Gp_StateF0.field_C` |
| 0x0A | `field_A` | u8; `Gp_ReleaseStateF0Add` adds it into `Gp_StateF0.field_10` |
| 0x0B | `field_B` | u8; loaded by nearby pair-source helpers |
| 0x0C | `field_C` | u8; `Gp_TickObjFlag2` multiplies it by `D_80113D30[field_5D]` / 100 |
| 0x0D | `field_D` | u8; `Gp_SetObjFlag4` uses it as a threshold (`<< 12` / 100) |
| 0x0E | `field_E` | u8; `Gp_ObjFlag4Expired` multiplies it by `D_80113D28[field_5C]` / 100 |

### `GpObj5C` (0x60) — `3A34.h`
Sparse overlay of the same object family as `GpObj5D` / `GpObj50`.
Trailing pad keeps pointer alignment; full object size is not known yet.

| Off | Member | Role |
|-----|--------|------|
| 0x4C | `field_4C` | Flag byte; `Gp_SetObjFlag4` ORs bit 0x4; `Gp_ObjFlag4Expired` tests that bit |
| 0x50 | `field_50` | `GpPairSrcE*` source object |
| 0x59 | `field_59` | u8 countdown; seeded by `Gp_SetObjFlag4` and reseeded by `Gp_TickObjFlag4` as `((Gp_LcgState * 5 + 0x71357911) >> 16 & 0xF) + 0x53` |
| 0x5A | `field_5A` | u8 counter; cleared by `Gp_SetObjFlag4`; incremented by `Gp_TickObjFlag4` on `field_59` expiry; compared by `Gp_ObjFlag4Expired` against the scaled `field_50->field_E` |
| 0x5C | `field_5C` | u8 index into `D_80113D28` / `D_80113D38`; `Gp_SetObjFlag4` writes `Gp_StateC08.field_0 % 10` when `arg1` has the 0x8000 bit, else 0 |

### `GpObj5D` (0x60) — `3A34.h`
Sparse overlay of the same object family as `GpObj4C` / `GpObj50`.
`Gp_SetObjFlag2` initializes `field_58` / `field_5B` / `field_5D`.
Trailing pad keeps pointer alignment; full object size is not known yet.

| Off | Member | Role |
|-----|--------|------|
| 0x4C | `field_4C` | Flag byte; `Gp_SetObjFlag2` ORs bit 0x2 |
| 0x50 | `field_50` | `GpPairSrcE*` source object |
| 0x58 | `field_58` | u8 counter; compared by `Gp_TickObjFlag2` against the scaled `field_50->field_C` |
| 0x5B | `field_5B` | u8 tick; incremented toward 0x1F while `field_58` is below the limit, decremented to 0 above it |
| 0x5D | `field_5D` | u8 index into `D_80113D30` |

### `GpObj20E` (0x24) — `3A34.h`
Sparse overlay. Pointer at 0x20 is a `GpObj5C*` (same family as
`GpObj50`).

| Off | Member | Role |
|-----|--------|------|
| 0x20 | `field_20` | `GpObj5C*`; `Gp_ReleaseStateF0Add` loads `field_50` from this object |

### `GpEnemy` — `1BC.h`
0x60-byte work object (`Mem_Calloc` in `Gp_AllocEnemy`). Stored in `Task::spawnArg2`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `task` | Owning `Task*` |
| 0x08 | `field_8` | Packed work id; `Gp_SpawnAtPlace` writes `place.field_0 \| (place.field_4 << 8)` (same slot as `GpWorkObj.field_8`) |
| 0x0A | `field_A` | Work type halfword; `Gp_SpawnAtPlace` copies `GpEnemyPlace.field_2` (same slot as `GpWorkObj.field_A`) |
| 0x0C | `field_C` | Countdown word; `Gp_EnemyWaitStart` sets 0x78, `Gp_EnemyWaitTick` decrements and advances `Task::state` at 0 |
| 0x10 | `node` | `GpLinkNode` unlinked by `Gp_UnlinkNode` |
| 0x18 | `field_18` | `GsCOORDINATE2*`; set to `&D_80070F10` by `Gp_AllocEnemy`. `Gp_UpdateLinkXforms` reads it from the node overlay as `coord` |
| 0x1C | `field_1C` | Local `VECTOR3`; `Gp_UpdateLinkXforms` loads the low halves as an SVECTOR |
| 0x2C | `field_2C` | Player-relative `VECTOR3` written by `Gp_UpdateLinkXforms` after rotating by the transposed player `workm` |
| 0x3C | `field_3C` | `GpAreaPlace*` stored by `Gp_SpawnArea` (same slot as `GpWorkObj.field_3C`) |
| 0x40 | `field_40` | Signed halfword passed to `func_800A6A9C` by `Gp_HudTrackEnemy` |
| 0x4B | `field_4B` | Occupancy tag; `Gp_RemapActorColor` requires 0 together with `field_4E` bit 0x80 for the sine flicker |
| 0x4C | `field_4C` | Flag byte (same slot as `GpObj4C.field_4C`); `Gp_RemapActorColor` tests bits 0xC |
| 0x4E | `field_4E` | Flags; `func_800A4904` ORs bit 0x80. Lighting mode in bits 0-1 (previous in bits 2-3); `Gp_RemapActorColor` consumes bit 0x80 as a flicker request |
| 0x50 | `field_50` | `GpPairSrcE*`; `Gp_HudTrackEnemy` loads `field_4` (or -1 when `node.field_4` bit 0x8) |

### `GpAnimObj` — `1BC.h`
Source object for `Gp_AnimInitCtx` / `Gp_AnimInitCtxSlots`. Full size unknown.

| Off | Member | Role |
|-----|--------|------|
| 0x30 | `field_30` | Copied into `GpAnimCtx.field_10` |
| 0x34 | `field_34` | Address stored as `GpAnimCtx.field_4` (0x50-byte `GpAnimMtxRec` base) |

### `GpAnimPose` (0x10) — `1BC.h`
Translation + rotation pair used by `Gp_AnimWritePoseCopy`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `trans` | Copied into `GpAnimMtxRec.mtx.t` when `GpAnimSlot.field_B == 1` |
| 0x08 | `rot` | GPF/GPL-blended with the other pose, then `RotMatrix_gte` |

### `GpPackedSvec` (4) — `1BC.h`
11-10-11 packed signed vector. Unpacked `<< 3` / packed `>> 3` by `Gp_AnimBlendPacked`.

| Off | Member | Role |
|-----|--------|------|
| bits 0–10 | `vx` | Signed 11-bit X |
| bits 11–20 | `vy` | Signed 10-bit Y |
| bits 21–31 | `vz` | Signed 11-bit Z |

### `GpPackedPose` (0xC) — `1BC.h`
Packed translation + rotation (no `SVECTOR` pad). Used by `Gp_AnimBlendPose` when `GpAnimSlot.field_B == 1`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `vx` | Translation X; GPF/GPL-blended into `GpAnimScratch80.trans` |
| 0x02 | `vy` | Translation Y |
| 0x04 | `vz` | Translation Z |
| 0x06 | `rx` | Rotation X; copied into `vec0` / `vec1` |
| 0x08 | `ry` | Rotation Y |
| 0x0A | `rz` | Rotation Z |

### `GpAnimBlendSrc` (0x14) — `1BC.h`
At +4 of the 0x18-byte scratch `func_800B3448` allocates. Passed to `Gp_AnimBlendPacked` / `Gp_AnimBlendPose`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Current-frame source (`GpPackedSvec` if `field_B == 4`, `GpPackedPose` if `field_B == 1`) |
| 0x04 | `field_4` | Next-frame source; equal to `field_0` skips blend |
| 0x08 | `field_8` | Optional packed dest (`arg3` of `func_800B3448`) |
| 0x0C | `field_C` | Optional translation dest (`arg2` of `func_800B3448`); NULL writes `mtx.t` |
| 0x10 | `field_10` | Copy of `GpAnimSlot.field_17` |

### `GpAnimScratch80` (0x80) — `1BC.h`
Scratch from `G_SCRATCH_HEAD` for `Gp_AnimBlendPacked` / `Gp_AnimBlendPose` / `func_800B2998`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `trans` | GPF/GPL-blended translation (`Gp_AnimBlendPose`); copied to `mtx.t` or `field_C` |
| 0x08 | `vec0` | Unpacked `field_0` (`<< 3` in `Gp_AnimBlendPacked`; rotation in `Gp_AnimBlendPose`) |
| 0x10 | `vec1` | Unpacked `field_4`; packed back into `field_8` |
| 0x18 | `mtx0` | Used by `func_800B2998` |
| 0x38 | `mtx1` | Used by `func_800B2998` |
| 0x58 | `mtx2` | Used by `func_800B2998` |
| 0x78 | `blend` | `(s16)slot->field_C << 12` / `field_E`; 0 if frames match |
| 0x7C | `invBlend` | `0x1000 - blend` |

### `GpAnimMtxRec` (0x50) — `1BC.h`
Element of the array at `GpAnimCtx.field_4`, indexed by `GpAnimSlot.field_14`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Cleared by `Gp_AnimWritePoseCopy` after writing `mtx` |
| 0x04 | `mtx` | `RotMatrix_gte` dest; `t[]` at +0x18 from record start |

### `GpAnimRec` (0x4) — `1BC.h`
Element of the table at `GpAnimSet.field_0`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Halfword compared with `GpAnimSlot.field_6` in `func_800B3AA4` |
| 0x02 | `field_2` | Scaled `<< 4` into `GpAnimSlot.field_C` / `field_E` (`func_800B3E74`) |
| 0x03 | `field_3` | Opcode-like byte (`func_800B3AA4` tests `< 0xC0` / sign; `Gp_AnimResetSlot` stores `& 0xF` in `field_B`) |

### `GpAnimSet` — `1BC.h`
Object behind each pointer in `GpAnimSlot.field_20` / `GpAnimCtx.field_0`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | `GpAnimRec*` table; `Gp_AnimGetRec` returns `field_0 + slot->field_2` |

### `GpAnimSlot` (0x28) — `1BC.h`
Element of `GpAnimCtx.field_C`. Initialized by `Gp_AnimInitSlot` /
`Gp_AnimResetSlot` / `Gp_AnimResetSlotEx`; advanced by `func_800B3448`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Set index into `field_20`; `0x7FFF` = inactive (`Gp_AnimGetRec`); `Gp_AnimInitSlot` remaps 0→1 and takes abs |
| 0x02 | `field_2` | Record index within `field_20[field_0]->field_0`; `Gp_AnimInitSlot` writes the same `field_4[field_15]` value as `field_6` |
| 0x04 | `field_4` | Second set index; `Gp_AnimResetSlot` copies `arg2` here with `field_0` |
| 0x06 | `field_6` | Index into `field_20[set]->field_0` (`func_800B3E74`); `Gp_AnimResetSlot` loads `field_4[field_15]` |
| 0x09 | `field_9` | Written `0x10` by `Gp_AnimResetSlot` / `Gp_AnimInitSlot` |
| 0x0B | `field_B` | `recs[field_6].field_3 & 0xF` (`Gp_AnimResetSlot`); `Gp_AnimWritePoseCopy` copies pose translation when this is 1; `func_800B3448` dispatches 1 → `Gp_AnimBlendPose`, 4 → `Gp_AnimBlendPacked` |
| 0x0C | `field_C` | Timing value; `func_800B3E74` sets `field_2 << 4`; cleared by `Gp_AnimResetSlot` |
| 0x0E | `field_E` | Timing value; `func_800B3E74` sets the same `field_2 << 4` |
| 0x10 | `field_10` | Flags word; cleared by `Gp_AnimResetSlot` |
| 0x12 | `field_12` | Halfword; cleared by `Gp_AnimResetSlot` |
| 0x14 | `field_14` | Index into `GpAnimCtx.field_4` (`Gp_AnimWritePoseCopy`); `Gp_AnimResetSlot` writes `arg1` here and to `field_15`; `Gp_AnimResetSlotEx` writes `arg4` |
| 0x15 | `field_15` | This slot's index in the `field_C` array (`Gp_AnimResetSlotEx` writes `arg3`) |
| 0x16 | `field_16` | Byte; cleared by `Gp_AnimResetSlot` |
| 0x17 | `field_17` | Byte; cleared by `func_800B4114` / `func_800B4538` / `Gp_AnimPlaySlot` after writing a stack arg `<< 4` into `field_C` / `field_E` |
| 0x20 | `field_20` | `GpAnimSet**` table (copy of `GpAnimCtx.field_0`); `Gp_AnimPlaySlot` overwrites this and `GpAnimCtx.field_0` when the last arg is non-NULL |

### `GpAnimCtx` (0x14) — `1BC.h`
Context filled by `Gp_AnimInitCtx` / `Gp_AnimInitCtxSlots`. Used as arg0 by the
`func_800B3448` cluster.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | `GpAnimSet**` table (arg1); copied onto 0x28-byte slots at +0x20 |
| 0x04 | `field_4` | `&src->field_34`; 0x50-byte `GpAnimMtxRec` array (`func_800B3448` / `Gp_AnimWritePoseCopy`) |
| 0x08 | `field_8` | Pointer (arg3); 0x10-byte stride in `func_800B3448` |
| 0x0C | `field_C` | `GpAnimSlot*` array (`Gp_AnimInitCtxSlots` writes it; `Gp_AnimInitCtx` does not) |
| 0x10 | `field_10` | Copy of `src->field_30` |

### `GpAreaObj` — `1BC.h`
Object behind `GpAreaRec.field_4`. Full size unknown.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Signed id; compared with `GpAreaKey.field_5` |
| 0x01 | `field_1` | Flags (bits 0/1/2/4 in nearby 1BC / 1A8 helpers; `Gp_GetAreaFlag2` returns bit 1) |

### `GpAreaTmdRec` (0xC) — `1BC.h`
0xFF-terminated table at nested `GpAreaRec.field_4`. Walked by `Gp_ApplyAreaTmdFlags`.
`Gp_PollAreaCdLoads` views the same 0xC stride as `GpCdRec0C` (`field_2` / `field_4`).

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | u16 id; compared with `*GpWorkObj.field_3C`; 0xFF ends the table |
| 0x08 | `field_8` | `u16*`; 1 clears `TmdObject.field_C` bit 2, 0x101 sets it |

### `GpCdRec10` (0x10) — `D4.h`
0xFF-terminated CdCmd 0x21 source list at inner `GpAreaRec.field_0`.
Walked by `Gp_PollAreaCdLoads` (`D_80114C6C`).

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | u8 id; matches `GpCdRec0C.field_0`; 0 skips; 0xFF ends the list |
| 0x0C | `field_C` | u8 CdCmd 0x21 param1[0]; 0 skips the record |
| 0x0D | `field_D` | u8 CdCmd 0x21 param2[2] |
| 0x0E | `field_E` | u8 CdCmd 0x21 param2[3] |

### `GpCdRec0C` (0xC) — `D4.h`
0xFF-terminated list at inner `GpAreaRec.field_4` (`D_80114C68`). Same stride
as `GpAreaTmdRec`. `Gp_PollAreaCdLoads` matches `field_0` against `GpCdRec10.field_0`.

| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | u16 id; 0xFF ends the list |
| 0x2 | `field_2` | u16 packed location; `% 100` / `/ 100` when `>= 100` for CdCmd 0x21 param2[0] / param1[2] addend |
| 0x4 | `field_4` | u8 index into `D_8010CAD0` (param1[2] base) |

### `GpAreaRec` (0x8) — `1BC.h`
Element of tables pointed to by `Gp_AreaTables`. Indexed by `GpAreaKey.field_2`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Outer: nested `GpAreaRec*` table (`Gp_GetNestedAreaObj`). Inner (`Gp_GetNestedAreaRec` result): `GpCdRec10*` list (`Gp_PollAreaCdLoads`) |
| 0x04 | `field_4` | Outer: `GpAreaObj*` (`Gp_GetAreaObj`). Nested: `GpAreaTmdRec*` / `GpCdRec0C*` table (`Gp_ApplyAreaTmdFlags` / `Gp_PollAreaCdLoads`) |

### `GpAreaKey` — `1BC.h`
Location key for `Gp_AreaTables`. Same 4-byte prefix as `GameSessionFrom4` /
`D_8007216C`. Full size unknown.

| Off | Member | Role |
|-----|--------|------|
| 0x02 | `field_2` | Record index (`* 8`) into `Gp_AreaTables[field_3]` |
| 0x03 | `field_3` | Table index into `Gp_AreaTables` |
| 0x05 | `field_5` | Nested-record index (`Gp_GetNestedAreaObj`); compared with `GpAreaObj.field_0` |

### `GpWorkObj` — `1BC.h`
Overlay of `Task::spawnArg2` for sibling walkers. Full size unknown.

| Off | Member | Role |
|-----|--------|------|
| 0x08 | `field_8` | Id compared with the search key (`as_u16` in `Gp_FindChildType9`, `as_u8` in `Gp_FindChildExceptType9`) |
| 0x0A | `field_A` | u16; high byte is the work type (`Gp_FindChildType9` / `Gp_ExitChildrenType9` match 9; `Gp_FindChildExceptType9` skips 9) |
| 0x3C | `field_3C` | `u8*`; `Gp_ApplyAreaTmdFlags` compares `*field_3C` with `GpAreaTmdRec.field_0` |

### `GpSndMaskRec` (0x8) — `1BC.h`
Element of the 0-terminated table `Gp_SndMaskTable`. Walked by `Gp_ApplySndMasks`
(disable matching banks) and `Gp_ApplySndBankMasks` (enable matching banks).

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `mask` | Bit tested against the `u16` argument |
| 0x04 | `flags` | `SndEvt_EnqueueType7` / `SndBank_SetEnableFlags` bank id |

### `GpItemSlot` (0x8) — `268.h`
Indexed as `D_80072330[idx]` by `Gp_GetItemSlot`. Two (id, count) pairs.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Item id (first pair) |
| 0x01 | `field_1` | Count for `field_0` |
| 0x02 | `field_2` | Item id (second pair) |
| 0x03 | `field_3` | Count for `field_2` |

### `GpItemMap` (0x4) — `268.h`
Indexed as `Gp_ItemMaps[idx]` by `Gp_GetItemMap`. 8-entry map from item id
to a `GpItemSlot` pair.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Which `GpItemSlot` pair to write (0 = first, else second) |
| 0x01 | `field_1` | Item id / `D_80072330` index |
| 0x02 | `field_2` | Mapped item id written into that pair |

### `GpItemQty` (0x4) — `268.h`
32-entry tables at `Gp_RelatedQty0` (first `GpItemSlot` pair) and `Gp_RelatedQty1`
(second pair), indexed by `itemId - 0x80`. Looked up by `Gp_GetRelatedQty`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Count written into the matching `GpItemSlot` pair |
| 0x01 | `field_1` | Related item id (first of three) |
| 0x02 | `field_2` | Related item id |
| 0x03 | `field_3` | Related item id |

### `GpItemScan` (0x4) — `268.h`
Scan descriptor for `Gp_SumScanQty`. Built on the stack by `Gp_SumItemQty`
(memset 0, then `field_1 = 0xFF`). Same layout as `Mc_SaveData` at 0x5BC
(`D_80072724`), which other callers pass as `Gp_SumScanQty`'s first arg.
`D_80072724` is also `D_80072330 + 0x3F4` (`GpItemBlock.scan`); `Gp_UnequipRelated`
passes it that way so the slot base stays in `$v0`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Start index into a 4-byte item table |
| 0x01 | `field_1` | Number of table entries to scan |
| 0x02 | `field_2` | Table select (1 = `Gp_ItemTable1`, 2 = `Gp_ItemTable2`, else `D_80072314`) |

### `GpItemRec` (0x4) — `268.h`
Row in the item tables selected by `GpItemScan` (`D_80072314` /
`Gp_ItemTable2` / `*Gp_ItemTable1`). `Gp_GetScanSlot(scan, idx)` returns
`&table[scan->field_0 + idx]`. Also returned by `Gp_FindItemById`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Item id |
| 0x01 | `field_1` | Count (compared as signed by `Gp_EquipHeld` / `Gp_RefreshItemRow`) |
| 0x02 | `field_2` | u16 quantity accumulated by `Gp_SumScanQty` |

### `GpItemAttr` (0x8) — `268.h`
Indexed as `Gp_ItemAttrs[itemId]` by `Gp_GetModLevel`. Item ids 0x60–0x7F
map onto the 32-entry slice at `Gp_ModStatAttrs` (`Gp_ItemAttrs + 0x60 * 8`).

| Off | Member | Role |
|-----|--------|------|
| 0x04 | `field_4` | Unsigned bonus added to `Wip_SysConfig.field_1a` (`Gp_RecalcMaxHp`) when `field_23` is non-zero |
| 0x05 | `field_5` | Unsigned base added to `Mc_SaveData.field_908[itemId-0x60]`; result clamped to 10 |
| 0x06 | `field_6` | Unsigned bonus added to `Wip_SysConfig.field_1e` (`Gp_RecalcMaxMp`) when `field_23` is non-zero |

### `GpStatRow` (0x8) — `268.h`
4-entry table at `Gp_StatRows`, indexed by `Mc_SaveData.field_F`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Unsigned base written into `Wip_SysConfig.field_1a` (`Gp_RecalcMaxHp`) |
| 0x04 | `field_4` | Word added into `Wip_SysConfig.field_1e` (`Gp_RecalcMaxMp`) |

### `GpBit2Rec` (0x10) — `268.h`
0xFFFF-terminated records walked by `Gp_ApplyBit2List` / `Gp_ApplyBit2Bank` /
`Gp_LookupBit2Item`. Each record writes one 2-bit field into a dest bank.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Packed item id (low 4 bits = nibble index; `>> 4` = word index); 0xFFFF ends the list. Matched against `Gp_LookupBit2Item`'s arg |
| 0x02 | `field_2` | Item id published to `Gp_PubItemLoc` by `Gp_LookupBit2Item` |
| 0x06 | `field_6` | Extra halfword published to `D_80114DDE` by `Gp_LookupBit2Item`; low 2 bits are the value stored into the dest bank |

### `GpBit2List` (0x8) — `268.h`
Table of record-list pointers walked by `Gp_ApplyBit2List` / `Gp_ApplyBit2Bank`.
`Gp_Bit2Banks[i].field_0` points at one of these tables.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | `GpBit2Rec*` list (NULL skips; `(GpBit2Rec*)-1` ends the table) |

### `GpBit2Bank` (0x8) — `268.h`
Per-stage table at `Gp_Bit2Banks`, indexed by `GameSession.field_7` /
`GameSessionFrom4.field_3` / `Mc_SaveData.field_7`. `Gp_GetBit2Flag` /
`Gp_GetCurBit2Flag` extract a 2-bit field; `Gp_SetBit2Flag` / `Gp_SetCurBit2Flag`
write one. `Gp_ApplyBit2Bank` applies `field_0`'s record lists into `field_4`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | `GpBit2List*` table of record lists (`Gp_ApplyBit2Bank` / `Gp_ApplyBit2List`) |
| 0x04 | `field_4` | `u32*` packed 2-bit flags (16 fields per word; index `>> 4` / `& 0xF`) |

### `GpItemDesc` (0x8) — `4CC.h`
Indexed as `Gp_ItemDescs[itemId]` by `Gp_ItemUseRestricted`. `Gp_InitItemSeenBits` /
`Gp_GetItemText` select `Gp_ItemDescs[id]` when `id < 0x100` and
`Gp_ItemDescsHi[id]` otherwise (same raw index, different base). `field_3`
bit 0 selects whether `Gp_ItemUseRestricted` returns `arg1 == 1` (else 0),
before the `Gp_MoveItemKey == 0x703` / item `0x81` / `D_8007216F == 1`
override.

| Off | Member | Role |
|-----|--------|------|
| 0x03 | `field_3` | Flags; bit 0 tested by `Gp_ItemUseRestricted` |
| 0x04 | `field_4` | Name string; fields separated by NUL or `'\n'`. `Gp_InitItemSeenBits` skips 3 delimiters and sets the item's `field_6D0` bit when the next byte is `'\n'` |

### `GpItemObj8` — `4CC.h`
Object at `Task::spawnArg2` for `Gp_BindItemObj2` / `Gp_PublishItemObj` /
`func_800CE094`. Full size unknown.

| Off | Member | Role |
|-----|--------|------|
| 0x08 | `field_8` | Packed item id passed to `Gp_GetCurBit2Flag`; copied to `Gp_PubItemId` by `Gp_PublishItemObj` |
| 0x0A | `field_A` | Item/location halfword; copied to `Gp_PubItemLoc` by `Gp_PublishItemObj`, cleared on cancel by `func_800CE094` |

### `GpItemA0` (4) — `268.h`
Row in `Gp_StackLimits`, indexed by item id − 0xA0.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | Default stack count; copied to `Gp_PubItemQty` by `Gp_PublishItemObj` |
| 0x02 | `field_2` | Max quantity (`Gp_AddItem` / `Gp_LookupBit2Item`) |

### `GpItemObj2` — `4CC.h`
Object stored in `Task::extraState` by `Gp_BindItemObj2`. Full size unknown.

| Off | Member | Role |
|-----|--------|------|
| 0x02 | `field_2` | Set to 1 when `Gp_GetCurBit2Flag` returns 2 |

### `GpEvt12` (0xC) — `3CD8.h`
Current sequence table at `Gp_CapTable` (`Gp_StartCap` stores its first arg
there). `Gp_FindCapEvt` walks from a start index until `field_8 == -1` or
`field_5` equals `Gp_CapEventKey` (saved from `Gp_StartCap`'s third arg).

| Off | Member | Role |
|-----|--------|------|
| 0x4 | `field_4` | u8 flags copied to `D_80115670` by `Gp_ApplyCapEvtFlags`; bit 0 is cleared when `field_7 != 0` |
| 0x5 | `field_5` | u8 key compared with `Gp_CapEventKey` |
| 0x7 | `field_7` | u8 copied to `D_80115678` (countdown) by `Gp_ApplyCapEvtFlags` |
| 0x8 | `field_8` | s32; `-1` terminator, else relocated `u16*` encoded text (`Gp_RelocCapFile` adds the file base). During that relocate, `-1` also skips the next record. `Gp_CapTextTopY` / `Gp_CapTextHeight` / `Gp_CapCenterX` walk it: `-1` ends, `-2` is a newline, `-3` is skipped, else glyph index `& 0x3FF` into `Gp_CapGlyphs` |

### `GpCapFile` (0x14) — `3CD8.h`
In-memory CAP dialogue file (`strncmp` magic `"CAP"`, `Gp_StrCapMagic`).
Folder-slot type 3 (`D_8006C338[].field_4` → `Gp_CapFile`) is passed to
`Gp_RelocCapFile`. Offsets are file-relative until that function adds the
file base (`+= (s32)file`); `field_8 <= 0` skips relocation but still
publishes the (possibly null) tables.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `magic` | `char[4]`; must match `"CAP"` (3 chars) |
| 0x04 | `field_4` | unused by `Gp_RelocCapFile` |
| 0x08 | `field_8` | glyph table offset / `GlyphUvwh*` → `Gp_CapGlyphs` |
| 0x0C | `field_C` | event table offset / `GpCapEvtTable*` |
| 0x10 | `field_10` | pointer table offset / `GpCapPtrTable*` |

### `GpCapEvtTable` (0x10) — `3CD8.h`
Header at `GpCapFile::field_C`. Records are `GpEvt12` starting at `hdr + 1`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `count` | s16 number of event records to relocate |
| 0x02 | `pad_2` | 0xE unread bytes |

### `GpCapPtrTable` (4) — `3CD8.h`
Count word at `GpCapFile::field_10`. `Gp_RelocCapFile` relocates each
following nonzero `s32` (file-relative `GpEvt12*`) and stores `hdr + 1`
in `Gp_CapCmds`. `Gp_StartCapSlot` indexes that array.

| Off | Member | Role |
|-----|--------|------|
| 0x0 | `count` | number of pointers after the header |

### `GpVolFade` (4) — `3CD8.h`
Volume-fade payload at `Task::spawnArg2` for `Gp_VolFadeTask`. Start
volume is snapshotted from `D_8007A396` into `D_801156C0`; `D_801156C2`
counts frames. Completing or instant-applying the fade clears
`D_8010FBE4` (the live fade task).

| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | Target volume (`Snd_ApplyVolumeTable`) |
| 0x2 | `field_2` | Duration in frames; `0` applies `field_0` immediately |

### `GpSndFade` (0xC) — `3CD8.h`
Type-A sound-param fade at `Task::spawnArg2` for `Gp_SndFadeTask` (bank 9
type 0xE). Live instance is `D_801156E0`, filled by `func_800E75C8`. Start
param is snapshotted from `field_4` into `D_801156C4`; `D_801156C6` counts
frames. Completing or instant-applying the fade clears `D_8010FBE8`.

| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | Sound id (`SndEvt_EnqueueTypeA` a0) |
| 0x4 | `field_4` | Start / current param (updated each frame) |
| 0x6 | `field_6` | Target param (passed as `s8` to Type A) |
| 0x8 | `field_8` | Duration in frames; `0` applies `field_6` immediately |

### `GpScriptCmd` (4) — `3CD8.h`
One step of the dual script at `GpState34::field_0`. Indexed by the
script A/B program counters. Low byte of each halfword is the opcode
handled by `Gp_StepScriptA` / `Gp_StepScriptB`; high byte is the payload.

| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | Script A command |
| 0x2 | `field_2` | Script B command |

### `GpScriptRec` (4) — `3CD8.h`
Pad/interpolator record at `GpState34::field_4`. Opcode 1 indexes this
table by the command high byte. Script A copies `field_2` into delay A
and calls `Gp_SpawnPadHold`; script B also passes `field_0`/`field_1` to
`Gp_SpawnPadLerpScaled`.

| Off | Member | Role |
|-----|--------|------|
| 0x0 | `field_0` | Start value (script B → `Gp_SpawnPadLerpScaled`) |
| 0x1 | `field_1` | End value (script B → `Gp_SpawnPadLerpScaled`) |
| 0x2 | `field_2` | Delay / duration copied to `field_10` / `field_11` |

### `GpState34` (0x34) — `3CD8.h`
Allocated by `Gp_ScriptInit` (`Mem_Calloc(0x34, 0)`); stored at `Task::idMap`.
Dual script interpreter: A uses `field_E`/`field_10`/`field_14`, B uses
`field_F`/`field_11`/`field_15`. `Gp_TickScriptADelay` / `Gp_TickScriptBDelay` decrement
the delay counters and advance via `Gp_StepScriptA` / `Gp_StepScriptB`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | `GpScriptCmd*` table (from `Task::spawnArg2`); A uses `field_0`, B uses `field_2` |
| 0x04 | `field_4` | `GpScriptRec*` pad table; opcode 1 indexes it by the command high byte |
| 0x0A | `field_A` | Current command A (low byte = opcode) |
| 0x0C | `field_C` | Current command B (low byte = opcode) |
| 0x0E | `field_E` | Script A program counter |
| 0x0F | `field_F` | Script B program counter |
| 0x10 | `field_10` | Delay A; `Gp_TickScriptADelay` decrements |
| 0x11 | `field_11` | Delay B; `Gp_TickScriptBDelay` decrements |
| 0x12 | `field_12` | Opcode-1 index A (high byte of the command) |
| 0x13 | `field_13` | Opcode-1 index B |
| 0x14 | `field_14` | Loop counter A |
| 0x15 | `field_15` | Loop counter B |

Opcode (low byte of the current command), shared by `Gp_StepScriptA` (A) and
`Gp_StepScriptB` (B): 0 = stop, 1 = timed pad from `field_4`, 2 = set delay from
the high byte, 3 = set loop counter from the high byte if zero else decrement,
4 = jump to the high byte if the loop counter is nonzero else step and recurse.

### `GpState18` (0x18) — `3CD8.h`
Allocated by `Gp_SpawnScript18` / `Gp_SpawnScript18Ex` (`Mem_Calloc(0x18)`); stored at
`Task::idMap` of the bank-2 type 0xD task. `Gp_DispatchScript18` indexes two
dispatch tables with `field_A` and `field_C`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | s32; first spawn arg (`Gp_SpawnScript18` / `Gp_SpawnScript18Ex`) |
| 0x04 | `field_4` | s32; second spawn arg |
| 0x08 | `field_8` | s16; cleared by `Gp_SpawnScript18`, third arg of `Gp_SpawnScript18Ex` |
| 0x0A | `field_A` | u8 dispatch index A (`Gp_DispatchScript18`) |
| 0x0C | `field_C` | u8 dispatch index B (`Gp_DispatchScript18`) |

### `GpState1C` (0x1C) — `3CD8.h`
Allocated by `Gp_InitState1C` (`Mem_Calloc(0x1C, 0)`); pointed to by `Gp_State1C`.
Most members are s16. `Gp_InitState1C` writes the block to the owner task at
`Task::idMap` and stores that task in `Gp_State1CTask`. `func_800EA3A0` writes
`arg0 + 1` to `field_C`.

| Off | Member | Role |
|-----|--------|------|
| 0x06 | `field_6` | s16; initialized to 1 by `Gp_InitState1C` |
| 0x10 | `field_10` | s16 flags; `func_800EC9C8` tests bit 0 before `func_800EA478(0x800600E8, …)` |
| 0x12 | `field_12` | s16 flags; bit `0x400` set by `func_800FB7E4`, bit `0x800` set by `func_800FC500` and cleared by `func_800ECA54` |
| 0x18 | `field_18` | s16 PE/status bit written by `Gp_SetState1CPe` (low byte of arg; same bits as `WipSysConfig.field_25`) |
| 0x1A | `field_1A` | u16 flags; `Gp_PulseState1C80` ORs in `0x80`, `Gp_PulseState1C` ORs in `0x100` |

### `GpCoord64` (0x64) — `3CD8.h`
Eight-slot table at `Gp_RoomCoords`. `Gp_InitRoomCoords` inits every slot;
`Gp_DecRoomCoordRefs` decrements `field_0` when non-zero. `Gp_CountRoomCoords`
returns the number of slots whose `field_0` is non-zero.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | s32 refcount; cleared by `Gp_InitRoomCoords`, decremented by `Gp_DecRoomCoordRefs`, counted by `Gp_CountRoomCoords`; `func_800ED198` sets it to 1 or 4 |
| 0x04 | `coord` | Embedded `GsCOORDINATE2`; `coord.sub` is parented to `&D_80070F10` |
| 0x54 | `field_54` | s16; `func_800ED198` inits to `0xC00` (same as `GpCoordTail.field_50`) |
| 0x56 | `field_56` | s16; `func_800ED198` inits to `0xC00` |
| 0x58 | `field_58` | s16; `func_800ED198` inits to `0xC00` |
| 0x5C | `field_5C` | s32; `func_800ED198` inits to `0xFA0` and subtracts `0x190` while `>= 0x191` |
| 0x60 | `field_60` | s32; `func_800ED198` inits to `0x12C0` |

### `GpMapRec` (0xE) — `3688.h`
Per-room record in tables pointed to by `Gp_MapRecTables`. Index is
`GameSession.field_7 - 1` then `GameSession.field_6`.

| Off | Member | Role |
|-----|--------|------|
| 0x00 | `field_0` | s16 X coord (`func_800D02A4`) |
| 0x02 | `field_2` | s16 Y coord (`func_800D02A4`) |
| 0x04 | `field_4` | u16 X extent (`func_800D02A4`) |
| 0x06 | `field_6` | u16 Y extent (`func_800D02A4`) |
| 0x08 | `field_8` | s16 X scale (`func_800D02A4`) |
| 0x0A | `field_A` | s16 Y scale (`func_800D02A4`) |
| 0x0C | `field_C` | u8 room id stored in `Gp_MapRoomId` (`Gp_GetMapRoomId`); also `lhu` / `0xFFFF` sentinel (`func_800D02A4` / `func_800D1434`) |

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
