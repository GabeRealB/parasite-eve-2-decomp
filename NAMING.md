# Naming conventions

This decomp still has many address-based placeholders (`func_800xxxxx`, `D_800xxxxx`,
`field_XX`). Prefer descriptive names as soon as a symbol’s role is clear.

**A field's role is recorded at its declaration**, in the header that owns the
type. There is no separate catalogue: a second copy of a field's meaning only
goes stale, and a stale one is worse than none because it reads as established.

## Scheme

Functions and data are **lowerCamelCase, one identifier, no separators**, opening
with the subsystem or package that owns the symbol. Choose that owner from the
implementation, interface and consumers, not from the symbol's old prefix.
Externally linked names must remain unambiguous across their consumers; static
instances of included shared source may reuse their subsystem names.
Three markers carry the rest of the meaning:

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
| **Included shared implementation** | `subsystemVerbNoun` with the private marker for static instances | `_actorContactApplyGridPushback`, `_planarReflectionDraw` |
| **Global data** | `gModuleName` | `gFsFileTable`, `gPlayerStatus` |
| **Private function** | `_moduleVerbNoun` | `_fsReadSector` |
| **Private data** | `_gModuleName` | `_gSectorCache` |
| **Types** | PascalCase role name | `CdCmdQueue`, `TaskDesc`, `PlayerStatus` |
| **Known members** | camelCase role | `writeIdx`, `hp`, `hpMax` |
| **Unknown members** | `field_XX` / `unknown_XX` | keep until the role is proven |
| **Unnamed symbols** | `func_<package>_<VRAM>`, `D_<VRAM>` | generated; only until matched and understood |

Choose the owner before composing the name:

- **Resident main and gameplay code** — use the owning subsystem from the guide
  below, such as `fs`, `cap`, `inventory`, `actorRender` or `worldCollision`.
  Gameplay is a collection of subsystems, not a naming module. Do not turn
  `Gp_*` mechanically into `gp*`, replace it with a blanket `gameplay*`, or keep
  `Gp` on a type merely because of its history. Use the subsystem for globals
  and types as well: `gCap...`, `Cap...`, `Inventory...`, with `_` for TU-local
  items. A descriptive type such as `PlayerStatus` needs no extra owner repeated
  in front of it. Existing `gp...` spellings still require ownership review even
  when the index calls their casing `current`.
- **Package-specific overlay code** — use the manifest key, camelCased:
  `mine_mesa` → `mineMesa…`, `shelter_1f_bulwark` → `shelter1fBulwark…`.
  Do not abbreviate package keys; dropping a leading word can cause collisions.
  This rule applies to room, actor, weapon and other package-specific routines,
  not to every subsystem implemented in the gameplay overlay.
- **Actors** — the actor id, until the packages are identified:
  `actor00300UpdateTransform`. One body serves that actor's several RAM slots,
  so the id, not a load address, is its identity.
- **Included shared source** — use the shared subsystem's identity, such as
  `actorContact`, `capCaption`, `planarReflection`, `roomEffect`, `shop`,
  `telephone` or `water`. Static instances retain `_`; an externally linked overlay
  wrapper uses its package prefix. The wrapper and included implementation have
  different owners. Do not rename shared code after whichever carrier was read
  first, or add a generic `Shared` component solely because code is reused.
  Where another image refers to one package's compiled copy - gameplay's task
  table pointing at a room's effect task - that copy is an export of the
  package and takes its prefix in front of the shared identity:
  `shelterB2PodBottomEffectSpriteRiseTask`. The carrier binds the shared name
  to it before including the library's header (`#define EFFECT_SPRITE_RISE_TASK
  shelterB2PodBottomEffectSpriteRiseTask`), as the paired factory rooms do, so
  the definition and every reference carry one project-unique name and the
  reference's `owner=` (see `tools/check_symbols.py`) can be checked by name. A
  copy nothing outside the package refers to keeps the plain shared name.
  The packages one source is built into are the same case: gameplay's weapon
  table is indexed by weapon, each index is one package, and so each package
  exports its handler under a name of its own - `func_m4a1_p1_8011D1C4` beside
  `func_m4a1_8011D1C4`. The source defines the handler once, under one ordinary
  name; the other packages' names are aliases, declared on their slots in the
  overlay manifest (`aliases = { definition = "public name" }`) and emitted by
  the `PACKAGE_ALIASES` line that ends the source, as `DEFINE_ALIAS`
  statements. Both names are renamed like any symbol: `rename_item.py
  --sidecars` rewrites the manifest with the symbol maps.

A TU or a header may contain several subsystems, and one subsystem may span
several TUs. For example, an effect handler in `player_actor.c` still belongs to
the effect subsystem. A function exported to other overlays keeps its owner's
prefix; its callers do not become its owner. If the current guide lacks a
proven subsystem, establish its interface and responsibility, add a descriptive
entry, and check for name collisions. Leave unproven roles explicit instead of
inventing a namespace from an address, old prefix or temporary file grouping.

Generated placeholders keep their `func_<package>_<VRAM>` form. The vacuum parses
that shape to recognise a promoted alias, so only human-assigned names take the
convention.

### Macros

Project macros are naming-pass items too: constants, function-like helpers,
aliases/accessors and shared-source configuration bindings. Use
**UPPER_SNAKE_CASE**. When a prefix is useful, spell the owning subsystem or
package in full: `INVENTORY_`, `WORLD_COLLISION_`, `ROOM_VISUAL_EFFECTS_`,
`FILE_SYSTEM_`; do not reuse shortened C prefixes such as `GP_`, `INV_`, `FX_`
or `FS_`. Do not add a blanket gameplay prefix. Generic helpers, including
`ARRAY_SIZE`, `OFFSET_OF`, `PARENT_OF` and `ALIGN` in `common.h`, need no prefix.
**GTE macros are the exception:** preserve established PsyQ-style spelling
such as `gte_RotTransLV`, including project wrappers following that interface.
Macro parameters may use ordinary local camelCase.

Macros do not take the global `g` or TU-private `_` markers, and have no C
linkage. Keep definitions in the narrowest source/private/public header scope
covering their consumers, including shared-source carriers. Review all
conditional definitions and `#undef` sites together. Document purpose, units,
argument requirements, captured identifiers, repeated evaluation and other
non-obvious constraints beside the definition. For configuration bindings,
document the required value/type and the instances that supply them.

Check parenthesization, signedness, widths, side effects, control flow and
stringification/token pasting before simplifying a macro or replacing it with
an enum, typed constant or inline function. Preserve matching and semantics;
being a macro alone is not a reason to replace it. SDK macros, include guards
and compiler/assembly plumbing are outside ordinary semantic naming review;
project GTE wrappers remain eligible without needing uppercase renames.

### Types

A struct carries a tag only when C requires one — when it refers to itself, or
when something else refers to it as `struct _Tag`. Everywhere else the typedef
stands alone, so a tag that *is* present means the type is used in one of those
two ways.

```c
typedef struct ListNode {         /* self-referential: tag required */
    struct ListNode *next;
} ListNode;

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

Place each function, global and type according to its actual consumers:

- **One translation unit:** keep its declarations and type definitions in that
  `.c` file's prologue. It does not belong in any header.
- **Several translation units within one overlay:** use a private module header
  beside the source, such as `src/gameplay/attachments.h`.
- **Several overlays, or the resident executable and an overlay:** use the
  owning module's public header under `include/`, such as
  `include/gameplay/attachments.h`. Public headers must not include private
  source-directory headers.

Count definitions and actual uses, including callbacks, inline helpers and
types required by shared signatures. Merely including an old umbrella header
does not make a TU a consumer of everything it declares. Cross-image import
aliases count as shared even when just one TU uses the import spelling.
Consumers include the canonical header instead of copying its declarations.

Group shared declarations by subsystem. Related translation units may share
one header; a header does not need to mirror each source filename. Keep public
and private interfaces separate, and retain separate type headers where the
BSS declaration order described below requires them.

TU-local symbols are marked `_` and are `static` where the build still matches.
An overlay-private symbol shared between TUs still needs external C linkage.

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

Include only headers whose declarations, types or macros the file uses, plus
any prerequisites required by the SDK. A source file starts with the public
or private header declaring the interface it implements. Follow that with a
blank line, a Psy-Q include block, another blank line, and the overlay include
blocks. Group each overlay separately, with the current overlay first. Shared
decompilation helpers such as `common.h`, `types.h` and `gte.h`, when needed,
have their own block between Psy-Q and the overlays. Headers use the same
grouping without the initial implementation-header include. Keep SDK
prerequisites before the SDK headers that need them.

Do not retain `common.h` merely as a standard preamble. Include it for its
macros; use `types.h` when only the primitive typedefs are needed. Headers must
compile independently, including the dependencies used by their public macros.

A translation unit has two halves. The first holds the includes, type
definitions, forward declarations and whatever globals may safely live there.
The second holds the function definitions and nothing else.

The split is presentational for types and declarations, which the compiler may
see in any order. It is **not** free for definitions:

- **BSS follows first-declaration order in this compiler.** Where including an
  API header first changes BSS order, keep the existing BSS definitions in
  address order in the prologue, after the needed type declarations and before
  the affected API headers. This is an exception to the include layout above;
  choose a safe implementation header first when one is available. Type
  headers and API headers remain separate where this ordering requires it.
  Do not change object sizes, add padding, or change build alignment to repair
  a header-order mismatch.

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
| Gameplay effect tasks | `effect<Kind>Task<ID>` (`effectSpriteTask34`) | `taskSpawn(6, ID, …)` via the bank-6 `TaskDesc` table `D_8010FC2C`; `effectSpawn(0x6xxxx, …)` passes the same ID. `Kind` describes the primitive or role: `Sprite`, `Line`, `Tile`, `Poly`, `Model`, `Attach`, or `Control`. |
| Player actor states | `playerActor<Mode>State<N>` (`playerActorNormalState5`) | `GameActor.state` indexes `D_8009794C` in mode 0 (`Gp_TickPlayerNormal`) and `Gp_PlayerMode2States` in mode 2; `mode` picks the mode through `Gp_PlayerModeFns`, which also has a mode 1 (`Gp_TickPlayerMode1`). `Gp_PlayerWorkStates` is a separate four-entry `Task::state` dispatcher, not a mode. |

A handler shared by several slots takes a behavioural name instead
(`effectThrownModelTask` covers bank-6 0x36/0x66/0x67/0x68/0x91).

Names in the dispatch evidence above identify the current code, not target
prefixes. Apply the TU-private `_` marker to the target pattern where appropriate.

## Subsystem ownership guide

These are target prefixes and starting points for finding each subsystem's
interface. Source and header columns are navigation aids, not a requirement
that everything in one file take one prefix. Confirm ownership from the item's
role and consumers. Related TUs can share a subsystem; their names need not
repeat a source filename. The guide does not establish original library or TU
boundaries and does not require splitting or moving code.

Headers under `include/` are public; headers under `src/` are private to their
overlay or included implementation. A listed header does not authorize moving
a TU-local declaration into it. Keep type-only headers where declaration order
requires them, and include only the interface actually used.

### Resident executable

Platform memory helpers in `include/decomp/common.h` describe fixed PlayStation
hardware regions and use the full `PLAYSTATION_` macro prefix. They provide byte
addresses, not allocation or ownership. The scratch stack in
`include/main/scratch.h` manages temporary blocks within that region; its cursor
access, block reservation/release helpers and byte-offset constants use
`SCRATCH_STACK_`. `ScratchStackCursor` in the same header is the cursor slot
seen as the one member of a structure, for the few routines that only match
when the cursor is read and written as a member.

Source basenames in this table are relative to `src/main/`. Multiple prefixes
in a row identify different responsibilities in the same source group.

| Prefix | Responsibility | Source examples | Interface / type headers |
|---|---|---|---|
| `fs`, `cdCmd`, `cdSync`, `cdVol` | Filesystem, queued CD requests, drive recovery, CD volume | `fs.c`, `cdcmd.c`, `cdsync.c`, `cdvol.c` | `include/main/fs.h`, `include/main/fs_types.h`, `src/main/fs.h` |
| `boot`, `gameMain` | Initialization and main loop | `main.c`, `boot.c`, `gamemain.c` | `src/main/boot.h`, `src/main/gamemain.h`, `include/main/gamemain.h` |
| `gameFlow`, `fade` | Session flow and screen fades | `gameflow.c`, `bootload.c` | `include/main/gameflow.h`, `src/main/gameflow.h` |
| `playClock` | Resetting the resident partial-minute play-time accumulator after a save load; gameplay advances it | `gameflow.c` | `src/main/session.h` |
| `gameDebug` | Resident diagnostic controls, input overrides/replay and session-load status shared with gameplay | `stage.c` (resident storage), `gamemain.c` (reset), gameplay consumers | `include/main/game_debug_types.h` (state type); pointer currently declared in `include/main/pad.h` |
| `loadUi` | Loading and disk-swap presentation | `loadui.c` | `include/main/loadui.h` |
| `stage` | Stage transitions and music selection | `stage.c`, `stage_music.c` | `include/main/stage.h`, `include/main/stage_types.h`, `src/main/stage.h` |
| `display`, `gpu` | Frame presentation, display state and ordering tables | `gamemain.c`, `displaymode.c`, `otutil.c` | `include/main/display.h`, `include/main/display_types.h`, `src/main/display.h` |
| `gfx` | Graphics coordinates, matrices, lights and image slots | `gfxlight.c`, `gfxmtx.c`, `boot.c` | `include/main/gfx.h`, `include/main/gfx_types.h`, `include/main/coord.h`, `src/main/gfx.h` |
| `gpuExt` | GPU status helpers | `gpuext.c` | `src/main/gpuext.h` |
| `mem` | Main and auxiliary heaps and memory operations | `mem.c` | `include/main/mem.h`, `src/main/mem.h` |
| `task` | Task allocation, lists, dispatch and banks | `task.c`, `taskbank.c`, `taskutil.c` | `include/main/task.h`, `include/main/task_types.h`, `src/main/task.h` |
| `pad` | Controller state and polling | `pad.c`, `padutil.c` | `include/main/pad.h`, `include/main/pad_types.h`, `src/main/pad.h` |
| `mc`, `mcMenu` | Save data, memory-card operations and prompts | `mc.c`, `mcmenu.c` | `include/main/mc.h`, `include/main/mc_types.h`, `src/main/mc.h` |
| `ui` | Generic windows, panels and list controls | `ui.c`, `mcmenu.c` | `include/main/ui.h`, `include/main/ui_types.h`, `src/main/ui.h` |
| `text`, `font`, `prim` | Text layout, glyphs and basic drawing primitives | `textdraw.c`, `textutil.c`, `caption_draw.c` | `include/main/text.h`, `src/main/text.h` |
| `cdAudio` | CD audio playback | `cdaudio.c` | `include/main/cdaudio.h`, `include/main/cdaudio_types.h`, `src/main/cdaudio.h` |
| `cdStream`, `cdReady` | CD-to-SPU streaming and ready queue | `cdstream.c` | `src/main/cdstream.h` |
| `sndVolume` | Combined MIDI/script master-volume policies and their request gates | `sndscript.c`, `sndevt.c` | `src/main/sound.h` |
| `sndOutput` | Resident mono/stereo output selection shared by MIDI, sound scripts, CD input and streaming voices | `cdvol.c` | `include/main/sound.h`, `src/main/sound.h` |
| `midi`, `sndEvt` | Music sequencing and sound events | `sndevt.c` | `include/main/sound.h`, `include/main/sound_types.h`, `src/main/sound.h` |
| `sndLoad`, `sndScript`, `sndVoice`, `sndBank`, `sndBankSlot` | Sound loading, scripts, voices and banks | `sndscript.c`, `sndbank.c`, gameplay `scene_runtime.c` (PE file requests) | `include/main/sound.h`, `src/main/sound.h`, `src/main/sound_types.h`, `src/gameplay/scene_runtime.h` (PE file requests) |
| `sndHeap`, `linInterp`, `audioTick`, `spu`, `asyncCb` | Sound heap, ramps, audio ticks, SPU control and callbacks | `sndbank.c`, `spu.c` | `include/main/sound.h`, `src/main/sound.h` |
| `stream`, `mdec` | Stream slots and MDEC movie/image decoding; scene/audio selection, payload-sector intake, completion and stream halt requests | `stream.c`, `stage.c` (image decoding), gameplay `scene_runtime.c` (scene stream coordination) | `include/main/stream.h`, `include/main/stream_types.h`, `src/main/stream.h`, `include/gameplay/scene_runtime.h` (scene stream coordination) |
| `tmd` | TMD model streams and primitive dispatch | `tmd.c`, `hasm/` | `include/main/tmd.h`, `include/main/tmd_types.h`, `src/main/tmd.h` |
| `gameFlag` | Packed game flags | `gameflag.c`, gameplay `cap_commands.c` (optional nibble and whole-byte access) | `include/main/gameflag.h`, `include/main/gameflag_types.h`, `include/gameplay/gameflag.h` (gameplay exports) |
| `game`, `player` | Resident session and saved player state | `task.c`, `gameflow.c`, `wipsyscfg.c` | `include/main/session.h`, `include/main/session_types.h`, `include/main/wipsys.h`, `include/main/wipsys_types.h` |
| `random` | Shared 32-bit pseudo-random sequence for gameplay and loaded overlays; consumers advance the recurrence directly, independently of SDK `rand()` | `gamemain.c` (resident state), gameplay and overlay consumers | `include/main/random.h` |

`Wip*` and `Wip_*` are provisional spellings, not a subsystem to perpetuate.
Establish the owner of each remaining item. The historical `wipsyscfg.c` filename
does not make its unrelated contents one module. Likewise, a `Gp` type declared
in a main header is not automatically owned by gameplay.

### Gameplay subsystems

Source basenames here are relative to `src/gameplay/`. Use these subsystem
identities instead of an overlay-wide `gp` prefix. Where a row offers multiple
prefixes, choose the responsibility the symbol actually implements.

| Prefix | Responsibility | Source examples | Interface / type headers |
|---|---|---|---|
| `actorRender` | Actor model drawing, coordinate updates and position/orientation records | `actor_render.c` | `include/gameplay/actor_render.h`, `include/gameplay/message.h` (`ActorTransform`), `src/gameplay/actor_render.h` |
| `modelObject`, `animation` | Model attachments, child-model task lifecycle and animation state | `model_objects.c`, `player_state.c` (child-model task states), `scene_runtime.c` | `include/gameplay/model_objects.h`, `include/gameplay/animation.h`, `include/gameplay/animation_types.h`, `src/gameplay/model_objects.h` |
| `modelLighting` | Lit model transforms and primitive emission | `model_lighting.c` | `include/gameplay/model_lighting.h`, `src/gameplay/model_lighting.h` |
| `worldCoord` | Room/world transforms and light queries | `world_coords.c` | `include/gameplay/world_coords.h`, `src/gameplay/world_coords.h` |
| `gfx` | Transform composition, Euler extraction and direction-facing rotations exported by gameplay | `scene_runtime.c`, `hud_sprites.c`, `object_lists.c` | `include/gameplay/scene_runtime.h`, `include/gameplay/hud_sprites.h` |
| `fade`, `display` | Full-screen colour pulses and fades; display-transition fades; redraw of the previous framebuffer for temporal blending | `scene_runtime.c`, `item_menu.c` (display-transition fade) | `include/gameplay/display.h` (fade control and display-transition task), `include/gameplay/scene_runtime.h` (task dispatch); individual task states are TU-local |
| `worldCollision` | Collision grids, object lists, contact dispatch and contact offsets | `world_collision.c`, `collision_grid.c`, `object_lists.c`, `scene_runtime.c` (contact offsets) | `include/gameplay/world_collision.h`, `include/gameplay/world_collision_types.h`, `include/gameplay/collision.h`, `include/gameplay/scene_runtime.h` (contact offsets), `src/gameplay/world_collision.h` |
| `worldTarget` | Target tracking, lock-on, reticle and floating damage/heal readouts | `world_targets.c` | `include/gameplay/world_targets.h`, `include/gameplay/world_targets_types.h`, `src/gameplay/world_targets.h` |
| `objectTask` | Object task/message control | `object_task.c` | `include/gameplay/object_task.h`, `src/gameplay/object_task.h` |
| `taskMessage` | Synchronous id-selected task messages and integer/address argument transport | `companion_load.c` (`taskMessageDispatch`) | `include/gameplay/message.h` (`TaskMessageArg`, `TaskMessageHandler`, `TaskMessageEntry`) |
| `linkedActor` | Linked actor helpers | `linked_actors.c` | `src/gameplay/linked_actors.h` |
| `area`, `direction` | Area layouts, entry, flags, transitions, facing and warps | `area_entry.c`, `area_transitions.c`, `direction_input.c`, `direction_facing.c` | `include/gameplay/area.h`, `include/gameplay/areaplace.h`, `include/gameplay/area_entry.h`, `include/gameplay/area_transitions.h`, `include/gameplay/direction.h`, `src/gameplay/area_transitions.h` |
| `loading`, `view` | Gameplay loading, view images and camera records/application | `area_cd.c`, `cd_loading.c`, `load_screen.c`, `view_load.c`, `view_image.c`, `hud_sprites.c` (camera application) | `include/gameplay/loading.h`, `include/gameplay/view.h`, `src/gameplay/loading.h` |
| `sprite` | Sprite allocation and linking | `sprite_link.c` | `include/gameplay/sprites.h`, `include/gameplay/loading.h` |
| `companion` | Companion selection, actor setup, decision timing and health bands | `companion_load.c`, `player_state.c` (behavior helpers) | `include/gameplay/companion_load.h`, `include/gameplay/player_state.h` (behavior helpers), `src/gameplay/companion_load.h` |
| `inventory`, `item` | Inventory contents, quantities, sorting, item properties and use | `item_inventory.c`, `item_sort.c`, `item_collect.c`, `item_boost.c`, `item_use.c`, `starter_inventory.c` | `include/gameplay/items.h`, `include/gameplay/inventory.h`, `include/gameplay/battle_reward.h` (battle reward lists), `src/gameplay/items.h`, `src/gameplay/item_use.h` |
| `equipment`, `attachment` | Equipment selection, attachment combinations, modifiers and Parasite Energy target contacts | `equipment.c`, `attachments.c`, `attach_combo.c`, `attachment_stats.c`, `attachment_menu.c`, `object_lists.c` (target contacts) | `include/gameplay/items.h`, `include/gameplay/attachments.h`, `include/gameplay/attachment_state.h`, `src/gameplay/attachments.h` |
| `itemMenu`, `menu` | Inventory panels, commands, menu flow and the map screen | `item_menu.c`, `item_panels.c`, `item_stats.c`, `menu_root.c`, `menu_actions.c`, `menu_armor.c`, `menu_prompt.c` | `include/gameplay/item_menu.h`, `include/gameplay/map.h` (map-screen records the `mapui` packages define), `src/gameplay/item_menu.h`, `src/gameplay/menu.h` |
| `itemPickup`, `itemPlacement` | Pickup dispatch and placed items | `pickup_dispatch.c`, `item_placement.c` | `include/gameplay/item_placement.h`, `src/gameplay/item_placement.h` |
| `weapon` | Resident weapon/ammunition properties and stat presentation | `weapon_stats.c` | `src/gameplay/weapon_data.h` |
| `damage` | Damage calculation, attack and hazard properties, enemy hit reactions and combat modifiers | `damage.c`, `object_fields.c` | `include/gameplay/damage.h`, `src/gameplay/damage.h` |
| `enemy` | Enemy work objects and the shared parameters of one enemy kind | `scene_runtime.c` (`_enemyAllocateWork`) | `include/gameplay/enemy.h`, `include/gameplay/enemy_params.h` |
| `cap` | CAP relocation, dialogue, commands, rendering and playback | `cap_commands.c`, `cap_control.c`, `cap_reloc.c`, `cap_script.c`, `cap_start.c`, `captions.c` | `include/gameplay/cap.h`, `include/gameplay/captions.h`, `src/gameplay/cap.h`, `src/gameplay/captions.h` |
| `evs` | Event-script dispatch | `evs_scripts.c` | `include/gameplay/evs.h`, `include/gameplay/evs_scripts.h`, `src/gameplay/evs_scripts.h` |
| `effect` | Gameplay effect tasks and scratch workspace records shared by effect drawers across overlays | `effect_tasks.c`, `effect_attach.c`, `player_actor.c` | `include/gameplay/effect_tasks.h`, `include/gameplay/effects.h` (workspace types), `src/gameplay/effect_tasks.h` |
| `gpu` | RGB555 palette interpolation, raw texture and palette uploads to VRAM, and GPU packet blend-mode commands linked into the current depth-sorted ordering table | `scene_runtime.c` (palette interpolation), `world_targets.c` (image uploads), `room_effects.c` | `include/gameplay/scene_runtime.h` (palette interpolation), `include/gameplay/gpu_image_upload.h`, `include/gameplay/actor_render.h` (actor texture placement), `include/gameplay/room_effects.h` |
| `roomEffect` | Room effect state and tasks | `room_effects.c` | `include/gameplay/room_effects.h`, `src/gameplay/room_effects.h` |
| `hud` | HUD sprites, numbers and tracking | `hud_sprites.c` | `include/gameplay/hud_sprites.h`, `src/gameplay/hud_sprites.h` |
| `playClock` | The task that runs play: play-time accounting, player and companion death checks, and the per-frame HUD update it calls | `model_lighting.c` (`_PlayClockWork`, start and per-frame states), `ending.c` (state table), `hud_sprites.c` (dispatch, death fade) | `src/gameplay/model_lighting.h`, `src/gameplay/ending.h`, `src/gameplay/hud_sprites.h` |
| `actionPrompt` | Point-and-click action cursors shared by room and actor overlays | `menu_actions.c` (resident per-port slots) | `include/gameplay/action_prompt.h`, `src/shared/action_prompt.h` |
| `padInput`, `padScript` | Gameplay input mapping, and scripted on/off and variable-intensity controller vibration | `pad_input.c`, `pad_scripts.c` | `include/gameplay/pad_input.h`, `include/gameplay/pad_script.h`, `src/gameplay/pad_input.h`, `src/gameplay/pad_script.h` |
| `playerActor`, `playerState` | Player actor dispatch, movement and action states | `player_actor.c`, `player_state.c` | `include/gameplay/player_actor.h`, `include/gameplay/player_state.h`, `include/gameplay/actor_spawn_types.h` (player/companion spawn transforms), `src/gameplay/actor.h` (player/companion spawn options), `src/gameplay/player_actor.h`, `src/gameplay/player_state.h` |
| `scene` | Scene tasks, actor-command routing, combat state and runtime coordination | `scene_runtime.c`, `world_targets.c` (combat state) | `include/gameplay/scene_runtime.h`, `include/gameplay/scene_combat.h`, `include/gameplay/world_state.h` (combat type), `include/gameplay/message.h` (`ActorCommand`), `src/gameplay/scene_runtime.h` |
| `ending` | Ending sequence control | `ending.c` | `include/gameplay/ending.h`, `src/gameplay/ending.h` |

This is not a blanket assignment of every symbol in those files. For example,
inventory state, effects and player state are different owners even when their
implementations occur in one TU. Check each item's references before selecting
its prefix. Uncertain state blocks in otherwise understood files remain subject
to analysis.

`include/gameplay/actor_presentation.h` groups the current player and companion's
presentation messages and animation-bank index writers under `playerActor` and
`companion`. `include/gameplay/sound.h` declares `sndEvt` stage-relative script
requests and the `sndScript` stage-id resolver. These interfaces are gameplay
exports even though their saved state and sound queue are resident.

### Packages and included shared implementations

Package-specific routines keep their manifest-derived prefixes. The title
interface is `include/title/title.h`, implemented by `src/title/title.c`.
Resident `playerActor` and `weapon` APIs are distinct from actor/weapon packages,
whose entry points retain package identities.

`golemPawnRook` owns the included Pawn/Rook GOLEM behaviour and its Beam Sword,
grenade launcher, grenade and shield child tasks. Its implementation interface
is `src/shared/golem_pawn_rook.h`; carriers select the GOLEM kind and weapon.
Handlers reached only by each carrier's own dispatch tables keep static linkage
and the `_` marker. The resident enemy, animation and collision APIs retain
their own subsystem identities.

`actorRender` also owns the inline yaw rebuild and joint-rotation composition
helpers in `include/actors/actor.h`. Each actor translation unit keeps its own static
instance, with the `_` marker. The yaw rebuild replaces pitch, roll and scale
with the coordinate's current yaw at a signed 12-fractional-bit uniform scale;
translation stays intact and composition is marked dirty. Composing up to an
excluded view node produces a world-space rotation without changing the
coordinate hierarchy.
The included world-yaw joint update in
`src/shared/actor_contacts_turn_joint.inc.c` belongs to the same subsystem;
`src/shared/actor_contacts.h` declares its static per-carrier interface beside
the contact routines it is carried with.

The common walker frame state in `src/shared/walker_frame.inc.c` also belongs
to `actorRender`: it composes the model root, samples lighting, calls the
carrier's motion/animation update and draws its selected ground shadow.
`ACTOR_RENDER_WALKER_FRAME` selects each static `EnemyTaskFunc` instance;
carriers declare it in their prologues and bind it around each inclusion.
Further instances in one carrier retain the subsystem prefix and `_` marker.
The bound walk update keeps the identity of its own movement subsystem.

`actorMovement` owns the shared coordinate steps used by actor packages:
translation along a normalized local axis, subject to the live actor-freeze
state, with distances in parent-coordinate units. Its inline interface is
`include/actors/actor.h`; each translation unit keeps a static instance with
the `_` marker. Collision correction and animation scheduling belong to their
own subsystems.

`animDriver` owns the included animation-request driver used by the four
actor families carrying `src/shared/anim_driver_tick.inc.c`. Its private
implementation interface is `src/shared/anim_driver.h`. Each translation
unit keeps a static instance: requests reset slots 1 to 5, and playing calls
advance their poses and count ticks and slot-1 control-jump ticks. The resident
`animation` subsystem supplies slot playback; it does not own these actors'
request state or counters.

`actorAngle` owns scalar heading and turn-angle normalization, and the turn from
an actor's heading toward the live player, shared by actor packages. Its inline
interface is `include/actors/actor.h`, with static instances marked `_`. Angles
use 4096 units per turn; signed wrapping retains both half-turn endpoints and
narrows the input to 16 bits before wrapping. The player turn also writes the
translation offset in signed 16-bit game coordinates and requires both roots
in the same parent coordinate frame; it does not compose or rotate them.
The cached-frame bearing helper also belongs to `actorAngle`: it takes a target
offset through the transpose of the reference's composed basis, then measures
its X/Z yaw. Both caches must already describe the same composition frame;
the helper borrows caller-owned scratch storage and does not refresh them.

Shared implementation interfaces live beside their source in `src/shared/`,
including `actor_contacts.h`, `cap_captions.h`, `planar_reflection.h`,
`room_visual_effects.h`, `screen_wave.h`, `shop.h`, `telephone.h`,
`water_effects.h`, `glow_draw.h` and `jukebox.h`. Use their subsystem prefixes with static
per-instance linkage as described above. `water_effects.h` uses the prefix
`water`; its configuration macros use `WATER_`. It is the included splash,
drift, distortion and refraction code. Gameplay `roomEffect` remains the
resident room-effect state. `room_visual_effects.h` uses the prefix
`roomVisualEffects`, with `_` for static per-instance functions; its constants
use `ROOM_VISUAL_EFFECTS_`. The
remaining `RoomFx_…` functions and `ROOM_FX_` configuration bindings are legacy
spellings. It owns the included halo, flash, trail and flying-effect drawing,
separate from the resident `roomEffect` state API. `glow_draw.h` uses the
prefix `glow`; its
configuration macros use `GLOW_`. It is the included projected glow, flare and
light-beam drawing. `screen_wave.h` uses the prefix `screenWave`; its
ramp-phase and texture-modulation constants use `SCREEN_WAVE_`. It redraws the
captured frame as a grid of textured quads whose corners sine waves displace.
`ScreenWaveCtx`, the ramp context packages embed and pass as the task's spawn
argument, is declared in `include/overlay.h`. Scratch records that several overlays share, such as the
one-centre projection block, are declared in `include/rooms/room_common.h`. If a shared implementation and a gameplay
subsystem have similar names, distinguish actual ownership and linkage before
introducing a qualifier; do not assume that they are one API.

`actorMsg` owns the included standard actor message handlers: model placement,
visibility, draw modes, presence queries and player-hold release. Its private
implementation interface is `src/shared/actor_messages.h`; configuration
bindings and constants use `ACTOR_MESSAGE_`. A carrier can select a private
instance of a placement fragment separately from its ordinary shared entry.

`modelPlacement` owns the included TMD coordinate placement helpers and child
model attachment states. Its private implementation interface is
`src/shared/model_placement.h`; configuration bindings use `MODEL_PLACEMENT_`.
An attachment instance links a model root to a parent model part, borrows the
parent's lighting matrices and joins the parent's task teardown tree. A carrier
can bind a second private instance independently of the ordinary shared entry.

`actor_contacts.h` uses the prefix `actorContact`. The block of its push
along obstacle bearings is `ActorContactBearingPushScratch`, private to that
interface; the markers its bearing slots take use
`ACTOR_CONTACT_BEARING_PUSH_`. The block of the walk that finds the push
out of the last obstacle record is `_ActorContactFindPushScratch`, defined in
the one fragment that uses it; its end marker is
`ACTOR_CONTACT_FIND_PUSH_MARK_END`. `ActorContactPushScratch` in
`include/actors/actor.h` is a different block, that of the push resolved from
the contact records' own correction.

`incinerator_blaze.h` uses the prefix `blaze`. It is the fade-to-white and
body-fire tasks included by the incinerator room and `actor_342100`.
`BlazeParentWork` is the prefix of the spawning task's work those tasks know:
bytes they never read, then the heat-haze `ScreenWaveCtx` the fade finishes.
Each package extends that prefix with its own tasks and scene state.

`screenFade` owns the included subtractive full-screen fade tasks, with
`src/shared/screen_fade.h` as their private implementation interface. The
resident `fade` API draws their overlay; each included task owns its ramp
state and lifetime. Configuration bindings and task constants use
`SCREEN_FADE_`. A carrier with a second fade-in instance binds
`SCREEN_FADE_IN_TASK` to that instance's identifier; TU-local instances keep
the subsystem prefix and the `_` marker.

`actionPrompt` owns the point-and-click action cursor shared by room and actor
overlays. The resident per-port state and its public types are gameplay
(`include/gameplay/action_prompt.h`, slots in `menu_actions.c`). The hotspot
table entry is `ActionPromptHotspot` in that header. The included
motion, drawing, hotspot test and outline are `src/shared/action_prompt.h` and
its fragments; a package includes only the fragments it carries.

`planarReflection` owns player and held-object reflections. Gameplay dispatches
the player task to the captured room through
`include/gameplay/planar_reflection.h`; room-specific wrappers export the
included implementation from `src/shared/planar_reflection.h` under their
package prefixes.

`roomEvent` owns the included room event gates, latched event records and event
tasks. Its implementation interface is `src/shared/room_events.h`; record types
used by several room overlays are declared in `include/rooms/room_common.h`.

`roomVariant` owns the included progress-dependent destination-room selection.
Its implementation interface is `src/shared/room_variants.h`. Resolvers borrow
a `RoomEventMsg` request and update the initialized reply's room selector;
the same record may serve both roles. `ROOM_VARIANT_` configuration bindings
select each carrier's function identifier. A map overlay exports its instance
under its package prefix for other rooms; room-local copies keep the shared
subsystem identity and static linkage. `ROOM_VARIANT_RESOLVE_SHELTER` selects the
Mine/Shelter definition's identifier, defaulting to `_roomVariantResolveShelter`.
Each carrier declares that instance in its prologue to establish its linkage;
`map_shelter` instead binds the public `mapShelterRoomVariantResolve` instance,
declared in its public header.

`ROOM_VARIANT_RESOLVE_NEO_ARK` likewise selects the Neo Ark resolver's function
identifier. The two Shelter departure carriers declare `_roomVariantResolveNeoArk`
static in their prologues; `map_neo_ark` binds its public
`mapNeoArkResolveRoomVariant`, declared in its public header. Each binding
surrounds the corresponding fragment include and is undefined afterwards.

`effectSprite` owns the included animated sprite and debris tasks and their
textured quad drawers. Its interface is `src/shared/effect_sprite.h`; its
configuration macros use `EFFECT_SPRITE_` and select declarations matching each
carrier's drawer signatures.

`spriteQuad` owns the included camera-facing textured quads with carrier-selected
textures. Its interface is `src/shared/sprite_quad.h`; its configuration macros
use `SPRITE_QUAD_`. The frame-type binding selects each static instance's 16-bit
texture-frame argument, including its signedness.

`pyroFlame` owns the included eight-frame additive flame billboard shared by
Pyrokinesis and Combustion. Its interface is `src/shared/pyro_flame.h`; each
carrier keeps a static drawer. `PYRO_FLAME_` constants describe its animation
strip, perspective sizing and screen-space rotation.

`beamStrip` owns the included additive textured parallelogram between a cached
world translation and a world-space endpoint, used by Hammer's shock trails and
the M.I.S.T. shooting-gallery tracer. Its interface is `src/shared/beam_strip.h`;
each carrier's drawer is static. `BEAM_STRIP_` constants describe the four-cell
texture layout and perspective sizing.

`modelMorph` owns the included deformation of a TMD model by a ramp: scaled
vertex deltas added to a snapshot of the rest shape, and normals interpolated
toward a target set. Its interface is `src/shared/model_morph.h`. The record
describing one morph is `ModelMorph` in `include/overlay.h`, public because a
package can morph its model with a record another package defines.

The included Dryfield factory room code shares the lift, hatch, operator panel,
entry task and scenes between the day and night packages. Its interface is
`src/shared/factory_lift.h`; configuration macros use `FACTORY_ROOM_` and select
the compiled instance's function bindings independently of runtime stage
selection.

The paired Dryfield factory, G & R kitchen, motel room 6 and trailer coach
implementations use `src/shared/dryfield_time.h` for their `DRYFIELD_`
configuration constants. Carriers bind `DRYFIELD_TIME` before their shared
header to select a daytime or nighttime compiled instance; these discriminator
values are separate from runtime stage numbers.

`glutton` owns the included boss helpers shared by `actor_403200` and
`actor_444000`. Its implementation interface is `src/shared/glutton.h`;
`GLUTTON_` instance bindings select each carrier's local boss state.

`bossStranger` owns the included Boss Stranger movement core shared by
`actor_110600` and `acropolis_bridge`. Its implementation interface is
`src/shared/boss_stranger.h`. The movement record is `BossStrangerWalker` in
`include/overlay.h`, with `BossStrangerNode`, `BossStrangerNav` and
`BossStrangerRoute` beside it. State values use `BOSS_STRANGER_WALKER_`.
The blocks of the two nearest-node scans are
`BossStrangerNodeNearestSelfScratch`, measured from the walker, and
`BossStrangerNodeNearestPlayerScratch`, measured from a player. The block of
the re-plan along the node order is `BossStrangerPlanTowardScratch`, whose
list capacity and sentinels use `BOSS_STRANGER_PLAN_`. The frame the tick
opens around the whole step is `BossStrangerTickScratch`, which holds the
position the walker turns towards. All of these are private to that
interface. The arrival test has no block type of its own: the offset it
stages is a plain `SVECTOR`.

`madChaser` owns the included Mad Chaser enemy shared by `actor_04400`,
`actor_341700` and `actor_342400`. Its implementation interface is
`src/shared/mad_chaser.h`, one fragment per function. `MadChaserWork` is the
task's work block; its animation request, hit reaction and room command values
use `MAD_CHASER_ANIM_REQUEST_`, `MAD_CHASER_HIT_REACTION_` and
`MAD_CHASER_COMMAND_`. Per-carrier handlers and helpers with no external users
keep static linkage and the `_madChaser` prefix. Task dispatch indices use
`MAD_CHASER_TASK_`; behavior indices use the table's identity, such as
`MAD_CHASER_LURK_STATE_` or `MAD_CHASER_COMBAT_STATE_`.
`MadChaserLimbShadowScratch` is the scratch block of one
limb shadow quad: both parts' world transforms and positions, the four corners,
their projection, and the half span the quad overhangs each end by, which this
drawer keeps in the block. The scripted waves that spawn the enemy are the
separate `src/shared/mad_chaser_waves.h`.
The place a wave's enemy enters the room is an `OverlayEncounterSpot`, declared in
`include/overlay.h` beside `OverlayEncounterSlot`: the two Shelter B3 rooms define
the tables, and both the Mad Chaser and the Sucklerceph read them.

`limbShadow` owns the included limb shadow drawer, `src/shared/limb_shadows.h`:
a subtractive quad under the segment between two model parts. Its scratch block
is `ActorLimbShadowScratch` in `include/actors/actor.h`, public because
the shared floor drawer and `actor_400600`'s wall drawer use the same projection
record. Each carrier includes its own static `_limbShadowDrawSegment` instance.

`actorMotion` owns the included animation-request handlers and scripted-walk
steps in `src/shared/actor_motion.h`. Playback serves actors whose work opens
with a nineteen- or twenty-part rig and model state; walk steps additionally
need destination, velocity and motion state. Each carrier keeps static function
instances marked `_`. The twenty-part handler restarts repeated clips; the
nineteen-part handler skips them unless a carrier selects its own arrival-play
handler through `ACTOR_MOTION_PLAY19_HANDLER`. Constants use `ACTOR_MOTION_`.
The resident `animation` API owns slot playback rather than this request and
walk sequencing.

`reverseWalk` owns the included nineteen-part scripted walker that can back
toward its target, shared by `actor_350500` and `actor_350700`. Its
implementation interface is `src/shared/reversing_walker.h`, which declares the
task's work block `ReverseWalkWork`. `KyleMadiganWalkerWork` in
`include/actors/actor.h` is the work block of the twenty-part Kyle Madigan
walker that `actor_135600` and `actor_350700` each carry as their own
functions, with the tasks of his hands and of what he holds.
The spawn, frame update, facing, movement-start,
final-turn, lighting, idle, walk-phase dispatch, exit and walk/draw message
handlers keep static per-carrier instances marked
`_reverseWalk`. Package-local direction-command handlers retain their package
prefix.
The frame-update fragment's root-velocity integration is the locally scoped
`REVERSE_WALK_INTEGRATE_VELOCITY` macro; it operates on the embedded walk state.

`desertChaser` owns the included Desert Chaser enemy, one source built three
ways: the cutscene build (`actor_323000`, `actor_323400`), the regular build
(`actor_00100`) and the Water Tower build (`actor_421600`). Its implementation
interface is `src/shared/desert_chaser.h`, one fragment per function; a carrier
binds `DESERT_CHASER_BUILD` before including it. `DesertChaserWork` is the
task's work block, whose head the three builds share and whose tail each build
declares for itself. Animation request values use
`DESERT_CHASER_ANIM_REQUEST_`, and the armed builds' collision spheres are
indexed by `DESERT_CHASER_SPHERE_`. `DesertChaserAvoidScratch` is the scratch
block of the armed builds' avoid walk, holding `DESERT_CHASER_AVOID_BEARINGS`
bearings in each build, `DesertChaserDamageScratch` the block of their
damage step and `DesertChaserTurnStepScratch` the block of their two turn-step
states. `DesertChaserRoamScratch` and `DesertChaserPursueScratch` are the
blocks of their roam and their pursuit. `DesertChaserFrameScratch` is the block
of the per-frame driver in the cutscene and Water Tower builds; the regular
build's driver reserves a bare `SVECTOR`. `DesertChaserTaskStates` is a
package's table of enemy task handlers, indexed by `Task::state`: three in the
cutscene and Water Tower builds, four in the regular build.
`DesertChaserStateTable` holds an armed package's
`DESERT_CHASER_STATE_COUNT` state handlers, and `DesertChaserVariant` is one of
the four tunings both armed packages define and the spawn argument picks from.

`oddStranger` owns the included Odd Stranger enemy, one source built twice:
`actor_401000` and `actor_401800`. Its implementation interface is
`src/shared/odd_stranger.h`, one fragment per function; a carrier binds
`ODD_STRANGER_VARIANT` before including it. `OddStrangerWork` is the task's
work block, state values use `ODD_STRANGER_STATE_` and animation request
values `ODD_STRANGER_ANIM_REQUEST_`. `OddStrangerStateTable` holds a package's
`ODD_STRANGER_STATE_COUNT` state handlers, and `OddStrangerTransformStorage`
is the static allocation of the placement a grab sends the player.
The three tunings a package defines, picked by the spawn argument, are
`ActorStrangerVariant` records in `include/actors/actor.h`: public because
`actor_01900` seeds its own work block from the same record.

`stalkerZebraIvory` owns the included pose, animation-request and
pending-action code shared by the Zebra Stalker (`actor_400600`) and the Ivory
Stalker (`actor_405800`). Its implementation interface is
`src/shared/stalker_zebra_ivory.h`, one fragment per function. Each package
names its own work type `StalkerZebraIvoryWork` before including the fragments
and spells the members they reach alike. Animation request and pending action
values use `STALKER_ZEBRA_IVORY_ANIM_REQUEST_` and `STALKER_ZEBRA_IVORY_PENDING_`.

`diver` owns the included strike child, impact sparks, joint turn and
animation-request code shared by the Bog Diver (`actor_00400`) and the Sea
Diver (`actor_206100`). Its implementation interface is `src/shared/diver.h`,
one fragment per function. Each package names its own work type `DiverWork`
before including the fragments and spells the members they reach alike; the
header lists them. Each carrier keeps static instances marked `_diver`.
Animation request values use `DIVER_ANIM_REQUEST_`, and strike-effect recipes
use `DIVER_BURST_`.

`sucklerceph` owns the included Sucklerceph enemy shared by `actor_04600` and
`actor_07000`. Its implementation interface is `src/shared/sucklerceph.h`, one
fragment per function. `SucklercephWork` is the task's work block; its
behaviour, awake stage, death phase and animation values use
`SUCKLERCEPH_STATE_`, `SUCKLERCEPH_AWAKE_STAGE_`, `SUCKLERCEPH_DEATH_PHASE_` and
`SUCKLERCEPH_ANIM_`. `SucklercephContactsScratch` is the scratch block of its
contact pass, private to that interface. The block of its drop's collision
step is `ActorContactDeltaWideScratch` in `include/actors/actor.h`, public
because `actor_521100` and `actor_403600` reserve the same block.

`viewFigure` owns the included figure parented to the view coordinate, shared
by `actor_110300` and `actor_110800`. Its implementation interface is
`src/shared/view_figure.h`, which declares the task's work block
`ViewFigureWork`. Other figures can reuse its helper-model exit fragment
independently of that work block and the twenty-part animation rig.

Scratch blocks that packages reserve in functions of their own stay in
`include/actors/actor.h`. `ActorContactDeltaScratch`,
`ActorContactDeltaWideScratch` and `ActorContactOverlapPushScratch` open with
the same 0x20 untouched bytes and keep the collision grid's correction after
them; the last is the block `ActorOverlapPushScratch` serves without that
lead. `ActorPlayerKnockbackScratch` is the block of the knockback
`actor_105100` and `actor_205200` step the player through,
`ActorPlayerHoldScratch` that of the button-press hold `actor_03700` and
`actor_510900` put the player in, `ActorEulerTurnScratch` that of the Euler
turn `actor_02100`, `actor_03800` and `actor_510900` multiply onto a rotation,
and `ActorHitTakenScratch` that of the hit check `actor_01200` and
`actor_04000` turn toward a hit with.

`moth` owns the included Moth enemy shared by `actor_00700` (packages
`actor_100700` and `actor_200700`) and `actor_300700`. Its implementation
interface is `src/shared/moth.h`, one fragment per function. `MothWork` is the
task's work block: the animation rig, the hit, grid and attack spheres with
their contact tables, and the wander, wing-beat and death state the handlers
share. Each package defines the tables the fragments read (`gMothParams`,
`gMothAttack`, `gMothSpeeds`, `gMothAnimSets`, `gMothBurstUvs`).

`scriptedWalk` owns the included walk of the twenty-part NPCs that cutscene
scripts move around, over a work block each walker publishes in a global. Its
implementation interface is `src/shared/scripted_walk.h`, one fragment per
function, carried by `actor_143900` (two walkers), `actor_146300`,
`actor_260400`, `actor_420700` and `actor_461800`. `ScriptedWalkAttachmentsWork`
is the whole block of a walker that carries two attachment tasks, the first
walker of `actor_461800` and the second of `actor_143900`; the other walkers'
blocks are their packages' own. The two fragments that take the block from the
task, `_scriptedWalkUpdate` and `_scriptedWalkTo`, declare it as
`SCRIPTED_WALK_WORK_T`, which each carrier binds to the type its walker
allocates. `SCRIPTED_WALK_WORK` selects that walker's borrowed work pointer
for the animation and placement fragments. `SCRIPTED_WALK_MODE` selects its
writable signed-halfword approach mode; `SCRIPTED_WALK_MODE_*` names the
forward, backward and short forward distances. `SCRIPTED_WALK_TICK_ANIM`
selects the private slot-tick instance and its update calls.
`SCRIPTED_WALK_RESET_ANIM` selects the private track-restart instance and its
update calls, with the same published work binding at both sites. A carrier with
two walkers selects its update definition with `SCRIPTED_WALK_UPDATE`, which
defaults to the private `_scriptedWalkUpdate`; its header and fragment declare
each instance `static`. `actor_143900` binds its additional private instance to
`_scriptedWalkUpdateSecond`. The same instance's work, mode,
duration and animation-helper bindings accompany the update binding.
An additional walker also selects its child-track blend definition and update call with
`SCRIPTED_WALK_BLEND_ANIM`, defaulting to the private `_scriptedWalkBlendAnim`.
`SCRIPTED_WALK_BLEND_FRAMES` selects its writable signed-halfword duration
latch, in whole normal-rate frames, defaulting to `_gScriptedWalkBlendFrames`.
Each carrier keeps that storage private; a play request narrows the duration
to its low signed halfword. A carrier rebinds both around each additional
fragment instance and restores its first walker's bindings afterwards.
`actor_420700` carries only the tick and reset fragments; its private
fixed-duration blend function is declared directly in the carrier's prologue.
`SCRIPTED_WALK_PLACE` selects the placement callback, with the same instance's
published work pointer bound around its fragment. Its default is the private
`_scriptedWalkPlace`; the header and fragment declare each instance `static`.
The carrier declares an additional static instance in its prologue before its
message table; actor_143900's second copy is `_scriptedWalkPlaceSecond`.
`SCRIPTED_WALK_TO` selects the approach-message callback, with the receiver's
allocated work type and signed-halfword mode bound around its fragment.
It defaults to `_scriptedWalkTo`; the header and fragment declare each instance
`static`, reached through that carrier's own message table.
The carrier declares an additional private instance in its prologue before
its message table; actor_143900's second copy is `_scriptedWalkToSecond`.

`pacedWalk` owns the included twenty-part cutscene NPC walk whose work block
is kept at `Task::work`, and the animation-slot tick, reset, blend and placement
fragments other walkers reuse. Its implementation interface is
`src/shared/paced_walk.h`. `PACED_WALK_WORK_T` selects the walker's allocated
work type; `PACED_WALK_TICK_ANIM` binds the tick definition and its update
callers to the same function instance when a package carries several walkers.
`PACED_WALK_RESET_ANIM` likewise binds a walker's private clip-restart definition
and update callers, defaulting to `_pacedWalkResetAnim`. Its header and fragment
declare each instance `static`; further instances are declared `static` in the
carrier's prologue before their callers.
Private instances keep the `pacedWalk` prefix and the `_` marker.
`PACED_WALK_BLEND_ANIM` selects a blended clip-reseed definition and its update
callers, defaulting to `_pacedWalkBlendAnim`. Its header and fragment declare
each instance `static`; a carrier declares additional instances `static` in
its prologue and binds `PACED_WALK_WORK_T` to their allocated type around the
definitions. That type also provides `blendFrames` in whole normal-rate frames.
`PACED_WALK_UPDATE` selects a private update definition and its shared-fragment
callers, defaulting to `_pacedWalkUpdate`. The header and fragment declare each
instance `static`; additional instances keep the subsystem prefix (for example
`_pacedWalkUpdateSoldierC`) and are declared `static` in the carrier's prologue
before their callers. The update always uses `PacedWalkWork`; the work-type binding
only selects the reusable animation and placement helpers' work type.
`PACED_WALK_PLACE` selects the placement message callback, defaulting to
`_pacedWalkPlace`. The header and fragment declare every instance `static`;
additional copies are declared `static` in the carrier's prologue before their
tables. Each retains the `pacedWalk` identity and is bound to its allocated type
through `PACED_WALK_WORK_T` around the placement fragment. Restore both bindings
after a further instance.
`PACED_WALK_SET_WALK_TARGET` selects the heading-and-travel message callback,
defaulting to `_pacedWalkSetWalkTarget`. The header and fragment declare every
instance `static`, reached through its carrier's own message table. Additional
copies are declared `static` in the carrier's prologue before their tables;
actor_460200 selects `_pacedWalkSetSoldierCWalkTarget` for its third walker.
Each copy requires
`PacedWalkWork` independently of `PACED_WALK_WORK_T`; rebind around its fragment
and restore the first binding afterwards.
`PACED_WALK_SET_PAIR_MODEL_DRAW` selects the paired model-draw message callback.
Every selected instance is declared `static` in its carrier's prologue before
its message table and defined `static` by the shared fragment. The binding
defaults to `_pacedWalkSetPairModelDraw` in actor_160600 and actor_460200;
actor_460200 selects `_pacedWalkSetSoldierCModelDraw` for its third walker,
then restores the default. A nonzero `Task::spawnArg1.value` selects a live
`pairTask` from `PacedWalkWork`, independently of the work-type binding;
otherwise both model pointers refer to the receiver without dereferencing work.

`strideWalk` owns the included walk of the soldier NPC that can carry a second
model and turns its head toward the player during talk scenes, carried by
`actor_161500` and by the second walker of `actor_460200`. Its implementation
interface is `src/shared/stride_walk.h`, one fragment per function.
`StrideWalkWork` is the task's work block, kept at `Task::work`; its head-turn
modes use `STRIDE_WALK_TURN_`. The walker borrows the paced walk's slot tick,
reset, blended reseed and placement, so a carrier binds `PACED_WALK_WORK_T` to
`StrideWalkWork` around those fragments.
Its animation update, frame state, message handlers and carried-model callback
have static per-carrier instances marked `_strideWalk`.

`pairWalk` owns the included walk of the nineteen-part NPC that carries a
second model on one of its parts, carried by `actor_150400`, `actor_450800`,
`actor_451100` and `actor_535700`. Its implementation interface is
`src/shared/pair_walk.h`, one fragment per function. `PairWalkWork` is the
task's work block, kept at `Task::work`. Its animation update, slot helpers,
message handlers and carried-model callback have static per-carrier instances,
marked `_pairWalk`; their declarations describe the same shared implementation.

`footstepWalk` owns the included walk of the nineteen-part NPC that publishes
its work block in a global and can sound its steps, carried by `actor_151000`,
`actor_461800` and `actor_535700`; `actor_260500` and `actor_451100` carry the
quiet update, which plays none. Its implementation interface is
`src/shared/footstep_walk.h`. `FootstepWalkQuietWork` is the quiet walker's
whole block and the head `FootstepWalkWork` opens with; a package declares
`gFootstepWalkWork` with the type its walker allocates.

The player detection tests enemies include - line of sight, reach and the
segment-versus-wall query - have `src/shared/player_detection.h` as their
interface. `PlayerDetectionSightScratch` is the scratch block of the
line-of-sight test, private to that interface.

`jukebox` owns the included SELECT menu that lists music tracks and plays the
chosen sequence. Its interface is `src/shared/jukebox.h` (`jukeboxDrawRow`,
`jukeboxHostTask`). Each row is a `JukeboxTrack`: a MIDI sequence id and the
label drawn for that row. The saloon and shooting-gallery overlays each define
their own tables, so the record stays in `include/rooms/rooms_shared_8018055c.h`
beside `JukeboxTrackLists`, the ten-list table whose stack copy the row callback
also uses for its text request.

`pykeFlame` owns the included nozzle sprite and flying flame of the M4A1 Pyke
attachment, also carried by `actor_800100`. Its interface is
`src/shared/pyke_flame.h`. Each package names the effect-table task itself and
includes the shared body; `PYKE_FLAME_` bindings select the collision key, the
paused-flame redraw and the splash depth bias. `PykeFlameBody` is the flying
flame's collision block: the list-1 sphere and the one contact it borrows.
The splash, the ground glow and the fireball's floor quad share
`EffectGroundQuadScratch` in `include/gameplay/effects.h`: the flat quad's
corners and their projected screen positions, with ordering depth and the GTE
FLAG word left on the call stack.

`bladeTrail` owns the included swoosh the Gunblade, M4A1 bayonet and tonfa
baton leave behind a swing. Its interface is `src/shared/blade_trail.h`.
Each weapon keeps eight frames of the blade base and tip and includes
`_bladeTrailDraw`, which joins seven adjacent pairs into fading gouraud quads.
`BladeTrailScratch` is one quad's scratch block: four world corners, the
ordering depth and the GTE FLAG word, plus one word the drawer never touches.

`muzzleFlash` owns the included textured core, Gouraud streaks and short-lived
flash task shared by the P229 and MP5A5 variants. Its interface is
`src/shared/muzzle_flash.h`; constants use `MUZZLE_FLASH_`. Each carrier exports
its own effect-table wrapper and keeps the shared functions private. Carriers
supply the muzzle offset and four retained streak angles.

`risingSpark` owns the included short-lived additive spark billboard task
shared by Healing and Life Drain. Its implementation interface is
`src/shared/rising_spark.h`; task constants use `RISING_SPARK_`. Each carrier
exports its own effect-table wrapper and keeps the shared inline body private.

`leaf` owns the included falling-leaf task and textured-square drawers shared
by the Acropolis gardens and Neo Ark woodland rooms. Its implementation
interface is `src/shared/falling_leaves.h`; `LEAF_` constants describe its
rendering and task phases. Each carrier exports its own effect-table wrapper
and keeps the shared task and selected drawer private.

`mainStreet` owns the included room handlers, play-time task and drifting puff
effect shared by the day and night Dryfield main street packages. Its interface
is `src/shared/main_street.h`. Each carrier exports its own puff task to the
gameplay effect table and keeps the puff drawer private; rendering constants
use `MAIN_STREET_PUFF_`.

`factory` owns the included Dryfield factory room implementation, with
`src/shared/factory_lift.h` as its private interface. Its view- and
progress-gated light-glow task is exported by each room carrier under the full
package prefix for gameplay's effect table. `FACTORY_DRAW_GLOWS_TASK` binds the
shared definition to that export; rendering constants use `FACTORY_GLOW_`.

`redBeacon` owns the included one-frame pulsing red diamond drawer shared by the
Acropolis elevator halls. Its private implementation interface is
`src/shared/red_beacon.h`; `RED_BEACON_` constants describe pulse and projection
units. Each carrier binds `RED_BEACON_TASK` to its public package-prefixed
task callback, which gameplay's counted room-effect table imports.

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
/// See `_memSetActiveHeap` for more details.
///
/// @param size Number of bytes to allocate.
/// @return Allocated block or `NULL`.
```

Cross-references go in backticks so a reader can search for them, and so a
rename can find them.

### Struct fields

[`src/main/gpuext.c`](src/main/gpuext.c) is the example. The type carries
a `///` block; the fields carry **aligned trailing `//`**, which keeps the
declaration readable as a table:

```c
/// The player character: position, health, energy and equipment.
typedef struct {
    s16 hp;       // Current health (clamped to hpMax)
    s16 hpMax;    // Maximum health (level base + training + armour, capped at 250)
    u8  weapon;   // Equipped weapon (itemId - 0x7F, 0=none)
} PlayerStatus;
```

Where a field takes a small set of values, enumerate them in parentheses rather
than describing them in prose — `(0=Off, 1=On)`, `(0=4bit, 1=8bit, 2=15bit)`.
Keep a comment to one line where the struct mixes field widths: clang-format
aligns a run of trailing comments, and a two-line comment ends the run, so the
fields after it align to a different column.

**Bitfields are for layouts whose bits have proven meanings**, as in
`_GpuStatusRegister` and `AnimationPackedRotation` — a hardware register and a
packed 11-10-11 Euler rotation, where every component is named. A byte that is merely a bitmask of unidentified flags
stays a plain integer with a comment; unnamed bitfields add structure without
adding information, and rewriting a multi-bit test such as `& 0x84` as two
boolean reads changes the generated code. When bits do acquire names, the form
  to consider is a union of the whole value with a named bitfield struct, where
  the toolchain's layout and accesses match. Anonymous members are not supported
  by this compiler. Preserve whole-value operations and prefer named mask
  constants when bitfield accesses change the generated code.

Doxygen's trailing form `///<` is used by a handful of headers and would make
field comments machine-extractable. It was considered and deferred: plain `//`
is what the overwhelming majority of the tree already uses, and switching later
is a mechanical change. Do not reopen it per-file — either the whole tree moves
or none of it does.

**The same goes for a function's parameters**, and they are named in both
places: the prototype in the header and the definition in the `.c`. A signature
reading `(s32 index, s32 value, s32 arg2)` tells a caller nothing, and a
declaration that names its parameters while the definition still says `index` is
two descriptions of one function. `rename_item.py` takes
`<file>/<function>::<param>` and matches the parameter by position, so it
rewrites every declaration and the definition together. That rewrite covers
comment mentions too, which is what a real symbol name wants and a placeholder
does not: `argN` is the spelling examples use everywhere, so renaming a
parameter still carrying it edits unrelated functions' notes and the prose in
this tree. Pass `--no-comments` for those and the rename stays in the code that
declares it.

**A field you can describe is a field you can name.** Writing a comment that
states a field's role and leaving it called `field_14` is self-contradictory:
the offset name exists to say *the role is unknown*, and the comment has just
said otherwise. Name it from the same reading, and keep `field_XX` only where
the comment is absent or says the role is unproven.

**Fields carry no offset annotations.** The layout is already expressed by the
field types and fixed by `STATIC_ASSERT_SIZEOF`, nothing parses the annotations,
and a trailing `/* 0x1A */` sits in exactly the column the documentation needs.
An unproven field keeps its `field_XX` name, which carries the offset in the one
place it is still wanted.

### What a matching build proves

Two things may be relied on: that the project matches, and whatever has already
been verified and handled. Everything else in the tree is a hypothesis that
happened to compile.

A matching checksum proves the compiler emitted the same instructions. It
constrains a field's declared type only where the code actually accesses it,
because that is where width and signedness appear in the instruction stream. It
proves nothing about:

- **A field nothing reads.** Its type, its signedness, even whether it is one
  field or several, are unconstrained — any layout-compatible declaration
  matches. A run of unknown bytes may be one word, two halves or a bitmask.
- **Type identity.** Two separately named structs may be one type, invented
  twice. Layout-compatible scratch records can disagree about whether their
  sizing words are corner offsets or radii, and about the order of those radii.
  Only their writers and consumers establish the meaning; matching cannot.
- **The size of a type only ever reached through a pointer.** If the code just
  reads fields at offsets from a `T*`, nothing fixes where `T` ends. The
  declaration may be a window onto a larger object, or a sub-struct embedded in
  something defined elsewhere, and the decomp will have invented a boundary that
  is not there. A size is pinned only where the match depends on it — a
  `sizeof`, an array stride, pointer arithmetic, or the type embedded by value
  somewhere. A `STATIC_ASSERT_SIZEOF` is the decomp asserting its own guess, not
  evidence from the ROM. Most types are in this position: about 1460 of 2320
  never appear except as `T*`, `Task` and `GameActor` among them.
- **Any name.** A name is an earlier reader's hypothesis and carries no more
  authority than a comment.

Two things are worth reaching for when trying to establish a real size, because
both put the number into the instruction stream rather than into a declaration:

- **The type being copied by value.** A struct assignment emits a copy of
  exactly its size, so `*dest = *src` pins it.
- **A memory operation sized to the type** — an allocation, a fill, a copy in
  any form — where the constant matches the declared size.

Neither is proof, and how much weight to give one depends on how distinctive the
size is. A size of 8, 16 or 32 coincides constantly: dozens of types declare
each, and over fifty memory operations pass 8 as a literal. A distinctive size
is worth much more. `0x4CC` is the useful kind: four separately named `…Work`
types declare it, several actor overlays allocate with `memCalloc(0x4CC, 0)`,
and one assigns the result to an `_Actor113000Work*`. That corroborates the size
and, at the same time, suggests the four types are one type named four times.

So a struct is a *layout known to be compatible*, not a type known to be right.
When working on one, use the access sites to establish which fields are actually
pinned — `find_references.py` reports them with read/write classification — and
say which fields are settled and which are merely plausible. Where two types look
alike, whether they are one type is an open question worth asking, not an
observation to note and move past.

### Existing comments are not evidence

Assume the documentation already in the tree is wrong. It may have been written
from a guess, from an understanding since corrected, or about a neighbouring
symbol. A comment found on a symbol is a lead for where to look, never a source
to cite.

When renaming a symbol, derive what it is from the code that reads and writes it
and write the comment from that reading alone. Do not carry the old wording
forward. Where the code does not settle the question, say the role is unproven
rather than repeating what the previous comment claimed.

This is easy to violate without noticing, because an existing comment is usually
the first thing found and it frames the question — a field annotated "current
HP" invites confirming that rather than establishing it independently. The
failure is quiet: a plausible but wrong claim gets copied into a better-formatted
comment and now looks freshly verified.

### Struct overlays are an artifact

A type that describes bytes belonging to another type, at an offset into it, is
something decompilation produces and real code does not. Thirty-five types in
the tree are still documented as an overlay of something else.

What such a type usually means is that a **run of the owning struct is one
thing**, which the owning struct should say. Express it there, as a nested type,
and pick the form from how the bytes are used:

- Only ever used as a group — a **named nested struct member**.
- Used as a group *and* read field by field — still a **named nested struct
  member**, as long as the fields live on that nested type. `GameSession.location.loc`
  is this case: `&session->location.loc` is the 6-byte key passed into lookups, and
  `session->location.loc.stage` is the same bytes field by field.
- A byte view alone does not require a union. Cast the aggregate's address to
  a byte pointer for APIs that read its representation, and copy the complete
  aggregate when the transfer includes bytes beyond its interpreted fields.
- A fixed set of bits rather than fields — see the bitfield rule above, which is
  the same idea one level down.

```c
typedef struct {
    s8 field_0;
    GameLoc location;  /* location.loc.view / location.loc.stage */
    ...
```

**Every member has to be named.** This compiler does not support anonymous
struct or union members: it accepts the declaration and then rejects every
access to it. `_GpuStatusRegister` in `src/main/gpuext.c` is the worked
example: its bitfields sit in a struct member named `bits` beside the whole
`word`, so a field is `status.bits.displayDisabled`. So a union form is
`session->location.loc`, not `session->loc`; a nested struct form is `session->loc`.

**There is rarely only one.** The same run is usually described by several
invented types, reached from different callers, and the first one found is
often not the most used. Enumerate them before merging any — list the types the
owner is cast to, and the types `&owner->member` is cast to:

```
grep -rhoE '\(\s*[A-Za-z_][A-Za-z0-9_]*\s*\*\s*\)\s*&?\s*gOwner(->member)?' src include \
  | sort | uniq -c | sort -rn
```

Types describing the same run are one type: the member takes a single honest
type and the rest go. Which name survives is decided on the evidence, usually
the count. A duplicate left in place keeps its own `field_0..field_N` and its
own half of the documentation, which is the state this convention exists to
remove.

**A union needs evidence of multiple interpretations of the same storage.**
Folding a view into its owner can change address calculations or register
allocation without implying another interpretation. Read the accesses before
choosing an aggregate or union. Where both views are justified, use named members:

```c
/* A stream descriptor key is compared as a word or matched by group and id. */
union {
    s32 word;
    struct { u16 group; u16 id; } parts;
} key;
```

Do not introduce a union just to hide a cast or satisfy an audit. Casts may also
express necessary byte views, hardware address encodings, alignment checks or
SDK sentinels. Establish their purpose individually. When a justified type
correction still needs rematching, retain the matching implementation and record
the intended type, compiled attempts and remaining obstruction in the local
review. Matching validates the emitted code and layout, not the semantic model.

### As general as the subject allows

A comment states what something is and why it exists. Keep it at that level, and
reach for a specific only where the specific is what makes the thing
understandable.

Naming the mechanism is the usual way this goes wrong. "Frames left before the
body is released" says what the field is; "counted down by
`taskCountdownCallback`" adds the name of the function that happens to do it,
which the reader can find and which changes if the code is reorganised. The same
comment is better without it.

Specifics earn their place when the meaning is otherwise unavailable. A small
set of values has to be enumerated, because nothing else tells you what a 2
means — `Body kind (0 none, 1 TMD model, 2 2D display)`. A cross-reference earns
its place when the other symbol *is* the explanation rather than merely a user
of this one.

The test: if the sentence would still be true and still be useful after a
neighbouring function is renamed or moved, it is at the right altitude.

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
proven name. They are not required, and on a signature still carrying `index`
they add nothing — write the prose instead, or leave it until the roles are
known.

Coverage today is about 17% of declarations, so most modules are below this bar.
Bring a module up to it when working in it rather than as a separate sweep.

### Working order

Naming an item well means knowing what it is made of, so the work is ordered: a
function cannot be described honestly before the functions it calls, the globals
it touches and the types it passes around have been, because until then its
summary can only restate the assembly.

`tools/refactor/dep_graph.py` keeps that order. `ready <spec>` says whether
everything an item uses has been processed and names what is missing; `next`
gives the next leaf, optionally within one item's dependency closure; `stats`
reports progress. The graph is cached under the gitignored local directory and
should be rebuilt after landing a batch.

A cycle is collapsed into a single unit of work rather than treated as a
deadlock — the items in it have to be understood together. The real ones here
are small: a task and its list node, a TMD object and its list head, a sound
voice and its owner, and several actor structs paired with their work structs.

One kind of cycle is broken instead of collapsed: a dispatch table and its
callbacks. The table uses the functions it points at, the spawner uses the
table, and a callback that spawns reaches the spawner, so through gameplay's
task descriptor table 1373 functions, most of them room code, were a single
step. Where a table's wait for a function closes a cycle, that wait is dropped
(`break_table_cycles`): the table is described without waiting for every
entry, and each callback still has everything it calls in front of it. A
table in no cycle keeps waiting for its handlers.

Pending types declared in the same file are joined into one step too, up to
eight at a time (`dep_graph.py worklist --batch N`, or `name_pass.sh --batch
N`), and pending functions up to sixteen (`--batch-funcs N`), where the unit is
a source file or all the fragments of one shared library and a step may hold a
function with the callers that were waiting for it. Two steps that declare items in one file never run in the same round, so
without this a header's types are worked one per round whatever the number of
workers. Unlike a cycle, such a step is several independent reviews: each type
is judged on its own evidence and gets its own entry in the report. A type
joins a step only where everything it still waits on is already placed, so the
order stays a dependency order. Inline structs and unions with no tag are not
listed: there is nothing to name, and their fields are reviewed with the type
that holds them.

**Merge a duplicate as soon as the item in hand settles it.** A shared layout is
never the reason; the test is whether the two types mean the same thing, judged
from how the code uses each. Processing an item is what produces that evidence
for the types it touches, so where the answer is now clear the merge happens
now — the member takes the single honest type and the duplicate goes, with the
surviving name chosen on the evidence rather than on which was found first.

What is still deferred is only a pair with **no relationship to the item being
processed**. There the uses have not been read yet and the identity question
genuinely cannot be answered, so recording it and moving on is right.

### Resolve immediate values during the review

Name meaningful literals while the item's context is understood: established
states, flags, message IDs, sentinels and fixed-point scales. Reuse existing
constants and keep new ones in the narrowest scope covering their actual users.
Ordinary arithmetic constants need no names merely to remove literals.

Replace literal allocation, copy and clear sizes with `sizeof(T)` or
`sizeof(*object)` where the type and full extent are established. Use
`ARRAY_SIZE` for a proven whole-array bound, never on a pointer. Partial copies,
serialized lengths and capacities that differ from an active element count may
need their own constants. Preserve integer widths, promotions and layout: naming
a byte's states does not require storing an enum in place of the byte.

`memCalloc(sizeof(T), 0)` only keeps matching while `T` has the required size.
That checks the size relationship, but does not by itself prove type identity.

`tools/refactor/name_index.py --types` lists the candidates — the types whose
size a memory operation already corroborates.

### Review function bodies and contracts

Review the complete body, including local names and decompilation scaffolding.
Separate unrelated uses of a temporary and simplify expressions where matching
permits, preserving side effects, evaluation order, overflow and truncation.
Record scoped local renames when the rename tool cannot express them; check
shadowing before editing. Keep retained behavior even when it looks like a bug.

Use sparse, coarse `//` comments at meaningful boundaries inside function bodies.
Explain non-obvious phases, invariants and necessary ordering: state transitions,
coordinate conversions, packed formats, ownership and multi-stage calculations.
Do not narrate individual statements, repeat names, or comment every loop or
branch. Short clear functions may need none. Mark uncertain interpretations and
keep investigation history in the review report. Declaration comments explain
the contract; body comments explain significant steps and constraints.

Establish relevant units, fixed-point scales, valid index ranges, sentinels,
ownership and lifetime. Document what callers need. Verify bounds through aliases,
indirect accesses and full copy/clear widths across overlays. Existing array sizes
do not prove index domains, and observed accesses establish a minimum extent,
not the object's end. Do not invent padding or change BSS alignment to fit a type.

Use the pointer/callback audits and previous compiled attempts supplied by the
naming brief. Run the pointer checks on affected TUs and shared-source carriers;
the supplied findings are not an exhaustive scan. Use `PARENT_OF` for an
established containing-object relationship. Correct callback declarations,
definitions, tables and callers together. Distinguish necessary address encodings
from conversions caused by incorrect types, and track unresolved cases through
renames using the rename ledger.

Propagate conclusions through every consumer and configuration of shared source.
Preserve public, overlay-private and TU-local ownership, including static
per-instance linkage in included shared implementations. Remove declarations and
includes made redundant by the cleanup, retaining the BSS ordering exceptions.
Equal load addresses or layouts alone do not prove shared symbol/type identity.

### Naming-pass acceptance and follow-ups

Finish the cleanup justified by the item and its consumers. Record unrelated
discoveries separately. `venv/bin/python3 tools/refactor/verify_name_pass.py`
verifies image checksums, cross-image declarations and symbol ownership; with
every function matched in C, the checksum and the symbol-map checks prove each
function's bytes and name. `--objdiff` adds every individual function in a
fresh objdiff report, an audit the naming pass runs once over what it landed;
it restores the normal matching configuration. Preserve established prototype
exceptions without introducing new ones or implicit declarations.

**Never add a matching hack to keep a cleanup.** A pinned register
(`register T x asm("reg")`), an `asm` statement, or a steering macro from
`include/decomp/common.h` (`TOUCH_REG`, `USE_REG`, `SOFT_BARRIER`, ...) must not
be introduced by a naming step, and the verification fails a step that adds one
(`tools/check_hack_sites.py`). If a change you would like - an explicit `return`,
a separate local, a literal in place of a variable, a deleted macro - stops the
function matching, the code was written the other way: put that part back
exactly as it matched, say so in a comment if the shape is surprising (a handler
declared to return a value that has no `return` statement falls off its end in
the binary too), and record the cleanup as a `rematching` follow-up. The same
goes for a fake `do { } while (0)`, a dead store or a constant local added only
to steer the compiler.

Each step writes the structured JSON review requested in its brief: current name,
meaning, evidence, changes and unresolved issues for every assigned item. A
`complete` review has no remaining required work; `followup` records a specific
rematching, runtime, semantic or unrelated issue with its source location,
reason and next step. Preserve existing audit IDs. Fully reviewed items needing
no source change can finish with an evidence-backed report.

The driver keeps reports in `local/name-pass/reviews/` and records `ok` or
`followup` in its ledger. Both finish the naming visit, but follow-up work stays
explicit and is supplied to later related reviews. A passing build or a renamed
symbol alone is insufficient. Case-specific backlogs belong under `local/`, not
in general documentation.

## FS file-id encoding

When loading via `Fs_LoadFile` / `cdCmdEnqueue` (cmd `0x21`):

```text
fileId = entry->fileGroup * 10000 + entry->args.file.fileIdHundreds * 100 + entry->fileIndex
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
| `0x55` | Start reading `STAGE0.HED` (`fsStartStage0HeaderRead`) |
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

Finding references and renaming go through `tools/refactor/`. C declarations
resolve through libclang and the compilation database; macros use the separate
preprocessor inventory described below:

- `find_references.py <spec> [<spec>…]` — every reference, each classified as
  read, write, read-write, address-of, call, declaration or definition. `--asm`
  adds assembly references and reports whether the symbol's address is unique
  or shared between images.

  A query covers what the symbol contains as well: a type brings its fields, a
  function brings its parameters. They share one scan, because the units that
  can reference a member are very nearly the units that can reference its
  owner, so the members cost little beyond the owner and a great deal less than
  asking about each of them in turn. Their sites go to a file under
  `local/refs/`, summarised on standard output so a long listing does not crowd
  out the answer; `--shallow` asks about the named symbol alone. Several specs
  given at once share one scan the same way.

  Mentions in prose are counted for names that mean something and skipped for
  generated ones, where the identifier is too common for a mention to be about
  this symbol rather than another declaration spelling it the same way.
- `rename_item.py <spec> <newName>` — rewrites the declaration and every
  reference at the exact locations the parser reports, along with the mentions
  of the name in comments and in the markdown at the repository root and under
  `doc/`. `--dry-run` shows the plan; `--sidecars` also rewrites the version's
  `configs/` tree — symbol maps, splat configs, the overlay manifest — which
  belongs to no translation unit. It is refused for a field or parameter
  (skipped for those lines of a `--batch`), whose name is only a word outside
  C. Generated linker scripts are left alone on
  purpose: the next split rebuilds them, and they are not tracked, so writing
  to them only survives a revert of the sources.
  In markdown, a field or parameter is rewritten only where the prose names
  its owner (`Type::field`), since a bare backticked `inner` may be another
  struct's. `--batch FILE` applies one `<spec> <newName>` per line in order, in
  one process: each line is resolved against the tree the earlier ones left,
  so it may name what they renamed, and the reference index only refreshes
  what each rename touched. It stops at the first failure unless
  `--keep-going`.

A spec is a source path with the symbol appended:

```
<header>/<Type>::<member>      <source>/<function>::<param>
<header>/<Type>               <source>/<name>
<header>                       (renames the header and every include of it)
```

Both tools take `--version` (default `USA`).

**Macro items use `<definition-file>/<MACRO>` as their identity**, including in
the worklist, review reports and rename ledger. Unrelated macros with the same
spelling remain separate items; conditional definitions in one file are one
item. Build-defined keys in `configs/USA/overlays.toml` are configuration items.
Rebuild the dependency graph and worklist to include them:

```sh
venv/bin/python3 tools/refactor/dep_graph.py --build
venv/bin/python3 tools/refactor/dep_graph.py worklist
venv/bin/python3 tools/refactor/find_references.py include/decomp/common.h/PARENT_OF
venv/bin/python3 tools/refactor/rename_item.py <file>/<MACRO> <NEW_MACRO> --dry-run
```

`macro_refs.py` scans project definitions, replacement tokens, conditional tests,
undefinitions and potential uses in every branch, plus manifest `defines` keys.
It excludes comments and string contents. Include reachability limits candidates
but does not resolve include order, conditional expansion, SDK collisions or
tokens manufactured with `#`/`##`. The reference report preserves all definitions
and ambiguity details under `local/refs/`; these are lexical candidates, not
parser-proven uses. C declarations depend on the macros they spell; bindings
with ambiguous shared-source uses are grouped for coordinated review.

After inspecting references and the dry run, the reviewing agent may use
`rename_item.py ... --macro-reviewed` to apply an unambiguous lexical rename.
The flag records the caller's scope/branch/token-construction review; it does
not request user approval. Ambiguous bindings and manifest configuration keys
require coordinated explicit edits. Macro renames do not run the C prose or
sidecar pass: check comments, inline assembly, build configuration, generated
tokens and all variants separately. For manual changes, record each identity
with `rename_item.record_rename(root, "local/renames.tsv", "macro",
"<file>/<OLD>", "<file>/<NEW>", "<file>", edits)` and explain them in the report.
The report keeps its assigned qualified `name`; `current_name` is the final
identifier alone. Replacing a macro with another kind of declaration also needs
an explicit report of that change.
An item the review concludes should not exist - a duplicate merged into
another type, a scaffold replaced by real declarations - is reported with
`"removed": true` instead of a `current_name`, with `changes` saying what
replaced it; the validator then requires the name to be gone from `src/` and
`include/` (for a qualified macro, from its own file).

**The path in a spec is where the symbol is declared**, not where it is used. A
path that does not declare it cannot resolve, and the tools say so only after
parsing every translation unit in turn, which reads as a hang rather than as
the error it is.

**What the tools do not reach**, each of which is a hand edit:

- handwritten assembly under a source tree — its labels, and any sibling symbol
  it branches to. `--asm` scans the generated tree only. Generated assembly is
  the opposite case: never edit it, rename in the symbol map and re-split;
- notes in a symbol map that name a *different* symbol than the one renamed;
- inline assembly in C, which is a relocation rather than a parsed reference;
- C-symbol references reached through a macro, which are listed rather than edited,
  since the macro body is where the name is spelled;
- a reference a macro invocation carries *as an argument*, which the parser
  reports at the invocation rather than at the argument. A symbol used that way
  everywhere cannot be renamed by the tool at all: the run refuses the whole
  file with `expected '<old>', found '<other>'` rather than corrupting it, and
  those sites are hand edits. The uses that are not macro arguments are still
  the tool's, so splitting one rename into a hand edit of the rest plus a run
  over what remains is worth doing;
- prose outside the scanned set, such as the agent rules files and tool
  docstrings.

`rename_item.py` logs functions, globals, types and qualified macro identities to `local/renames.tsv`; a
field or parameter rename leaves no row, by design.

Why a parser and not a search-and-replace: hundreds of unrelated types here
declare a member of the same placeholder name, and one function may declare the
same identifier in several nested blocks with different types. Only the resolved
declaration distinguishes them, so text substitution silently edits the wrong
struct.

**A symbol that is still `INCLUDE_ASM` cannot be renamed this way.** It has no C
declaration, so there is nothing for the parser to resolve. Rename it in the
symbol map, update the `INCLUDE_ASM` argument, and re-split.
