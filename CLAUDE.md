# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Overview

This is a matching decompilation project for Parasite Eve 2 (PS1). The goal is to create C code that, when compiled, produces the exact same assembly as the original game ROM.

## Project Structure

- `src/<overlay>` decompiled C (`src/engine`, `src/gameplay`) and
  `src/<family>/<overlay>` for the generated overlay families (`src/weapons/m93r`, …)
- `packages/` overlays migrated out of splat, and the libraries they share, written by hand (see "Packages"
  below)
- `include/<overlay>` headers for that unit (`include/rooms`, `include/actors`, …); a package's public headers are
  its own `include/` (`packages/engine/include`, `packages/gameplay/include`), which split units include by the
  package's name (`engine/fs.h`)
- `asm/<ver>/<overlay>/nonmatchings` unmatched functions (one `.s` per function). Overlays may be nested (e.g. `asm/USA/stage1/101/nonmatchings`).
- `asm/<ver>/<overlay>/matchings` already-matched functions
- `packages/psyq` the Psy-Q SDK as the game links it: its headers and, in `lib/`, the members of its libraries the executable links (`lib/versions.txt`)
- `assets` binary asset blobs extracted from the rom

### Packages

A fully decompiled overlay leaves splat and becomes a package. So far: the title
(`packages/title`), Aya's costumes (`packages/aya/{mist,dryfield,akropolis,range}`),
the options menu and the package after it (`packages/options`,
`packages/stub_20200`), the enemy name table (`packages/debug/enemy_names`), the
Parasite Energies (`pe/pyrokinesis`, ..., under the umbrella `pe`), the items cast like them
(`packages/items/*`) and the player's 32 weapons (`packages/weapons/*`, each with its own header,
`<weapons/m93r/m93r.h>`; the M4A1's and MP5A5's upgrades, the P08's snail magazine and the Mongoose,
the SP12, the MM1 and the four unused slots are variants). Code the overlays share is a library per module, which a
package depends on alone: the fragments it includes where its copy sits, with the
types they use, under the umbrellas `effects` (`effects/glow`,
`effects/sprite_quad`, ...), `screen` (`screen/fade`, ...), `projectiles`
(`projectiles/launcher_grenade`, ...) and `weapons` (`weapons/weapon`, the indices,
sound slots and types the player's code and every weapon share, `weapons/attack`,
...), which have no files of their own. A module lives at the lowest level holding all its users: what
only the Parasite Energies share is `pe/effects/*`, what only the weapons draw `weapons/effects/*`, and a module shared within
one stage says so, to go under that stage once it is a package. Its folder is its
source, and its declaration (`declarations/packages/`, the path being the
folder) says what of it is built and what fills its slots. The same files are
compiled by two builds that know nothing of each other: this repository's
ninja build, which needs no Rust, and the editor's, which builds each package
from a tree of exactly what it declares and reaches (ninja builds in place and
only has to follow it).

**A package declares its files by kind and by what each is for**, as CMake
does, and nothing is assumed: what is not declared is not built.
`headers.public` is the one folder its name stands for (`<engine/fs.h>`),
`headers.private` the folders its own quoted includes are found in;
`sources` and `objects` are files (globs, a `!` leaving out what it matches).
`private` is its own build's; `public` is its own and, as far as an image,
what links it - Psy-Q's members and libsn's start-up (`sources.public`) go to
the executable that links Psy-Q. A target adds under `target.<name>`, its
tables merged and its lists appended (`targeted`).
A package of one source names it `src/package.c`, since only its declaration
names it; its public header is named after the package (`<weapons/pa3/pa3.h>`,
never `package.h`), since every file including it names it, and diagnostics,
tabs and searches show only the file's name.

| File | Role |
|---|---|
| `packages/<...>/src`, `include`, `psx/link.ld` | source, written by hand |
| `packages/core/include` | `pe2.h`, the macros a source declares slots and options with, and the types and helpers every package uses (`types.h`, `common.h`); written by hand |
| `packages/{engine,gameplay}/include` | the public headers of the two images still split, written by hand like any package's |
| `packages/math` | `gte.h`: the GTE's commands as the COP2 words Sony's DMPSX made of the SDK's placeholders |
| `packages/psyq` | the SDK: `include/`, a copy of `include/psyq` (which the decomp's tools read), and `lib/`, its objects and `snmain.s`, which the package compiles |
| `packages/{engine,gameplay}/psx/symbols.ld` | made from their ELFs by ninja (`gen_package_library.py`), gitignored |
| `configs/USA/resident.toml` | the models a package carries, which `dump_asset_db.py` catalogs |

**A package reaches another by depending on it, as Unreal's modules do.** Its
declaration lists what it uses (`dependencies`, which a target may change like
any list). A package is named by the packages it is in, not by its folders: its
folder's name, after the name of the nearest package whose folder holds it
(`engine`, `aya/mist`; `packages/stages/akropolis/rooms/patio`, where `stages`
and `rooms` are plain folders, would be `akropolis/patio`). A name is one
package's within the package it is in, or among those in none, which both
builds check (`aya/akropolis` beside `akropolis/patio` is fine, two packages
named `aya` are not). Its image is the file the release names, the name's steps
joined by `_` (`aya_mist`). The bracket says whose a file is:

- `#include <engine/fs.h>` is always another package's public file. The
  package's own folders are on the path only for quotes (`-iquote`), so an
  angle include cannot reach them; dependencies are found under their names in
  `build/USA/packages/<name>/deps` (the editor's tree: `dep/`).
- `#include "scratch.h"` is always the package's own: beside the file, then its
  header folders, public then private. From a public header it stays inside the public
  folder, so a dependent never needs another package's private files. The only
  exception is what the build makes for the package (`pe2_options.h`,
  `pe2_values.h`), which `pe2.h` includes.

Both builds hold every file read to these rules and fail by name: an angle
include of a package the file's own package does not declare, or a quoted one
that is not its package's. What a dependency's public headers include of its
own dependencies it *exposes*, derived from the text and never declared, so a
package compiles with what it declares and what those expose, and theirs
(`Package.reachable`). An image links everything it depends on as far as an
image: an image dependency is linked against by its symbols, and a library
reached through it is that image's, so Psy-Q's members come from the
executable rather than into the title (`Package.linked`). The two builds
implement this once each: `tools/buildsteps/declarations.py` and the editor's
`gather.rs`. `PE2_BIND(x, package)` is a slot, a choice made in the editor
(`self`), never how a package reaches a library.

**A slot can say more of itself, and the build answers questions about it.** After
its type, `_self` fills a slot, where the declaration does not, with the package
being built, and `_hidden` keeps it out of the editor. `PE2_QUERY(file, contains,
self)` is 1 or 0: whether what one slot holds names, anywhere in it, what another
holds. A package cannot know which file holds it, as one package may sit in
several, so `PE2_BIND_SELF(file)` binds a file and holds it, by a `STATIC_ASSERT`
on that query, to being one that names the package (`title`, whose intro ids are
its file's streams).

**A package declares its slots and options in one of its units.** Every
`PE2_BIND`, `PE2_BIND_SELF`, `PE2_BIND_ARG`, `PE2_OPTION_DEF` and
`PE2_EDITOR_LABEL` of a package is in one C file it compiles - `src/package.c`
for Aya's costumes, `src/package.c` for the title - and the editor lists them in
the order they are written there. Never in a header (or an included `.inc.c`):
whatever includes it would read it. Both builds fail by name on a declaration
outside the package's units and on a second unit declaring one. What reads
them - `PE2_VALUE`, `PE2_OPTION`, `PE2_QUERY` - may be anywhere in the
package.

**What a designer tunes is an option.** A package's tuning constants - timings,
counts, screen positions - are `PE2_OPTION_DEF`s, read with `PE2_OPTION`, each
with the original value as its default (`PE2_OPTION_DEF(IDLE_TIMEOUT_TICKS, int,
900)`); its declaration's `option` table holds only what a project changes. A
parameter with no meaningful default, such as a variant's index, has none, and
has to be set. Constants coupled to each other become one option the others
follow from (`title`'s black fade: its length and brightness step both come from
`BLACK_FADE_STEP_SHIFT`). What stays in C is hardware and asset facts and wiring
such as task slots. A limit an option's uses put on it is a `STATIC_ASSERT` beside
them (`title`'s fade step divides full brightness), so a bad value fails by name.

**A package may be built several times: its variants.** A declaration's
`variants.<name>` tables each make another image of the package's one source, named
as a sibling of the package (`weapons/mp5a5` and its `mp5a5_p1`: the image
`weapons_mp5a5_p1`), with what the variant sets over the package's - its options,
its bindings, its dependencies appended, its link script (`target.psx.link_script`,
which writes its own id). The source does not know its variants: what only some of
them have is `#if PE2_OPTION(...)`, and a variant binds only what its live code
reaches. A file slot names a variant as the package's output of its name
(`output = "mp5a5_p1"`). In the editor the package's stack builds each variant
through its whole chain (an `each` output with `branch`, the layer it starts from);
in ninja each is a package of its own (`Declarations.with_variants`).

**A table of tuning is one option of a registered type.** `PE2_TYPE(Name)`, after a
typedef, registers it with the build: an alias of an integer primitive, or a struct
whose fields are integer primitives, registered types, or fixed arrays of either,
one to a declaration - never a pointer. A primitive is one of core's fixed-size
integers (`s8` to `u64`, `i8` to `i64`, `byte`, the Q types) or one of C's at the
target's size. An option of one, or of a fixed array of one, takes a C initializer
as its default (`PE2_OPTION_DEF(LEVELS, _LevelTuning[3], { ... })`), and the code
initializes its table with `PE2_OPTION(LEVELS)`; `PE2_OPTION_AT(LEVELS, 2, wedges)`
is one integer of it, for assertions and derived sizes (a table sized by the most
any level has, `LARGER` in core). Values code takes as constants rather than from a
table are read the same way, so nothing new is stored (inferno's fan variants).
An option may also be a dynamic array (`_Wave[]`, the option's own type alone; a
field's array has a count): its length is what it holds, one or more, and
`PE2_OPTION_COUNT(NAME)` gives it (any array's). What a package sets merges into
the default: a table by field, an array by element - a dynamic one as long as what
is set, an element past the default's set in full.
A primitive is an option type too (`PE2_OPTION_DEF(RING_Y, s16, -1024)`), held to
its range. Both builds read types as text (`option_types.py`, the editor's
`types.rs`) and write the same `pe2_options.h`; the editor lists one as an
inspector does, each element and nested struct a group that opens, each integer a
field held to its type's range, the default greyed in what a package leaves unset,
a dynamic array's elements added (a copy of the last) and removed.

`ninja_packages` in `ninja_config.py` finds what to build in the declarations - a
package is what names a `link_script` - through the rules everything else uses,
and writes `build/USA/out/<name>`, held to `checksum.sha` like any image. A
package needs no splat config, no symbol map and no manifest entry, so `diff.py`
and objdiff do not cover it: a break is a checksum failure on that package.

What a package compiles from is made in the editor's steps around a first pass
(`gen_package_values.py`): `options` scans text and writes `pe2_options.h`, `cpp`
runs with `PE2_FIRST_PASS`, and `values` reads what that left and writes
`pe2_values.h`. The tokenizers mirror the editor's (`package_tokenizers.py`) but
never decode: a slot is the bytes the disc stores, found through the declarations
(`declarations.py`, `stage0.py`), with no `asset_data.py`. So this build proves
code and layout; that assets rebuild those bytes is the editor's to prove. Where
both make a token it must be the same text.

**What splat still builds reaches the packages' headers where they are.**
`ninja_config.py` links each package's public folder into `build/include` by
its name, as a package's `deps` holds them, so a split unit includes
`engine/fs.h` or `title/title.h` with no copy, and their own angle includes
(`<core/pe2.h>`, `<psyq/libgte.h>`) resolve there too. `pe2.h` includes what
the build makes for a package; for a split unit those are empty
(`build/include/pe2_values.h`). `include/decomp`'s `types.h` and `common.h`
are core's with splat's additions (`INCLUDE_ASM`, the version macros), so both
builds have one set of types. A split image still takes the addresses of what
it calls from its imports file.

**Disc records stay.** `dump_asset_db.py` rebuilds `asset_data.py` from the disc,
so a package keeps its record and its models in `resident.toml`, which is also
what lets `pack.py` still rebuild the disc. Moving an overlay drops only its
`required` and its embedded records' `include`, not its presence in the table.

`clang-format` runs over the hand-written packages but core, whose `pe2.h` it
would rewrite.

### Generated overlay configs

`engine` / `gameplay` have hand-written splat configs; each has quirks a
template cannot express. Every other overlay is a flat `.pe2pkg` with the same
shape, and there are 442 of them, so their configs are **generated**:

| File | Role |
|---|---|
| `configs/USA/overlays.toml` | the manifest — keyed by package name, holding only what the decomp adds |
| `configs/USA/overlay.template.yaml` | the shared config body |
| `configs/USA/generated/*.yaml` | output, gitignored, rewritten by every `ninja_config.py` run |
| `configs/USA/sym/<family>/<name>.txt` | per-overlay symbol map |
| `configs/USA/sym/<family>.imports.txt` | main + gameplay imports, shared by the family |

`sha1`, size and the `.text` span are **derived from the package**, never
written in the manifest, so a config cannot drift from the data it describes.

**Model streams are named too**, by SHA-1 in `asset_data.MODELS`, because dedup
otherwise lets whichever package sorts first name a shared mesh. A named mesh
shows under that name in every package carrying it.

**An overlay is an extracted package.** Identified packages — and the folders
and stage-0 files that contain them — are *named in the extractor* (`tools/peassets/asset_data.py`, preserved across
`dump_asset_db.py` regeneration by sha1), so they extract as `m93r.pe2pkg`
rather than `pe2pkg_2.pe2pkg`. The manifest key is that name and the config is
built from the package in the data the build reads (`assets/USA/pe2pkg/<key>.pe2pkg`, reached through
the link `build/USA/project`) — file ids appear nowhere in the
decomp. Several stage-0 ids can load one package (the extractor dedups by
SHA-1, so `10500` and `10600` are one file); naming it once in the extractor is
what makes it one thing here. The generator errors if two entries resolve to the
same package.

Packages the decomp builds are marked `"required": True` in `asset_data.py`, so
`python3 ninja_config.py --iso_build` materialises exactly that set — 443 overlays —
with the rest of what the build reads (the stored chunks, the executables, the
instances of the data compiled into overlays, the folders' streaming lists) into
`assets/USA`: the CI/matching extract, and nothing a repack alone needs. A project
for the editor is made from the declarations: `venv/bin/python3 tools/pe2project.py
create --roms rom/image` makes one in `project/` (gitignored) - the game's nodes
(`.layers/game`, with its changeset) and the plan filling them with their files
(`.layers/game-files.plan.jsonl`, stating the discs it reads by size and sha1)
written by `tools/editor_project/`, the plan run by `pe2-cli project extract` into
the generated layer `.layers/game-files` (only what changed is made again; `pe2-cli
project status` says whether the layer is as its plan says, `verify` checks its
files, both without the discs). The discs are in the unmanaged mount `roms` -
read by path, never indexed - wherever this machine's `project.local.toml` says.
The project itself is made by the editor's commands (`pe2-cli project new`, `layer
add`, `layer add --generated --fills`, `mount add`, `mount locate`, `config add`),
never by writing its `project.toml` - and every asset tool defaults to it
(`tools/peassets/project.py`). The editor knows nothing of the declarations: it runs
the plan it is given.

**A project is of every release declared: USA and Europe.** Europe's discs have
USA's structure, so each asset's `[<asset>.from.EUR]` is written by
`tools/editor_project/release.py declare EUR --like USA`, which surveys both
releases' discs through the plan the USA's declarations make (`pe2-cli plan
observe`, USA's reads held to what it declares): an asset read alike takes USA's
placement; one read differently takes Europe's sha1; a package's run is USA's
rebased (Europe loads its packages 0x1110 higher); a texture or font is held by its
extractor to the declaration for Europe's bytes; what fits none is left out and
listed. What releases declare alike is one file of a node, made of any of them in
order (`files: {"native.bin": [USA's read, Europe's]}`, an input `{"any": [...]}`),
keyed by all; what differs is `<release>/<file>`, which the node's file step reads
when the active config is of that release. `pe2-cli project extract` reads the
releases whose discs are found (`--releases all|available|USA,EUR`); a layer of
some is `partial`. The old full Python extraction (`tools/peassets/extract.py`:
`stages.json` and the ISO manifests, which the viewer, `dump_asset_db.py` and the
repack read) is run by hand.
`ninja_config.py --project [DIR]` builds a project's data instead of
`assets/USA`.

**The `.text` span is derived, and rarely wrong.** A `0x03E00008` word inside a
data block can extend the span past the end of the code; splat then emits a
`dlabel` for that data inside the code subsegment. The build does *not* reliably
fail on it — from a clean split the reference and definition agree, and it only
breaks on a later re-split — so `ninja_config.py` checks for it after every
overlay split and stops with the `text = [start, end]` override to add. One room
of 168 (`s2_30`) needs one.

**The leading rodata is one subsegment, owned by the first code unit.** That is
fine while everything is `INCLUDE_ASM`, and wrong the moment you decompile a
function in a *later* unit whose jump tables live in that block: a unit's
`.rodata` appears once in the linker script, at the offset its subsegment names,
so the compiler-generated table lands after everything instead of at its
address. Cut the block where ownership changes with the manifest's `rodata` key
(`rooms/mine_cavern` is the worked example), then delete the affected `src/`
files and re-split so splat places the `INCLUDE_RODATA` lines itself. A cut
that has to move the *function* too — the usual case for a compiler-generated
jump table, which needs to start its object's `.rodata` or GCC's `.align 3`
pads it — pairs the `rodata` key with `units`, a list of extra `.text` cut
offsets (`rooms/mist_parking`). See `DECOMPILATION_LEARNINGS.md`,
"Compiler-generated jump tables".

**Deleting those `src/` files discards every matched C body in them, and the
build will not tell you.** splat regenerates the file as pure `INCLUDE_ASM`, and
an `INCLUDE_ASM` assembles to exactly the bytes the C compiled to - so the
checksum still matches, every overlay still links, and the only thing lost is
decompiled source. This has already cost 39 functions - 37 of them to a single
pass, `fee5f15b`, which replaced whole room `.c` files with freshly-split
`INCLUDE_ASM` stubs instead of editing them - including one in `mine_cavern`,
the overlay named above as the worked example. (Attributing these needs care:
`git log -S` on the `INCLUDE_ASM` line reports whichever later promotion
renumbered the unit, not the removal. Test when the *definition* disappeared.)

**A `rodata` cut is state that belongs to a matched body.** While the function
is `INCLUDE_ASM` its jump table is an `.incbin` and needs no cut, so a
regenerating pass drops the cut along with the body. Restoring the body then
puts a compiler-generated table mid-object, GCC pads it with `.align 3`, and the
overlay builds 4 bytes too long with everything after it shifted - failing at
the checksum with nothing pointing at rodata, while `diff.py` on the function
says it matches. The function does match; the object does not. Re-add the cut at
the jump-table offset and hand the run below it to a sibling unit.

So the delete-and-re-split step is destructive, and needs bracketing:

1. **Snapshot the bodies first.** `bodies_of()` in `tools/vacuum/land_overlay.py` parses
   every C definition out of a file; record what the affected files hold before
   deleting them.
2. **Prefer rebuilding over regenerating.** splat's fresh output is a correct
   skeleton, not a correct file: reconstruct each `.c` from the pre-promotion
   source, redistributing its chunks across the new units, rather than starting
   from the regenerated version and adding bodies back. A promotion sweep that
   did this rebuilt 595 carrier files without losing one.
3. **Check before committing.** `python3 tools/check/check_lost_matches.py` fails when
   a function with a `matched` commit is `INCLUDE_ASM` while its overlay still
   owns a `.s` under `nonmatchings/` - the exact signature of a dropped body. A
   genuine promotion cannot trip it, because a promoted body has neither a C
   definition nor a `.s` of its own.

The same hazard applies to any landing that writes whole files from a worktree
cut before another lane's commit; that is a different cause with an identical
symptom, and the same check catches both.

**splat creates a unit `.c` that is missing, but never rewrites one that
exists.** That is why the step above has to *delete* files to regenerate them,
and it has a second consequence: a manifest change that renumbers units cannot
be applied to an existing tree by re-splitting. Adding a `shared` span in the
middle of an overlay shifts every later unit, so the bodies belong in different
files afterwards - but a re-split only creates the one new unit and leaves the
existing files exactly as they were, holding the old distribution. Moving the
bodies is the manual step `overlay_dup_index.py promote` asks for, and it
happens in whichever tree the agent is working in. A second tree given the same
manifest does not follow; the two then disagree about which unit holds what.

This is what strands a promoting sweep at landing time, because
`land_overlay.py` maps bodies onto trunk by filename. The fix is not to
re-split harder: replay the worktree's own commits instead (`CAN_REPLAY` in
`vacuum_overlay.sh`), which needs no correspondence between the trees and keeps
each commit's attempt count.

**Recover data in a TU that contains code.** Place `.rodata`, `.data` and
`.bss` definitions as late as possible, normally in the first TU that uses them
(including references through other tables). Do not introduce data-only C files
to preserve a disconnected run. A data-only TU is right only where the bytes
show the original object's boundaries - `src/gameplay/item_catalog.c`, whose
sections begin exactly where its neighbour's did and whose `.rodata` ends in a
junk byte that differs in every release. Existing `_work` files are
provisional layout workarounds, not a pattern to extend. With hand-written configs,
`auto_link_sections: []` lets explicit dotted subsegments place each source
object's sections independently of where its code first appears. A source
object still contributes each of its sections only once; disconnected runs
need ownership analysis and, where supported by code/data evidence, TU splits.

**Zero bytes do not establish an object or its extent.** Check natural type
alignment and missing TU boundaries before defining storage. Do not invent
unreferenced zero arrays or widen an array just to consume a gap. Keep uncertain
intervals in generated assembly until their ownership and extent are known.
Do not change BSS alignment in `ninja_config.py` to force a proposed layout.

In BSS, an apparent gap may also be unrecognized trailing fields or unused
array capacity belonging to the preceding object. Observed accesses establish
a minimum extent; they do not prove where the original object ended. Check
indirect accesses, aliases, array strides, and clear/copy lengths before
choosing between a larger object, separate storage, alignment or a TU boundary.
A `pad` subsegment preserves bytes but does not establish their purpose, and
a matching checksum cannot distinguish these layouts when all bytes are zero.

**Require global accesses to be in bounds.** Use the working assumption that
the original code does not deliberately invoke undefined behavior. A proposed
object definition must accommodate every reachable read and write through its
base, aliases, derived pointers, table references and callees, across all
overlays. Derive index ranges from callers, loop limits, masks, branches and
sentinels; do not infer them from the array size currently written in C.
Include the complete access width and bulk clear/copy length. Forming a
one-past pointer is allowed where C permits it; dereferencing it is not.

If an access would extend into a `pad` interval, the proposed extent or the
base/type interpretation needs correction. Adjacent storage cannot make an
out-of-bounds C access valid, even when the binary matches. If required extents
overlap another proposed object, revisit the boundaries and alias/aggregate
model rather than accepting UB. Record unproved index ranges as unresolved;
do not declare a gap to be padding until the reference constraints allow it.

This rule guards against inventing object boundaries, so it does not apply
when the boundaries are certain. Where each object's extent is established
independently of the access - by its own contents and type, by where its
neighbours start, by the same code in another build that has larger tables -
and the access past it is the original program's own bug, declare the real
extents and state the overrun at the declaration. `actor_421600`'s catch
tables are the worked example: five entries each, written at entry 5 by a
handler made for the regular build's nine.

**Trace symbol bases and field offsets in assembly.** Follow the full address
loaded for an object and the displacements from that base, including copies of
the base register, derived pointers, indexed accesses, and uses in callees.
An access at `base + offset` can establish a field beyond the currently
recovered extent. Record the offset and access width; do not turn it into a
new global just because it falls in a current `pad` interval.

Known fields can also be accessed with the offset folded into `%hi/%lo`
immediates, as in `sceneSetEnemyAlert`. Cross-check these absolute addresses
against uses that establish the containing base. Splat's choice of `D_x`
versus `symbol + offset` depends on the symbol map and declared sizes, so the
printed expression alone is not independent evidence of an object boundary.

**Preserve separately addressed objects when the assembly requires it.** In
the two-vector examples below, folding `D_x_B` into `D_x_A[1]` compiles and
keeps the data bytes identical, but changes the address calculation and fails
the match. Indexing emits the array's address
plus 8 (`addiu $v0,$v0,0xe704`), naming it emits its own (`addiu $v1,$v0,
0xe70c`). Declare `SVECTOR D_x_A[1]` followed immediately by `SVECTOR D_x_B`,
which reproduces both. `weapons/gunblade`, `m4a1_bayonet` and `tonfa_baton` are
the worked examples; the same shape in `pe/energyshot` needed the opposite call,
where a `(u16)` cast at the one use site was enough.

**Overlay work state must remain inside the loaded image.** An overlay is a
flat LZSS image inflated straight to its load address (`doc/OVERLAYS.md` 4.2);
the loader does not clear separate BSS. Prefer declarations without initializers
for recovered zero-filled work when its layout permits BSS. The generated
overlay manifest's `bss` key places that unit's BSS at an explicit eight-byte
boundary and includes its zeros in the loaded image. It can follow the unit's
data and precede another unit's data; empty sections are not auto-linked.

Keep explicit zero initializers for objects interspersed with initialized
data, or whose addresses do not fit BSS alignment. One unit cannot contribute
several disconnected BSS runs. Removing an initializer without also placing
the resulting BSS would either move the object or omit its zeros from the
image. In particular, `ld_bss_is_noload: True` does not serialize BSS.

The separate `bss_size` manifest option describes an established terminal
runtime allocation whose stored prefix ends at the package boundary. It uses
NOLOAD BSS and serializes that prefix only; it cannot accompany ordinary
stored BSS in the same image.

Gameplay now models its stored work as `.bss` input sections using explicit
subsegments and `ld_bss_is_noload: False`. The linker combines those inputs with
the code/data in the loaded `SHT_PROGBITS` output section, so the image still
contains every zero. Verify both the linked symbol addresses and the complete
image checksum; input `SHT_NOBITS` alone does not tell you whether zeros are
stored in the final overlay.

Do not switch `ld_bss_is_noload` globally in `overlay.template.yaml`: empty BSS
sections can also introduce alignment and break packages that end flush against
their last data byte. Establish the section layout separately for each image.

**Several packages can be one source built with different parameters.** Once
packages, these are variants (above); in splat, as some actors still are, such an
entry lists its packages in
`slots`, each with its own `id`, `label` and `defines` (`{ WEAPON_INDEX = 0x1F }`),
and marks the code unit `variant = true`: the unit is then named per package,
so each gets its own object, but all compile the entry's one source with that
package's defines (`tools/splat_ext/variantsrc.py`). A slot may also carry its
own `objects` list where the packages' models and data sit at different
offsets, and `aliases` where the resident images need a name of its own for
that package's copy of a definition (`DEFINE_ALIAS` in
`include/decomp/common.h`; the alias has to be made in the object that defines
the symbol, so the source ends with `PACKAGE_ALIASES`). Declare a parameter only where the bytes show it varies, derive what
follows from it (a weapon's item is always its index + 0x7F, `WEAPON_ITEM_ID` in
`<weapons/weapon.h>`), and keep a value that merely looks derivable as a
declared one when the original did not follow the pattern.

**Every overlay starts with its own id: a `u16` at offset 0, in a `u32` slot.**
Every package but `enemy_names`, whose word 0 is its first datum, carries a
distinct value there, a running count by family: gameplay 4, title 5, weapons
8-39, the other stage-0 packages 40-46 (options 40, game over 43, ending 45, and
Aya's costumes 7, 41, 44 and 46 among them), pe 48-62, map pages 63-79, actors
80-279, the stage overlays 280-284, rooms 285-452; 1-3, 6 and 47 are unused.
The high half is always zero.

Do not read it as text. pe's block is 48-62, which is `0x30`-`0x3E`, so splat
renders those bytes as `.asciz "0"` … `";"` and the id looks like a character
scheme; it is not, and ids 58/59 printing as `:` and `;` is just the ASCII table
running out of digits. Only the overlays with `rodata_head` split the id into a
`<name>_hdr.rodata.s` - elsewhere it is folded into the first code unit's
rodata, or, for rooms, sits as `D_<room>_8017D5C0`.

**The id is its own word, written by the link.** Wherever the first code
object opens with a compiled jump table - `pe/pyrokinesis`, `acropolis_plaza`,
and many more - the table sits at `0x4`, and GCC aligns a jump table to 8 from
the start of its object. Defining the id as that object's first datum moves the
table to offset 4, the `.align 3` pads it, and the image grows by 8 bytes. So
the id cannot share the first code object. The linker script writes it:
`tools/splat_ext/pkgid.py` turns the manifest's `packageId` entry - or a
hand-written config's `pkgid` subsegment - into `LONG(<id>)` at offset 0. Do not
fold it into the next file's data, and do not give it any data or code of its
own. No code reads it - the loader inflates a chunk to its header's address and
checks nothing, and no code or data forms the address of any slot's word 0 -
and none indexes by it: where a family's ids follow how its packages are
chosen (a weapon's is its `WEAPON_INDEX` + 7, a stage overlay's its stage +
279, a room's 285 and its place in stage and folder order), that is only the
order they were counted in, which breaks elsewhere (actors within a load
bucket, the map pages, stage 0). So it is the packaging tool's, and a
package's link script writes it for the one target that has one.

**A room's index is its folder's disc id divided by 100.** Room folders have
ids `index * 100 + 1` (`asset_data.TREE`), and the game's per-stage tables -
`gAreaRecordsByStage[stage][room]`, `gAreaObjectStages`, the map overlays' room tables -
are indexed by that number. It is *not* the order folders appear under
`stageN.folders`: a stage that skips an index (Dryfield has no room 10, the
night stage no room 4 either) shifts every later room. Those tables point
*into* the room overlay, so resolving them needs both images mapped at once -
gameplay at `0x80093800` and the room at `0x8017D5C0`. `tools/find_models.py`
resolves the model references this way.

**A unit is re-split only when something it reads has changed.** The stamp in
`linkers/USA/.split/<name>.json` hashes the config, the target binary, the
symbol and reloc files the config names, `sym.<name>.imports.txt`, the unit's
`c`-subsegment `.c` files (splat reads those to sort each function into
`matchings/` or `nonmatchings/`, and creates a `.c` that is missing), the splat
and spimdisasm versions, and `ninja_config.py` itself. A unit that misses has
its `nonmatchings/<name>` and `matchings/<name>` directories deleted before it
is re-split, because splat never removes a `.s` whose function moved. **If you
make the split, or a post-split fixup, read a new file, add it to
`split_inputs()`** - otherwise the cache serves outputs that ignore it and the
build still passes. `build/` is kept between runs for the same reason it can
be: the assembler's depfiles track the `.s` files `INCLUDE_ASM` pulls into a C
object, which cpp never sees, so any new rule reading files cpp cannot see
needs a depfile of its own. `--clean` / `ninja_config.py --fresh` bypass all of
it.

Two maintenance commands, neither run by the build:

- `python3 tools/buildsteps/gen_overlay_configs.py [--family F] [--list]` — regenerate the
  configs. `ninja_config.py` also calls this, so the build is self-consistent.
- `python3 tools/buildsteps/gen_overlay_imports.py [family]` — rebuild a family's imports
  file from the split output, then re-split so the sources pick up the names.
  Run it after adding a family or after a naming pass.

Overlays in a family all load at the same address, so `symbol_name_format`
prefixes generated names with the segment (for example, `func_sp12_8011D1DC`). Keep that:
the decomp tooling (`decomp_overlay.py find`, `tools/match/claude`,
`score_functions.py`, the vacuum) assumes a function name identifies one
overlay.

## Tools

- `./tools/build-and-verify.sh [--only SELECTOR[,SELECTOR...]] [--clean]`
  build the project and verify that it matches the target. Run it bare: it
  re-splits only the units whose split would change and rebuilds
  incrementally, so a no-op run is ~7s and a one-overlay edit ~9s — the same
  cost as the old scoped run, with none of its blind spots. `--only` splits,
  builds and checksums just those units — a family (`core`, `weapons`) or a
  single basename (`gameplay`, `m93r`) — and is worth reaching for in one
  case: iterating on a header many overlays include, where the full build
  recompiles all of them (~50s). Its `✅ SCOPED BUILD SUCCEEDED` says nothing
  about the overlays it skipped, which for a header change is the thing you
  have to check, so finish unscoped. `--clean` wipes `asm/`, `linkers/` and
  `build/` and splits everything, as every run used to.
- `python3 tools/check/suite.py [--jobs N] [--skip STEP ...]` the full check suite
  over the project's extraction (`project/`; it never extracts): asset-table stability, the
  repack of both discs, every declared texture, model, clip, shape, font and
  room encoding back (one task an asset), the build with the room order check,
  residency and declarations - a work graph on one pool of workers, about 2.5
  minutes. Re-extract (`tools/pe2project.py
  extract`, and `ninja_config.py --iso_build` for the build's data) only after
  changing the asset table's schema or a decoder.
- `PE2_JOBS=N` caps the parallelism of the build (`ninja_config.py`'s split
  pool and `ninja`) and of the libclang refactor tools, which otherwise use
  every CPU. Set it when several trees build at once: the naming pass sets it
  per worker to the CPUs divided by `--workers`, because a full split pool per
  worker (~110 MB a process) exhausted memory.
- `python3 tools/check/check_sym_coverage.py [IMAGE ...]` fail when a C function's
  name is not in its image's symbol maps (`symbol_addrs_path`). splat names an
  unmapped function `func_<segment>_<ADDR>` in the expected objects, so a
  function renamed in C but not in the map cannot be paired by objdiff or the
  naming pass's verifier, while the checksum still matches. The unscoped
  `build-and-verify.sh` runs it; when it fails, add `name = 0xADDR; // type:func`.
- `diff.py` you can view the difference between the compiled and target assembly code of a given function by running `python3 tools/asm-differ/diff.py --no-pager <function name>`
- `./tools/match/claude [--bootstrap-only] [--no-bootstrap] [--id ID] <function>` spin up a scratch matching env. Resolves **any** overlay; always m2c-bootstraps unless `--no-bootstrap`. It builds the environment and nothing else - the agent is launched by whatever called it. Matching loop: `tools/claude-decomp-env/MATCH_LOOP.md` (Grok also loads it from `.grok/rules/match-loop.md`).
- `python3 tools/match/decomp_overlay.py find|pack|list-nonmatchings|list-overlays <function>` overlay-agnostic path lookup and vacuum brief.
- `python3 tools/match/score_functions.py <directory>` find the easiest function to decompile in a given directory (and its subdirectories).
- `python3 tools/match/fit_difficulty_model.py [--write]` refit that scorer on the
  project's own record — every `matched <fn> <attempts>` commit plus every
  give-up in `tools/match/difficult_functions`, joined to the function's assembly
  metrics. Re-run it after a few hundred new matches; `--write` updates the
  constants block in `score_functions.py` in place.
- `./tools/vacuum/vacuum.sh [--grok|--claude] [--dry-run] [--orchestrator] [--difficult] [--overlay NAME]` pick the easiest unmatched function across every overlay, bootstrap, pack a brief, run the agent. `--difficult` retries only names in `tools/match/difficult_functions` (a verified match removes that name from the list); `--overlay gameplay` (or `USA/engine`) restricts to that overlay; both flags together are the intersection. Auto-commits a verified match if the agent forgot to. After a ≥95% give-up, runs decomp-permuter (`--stop-on-zero`, 6 min cap) and a short port follow-up on a hit. Best scratch C is kept at `tools/giveups/<func>/` (gitignored) so a later retry does not start from m2c. `--orchestrator` claims a function via `tools/vacuum/vacuum_orch.py`, matches in a throwaway `pe2-wt-<func>` worktree, independently verifies that worktree (agents often sha256 leftover `build/` artifacts), fast-ports when trunk files have not moved, otherwise runs a port agent. Failed trunk landings are retried (`VACUUM_PORT_TRIES`, default 2) then marked difficult so the same claim cannot loop. Do not run a non-orchestrator vacuum on this checkout at the same time as an orchestrator session.
- `./tools/vacuum/vacuum_overlay_list.sh --list FILE [--profile P] [--jobs N]
  [--difficult]` drive `vacuum_overlay.sh` through an ordered list of overlays,
  several at a time.
  The built-in `rank_overlays` sorts by unmatched-function count alone, which
  knows nothing about duplication: in actors the leading digit is a
  load-address bucket, so `actor_207000` holds 44 functions but adds one body
  `actor_107000` has not already covered. `local/actors_sweep_order.txt` is a
  duplicate-aware cover order for that family, generated by
  `local/sweep_order.py --family actors`. Concurrency needs no locking of its own - an overlay lease is
  exclusive, so a worker refused one takes the next name.
  `--difficult` is the whole-overlay form of `vacuum.sh --difficult`, and it is
  a flag on `vacuum_overlay.sh` too: the lease, the claim filter, the progress
  denominator and the inner pick all draw from `tools/match/difficult_functions`
  instead of skipping it, so a retry pass walks the list and skips every
  overlay with no parked work before it costs a worktree. With
  `--max-difficulty` it is the intersection, as in `vacuum.sh`.
- `python3 tools/vacuum/vacuum_orch.py claim|relinquish|finish|merge-acquire|merge-release|status|serve` coordinate multiple vacuums: function leases plus a merge lock on the original tree. State: `$(git rev-parse --git-common-dir)/vacuum-orch.json`.
- `./permute.sh --run --timeout 360 -j4 <func> <asm> <c>` when a match is stuck ≥95% on registers/scheduling. Stops on score 0.
- `python3 tools/match/learn.py <terms>` search `DECOMPILATION_LEARNINGS.md` by
  section instead of by line; a raw grep returns context-free lines because the
  searchable term is rarely in a title. `CODEGEN_MODEL.md` at the repo root is
  the short general model those entries are instances of — read it first.
- `python3 tools/bulk/overlay_dup_index.py stats|shared|find|solved|siblings|promote|similar`
  find code that repeats across overlays. Of the 8049 indexed functions, 36% are
  copies of another overlay's body (33% of instructions); only 10% are
  byte-identical. Equality is decided on splat's disassembly *text*, not on the
  instruction words - comparing words also equates `lw $v0, 0x4($t0)` with
  `lw $v0, 0xC($t0)`, which is a different field of a different struct, and that
  is what produced the older 56% claim. `find <fn>` lists every overlay carrying
  the same body, marking which are byte-identical; `solved` lists bodies already
  matched elsewhere, which the vacuum skips rather than matching again (677 are
  parked that way today, waiting on a promotion pass). `--family rooms` (or
  `USA/rooms`) restricts every subcommand: rooms are 43% copies, actors 32%.
  `similar <fn>` is the fuzzy tier beside those two exact ones: it ranks
  *already-matched* bodies resembling a function, in four classes - `shape`
  (opcode order, operands dropped), `fields` (load/store displacements),
  `calls` (the jal sequence) and `cflow` (branches only, long functions) -
  printing the file each candidate's C body lives in. A candidate scoring in
  more than one class is starred, and that agreement is the signal worth
  trusting. It generates candidates, never equalities: dropping operands
  equates `lw $v0, 0x4($t0)` with `lw $v0, 0xC($t0)`, the error behind the old
  56% claim. The brief embeds the top few, so an agent is handed its
  neighbours rather than having to go looking.
- `python3 tools/peassets/model preview <family or package> [--out DIR]` write
  every model of a manifest family, or of one package, as plain USD (geometry
  and parts, no textures) to open in Blender. Useful for identifying an overlay
  whose name is still a placeholder.
- `python3 tools/check/check_regalloc_model.py <scratch>/*.i.lreg [--inversions]`
  measure how much of a real allocation `CODEGEN_MODEL.md` section 10's ranking
  formula accounts for. Comparing only pseudos that competed in the same block,
  the ratio orders 66% of pairs correctly and 38% sit at an exact tie; the rest
  is the suggestion pass, which shows up as inversions clustered on `$v0`. Re-run
  it after anything that could move allocation - a compiler patch, a new maspsx -
  or on a function class the model has not been checked against.
- `python3 tools/check/check_pointer_arithmetic.py <file or directory>` detect pointer arithmetic with casts that should be replaced with struct field access. Use `--strict` to fail on violations.
- `tools/refactor/find_references.py` / `rename_item.py` resolve and rename C
  symbols through libclang and the compilation database, never by text
  substitution. A spec names the file the symbol is **declared** in. `NAMING.md`
  § Tooling states what they reach and what stays a hand edit — handwritten
  assembly, inline asm, macro-reached references and a symbol map's prose are
  all outside them.
- `venv/bin/python3 tools/refactor/ref_index.py build|refresh|stats|refs <usr>`
  the persistent reference index (`local/ref_index.sqlite`, ~58 MB) that
  `find_references.py` and `rename_item.py` query instead of parsing every
  candidate unit, and from which `dep_graph.py --build` assembles the naming
  graph (~14 s, no parse of its own). Every query refreshes it first: files
  whose content changed have their sites and graph records re-collected from
  the units that include them (a widely included header in ~3 s, a source file
  in under one), and a change of compile flags, of the collector or of the
  graph's extractor (`ref_index.graph_records`) rebuilds it (~75 s). Locals and parameters are not
  indexed and still parse their one unit; `PE2_REF_INDEX=0` turns it off. The
  naming pass's driver keeps it current and copies it into each worker.
- `venv/bin/python3 tools/refactor/check_message_handlers.py [--void]` check
  that every function installed in a `TaskMessageEntry` table has the shape
  `taskMessageDispatch` calls: four parameters, `Task*` first and the `s32`
  message id second. The table's callback type is unprototyped
  (`s32 (*)()`), so the compiler accepts anything; payload parameters keep
  each handler's own types. `--void` lists the handlers that return nothing.
  It parses every unit (~1 min) and is not run by the build.
- `python3 tools/refactor/check_decls.py [PATH_PREFIX ...]` report C symbols
  whose declarations disagree with their definition, or with each other,
  within one linked image, plus unprototyped and implicit declarations. The
  build never compares them, so a function declared `f(s32)` and defined
  `f(u16)` in another file still matches; such a conflict usually means one
  side's type was fitted to the instructions rather than recovered.

## Code Quality Standards

### Struct Usage

**NEVER use pointer arithmetic with manual offsets.** Always define and use proper structs.

**BAD - Pointer Arithmetic:**

```c
s16 func(void* arg0, u16 arg1) {
    return *(s16*)((u8*)*(void**)((u8*)arg0 + 0xC) + arg1 * 36 + 0xA);
}
```

**GOOD - Proper Structs:**

```c
typedef struct {
    s16 unk0;
    u8 _pad[0x8];
    s16 unkA;
    u8 _pad2[0x18];
} ArrayElement;  // Total size: 0x24 (36 bytes)

typedef struct {
    u8 _pad[0xC];
    ArrayElement *unkC;
} FunctionArg;

s16 func(FunctionArg* arg0, u16 arg1) {
    return arg0->unkC[arg1].unkA;
}
```

### Struct Definition Guidelines

When you see pointer arithmetic patterns like `*(type*)((u8*)ptr + offset)`:

1. **Identify the access pattern:**

   - What offset is being accessed? (e.g., `0xC` means field at offset 12)
   - Is it accessing an array element? (e.g., `value * 36` means 36-byte elements)
   - What field within the element? (e.g., `+ 0xA` means field at offset 10)

2. **Create appropriate structs:**

   - Define the element struct with correct size and field offsets
   - Define the container struct with pointer at correct offset
   - Use meaningful names or `field_[Offset]` naming convention
   - **Keep types and declarations in the smallest shared scope.** A function,
     global or type defined and used within one TU belongs in that `.c` file's
     prologue, never a header. Sharing within gameplay uses private headers
     beside `src/gameplay/*.c`; sharing between overlays (including the main
     executable) uses public module headers under `include/`. Include the
     canonical declaration instead of copying caller-local prototypes. Count
     callback/inline uses and cross-image aliases when deciding ownership.
     - **Main executable** (`src/engine/`, `packages/engine/include/`): see `NAMING.md` for
       the module → header map (e.g. sound in `packages/engine/include/sound.h`, UI in
       `ui.h`, FS/CdCmd in `fs.h`). Include the specific module header; do not
       add a kitchen-sink aggregator.
     - **Overlays:** public headers belong to their owning overlay under
       `include/`; private headers live next to its source. Public headers must
       not depend on private headers. Do not put overlay-only types in
       `packages/engine/include/`.
     - **Gameplay BSS:** keep the existing definitions in first-declaration
       order in the prologue, before API headers, with their required types
       available first. Header moves must preserve the matching storage layout;
       they do not justify changing alignment or inventing padding.
   - Use `packages/engine/include/unknown_syms.h` only for residual main-executable symbols
     (`func_800*`, unfiled BSS/data) with no module home yet. Do **not** add new
     named types or Module_ APIs there.

3. **Verify struct sizes:**

   - Calculate total size to ensure it matches the multiplier in pointer arithmetic
   - Example: `value * 36` means struct must be exactly 36 (0x24) bytes

### When Decompiling

If you write code with pointer arithmetic:

- **STOP immediately**
- Create proper struct definitions first
- Then write the function using struct access
- This applies even if the pointer arithmetic "works" - it's always wrong in a decompilation project

## Tasks

### Decompile directory to C code

You may be given a directory containing assembly files either in its own directory or its subdirectories.

1. Use `python3 tools/match/score_functions.py $(python3 tools/match/decomp_overlay.py list-nonmatchings)` to find the easiest unmatched function across every overlay. Start with that one.
2. Follow the instructions in the `Decompile assembly to C code` of this document.
3. If you are able to get a perfect matching decompilation, commit the change with the message `matched <function name> <attempts>` and return to step (1). If you cannot get a perfect match after several attempts, add the function name to `tools/match/difficult_functions` along with the number of attempts and best match percentage (function names should be separated by newlines). This should be in the form `<function name> <number of attempts to match> <best match percentage>\n`. By adding the function name to difficult_functions. You should also revert any changes you've made adding the function to the C file (we do not want to save incomplete matches).
4. You are done. Do not attemp to find the next closest match.

### Decompile assembly to C code

You may be given a function and asked to decompile it to C code.

First we need to spin up a decomp environment for the function, run:

```
./tools/match/claude --bootstrap-only <function name>
```

The script searches every overlay under `asm/<ver>/` (not just `engine`). Move to the directory it prints (`SCRATCH_DIR=…`). Read `BRIEF.md` there instead of re-exploring the repo.

Use the tools in this directory to match the function. You may need to make several attempts. Each attempt should be in a new file (base_1.c, base_2.c, ... base_n.c, etc). It's okay to give up if you're unable to match after _10_ attempts.

Once you have a matching function, update the C code to use it. The C code will be importing an assembly file via `INCLUDE_ASM("<overlay>/nonmatchings/<unit>", <function>)`. Replace this with the actual C code.

If the function is defined in a header file (located in include/), this will also need to be updated. These other usages may teach you about the correct type of your function arguments or return types. DO NOT JUST MAKE EVERYTHING void\*!.

Update the rest of the project to fix any build issues.

After adding your decompiled function, check for any redundant extern declarations:

1. **Search for existing declarations**: For each extern function you used, search the codebase to see if it's already declared in a header file:

   - Use `grep -r "void functionName" include/` to search headers
   - Use `grep -r "void functionName" src/*.h` to search source headers

2. **Remove redundant externs**: If a function is already declared in an included header file, remove your extern declaration to avoid duplication

3. **Verify the build still works** after removing redundant externs

Example: If you added `extern void setCallback(void *);` but `task_scheduler.h` (which is already included) declares it, remove your extern declaration.

**Post-success cleanup and learnings (always do these):**

When the permuter contributed a full or partial improvement, first follow the
bounded investigation in `tools/claude-decomp-env/MATCH_LOOP.md`. Preserve the
improved candidate, isolate its source change, and check a compiler prediction.
An unresolved mechanism is a valid recorded outcome. Before scratch cleanup,
run `python3 tools/match/archive_giveup.py --func <function> --scratch <scratch> --permuter-findings`
so sources, compiler inputs and observations survive successful-match cleanup.
Vacuum owns cleanup when it launched the session; leave its scratch in place.

1. **Clean up the scratch environment.** Delete the `nonmatchings/<function name>` directory (and the empty `nonmatchings/` parent if nothing else remains). Do not leave base_*.c attempts, object dumps, or symlinks in the tree after a successful match or after giving up.

2. **Record notable findings** in the project-root `DECOMPILATION_LEARNINGS.md` (this file is symlinked into each scratch env). Add an entry only when something is generalizable — a new GCC 2.8.1 codegen quirk, a matching trick that was not already documented, a scratch-env gotcha, a struct/layout insight, etc. Skip trivial one-shot rewrites that will not help the next function. Keep the same style as existing entries: short problem → symptom → fix, with a minimal code example when useful.

3. If you updated `DECOMPILATION_LEARNINGS.md`, include it in the same commit as the match (or a follow-up commit if the match was already committed).

**IMPORTANT - Verification Requirements:**

1. **NEVER declare success based only on local environment matching.** Matching in the nonmatchings directory does NOT guarantee the full project matches.

2. **ALWAYS verify the complete build** by running:

   ```
   ./tools/build-and-verify.sh
   ```

3. **SUCCESS CRITERIA**: The ONLY acceptable success condition is:

   ```
   build/USA/out/SLUS_010.42: OK
   ```

   If this check fails, the decompilation is NOT complete, even if individual functions appear to match.

4. **When modifying struct definitions:**

   - Search the entire codebase for other references to the same struct
   - Check if other functions access fields at nearby offsets
   - Verify ALL affected functions still match after struct changes
   - Example: If you add a field at offset 0x14, search for all functions accessing that struct and verify they still compile to the correct offsets

5. **If the checksum fails after your changes:**
   - Use `python3 tools/asm-differ/diff.py --no-pager <function>` to check ALL functions in the modified file(s)
   - Look for functions that access the same structs you modified
   - Fix any mismatches before declaring success

## Self-Review Checklist

Before declaring a decompilation complete, verify:

- [ ] No pointer arithmetic with manual offset calculations
- [ ] All struct field accesses use `->` or `.` operators
- [ ] No `void*` parameters that should be typed structs
- [ ] Struct sizes match the assembly access patterns
- [ ] New types/APIs live in the correct module header (main: not `unknown_syms.h`; overlays: their own headers)
- [ ] `./tools/build-and-verify.sh` succeeds

## Decompilation tips

### Assets

**Never commit game data.** `README.md` states the repository contains no ROMs,
disc images or copyrighted assets, and `.gitignore` enforces it: `rom/`,
`assets/`, `asm/`, `linkers/` and `build/` are all regenerated from the user's
own discs. Anything you move out of those trees and into `src/` or `include/`
is being committed, so that move is a licensing decision, not a formatting one.

Game content lives in files, not in the executable - in a project's generated layer
(`project/.layers/game-files/`): `pe2img` (textures), `pe2clut` (palettes), `pe2pkg` (room/actor overlays), `cap` (dialogue),
`audio`, `movie`, `bs`. See [`doc/ASSET_FORMATS.md`](doc/ASSET_FORMATS.md).

**Only assets have to stay out of `src/` and `include/`.** Everything else a
binary embeds - data, rodata, bss, tables, strings - may be declared in C. The
settled asset categories are **fonts** (glyph pixels *and* glyph metrics),
**images**, **CLUTs**, **models**, **animations** and **collision geometry**.
Collision geometry is a room's grid - normals, vertices, faces and per-cell face
lists - which describes a shape the way a model does. It is a `collision` asset
placed by its package's `embedded` list: the placement covers the arrays, and the
`WorldCollisionGrid` header that follows it is the room's data. The arrays keep
their C declarations but take their initializers from includes that
`tools/buildsteps/gen_collision_inc.py` writes from the package, after checking the grid's
layout against its header. Non-NULL table entries call the including source's
face-list binding with an element offset into its packed `s16` pool. The grid
asset's `cell_face_list_macro` selects that binding; unconfigured grids
use `GRID_CELL`. Each binding captures its grid's pool and yields a borrowed
list-start pointer, with one argument evaluation. Headerless geometry
that code copies into a live grid is a patch: a `collision` asset declared by its
`pieces`. Stages not yet converted still declare both in the overlay manifest
(`collision` and `collisionSource` objects, `collisionPatch`); they are moving to
the assets (`local/asset_side_without_splat.md`), and the build reads both meanwhile.
Camera views are not assets: a view is a matrix and a position, data like any
placement. Like placements, they are a **room's data** (below), not C.

**A room's data is its room asset, not C.** What a stage editor would have
authored - the per-state collision, view maps, view counts and lighting, the
warps, cameras, sprite views, triggers, occluders, lights and ambient levels,
surfaces, placements and actor resources, area updates, vibration scripts, and
the world points (`markers`), view masks and sample curves effect code reads -
is decoded from the binary into the extraction's `<room group>/room.toml` (the
build's in `assets/USA`, the project's in its generated layer) and
written back to C by the build, one include per module
(`assets/<group>/<module>.inc`, plus `declarations.inc`;
`tools/buildsteps/gen_room_inc.py`). The room package's record has
`type = "room"` and states only the entry points the stage overlay's per-area
tables and gameplay's area table do not give, by role (`entries`;
`tools/peassets/room/`); every other object is reached through pointers, and
every object's type follows from its role or the field that reaches it. A
room's C keeps its code, its code tables, its one-off globals and the tables
its code reads as tuning parameters (item ids, radii, a mosaic's tiles), and
includes the module files where their blocks sit. Never hand-write room data
back into C; extend the room's entries or the room layouts. After changing a
room's entries or layouts, `python3 tools/peassets/room decode`, then
`python3 tools/peassets/room sync-c` puts each module include before the next
datum that stays (in address order; idempotent), and after the build
`python3 tools/peassets/room check-order` names the first object a misplaced
include put out of order - before the checksum only says the room differs. Converted so far:
stage 1 (Akropolis and the MIST). A room's event scripts and the payloads they
send (animation requests, actor transforms and commands, bank copies, scene keys)
are room data too, written as text in `<room group>/events.evs`
(`tools/peassets/room/events.py`: one statement per command, payloads by name,
message recipients and ids named from `event_vocabulary.py`); they build into
the `events` module include like any other module.

**Morph targets and baked tracks are assets too.** A morph target is the second
shape of a mesh, vertex for vertex (`dryfield_toilet`'s deltas and normals). A
baked track is authored or exported content played back by index, hundreds of
samples describing movement over time: a per-movie-frame player path
(`acropolis_observatory`), a scripted prop's motion curve
(`mine_forked_tunnel`), a slider path, and a camera moving frame by frame
(`actor_303600`'s 570 view keys). That last one is the distinction from the
sentence above: a room's fixed views, selected by id, are data; a per-frame
camera track is an asset. What stays in C is what a programmer would type and
tune by hand - a small offset table, a tuning constant, a table of rules such
as `actor_548100`'s circuit graph (a room's placements are its room data,
above). Classify by how the code uses
the array, not by its shape. The known embedded ones:

| Asset | Where | Note |
|---|---|---|
| Memory-card save header | `_gMcFileHeader` (main `.data`) | `"SC"` magic, Shift-JIS title, 16-colour CLUT, three 16x16 4bpp icon frames |
| UI font glyph metrics | `_gFontGlyphsMedium`, `_gFontGlyphsLarge`, `_gFontGlyphsSmall` (main `.data`) | 224/224/91 x `_FontGlyph`; pixels come from a CLUT image, see ASSET_FORMATS 7.6 |
| Meshes and animation banks | gameplay `.data` trailing region and room/actor `.pe2pkg` overlays | no separate chunk type; see [`doc/OVERLAYS.md`](doc/OVERLAYS.md) |

**Undecided - look at each case before moving it:** clip tables, and dialogue
and script text (the `.asciz` pools in gameplay `.rodata`). For these the
choice is not only C versus assembly: data that can exist in several versions,
such as text in several translations, may be better produced by a build step
from an extracted source than hard-coded in C.

**The item catalog is decided: an asset.** Each item's and key item's name
and description fields (`_gItem<Item>TextFields`) belong to the item's group
as `catalog`, a `txt` located in `gameplay.pe2pkg` by address. So are the
descriptor tables, `gItemDescriptors` and `gItemKeyDescriptors`: each item's
price, classification and flags are fields of its instance, and
`inventory/catalog` lists the items by id and writes, as one include
(`assets/inventory/catalog.inc`, `gen_shape_inc.py`), every catalog text and
both tables, which `src/gameplay/item_catalog.c` includes whole. See
`doc/ASSET_FORMATS.md` 7.7.

**Structured data is a shaped asset.** A shape (`tools/peassets/shape/`)
declares fields and the stored places they are written to, with a codec that
round-trips them exactly; the group's instance (`<group>/<name>.toml`) is the
edited form. Give data with structure a shape rather than a text format of its
own; the item family is the worked example.

**Not assets, so eligible for C:** program tables such as the balance
tables (`gAttachmentLevelTable`), function-pointer dispatch tables
(`_gStageModeControllerStates`, `_gMcLoadDialogStates`, `gItemMenuCaptionStates`), index
tables such as `gWorldCollisionFaceEdges`, constants, UI strings and zero-initialised
state. Declaring one in C means placing it in the unit that owns it, at the
position and alignment the original had - the same care as a `rodata` cut
(see "Generated overlay configs" above) - so move objects one at a time and
check the build after each.

An asset embedded in a binary is handled twice over, and neither route puts it
in git. For the **build**, a splat `databin` / `rodatabin` segment writes the
bytes to `asm/USA/incbin/` (per worktree, like the rest of `asm/`) and emits a
small `.s` that `.incbin`s them back, so the build keeps matching. Where the
asset sits inside a unit whose data is in C, the unit defines it itself: mark
its `EMBEDDED_ASSETS` record `"include": True`, and the build turns the
extracted file into `build/include/assets/<id>.inc` (`tools/buildsteps/gen_asset_inc.py`),
which the definition includes as its initializer (`_gFontGlyphsMedium` in
`src/engine/textdraw.c`). For **inspection**, catalog it by address in
`asset_db.EMBEDDED_ASSETS` and it flows through the normal extract pipeline
into `raw/{type}/` and the type directory, like any on-disc asset. See
`doc/ASSET_FORMATS.md` 7.7.

There is no `USE_ASSET` macro and no `dmaRequestAndUpdateStateWithSize` in
this project; earlier revisions of this file described a scheme from a
different decomp. Ignore any reference to them.
