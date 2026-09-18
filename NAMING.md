# Naming conventions

This decomp still has many address-based placeholders (`func_800xxxxx`, `D_800xxxxx`,
`field_XX`). Prefer descriptive names as soon as a symbol’s role is clear.

**Known field roles** (even when the C member is still `field_XX`) are catalogued in
[`STRUCT_FIELDS.md`](STRUCT_FIELDS.md). Update that file when you prove a new field.

## Scheme

Functions and data are **lowerCamelCase, one identifier, no separators**, opening
with the module or package that owns the symbol so a name still identifies
exactly one overlay. Three markers carry the rest of the meaning:

| Marker | Means | Example |
|---|---|---|
| none | a function | `fsLoadFile` |
| leading `g` | a global, public or not | `gFsFileTable` |
| leading `_` | private to its translation unit | `_fsReadSector`, `_gSectorCache` |
| PascalCase | a type | `FsCdfFile`, private `_SectorCache` |

A private symbol is simply the name it would have if public with `_` prepended,
so there is one rule rather than a special case per kind.

A leading underscore is reserved by the C standard — at file scope before a
lowercase letter, and in every scope before an uppercase one, which is what a
private type name uses. Nothing in this toolchain enforces either rule, and the
vendored Psy-Q library already ships `_SpuInit`, `_spu_init` and `_padStartCom`,
so the practice is established here. Our names carry a module prefix or a
distinct role word and so cannot collide with the library's.

| Kind | Pattern | Examples |
|---|---|---|
| **Module function** | `moduleVerbNoun` | `fsLoadFile`, `cdCmdEnqueue`, `bootLoadInitialFile` |
| **Overlay function** | `packageVerbNoun` | `gunbladeFireRound`, `pyrokinesisSpawnFlame` |
| **Shared overlay body** | `familySharedVerbNoun` | `weaponsSharedApplyRecoil` |
| **Global data** | `gModuleName` | `gFsFileTable`, `gPlayerStatus` |
| **Private function** | `_moduleVerbNoun` | `_fsReadSector` |
| **Private data** | `_gModuleName` | `_gSectorCache` |
| **Types** | PascalCase role name | `CdCmdQueue`, `TaskDesc`, `PlayerStatus` |
| **Known members** | camelCase role | `writeIdx`, `hp`, `hpMax` |
| **Unknown members** | `field_XX` / `unknown_XX` | keep until the role is proven |
| **Unnamed symbols** | `func_<package>_<VRAM>`, `D_<VRAM>` | generated; only until matched and understood |

The module or package part is derived mechanically, never invented:

- **Core** — the module prefix from the table below with its first letter
  lowercased: `Fs_` → `fs…`, `CdCmd_` → `cdCmd…`.
- **Overlay** — the manifest key, camelCased: `mine_mesa` → `mineMesa…`,
  `shelter_1f_bulwark` → `shelter1fBulwark…`. Do not abbreviate; the full key is
  unique across all 448 packages, while dropping a leading word collides.
- **Actors** — the actor id, until the packages are identified:
  `actor00300UpdateTransform`. One body serves that actor's several RAM slots,
  so the id, not a load address, is its identity.

Generated placeholders keep their `func_<package>_<VRAM>` form. The vacuum parses
that shape to recognise a promoted alias, so only human-assigned names take the
convention.

### Types

A struct carries a tag only when C requires one — when it refers to itself, or
when something else refers to it as `struct _Tag`. Everywhere else the typedef
stands alone, so a tag that *is* present means the type is used in one of those
two ways.

```c
typedef struct _GpLinkXform {      /* self-referential: tag required */
    struct _GpLinkXform *next;
} GpLinkXform;

typedef struct {                   /* plain value type: no tag */
    s16 x, y, z, yaw;
} PlayerPos;

typedef struct _SectorCache {      /* private, and self-referential */
    struct _SectorCache *next;
} _SectorCache;
```

A private type is marked the same way as any other private symbol, with a
leading `_`. Where such a type also needs a tag, the tag and the typedef share
the spelling; tags live in their own namespace, so no third name is invented.

### Public and private

A symbol is public only if something outside its translation unit reaches it.
Public symbols are declared in the owning module's header; private ones are
declared in the `.c` that defines them, are marked `_`, and are `static` where
the build still matches.

Two things decide this, and only one of them is visible from C:

- A function with no caller outside its own `.c` may still be reached from
  **assembly** — a dispatch table entry or a `jal` in an as-yet-unmatched body.
  Check with `find_references.py --asm` before privatising anything. Of the
  functions that look private from C alone, roughly two thirds are used this
  way.
- `static` is safer here than it looks. The usual hazard is the compiler
  inlining a body once it knows the function is file-local, but this build runs
  `-O2` without `-finline-functions`, which in this compiler generation comes
  only with `-O3`. Nothing is inlined unless it is explicitly marked, and a
  trial on five private functions changed no output. It can still affect
  register allocation where the compiler now sees every call site, so add it a
  subsystem at a time and let the checksum decide.

### File layout

A translation unit has two halves. The first holds the includes, type
definitions, forward declarations and whatever globals may safely live there.
The second holds the function definitions and nothing else.

The split is presentational for types and declarations, which the compiler may
see in any order. It is **not** free for definitions:

- **Function order is address order.** `.text` is emitted in definition order,
  so functions keep the sequence they already have. The second half is a place
  to gather them, not a licence to sort them.
- **Some globals cannot move.** `.rodata` is emitted in definition order too, so
  a global whose position is load-bearing has to stay between the functions it
  sits between. The clearest case is an alignment pad inserted so that a
  following compiler-generated jump table lands at the right address; hoisting
  one to the top of its file fails the checksum. 15 of 149 units currently
  define a global after their first function, and those are the ones to leave
  alone.

So: move types and declarations up freely, move a global only when nothing
depends on where its bytes land, and never reorder function definitions.

## Table-slot names

Some callbacks are only distinguishable by the table slot that dispatches
them; the slot *is* the game's own identifier, so it goes in the name rather
than an invented visual description:

| Family | Pattern | Dispatched by |
|---|---|---|
| Gameplay effect tasks | `gpEff<Kind>Task<ID>` (`gpEffSprTask34`) | `Task_Spawn(6, ID, …)` via the bank-6 `TaskDesc` table `D_8010FC2C`; `Gp_SpawnEff(0x6xxxx, …)` passes the same ID. `Kind` is the primitive the body draws: `Spr` (animated textured billboard), `Line`, `Tile`, `Poly` (gouraud tris/quads), `Model`, `Attach`, or `Ctl` when the task only spawns and steps other effects. |
| Player actor states | `gpPlayer<Mode>State<N>` (`gpPlayerNormalState5`) | `GameActor.field_956` indexes `D_8009794C` in mode 0 (`Gp_TickPlayerNormal`) and `Gp_PlayerMode2States` in mode 2; `field_954` picks the mode through `Gp_PlayerModeFns`, which also has a mode 1 (`Gp_TickPlayerMode1`). `Gp_PlayerWorkStates` is a separate four-entry `Task::state` dispatcher, not a mode. |

A handler shared by several slots takes a behavioural name instead
(`Gp_EffModelTask` covers bank-6 0x36/0x66/0x67/0x68/0x91).

## Modules (current)

The prefixes below are written in the target style. The tree is mid-migration,
so much of the code still spells them `Fs_`, `CdCmd_` and so on; treat a name in
the old style as not yet converted rather than as a second convention.

| Prefix | Area | Source / splat unit | Header |
|---|---|---|---|
| `fs` | CD filesystem, STAGE*.CDF / STAGE0.HED | `src/main/fs.c` | `include/main/fs.h` |
| `cdCmd` | CD load command ring buffer | `src/main/cdcmd.c` | `include/main/fs.h` |
| `fade` / bootload | Boot-image load + fullscreen fade TILE | `src/main/bootload.c` | `include/main/gameflow.h` (Fade) / `fs.h` |
| `cdSync` | CD seek/sync/disk-recovery helpers | `src/main/cdsync.c` | `include/main/fs.h` (CdCmd_*) |
| `cdVol` | CD-DA volume table apply | `src/main/cdvol.c` | `include/main/fs.h` |
| `mc` / mcprompt | Memcard prompts + early Mc states | `src/main/mcprompt.c` | `include/main/mc.h` |
| `gfx` / gfxlight / gfxmtx | Flat lights + rotation matrices + image slots | `src/main/gfxlight.c`, `gfxmtx.c`, `boot.c` | `include/main/gfx.h` |
| `display` / displaymode | Display mode / auto-clear setup | `src/main/displaymode.c` | `include/main/display.h` |
| `stage` / stage | Stage fade / transition / MDEC during load | `src/main/stage.c` | `include/main/stage.h` |
| `task` / taskutil | Small task helper near stage tables | `src/main/taskutil.c` | `include/main/task.h` |
| loadui | Loading SPRT + CD load enqueue | `src/main/loadui.c` | — |
| `mdec` | MDEC/STR strip decode | `src/main/stream.c`, `stage.c` | `include/main/stream.h` |
| `midi` | Song block / MIDI sequencer | `src/main/sndevt.c` | `include/main/sound.h` |
| `sndVoice` / `SndBank` / `sndBankSlot` | SFX voice slots + bank table | `src/main/sndscript.c` | `include/main/sound.h` |
| `cdAudio` | CD-driven audio player | `src/main/cdaudio.c` | `include/main/cdaudio.h` |
| `gpu` | OT / graph reset helpers | `src/main/otutil.c`, `tmd.c`, `gamemain.c` | `include/main/display.h` |
| `boot` | Cold-boot / title path | `src/main/boot.c` | `include/main/boot.h` |
| `title` | Title / demo / main-menu overlay | `src/title/title.c` | `include/main/title.h` |
| `gp` | Resident in-game overlay (actors, view, TMD attach, …) | `src/gameplay/` | `include/gameplay/` (per-TU, e.g. `gameplay.h`, `1BC.h`) |
| `mem` / `GHeap` | Main / aux heaps | `src/main/mem.c` | `include/main/mem.h` |
| `sndHeap` | Dedicated 0x3D00 first-fit sound heap | `src/main/sndbank.c` | `include/main/sound.h` |
| `task` | Cooperative task list / spawn / kill | `src/main/task.c` | `include/main/task.h` |
| `pad` | Controller state / button polls | `src/main/pad.c`, `padutil.c` | `include/main/pad.h` |
| `mc` | Memory-card save/load helpers | `src/main/mc.c`, `mcmenu.c`, `mcprompt.c` | `include/main/mc.h` |
| `ui` | UI layout / draw / list chrome | `src/main/ui.c` | `include/main/ui.h` |
| `text` / `font` / `prim` | Text measure / glyph / SPRT helpers | `textdraw.c`, `textutil.c`, `font.c` | `include/main/text.h` |
| `spu` / `asyncCb` | SPU voices + async callback ring | `src/main/spu.c` | `include/main/sound.h` |
| `sndLoad` / `sndScript` / `sndEvt` | Bank load / scripts / event queue | `sndscript.c`, `sndevt.c` | `include/main/sound.h` |
| `linInterp` / `audioTick` | Volume ramp + frame tick list | `src/main/sndbank.c` | `include/main/sound.h` |
| `game` | Session pointer-slot table | globals / `task.c` | `include/main/session.h` |
| `display` | Dual DISPENV/DRAWENV + system flags | used from `gamemain.c` etc. | `include/main/display.h` |
| `gameMain` | Entry after `main` | `src/main/gamemain.c` | `include/main/gamemain.h` |
| `gpuExt` | GPU helpers | `src/main/gpuext.c` | `include/main/gpuext.h` |
| `gameFlow` / `fade` | Pre-pad/task game-flow handlers | `src/main/gameflow.c` | `include/main/gameflow.h` |
| `gameFlag` | Packed 4-bit flag nibble table | `src/main/gameflag.c` | `include/main/gameflag.h` |
| `cdStream` / `cdReady` | CD→SPU MTS stream | `src/main/cdstream.c` | `include/main/cdstream.h` |
| `tmd` | TMD model lists / stream | `src/main/tmd.c` | `include/main/tmd.h` |
| `stream` | Stream channel slots | `src/main/stream.c` | `include/main/stream.h` |
| `game` | Main session object | globals | `GameSession`, `gameSession` |
| `player` | Player character state: position, HP/MP, equipped items | `src/main/wipsyscfg.c` | `include/main/wipsys.h` |
| `wip` | Weak-evidence placeholders | `wipsyscfg.c`, etc. | rename when proven |
`Wip*` types and `Wip_*` globals are provisional: keep them only until a better
role name is proven, and prefer replacing one over inventing a second
provisional alias. The file and prefix are historical — the block that gave them
their name turned out to be the player state and now uses `Player_`, while the
remaining `Wip_` symbols are unrelated to it and to each other.

Main-executable types live in module headers under `include/main/` (not a kitchen-sink header). Stage/file overlays may use a different `src/` / `include/` layout when decompiled:

| Header | Types |
|---|---|
| `session.h` | `GameSession`, `GameActor*`, `GBytes*` |
| `stage.h` | `StageCtx` |
| `wipsys.h` | `PlayerStatus`, `PlayerPos`, `WipSysFlags` |
| `gfx.h` | `GfxImageSlot` |
| `sound.h` / `ui.h` / `text.h` / `display.h` / … | subsystem types |

Prefer including the specific module header when you only need that subsystem.

## Documentation

[`include/main/mem.h`](include/main/mem.h) is the worked example. Read it before
documenting a new module. Its symbols have not been migrated yet, so the
examples below are shown in the target naming rather than quoted verbatim.

Doc comments use `///` and sit immediately above what they describe. That is
already the house style — `///` outnumbers every alternative in the headers and
`/** */` appears nowhere — so the convention keeps it rather than introducing a
second form.

A comment says **what** something is and **why** it exists. Anything further goes
after a blank `///` line, so the summary can be read on its own:

```c
/// Allocates a block of memory.
///
/// Prior to allocating the data, it sets the active heap.
/// See `memSetActiveHeap` for more details.
///
/// @param size Number of bytes to allocate.
/// @return Allocated block or `NULL`.
```

Cross-references go in backticks so a reader can search for them, and so a
rename can find them.

### Struct fields

Fields are documented the same way, with `///` above the field. There is room
for a real sentence there, which is the point:

```c
typedef struct {
    /// Current health. Recomputing the maximum clamps this down to it.
    s16 hp;
    /// Maximum health: the level's base value plus bonuses from equipment,
    /// capped at 250.
    s16 hpMax;
    /// Equipped weapon, or 0 when nothing is equipped.
    u8  weapon;
} PlayerStatus;
```

**Do not annotate fields with their offsets.** The layout is already expressed
by the field types and fixed by `STATIC_ASSERT_SIZEOF`, nothing reads the
annotations, and a trailing `/* 0x1A */` crowds out the sentence that would
actually help. Unproven fields keep their `field_XX` name, which carries the
offset in the only place it is still needed.

### What not to write

How a meaning was originally worked out is scaffolding, not documentation. It
matters while a symbol is being identified and becomes noise once the module is
understood, so it belongs in the commit message that established it, not in the
header that outlives it.

A symbol whose role is unproven is left undocumented, or its comment states only
what was observed and says the role is unproven. An unmarked comment is read as
established fact, so a guess must never be promoted to a description. `mem.h`
shows the honest form — a bare `extern int D_80068F98;` carrying no comment at
all, and a `// TODO:` where the behaviour is suspected but unconfirmed.

### Where a comment lives

Documentation follows visibility. A public symbol is documented at its
declaration in the module header, once; a private one at its definition in the
`.c`. Neither is documented twice, so there is no second copy to fall out of
date.

`@param` and `@return` are for parameters whose meaning is not obvious from a
proven name. They are not required, and on a signature still carrying `arg0`
they add nothing — write the prose instead, or leave it until the roles are
known.

Coverage today is about 17% of declarations, so most modules are below this bar.
Bring a module up to it when working in it rather than as a separate sweep.

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

- Do **not** invent names for unanalyzed `func_800*` / `D_800*` just to “clean up.”
  Record the evidence for a field whose role is unproven and leave the name alone.
- Prefer **one rename commit per subsystem**, so the sources and the symbol maps
  move together and a regression is easy to attribute.
- Renaming in a symbol map is enough for a **matched** function: the unit
  re-splits and the generated `.s` files are renamed and pruned automatically.
  An **unmatched** one also needs its `INCLUDE_ASM` argument updated, because
  that name is written in the C file and nothing regenerates it.
- A name that resolves to a matched body is not the same as one that resolves to
  an address. Before trusting an address, check whether it is unique — images
  that load at a shared base give one address several meanings.

## Tooling

Finding references and renaming go through `tools/refactor/`, which resolves
symbols with libclang and the compilation database instead of matching text:

- `find_references.py <spec>` — every reference, each classified as read,
  write, read-write, address-of, call, declaration or definition. `--asm` adds
  assembly references and reports whether the symbol's address is unique or
  shared between images.
- `rename_item.py <spec> <newName>` — rewrites the declaration and every
  reference at the exact locations the parser reports. `--dry-run` shows the
  plan; `--sidecars` also rewrites the symbol maps and linker scripts, which
  belong to no translation unit.

A spec is a source path with the symbol appended:

```
<header>/<Type>::<member>      <source>/<function>::<param>
<header>/<Type>               <source>/<name>
<header>                       (renames the header and every include of it)
```

Both tools take `--version` (default `USA`).

Why a parser and not a search-and-replace: hundreds of unrelated types here
declare a member of the same placeholder name, and one function may declare the
same identifier in several nested blocks with different types. Only the resolved
declaration distinguishes them, so text substitution silently edits the wrong
struct.

**A symbol that is still `INCLUDE_ASM` cannot be renamed this way.** It has no C
declaration, so there is nothing for the parser to resolve. Rename it in the
symbol map, update the `INCLUDE_ASM` argument, and re-split.
