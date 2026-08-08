# Naming conventions

This decomp still has many address-based placeholders (`func_800xxxxx`, `D_800xxxxx`,
`field_XX`). Prefer descriptive names as soon as a symbol’s role is clear.

**Known field roles** (even when the C member is still `field_XX`) are catalogued in
[`STRUCT_FIELDS.md`](STRUCT_FIELDS.md). Update that file when you prove a new field.

## Scheme

| Kind | Pattern | Examples |
|---|---|---|
| **Module function** | `Module_VerbNoun` | `Fs_LoadFile`, `CdCmd_Enqueue`, `Boot_LoadInitialFile` |
| **Global data** | `Module_Name` (same style as functions; no `g` prefix) | `Fs_FileTable`, `CdCmd_Queue`, `Fs_StageCdfSectors` |
| **Types** | PascalCase role name | `CdCmdQueue`, `CdCmdEntry`, `TaskDesc`, `FsCdfFile` |
| **Struct tags** | `_TypeName` | `struct _CdCmdQueue` |
| **Known members** | camelCase role | `writeIdx`, `readIdx`, `cmd`, `busy` |
| **Unknown members** | `field_XX` / `unknown_XX` | keep until role is proven |
| **Unnamed functions** | `func_800XXXXX` (VRAM) | only until matched + understood |
| **Unnamed data** | `D_800XXXXX` or `D{file}{off}_…` | file-local prefix ok while WIP |

## Modules (current)

| Prefix | Area | Source / splat unit | Header |
|---|---|---|---|
| `Fs_` | CD filesystem, STAGE*.CDF / STAGE0.HED | `src/main/fs.c` | `include/main/fs.h` |
| `CdCmd_` | CD load command ring buffer | `src/main/cdcmd.c` | `include/main/fs.h` |
| `Fade_` / bootload | Boot-image load + fullscreen fade TILE | `src/main/bootload.c` | `include/main/gameflow.h` (Fade) / `fs.h` |
| `CdSync_` | CD seek/sync/disk-recovery helpers | `src/main/cdsync.c` | `include/main/fs.h` (CdCmd_*) |
| `CdVol_` | CD-DA volume table apply | `src/main/cdvol.c` | `include/main/fs.h` |
| `Mc_` / mcprompt | Memcard prompts + early Mc states | `src/main/mcprompt.c` | `include/main/mc.h` |
| `Gfx_` / gfxlight / gfxmtx | Flat lights + rotation matrices | `src/main/gfxlight.c`, `gfxmtx.c` | — |
| `Display_` / displaymode | Display mode / auto-clear setup | `src/main/displaymode.c` | `include/main/display.h` |
| `Task_` / taskutil | Small task helper near stage tables | `src/main/taskutil.c` | `include/main/task.h` |
| loadui | Loading SPRT + CD load enqueue | `src/main/loadui.c` | — |
| `Mdec_` | MDEC/STR strip decode | `src/main/stream.c` | `include/main/stream.h` |
| `Midi_` | Song block / MIDI sequencer | `src/main/sndevt.c` | `include/main/sound.h` |
| `SndVoice_` / `SndBank` / `SndBankSlot_` | SFX voice slots + bank table | `src/main/sndscript.c` | `include/main/sound.h` |
| `CdAudio_` | CD-driven audio player | `src/main/cdaudio.c` | `include/main/cdaudio.h` |
| `Gpu_` | OT / light / graph reset helpers | `src/main/otutil.c`, `tmd.c` | `include/main/display.h` (OT types) |
| `Boot_` | Cold-boot / title path | `src/main/boot.c` | `include/main/boot.h` |
| `Title_` | Title / demo / main-menu overlay | `src/title/title.c` | `include/main/title.h` |
| `Mem_` / `GHeap` | Heaps | `src/main/mem.c` | `include/main/mem.h` |
| `Task_` | Cooperative task list / spawn / kill | `src/main/task.c` | `include/main/task.h` |
| `Pad_` | Controller state / button polls | `src/main/pad.c`, `padutil.c` | `include/main/pad.h` |
| `Mc_` | Memory-card save/load helpers | `src/main/mc.c`, `mcmenu.c`, `mcprompt.c` | `include/main/mc.h` |
| `Ui_` | UI layout / draw / list chrome | `src/main/ui.c` | `include/main/ui.h` |
| `Text_` / `Font_` / `Prim_` | Text measure / glyph / SPRT helpers | `textdraw.c`, `textutil.c`, `font.c` | `include/main/text.h` |
| `Spu_` / `AsyncCb_` | SPU voices + async callback ring | `src/main/spu.c` | `include/main/sound.h` |
| `SndLoad_` / `SndScript_` / `SndEvt_` | Bank load / scripts / event queue | `sndscript.c`, `sndevt.c` | `include/main/sound.h` |
| `LinInterp_` / `AudioTick_` | Volume ramp + frame tick list | `src/main/sndbank.c` | `include/main/sound.h` |
| `Game_` | Session pointer-slot table | globals / `task.c` | `include/main/game.h` / `task.h` |
| `Display_` | Dual DISPENV/DRAWENV + system flags | used from `gamemain.c` etc. | `include/main/display.h` |
| `GameMain` | Entry after `main` | `src/main/gamemain.c` | `include/main/gamemain.h` |
| `GpuExt_` | GPU helpers | `src/main/gpuext.c` | `include/main/gpuext.h` |
| `GameFlow` / `Fade_` | Pre-pad/task game-flow handlers | `src/main/gameflow.c` | `include/main/gameflow.h` |
| `GameFlag_` | Packed 4-bit flag nibble table | `src/main/gameflag.c` | `include/main/gameflag.h` |
| `CdStream_` / `CdReady_` | CD→SPU MTS stream | `src/main/cdstream.c` | `include/main/cdstream.h` |
| `Tmd_` / `Stage_` | TMD models + stage flow | `src/main/tmd.c` | `include/main/tmd.h` / `stage.h` |
| `Stream_` | Stream channel slots | `src/main/stream.c` | `include/main/stream.h` |
| `Game_` | Main session object | globals | `GameSession`, `Game_Session` |
| `Wip` / `Wip_` | Weak-evidence placeholders | `wipsyscfg.c`, etc. | rename when proven |

`Wip*` types and `Wip_*` globals are provisional: keep them only until a better role name is proven. Prefer replacing a `Wip` name over inventing a second provisional alias.

`include/main/game.h` is an include aggregator (macros + module headers) for **main executable** TUs (`src/main/`) that historically only pulled this file. It is not shared with overlays; stage/file overlays may use a different `src/` / `include/` layout when decompiled. Types for main live in module headers:

| Header | Types |
|---|---|
| `session.h` | `GameSession`, `GameActor*`, `GBytes*` |
| `stage.h` | `StageCtx` |
| `wipsys.h` | `WipSysFlags`, `WipSysConfig` |
| `sound.h` / `ui.h` / `text.h` / … | subsystem types |

Prefer including the specific module header when you only need that subsystem.

## FS file-id encoding

When loading via `Fs_LoadFile` / `CdCmd_Enqueue` (cmd `0x21`):

```text
fileId = idB2 * 10000 + idB1 * 100 + idB0
```

Category tables:

| id / 10000 | Runtime table |
|---|---|
| 0 | `Fs_FileOffsetsCat0` |
| 1–4 | `Fs_FileTableCat1`–`Cat4` |
| 5 | `Fs_FileOffsetsCat5` |
| 90 | `Fs_FileOffsetsCat90` |
| high (≥ ~10) | `Fs_FileTable` |

## CD commands (non-exhaustive)

| `cmd` | Meaning |
|---|---|
| `0x21` | Load CDF file by packed id |
| `0x54` | Select / mount stage CDF (`Fs_SelectStage`) |
| `0x55` | Parse `STAGE0.HED` (`Fs_InitStage0Tables`) |
| `0x81` / `0x82` | Stream / audio related |

## What not to do

- Do **not** invent names for unanalyzed `GStructN` / `func_800*` just to “clean up.”
- Prefer **one rename PR/commit per subsystem** so `sym.main.txt` + `.s` basenames stay consistent.
- When renaming a `glabel`, also rename `asm/.../nonmatchings|matchings/.../<name>.s` and update `INCLUDE_ASM`.
- Use **whole-token** renames so `GStruct3` does not clobber `UiPanel`.

## Tooling

Bulk renames live in:

- `tools/rename_fs_syms.py` — FS / CD / boot
- `tools/rename_task_mc_pad_syms.py` — Task / Pad / Mc / Display
- `tools/rename_cdstream_syms.py` — CdStream / CdReady / MtsSector types + APIs
- `tools/rename_snd_font_syms.py` — Snd / Spu / AsyncCb / Font / TextStream / Prim / GameOt
- `tools/rename_sndscript_midi_syms.py` — SndScript / SndVoice / Midi / LinInterp / dialog UI types
- `tools/rename_evtuipanel_tmd_syms.py` — SndEvt / UiPanel / Tmd / TaskIdMap / GameSession
- `tools/rename_cdaudio_tick_syms.py` — CdAudio / AudioTick / SpuVoiceRange / StreamSlot / StageCtx
- `tools/rename_remaining_wip_syms.py` — last GStruct leftovers (+ Wip* provisional names)

Idempotent only if old names are already gone. Extend the map rather than hand-editing hundreds of `.s` files.
