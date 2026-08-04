# Naming conventions

This decomp still has many address-based placeholders (`func_800xxxxx`, `D_800xxxxx`,
`GStructN`, `field_XX`). Prefer descriptive names as soon as a symbol’s role is clear.

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
| `Fs_` | CD filesystem, STAGE*.CDF / STAGE0.HED | `src/main/fs.c` (+ `fs` rodata/bss) | `include/main/fs.h` |
| `CdCmd_` | CD load command ring buffer | `src/main/cdcmd.c` | `include/main/fs.h` |
| `Boot_` | Cold-boot / title path | `src/main/boot.c`, `boot_loadbuf` data | `include/main/boot.h` (+ `Boot_LoadInitialFile` in `fs.h`) |
| `Mem_` / `GHeap` | Heaps | `src/main/mem.c` | `include/main/mem.h` |
| `Task_` | Cooperative task list / spawn / kill | `src/main/task.c` | `include/main/task.h` |
| `Pad_` | Controller state / button polls | `src/main/pad.c`, `src/main/padutil.c` | `include/main/pad.h` |
| `Mc_` | Memory-card save/load helpers | `src/main/mc.c`, `src/main/mcmenu.c` | `include/main/mc.h` |
| `Display_` | Dual DISPENV/DRAWENV + system flags | used from `gamemain.c` etc. | `include/main/display.h` |
| `GameMain` | Entry after `main` | `src/main/gamemain.c` | `include/main/gamemain.h` |
| `GpuExt_` | GPU helpers | `src/main/gpuext.c` | `include/main/gpuext.h` |

`include/main/game.h` is the shared kitchen-sink of still-generic types (`GStruct*`, UI helpers). It includes the module headers above for compatibility. Prefer including the specific module header when you only need that API.

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
- Use **whole-token** renames so `GStruct3` does not clobber `GStruct30`.

## Tooling

Bulk renames live in:

- `tools/rename_fs_syms.py` — FS / CD / boot
- `tools/rename_task_mc_pad_syms.py` — Task / Pad / Mc / Display

Idempotent only if old names are already gone. Extend the map rather than hand-editing hundreds of `.s` files.
