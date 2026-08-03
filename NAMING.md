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

| Prefix | Area | Source file(s) |
|---|---|---|
| `Fs_` | CD filesystem, STAGE*.CDF / STAGE0.HED | `12D18.c`, `fs.h` |
| `CdCmd_` | CD load command ring buffer | `C37C.c`, `game.h` |
| `Boot_` | Cold-boot / title path | `4CF8.c` |
| `Mem_` / `GHeap` | Heaps | `mem.c` |
| `GameMain` | Entry after `main` | `179D4.c` |
| `GpuExt_` | GPU helpers | `gpuext.c` |

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

Bulk renames for the FS/CD pass live in `tools/rename_fs_syms.py` (idempotent only if old names are gone). Extend that map rather than hand-editing hundreds of `.s` files.
