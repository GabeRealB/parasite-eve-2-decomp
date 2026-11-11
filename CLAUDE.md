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
(`.layers/game`, written by `tools/editor_project/` as a tree of files that
`pe2-cli project layer import` makes the layer's records) and the plan filling
them with their files (`.layers/game-files.plan.jsonl`, stating the discs it
reads by size and sha1), the plan run by `pe2-cli project extract` into
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

**A project is served as a filesystem: its merged layers, one tree.** Only the
index knows how layers resolve, so other programs read the project through it
(`editor/crates/vfs`), named as the asset browser names it: every node a folder
of its name - no `.node` - holding its files and the entries inside it, every
folder and plain file where it resolves. A node and the folder of its name
beside it in a layer (`x.node`, `x`) are one: a node with what is inside it,
renamed, moved and deleted together (`edit::paired`), which is how a package
holds its packages and a stage-0 slot its actors. It is always at the project's
`assets/` folder, which its `.gitignore` leaves out.

**What serves a tree is the backend suited to the job** (`vfs::backend`). A
backend says what it can do (`Can`: `write`, all of what programs change applied
to the project; `folder`, served where programs open it by path) and whether it
is available at a place; a job says what it needs - the project's folder is
written through, a scope read by path - and the first of the system's backends,
best first, that does all of it and is available there serves it. So a backend
is only ever given a job it does whole. FUSE on Linux and macOS (`fuse`; under
WSL, Windows programs open it through WSL's file server, which reads as root:
only where `/etc/fuse.conf` has the line `user_allow_other`, which `serve` and
the editor say when it lacks; the kernel keeps what the tree holds - entries,
attributes, listings, content, and that a name asked for is not there - for
as long as nothing changes it, each change
beneath the mount told to it (`fuse::tell_kernel`), and asks again after a
second only of what is staged over). On Windows, in this order: Dokany (`dokan`,
where the user has installed it - a real filesystem mounted at the folder,
written through as FUSE is, nothing kept on disk; its library loaded when used,
its declarations transcribed from Dokany's header and attributed in the file;
names matched whatever their case through a per-folder index kept while the
tree stands, so a name that is not there costs no listing),
the Projected File System (`projfs`, where the feature is turned on, NTFS or
ReFS: read-only, since Windows refuses to rename or move a folder it projects
whatever its provider does - so scopes, never the project's folder) and the
Cloud Files API (`cloud`, what OneDrive uses - nothing to install - the folder
a sync root of placeholders filled as they are read, on an NTFS volume: registered
by the folder's name and its place, so two projects in folders of one name are
two; a path its watcher says changed that is no longer there was removed, as
Windows tells only of a placeholder's removal; bringing the folder in step with
the tree, its worker leaves alone what a program changed there and the project
has yet to be told - a file or folder renamed keeps its in-sync state, and was
taken for something the tree no longer has: removed, and its old name put back -
and removes of a folder the tree no longer has only what is in sync), and the same API with what it
serves kept apart (`cloud-cache`: the sync root a folder of its own where the
machine keeps such things - `[filesystem] cache`, by default the user's local
data, on NTFS - and the project's folder a junction to it, so a project on a
volume the API cannot serve, ReFS, is served by it all the same; a junction of
ours that leads to another project's tree, as a copied project's does, is made
again, and a tree nothing leads to any more is swept). WebDAV everywhere (`webdav`), at an address
for the system's own client to mount, so never a scope. `project.local.toml`
says otherwise (`[filesystem] serve` for the folder, `scopes` for the files
programs are given: `auto`, `none` or a backend's name; `address` for WebDAV),
then the machine (`[filesystem]` of the editor's `settings.toml`, Settings ▸
Files); a backend named that cannot do the job, or is not available, is
refused with why, never replaced by another. `pe2-cli project backends` says
what holds here.

**Among the backends able to do a job, `auto` takes the one suited to how the
tree will be used.** Whoever asks says it (`pe2_ext::Use`: each file read
`often` rather than once, in `ranges` rather than whole - `Use::general()` for
a project's folder, `Use::once()` for a compiler's sources; the contract's
`serve-use`, the second argument of `asset.serve`), and a backend says two
things of itself: whether it `keeps` on disk what was read (ProjFS, Cloud
Files) and whether it `prepares` something for every file up front (Cloud
Files' placeholders). Read often, a keeper serves; read once, or in ranges of
large files, one that prepares and keeps nothing (FUSE, Dokany). A program's
files are first of all laid out as copies - what a layer holds as one file
linked - where that writes no more than `[filesystem] copies` (16 MiB): the
fastest in every measurement, and what a build's trees get. What a keeper
holds of a project's folder is bounded by `[filesystem] kept` (2 GiB): the server
remembers what is read there (`vfs::access`, `.cache/access/`) and, at start,
every ten minutes and as it stops, has the backend let go of what was read
longest ago (`Backend::collect`: Cloud Files makes it a placeholder again, only
what is still as serving left it). A placeholder's state is asked through a
handle opened for its attributes alone (`cloud::state`): opened as the Cloud
Files API opens one, Windows fetches back in, whole, a file that was let go of. Filling files ahead of their first read was
tried and measured, and is not done: it costs what the read does. A backend is a module implementing `Backend` and a line of
`backend::all`: nothing else names one, and what it needs told (`Mount::placed`,
`released`) or cleared after it (`Backend::clear`) is its own. Before one serves
a folder, every other retires what it left there (`backend::hand_over`,
`Backend::retire`): Cloud Files' placeholders still in sync go and its sync root
is unregistered - each file opened to be asked, some tens of seconds for a
project listed whole, once - a junction Dokany left is a plain folder again; what
was changed or put there while nothing served it stays, as strays. Dokany pulls
a read-only mount's requests on one thread: its library parks one a processor
(16 at most) for each mount in a pool they share, and a process's later mount
then waits about two seconds for the pool to grow (`dokan::mount` waits for the
folder to answer); a mount written through keeps them all.

**Two processes: a server that serves the folder, and whoever holds the project.**
The server (`pe2-cli project server`, `project::server`, `cli/src/server.rs`) is
the virtual filesystem and nothing else: it reads no index and loads no extension
(`project::ext::forbid`, said at its start: loading one fails by name), so it may
load what a process running an extension's code must not be one with. Whoever
holds the project - an editor, or a host (`pe2-cli project host`,
`project::host`, `cli/src/host.rs`) - has its only index and says what is served
(`Index::served`: the whole stack, whatever its view hides) in the filesystem
the project's processes share (`project::shared`, below), on a thread of its
own (`holder::files::Files`), and tells the server only that it did
(`ToServer::Shown`). The server has the shared space attached for as long as
it serves, and reads there: the first time all of it (`shared::listed`), after
that what the commits since it last read did (`shared::changed`, of
`sfs::fs::changes`: the paths shown anew or otherwise and those gone -
`server::Delta`; all of it again where those changes are no longer kept), and
changes the tree it serves by those paths (`Server::follow` in `cli/src/server.rs`,
`Served::change`). Each file's node says where its content lives
(`server::Source`: where a layer holds it, the record holding its text, the
store keeping it by SHA-1), so the server reads no records: a read finds its
file by where a layer holds it among those it was told live elsewhere
(`content::serve_whole`, `serve_changed`) and reads there - a record's text from
that record's own file. Its packs' listings it reads again where the holder
says files changed (`ToServer::Changed`, sent before it is told what is served)
or its own watch sees it. No table is written and no listing sent: a holder
that takes the project over says what differs from what the space holds, and
the server follows that as any change. A refresh
says which entries of the whole stack it touched and which are gone
(`Refreshed::touched`, `gone`: where the stack was applied again, those that
differ from before, and the node an entry came or went in), and the holder
brings the listing it last said up to that and says what differs
(`Index::serve_changed`): no listing of the whole project is made for an edit -
every edit the project crate's tests make is held to the two being the same.
What tells others that the tree changed - the kernel under FUSE, what watches a
Dokany mount, the Cloud Files worker - is woken by the change
(`Served::on_change`), where each looked every 200 ms. The records are read
again by change too (`records::Set::open_after`): a layer none of the changed
files is a record of, with nothing changed below it, is taken as it was read.
Measured, a project of 23,000 entries and 8,800 records, the folder served
through FUSE: a command changing a file 38 ms (it was 235-370 ms), in the folder
0.4 ms after it returns (up to 200 ms); one adding or removing a file 118 ms
(380 ms) - of which 60 ms is the stack applied again, whole (resolving it,
its dependencies, its entries), 27 ms the edit made, 9 ms the records; where no
asset changed, the links between assets stand (`Index::after_restack`), and what
lies in what was rescanned is told by layer and path, no path made for each
entry (`in_rescanned`); all 14,365 files
read the same through it as through one process with an index.

**An edit costs the holder what it changes.** Where one layer's entries came,
went or moved and nothing else changed, the stack is not applied again: it is
changed for those entries (`Index::restack_entries` - the layer a records layer
nothing fills, no layer above it creating or doing anything, none of the stack's
problems its own or at those paths, each entry gone the layer's own as it made
it, each entry come at a path nothing resolves to under a uid no layer lists;
anything else, and the stack is applied whole as before). The edit asks the
layers below entry by entry (`Index::under`) where it copied all they resolve to
place one record, and what they resolve is kept while they stand. A reading of
the records after an edit reads the records named, where it listed every folder
of the layer's and looked at every record (`records::read_changed`), and places
a layer's entries through the layers below by their own maps, none copied
(`records::Below`); a tree layer's changeset is read again when its file
changed. The index's store is written every two seconds, or as it is let go of,
where it was a transaction an edit (`store::Store::write_later`: what it keeps is
what the next opening is spared reading). Every edit the tests make holds the
index changed in place to one opened afresh - entries, assets, links, problems,
what each layer needs of another (`Index::shown_whole`). Measured in the holder,
the same project, eight edits of a kind, on a machine doing other work: a file
added 2.4 ms where it took 243 ms a moment before under the same load (82 ms on
a quiet machine); removed 3.6 ms; moved 7 ms; overwritten 3.5 ms. An asset come
or gone is linked by itself where nothing names its uid and nothing refers to
one that goes (`Index::named`: how many references name each uid), and what the
layers need of each other counted for it alone (`Index::recount`); an entry is
found by its uid at once (`Index::real_path`); the store is written by the host
between edits (`Index::tick`). With those, each kind of edit - a file or a node
added, removed, moved, overwritten - is 1-1.5 ms to make and 0.2-1 ms to take
in, on the same machine; of the 1-1.5 ms, 0.75-1.3 is saying the change in the
log, having that on disk and writing the files. The editor's session is
appended to (`History::journal_last`: the steps from the last its file held on,
with the documents they edited), written whole only where it changed otherwise
or its file grew long. A record, and a changeset, are written as they are
whatever their length (`Transaction::write_stored`): one of 1 MiB or more - a
long text is kept in its record - was kept as chunks, as a layer's long files
are, and then not read as a record at all.
It serves from the first time it is told what is served -
until then the folder is as it was, so a backend never mounts an empty tree - and
the connection that told it feeds it until it ends: one feeder at a time, a
second told who feeds (`FromServer::Fed`), which whoever takes the project over
asks again a moment later. It does not serve over files in the folder that are
not the project's (`vfs::strays`: anything on Linux and macOS, where a mount
would hide it; on Windows a plain file, or one read on an earlier run and changed
since): it says so and ends, and the user sets them aside (`serve --set-aside`,
to `assets.set-aside/<time>`) or takes them in as edits (`--take-in`) - the
editor offers both. It stops 30 seconds after nobody keeps it (`Keep`, a lease
while the connection lasts), once no file is open in it: a read in progress is
never cut off, and what only looks at the folder (a watcher, a file browser)
keeps nothing.

**A host holds the project while no editor does**: its lock, its index and a
history of its own, answering what is asked on the holder's link - the edits
commands and agents make and programs write through the folder, the reads (from
its index: the extensions and imports loaded at the first, let go of after two
minutes without one, so `pe2-cli mcp` never opens a project a host holds), what
events set off. Where nothing holds the project, what a command or an agent asks
starts one (`project::host::ensure`, `mcp::server::execute`) that serves its
folder nowhere (`--backend none`), answers this and what is asked next with the
project open, and keeps one history for the commands' edits - so `undo` takes
back what an earlier command did. `pe2-cli project serve` (`--backend`,
`--address` in place of the setting; `--stop` stops host and server) starts one
that keeps the folder served: it starts the server, keeps it for as long as it
runs and feeds it while it holds the project, and is listened to once the server
serves - so a server that cannot (no backend here, strays) is said by the host
ending, as its log has it. A host joins a server that runs already, whatever it
was started for. It leaves only while it holds the project, two minutes after
the last of: something asked on its link, a lease on it ended (`project serve`,
a borrower), a file open in what the server serves; `[filesystem] keep = true`
keeps one that serves until stopped, and one started to keep the folder served
ends where the server is stopped. The server outlives it by its 30 seconds. A
process taking the project lock while a host holds it borrows it
(`Lock::try_acquire`): the host lets go of the lock, its history, its index and
its link, feeds the server no more - whoever holds the project does - and takes
the project back once let go, feeding again - the editor, an extraction, a
Python tool (`tools/project_lock.py`) alike. The editor keeps and feeds the
server itself (`Files::keep`), and has a host stand by where its files are
served (`project::host::stand_by`: the one running, or one started holding
nothing): closing the window ends no serving - the host takes the project and
feeds the same server. Who lets go of the project says so with a connection
(`ToHost::Leaving`, as a borrower's `Lend`), and the host takes it the moment
that ends, not when it next looks (every 200 ms, what is left for a holder that
died); its log says how long each part took. The editor lets go of the lock
once what may still write has ended and its index's store is closed
(`Leaving`'s drop), and frees the index's memory after: 0.13-0.17 s after it is
closed, where freeing it first made that 0.5 s and more. The host holds about
1 s after that - opening the index, which is what a hand-over costs. What a program writes through the folder the
server sends to whoever holds the project, on the holder's link
(`mcp::Request::Edit`, `Edits`); where nothing holds it, a host is started to
(`mcp::server::ask_holder`), and where what holds it takes no edits (a tool, an
extraction, an editor still opening) the write waits for whoever holds it next
(below) - as it does where no host could be started that moment. A holder that
is stopping may take one last connection and close it with no answer: whoever
asked asks again a short while, and goes on as where nothing listens once
nothing does (`mcp::link::until_answered`), where that was an error - to a
program's write, a refusal for good. A program working
under another's lock (`PE2_PROJECT_LOCKED`) starts no host for what it asks, and
does the work itself; a host started under one (Python's `data_root`, which
serves `assets/` for its run where nothing serves a project whose layers hold
chunks) ends with its starter's lease, and its server with it. Asked to serve
the folder, a host that only holds is told to stop and one that serves started
in its place (`project::host::serving`). What both processes are to others - how
one is known to run, started on its own, connected to and listened as - is
`project::role` (`host::ROLE`, `server::ROLE`: `.cache/host.sock` and
`server.sock`, named pipes on Windows, one a project by `host.lock` and
`server.lock`, their logs beside them).

**Who answers is asked first.** Whoever asks first asks who answers
(`mcp::Request::Hello`, `mcp::Holder`: an editor or a host, its program as built
when it started, its data folder, whether it trusts every extension): a host
that is not what the asker would have started is told to stop where nobody keeps
it, and the asker's own answers; one that is kept, or an editor, is only said to
differ. What programs wrote at once is sent to the holder as one request
(`Request::Edits`, waited for as long as the link lives - an edit given up on
while it is made would be made twice), and the editor takes all of it in once
(`Workspace::apply_edits`); a folder the project was just given is shown until
the tree served follows (`Writes`' `given`), so what is written into it next
finds it. A host waits for the server to serve what an edit changed before it
answers (`Files::show_changed`), so a command's edit is in the folder once the
command returns. The agents' server lists the extensions' tools as the holder
has them (`Request::Tools`), those the project holds among them. What a holder
does besides its own work is one library with no window in it (`crates/holder`:
`files`, the feed to the server; `runner`, the epochs events set off; `watch`,
its layers watched), which the editor and the host both use: what is to be woken
when something ends is given as a call. A host runs what events set off as an
editor does (`Request::Signal`: `pe2-cli project build|clean|signal` ask whoever
holds the project, a host started for it where nothing does), off its own lock,
so it reads and edits on meanwhile; it fetches what the project's extensions
require first, and lends the project to nobody while an epoch runs - who asks
waits, as for any holder that is busy. On Windows a host and a server have a
console of their own with no window (`CREATE_NO_WINDOW`, never
`DETACHED_PROCESS`), and an extension's tool is started with none either: a
process with no console at all has Windows open a terminal for every console
program it starts, a build's hundreds of them. Processes say all this to each
other as CBOR, one message a frame (its length, then the message:
`project::wire`), the host's and the server's sockets and the holder's link
alike.

**What a program writes in the folder is an edit** made by whoever holds the
project, a step of that one's history in the highest writable layer (the active
one, in the editor): a file written is the project's once closed - given off
the program's thread, the close returning at once - or synced, which waits for
it; a new one once nothing has it open and its burst of changes has ended
(`writes::Pace`: the pause since its last change eight times the time between
its changes, 10 to 250 ms, where a fixed second was waited); a removal a quarter
of a second later, unless a file is made there meanwhile; a folder made is shown
at once and given in the order folders were made, before anything else is asked
(`Writes::give_queued`); a rename of what the project has is shown at once -
moved in the tree the server serves, with what is in it, each file keeping its
number (`Served::moved`, `Tree::rename`) - and the project told in its turn, in
that same order, what is written there and not the project's yet going with it
(`State::moved`), a rename the project refuses put back where it was; while
nobody takes what was asked in an order, nothing asked after it is given. A save
by rename is a save, and what editors
leave beside a file (`~`, `.swp`, `4913`, any name starting with `.`) is never
applied (`vfs::write`). One thread gives the project what is due, asleep until
the next is and woken where something is due before (`Writes::due`), or where
the tree served changed. A tree changed is a copy of the one before whose nodes
are shared with it (`Tree`'s `Arc<Node>`), so a change costs a count a node, not
a copy of each. Measured through FUSE on the project of 23,000 entries: a folder
made 0.09 ms where it took 100-310 ms; a rename of what the project has 1.7 ms
where it took 110-400 ms (the tree copied and the numbers moved: what is left to
a tree changed in place); a save - opened anew, written, closed - 0.4 ms, FUSE's
own calls, where its close waited for whoever holds the project, seconds while
that one was busy; a new file the project's 38 ms after its close where it took
1.1-1.3 s; a sync 1.2 ms - the log had on disk - where it waited for the holder
(7 ms). A file opened to be written anew is truncated by the kernel with
its handle not said: that is no edit of its own while a program has it open -
applied as one, every save was two steps of the history, the first an empty
file, and the program's open waited for it. What is due together is handed over together
(`Owner::changes`): the holder makes each its step and refreshes its index and
what is served once for all of them, so hundreds of files written or removed
land in seconds; a removal or rename waits for what is being applied to the same
path (`Writes::applied`), a moment. As serving ends, what still waits to settle is given
to the project (`backend::Finishing`, `Writes::finish`; the Cloud Files worker's
last pass), so a change made a moment before a stop is not lost. A change is
refused for good or only for now (`write::Refusal`: `Never`, `Later` - nobody
takes edits this moment): what was written and what was removed then stay as
the program left them, shown so, and are asked again every second until someone
takes them (`Writes`' `waiting`; the Cloud Files worker gives nothing while
`Owner::ready` says nobody would take it); a rename and a folder made are shown at once
and kept the same way, in their order. What nobody took by the time serving ends is set aside beside the folder
by its path (`Owner::left`, `assets.set-aside/<time>`), since what was
written is let go of when the folder is next served; under the Cloud Files API it stays a
file in the folder, a stray of the next serving. A file the project has, renamed
while content of it is staged, is renamed in the project and its content
follows; renamed onto another the project has, that one takes the content and
the first is removed - as is any file the project has that is moved onto a
file that is there, the project's or written and not the project's yet: its
content staged first where none of it is (`Writes::rename`), since a rename
onto an entry is one the project refuses. The filesystem
holds to the model: a `node.toml` written in a folder makes it a node (a folder
there already is its folder of children); a new file in a node is one of its
files, and a folder made in one waits to be a child node or a folder of its
files by what is written in it; a node's `node.toml` is not removed alone, its
folder is; a name ending in `.node` is refused.

**The filesystem a project's processes share** (`crates/sfs`) is being built
under what serves the folder: one library every process links, its state shared
by mappings of files in the cache (`.cache/filesystem/sfs`). So far its log (`sfs::log`): a
write-ahead log of records with their bytes, in segment files of 8 MiB, a
position in it the sequence number of everything. Any process appends, one at a
time - a lock of its threads and one the system holds on the header's file, let
go of where its holder dies - with a write call, the tail in the shared header
(`sfs::Header`: the tail, how far the project has the log, how far that is on
disk, who holds the project - atomics every process reads with no call) moved
past a record once it is whole: no reader sees part of one, a full disk is an
error to who wrote. Read from mappings. A record says its own position, so what
a segment held before is never taken for one, and where the log ends is found by
the first process to open it, checking records, never taken from the header.
Measured: an append of 64 bytes 1.8 µs on Linux and 6.4 µs on Windows, of 4 KiB
4.9 and 8.9 µs. What programs write through the folder is kept in it
(`sfs::Content`: a file in transit is the ranges of the log its bytes are in,
read where the log is mapped; what an editor leaves beside a file, never the
project's, is kept in memory instead), with no file of its own. The server says
a written file to whoever holds the project by those ranges
(`vfs::write::Written::Logged`), and the holder reads it from the same log,
mapped: nothing is written out for it. The log is kept from the first position
anything still reads from (`Writes::let_go_of_log`) - a file's content, what
the holder was given and is reading - and a file that waits long while much is
written after it is moved forward, so it alone keeps none of that. The header
says the size of the segments, so every process reads them alike. Measured, the
library alone: a write of 4 KiB 5.3 µs, a read of 4 KiB of what was written
2.0 µs, a new file made, written and closed 5.5 µs; through FUSE each call
costs its round trip, 50-60 µs here, whatever is behind it.

**What is done through the folder is in the log as it was done, and taken up
from it.** Beside a file's bytes the log holds each operation in the order it
was made (`sfs::Op`: a file `Began` at a path - the record's position its number,
which its data records say - `Truncated`, a path `Removed`, `MadeFolder`,
`Renamed`) and what became of it (`Taken`: the project has a file as it was when
the log ended at a position; `Done` and `Refused`, of a folder, a rename, a
removal; `Ended`: serving ended with nothing in transit). The server says each
under one lock with doing it (`Writes::ordered`), so the log has them as they
happened. A serving that starts takes up what the one before left
(`Writes::replay`) - its process gone, the power lost: each thing is done again
in the order it was, with nothing appended for it, what the project took by then
known as taken, and all that is still to be given due at once. So a sync is the
log had on disk (`Log::flush`), not a wait for whoever holds the project: synced,
a file is the project's whatever becomes of the server. The log is kept from
where everything still in transit is found again: each file from where it began,
what is still to be told from its record. A file begun again further on - moved
forward - stands as the one before it until the move is whole. What an editor
leaves beside a file is in no record, and gone with the process; a folder made
in a node and still waiting for what is written in it is found again only by
that. Measured: the server killed 2 ms after five files were synced, their
folder renamed and another made, the project having the rename and nothing else
- served again, the folder and the five files are the project's, where the
rename led.

**What whoever holds the project writes to its files goes through the same
log** (`project::storage`, `sfs::storage`). A transaction of the history gathers
its changes and touches no file (`history::Transaction`): what it asks of the
files meanwhile - whether one is there, what it holds - is answered as its
changes so far leave them (`Transaction::seen`, `read`, `pending`; an edit on a
tree layer asks through `Out::exists`, one on a records layer gathered its
writes already). Committed, the whole set is said in the log - a file's new
bytes, a file gone, a path moved, a folder made or removed, then that it is
whole - and the log had on disk, once (`Storage::say`); only then are the files
written, with no wait for the disk. Undo and redo are made the same way. So a
set that never came to be whole touched nothing, and one that is whole is
written again from the log by whoever takes the project next, before anything
reads its files (`storage::redo`, at `Cache::open`): a process that died
between, a loss of power. The transaction's crash record of before
(`.cache/history/applying`, rolled back at the next opening) is gone, as is
rolling back what was dropped uncommitted. What a set names in the store - a
large file's chunks, a record's binary file - is had on disk before the set is
said (`chunks::sync_written`), and a tree copied into place before its move is;
the log holds no bytes of those. Every few seconds, and as the holder lets go,
what was written is had on disk and the log needed for it no longer - by a
thread of the storage's own (`storage::Shared::run`), so that neither a change
being made nor a frame being drawn waits for a disk; a set whose files are being
written while it does keeps the log needed from that set on. Having the log on
disk for a set costs 0.4-1.1 ms on the solid-state drives here and 4.3 ms on the
hard drive (`bench_append`), of an edit's 30-100 ms: taking it off the path too
means every read of a layer's files going through what is still to be written,
which is not built.
Each side says in the header from where it needs the log - `transit`, what a
serving takes up; `storage`, sets not on disk yet - had on disk itself where it
goes from nothing to something, and either lets go of what neither needs
(`Log::keep`), so a project nobody serves does not keep its holder's log.
Measured: an edit by command costs what it did (a file added 0.08-0.10 s, one
overwritten 0.03-0.05 s: the flush is 1 ms of it); the host killed while it took
300 files written through the folder - the server starts another, all 300 are
the project's as written, the one it was given twice changed nothing. An edit
that is asked twice across such a death is the price of the server and the
holder each saying its own part: the holder's set whole comes before the
server's word that it was taken.

**Beside the log, the library has pages every process maps, and trees in them**
(`sfs::space`, `sfs::tree`): what the served tree and the layers' records are to
be kept in - the first is, as the server reads it there. Chunk files of 4 MiB in the cache, each
mapped by itself - nothing grows in place - made whole under a name of its own
before it is a chunk, half a chunk ahead of need; pages of 4 KiB by number. One
writer at a time, by a lock the system holds on a file and lets go of where its
holder dies. It changes no page a reader can reach: it copies it (`Txn::own`),
and a commit - the trees' roots, the pages in use, how far the log is
reflected - is written to one of two pages in turn and then published in a
header page under a counter, so a writer that died publishing is put right from
the page it wrote by the next. Readers take no lock: a reader pins the
generation it reads at in a slot its process holds by a lock on a file of its
own - pinned, then checked to be the current one still - and a freed page,
listed in a file with the generation that freed it in an order no death leaves
a page listed twice by, is taken again only when no pin is older; the pin of a
process that is gone is cleared. A pointer to a page says its number and the
generation that wrote it, and the page says both and its kind, so whoever
follows one finds the very page that was pointed to or marks the space damaged
for every process, with what was found (`Pages::page`; a cell not within its
page the same; the files kept aside in `damaged/` by the next to attach alone).
Attached to beside another process, a space is as its last commit left it;
alone, it is trusted only where `Space::sync` had it on disk at that generation
(`clean`, taken back on disk before the first change after) and begins empty
otherwise (`Space::fresh`), for whoever owns what it held to fill again:
nothing in it is the only copy of anything. Windows writes a mapping's changed
pages for the process that changed them alone, so each says whether it wrote
since it flushed (`Space::flush`), and `sync` says clean only where none has.
A tree (`tree::get`, `put`, `remove`, `scan`) is keys in order with their
values, a long value in pages of its own; a page the transaction owns is
changed in place; a page keys were taken from is not joined with the one
beside it. `Space::private` is one in a process's own memory. Their tests are a
harness (`space/tests.rs`): a tree against a map kept beside it, readers held
across commits reading what they began with; two processes writing and reading
one space; a writer stopped at each step of a transaction (`Space::fault`: the
space is as its last commit left it, and the next writer goes on); a reader
stopped before and after its pin; a reader killed while pinned; a page that is
not the one pointed to; a disk that kept any pages of what was last written and
says it is clean - read right or said damaged, never anything else. Measured,
100,000 names (Linux; Windows): a name looked up 0.4 µs (0.6); a folder of 40
listed 0.8-1.2 µs (0.8); a pin taken and let go 45 ns; one put and its
commit 1.5 µs in memory and 2.8 in files (2.1 and 5.9); a hundred puts a commit
0.5 µs each (0.7-1.2). Over the trees, a filesystem that knows nothing of
projects (`sfs::fs`): `names`, a folder's number and a name to a number - a
lookup one descent, a listing one range, a folder renamed one key whatever is
inside it - and the same by the name folded, for systems that ignore case;
`nodes`, a number to a folder or a file with its length, time, version, flags
its consumers give meaning to, where it is, and where its bytes are (in the
node, a file on disk, ranges of the log, or what its consumer derives them
from), numbers given once and never again; `changes`, what each commit made,
changed, moved and took away and who did it, kept to a length - who is behind
what it holds is told to take the tree whole. Several share a space, each in
trees of its own (`fs::SERVED`, `fs::STORED`). Whoever holds the project says
what it serves there (`project::shared`, the space in
`.cache/filesystem/space`), for the server to read: all of it where the space
holds none (`show_whole`, by what differs from what it holds), and after an edit
what the edit changed (`show`, of `Index::serve_changed`). A node is a folder that says so, a folder a node's
files made says that and goes with the last of what is in it, a file says
where its content lives as the listing does; a file's time never goes back
and its version goes up with each change. Every edit the project crate's tests
make is held to it: what the shared filesystem lists after the change is what
is served listed whole, and so is what a follower of its changes has
(`shared::changed`), as the server is one. Measured on the project of 23,000 entries: all of it
said 160 ms, said again with nothing differing 60-80 ms; a file changed
7-10 µs, one added or taken away 11-13 µs, each a commit.

**What was read is kept in a ring of pages the processes share** (`sfs::ring`),
which nothing reads through yet: slots of 4 KiB, each one page of one version of
one file by its number (`ring::Key`), for bytes that were put together - a
record's text, a large file out of its chunks, a file in transit out of the
log - and the only copy of nothing. No lock: a slot's counter is odd while it is
written; a reader checks it before and after copying and takes any doubt for a
miss, a writer claims a slot by one exchange and gives up where another has it. A
key is in one of four slots its hash names, put in the one used longest ago; a
version no longer read is never found again. A process that died writing leaves
its slot a miss until the ring is cleared by whoever has the space alone. Its
file belongs where the system keeps memory processes share, not on a disk: as
long as the ring from the start, taking memory only for the slots written.
Measured, 256 MiB (Linux; Windows), against the same reads asked of the system
of a file it has in memory: the same few pages again and again 25 ns each (20-25)
against 555 ns (1100); pages all over it 1.3 µs (1.1-1.4) against 2.3 µs (5.7); a
miss 20 ns; a page put 1.4-2.9 µs.

**The command line edits a project by path** (`vfs::ops::Op`): `pe2-cli project
write|mkdir|rm|mv|override|drop|keep-mine|take-base|forget|adopt|remove-unlisted|undo|redo`
(`--layer`, by default the one edits go to), paths as the served folder shows
them, and `problems`. An edit goes to whoever holds the project - the editor, a
step of its history and its undo; the host, started for it where nothing holds
the project - and is made under the lock as a step of its own only where no
host can be (a program under another's lock: undo then has nothing to take
back).

**A layer is stored as records, not as a tree of files** (`project::records`),
since nobody edits it by hand: one record an entry it creates or acts on, by uid,
in `.layers/<layer>/.records/<aa>/<uid>.toml` - what it does to the entry
(creates, overrides, moves, deletes), where it is (its parent's uid and its
name, so renaming a folder of thousands of entries is one record), and its
files: text inline whatever its length, so a layer's history always reads as
text in version control (diffs are computed, never stored), anything else - not
UTF-8, or holding a NUL - by SHA-1 in the store. Each layer's folder says its
format (`.records/layer.toml`, with what its changeset said: read only, ignored
names), and every record its own: a format a program does not know is refused,
never read or written - no readers of old formats are kept before the first
release, `pe2-cli` converts one way (`project store convert --layer`, kept only
where the project then shows the same; `project layer import` from a tree,
`export` to one). What reads a layer sees its files at the paths a tree would
have them (`records::Set`, placing entries through the layers below), read
through `project::content` as a packed layer's are; an edit writes record files
through the history (`edit::Out`), so undo is unchanged. A reading of the
records takes again only those whose files changed, by stamp, every core reading
its share, and a refresh scans again only the entries whose records changed
(`records::Set::differences`): a content edit is that asset alone, the stack
standing; one that places an entry otherwise applies it again. New asset layers are
records; code layers (a files layer, a mount) stay trees, worked on with their
own tools, and generated layers keep their packs. A layer's folder is storage,
not a place to edit: the project is edited through the editor, its served
folder (`assets/`) or `pe2-cli`. What a tree layer holds against its changeset -
a copy deleted by hand, a file put there - is no edit: the index reports it
(`Problem::Missing`, `Problem::Unlisted`) for the user to repair - forget, drop
the override, adopt or remove (`edit::adopt`, `edit::remove_unlisted`); a
mounted tree is listed as it is found, always.

**An extension a project holds is sealed: kept whole, never merged.** A layer
names it by a record of its own, `.records/extensions/<id>.toml` (any asset
layer, a tree one too, whose scan leaves `.records` out): `tree = "<sha1>"`, its
files one tree of whole objects in the store, the tree's hash its identity. It is
no entry of the project's tree; a higher layer's record of an id holds it in
place of a lower one's, and one deleting it (`op = "delete"`) leaves it off.
`pe2-cli project extension add|update|remove` (and the editor's Extensions pane)
write those records as steps of the history; adding one held as it is already
is no step. The tree is the object: what is in it is named only by its path
within it, read where its folder was - `<layer>/extensions/<id>.extension/`, a
path on no disk, read through `project::content` (`records::held_at`) - and
nothing is laid out. Store objects are read-only; a program handed a file of it
gets one through the filesystem the project is served as
(`content::for_program`), else the object itself, in place. It is trusted by
the place of the layer holding it, as its path says. A layer holding extensions as folders, as before
(`extensions/<id>.extension`), is not read: `project store convert --layer`
holds them as records.

**A layer stores a large file as chunks.** At 1 MiB or more
(`chunks::THRESHOLD`), a layer holds `<name>.chunks` - a manifest of
content-defined chunks (FastCDC, BLAKE3), the chunks in `.layers/.store` - so an
edit stores only what it changed and identical data, across layers and
releases, is kept once. Everything reads and writes a layer's content through
`project::content`, never the path; the history keeps the manifest. A program
given such a file by path (ffmpeg, an external app) gets it through a scoped
filesystem of its own (`.cache/scope/<pid>/<sha1>/<name>`, `vfs::scoped`), or a
copy in `.cache/materialized` where no transport is. An unsandboxed operation is
served a whole folder of files for its run the same way (`Call::serve`, the
contract's `serve`: each where its content is, or held in memory, under
`.cache/scope/<pid>/run-<n>/`, gone once the run is done; laid out in the store's
`tmp/` where nothing serves it, or where it is small: `copies`, above) - what
`pe2.psx` hands its tools. A scope is
served by the backend suited to it, read-only on each: FUSE and Dokany mount it
so; the Projected File System (its library loaded when used: nothing registered, a path
opened looked up as it is) refuses every other process what would change a
file; the Cloud Files API serves a sync root of its own, its files read-only,
each served folder's placeholders made up front, since Windows asks for them
only as a folder is listed. What a backend left is cleared when the scope is
let go of, or by the next process to sweep the project's scopes, which
`<pid>.alive` beside a scope tells whether its process runs and which backend
served it. An unsandboxed
extension's own file access sees each Windows drive as a root (`/D:/a`); the host
writes what it hands it so (`component::guest_path`), and reads it back, a program's
arguments in Windows' form (`D:/a`). What an extension reads of a tree it was served
it takes from the value, not the folder: its own file access does not follow a folder
Dokany mounts on (a junction: "no such file"), so only that works with every backend. The store keeps files three
ways, by what they are (`chunks.rs`): chunked, a manifest by SHA-1 for content
edited or shared in parts (`store/files`: a record's binary file, what an
override was made from); whole, the bytes themselves by SHA-1, for what is only
ever taken or replaced whole (`store/objects`); and trees, listings of named
whole objects kept as one (`store/trees`: an extension). Each says its own
format (a manifest's or tree's header), so the store needs no mark of its own. `pe2-cli project store migrate` converts a tree layer's large files;
`store gc` removes what no manifest, record, changeset, pack or history step
names (the editor sweeps daily what is an hour old). A layer may be stored packed
(`storage = "packed"` in `project.toml`, a generated layer's so far; `text`, its
files as files, by default): its files are one journaled listing in its folder
(`.pack`), their chunks in the store, with nothing to scan, and `content` and the
index read it as files (`project::pack`). `store pack --layer` and `store
unpack --layer` convert one; its plan's record stays a file beside it.

**What a node's fields and settings are, in memory, is `pe2_value`** (`crates/value`):
values whose strings and keys are kept once, whose arrays of numbers are packed,
whose tables and arrays are shared and copied only where changed - so an
import's settings resolved over a node's fields (`imports::resolve`) share what
they leave be, and a layer's settings lifted among the fields
(`graph::with_settings_values`) are the layer's own. TOML is what they are
written as; the store keeps them, with its traces, as CBOR (`graph::store`).
A plan reads a node's fields through `Plan::of_fields`; what still reads `toml`
(the editor's panes, the operations reading a node's fields by name, a linked
extension) gets a copy made for that asset when first asked (`fields_toml`,
`settings_toml`).

**An override holds what it changes, whole, and is merged.** A layer overriding
an entry from below copies nothing (but code into a files layer, which is edited
in place): it holds each file it writes whole, and its record (a tree layer's
changeset) says, per file, the SHA-1 of the version below it was made from -
kept in the store - or that it removes it. Every other file comes from below as it is there. Where the
version below changes, the index merges the layer's file over it three ways
(`project::merge`), by what the file is and never by what node it is of: TOML by
field (a path through tables, through an array of tables by each element's
`id`), other text by line (diff3), bytes not at all. Both sides changing the same
thing otherwise is a conflict: the layer's side is shown, and the override is
stale, its conflicts listed (`Index::stale_parts`) until kept (its files become
the merge, made from what is below now) or given up. Merges are kept in
`.cache/merged`, by what they were made of. Diffs are never stored.

**A package's sounds are four kinds of node** (the `pe2.sounds` extension): a
sound (a sample, its `sound.wav` imported by Sony's encoder into the body the
bank stores), an instrument (a program), a sound script (what a sound id
starts) and a sound bank binding them as the disc stores it, read off its file's
`sounds` slot. `tools/editor_project/sounds.py declare` writes them, USA only so
far, for the banks of the packages the repository builds, into
`<group>/sounds.toml`: each instrument, script and sample once, named by the
first bank binding it and its place there, never from the code's names.
A sound previews raw; a script, and a bank's slot (chosen in its contents),
play as the game plays them: the extension's `engine` ports
`src/engine/sndscript.c` and `spu.c`'s voice upkeep over an SPU model, and
computes the engine's pitch, gain and pan tables (they are formulas) rather
than copying them; keep it in step with those files. Their reverb is the one
mode the game sets, libspu's Studio B, read from the executable by the
`engine/sound/reverb` node; a preview's "room reverb" is the area's setting.
A script is a data node: its notes are what it is set to, from the
`settings.toml` extraction writes into the generated layer, beneath what its
node sets (`graph::with_generated`), so they are edited as fields without the
game's data entering the declarations. A script, and a bank's chosen slot, are
shown as a sequencer under the player's transport (the type's view, told where
it plays): lanes of notes as the engine plays them, a playhead, seeking and
zoom; a script's notes are dragged, stretched and typed in, and the player
plays the edit before it is saved, as everything shown follows a working copy.
**A room's bundle, its dialogues and their text are nodes.** A bundle (`pe2.game`'s
`bundle`, `rooms/<room>/files/bundle`, named by its file's `bundle` slot) declares its
resources by directory index - a palette resource by the palettes it lists and the
slot it uploads into, a dialogue by its node - and joins them as the chunk stores
them (`formats::bundle`): every room bundle round-trips byte for byte. Extraction
writes its `bundle.toml` (its RAM base, which differs by release, its extra
sectors, the 38 palette streams stored a byte short) and `resources/<index>.bin`
for what no node makes. A dialogue (`pe2.dialogue`, a CAP file: `doc/CAP_FORMAT.md`)
is a data node of its structure - commands and records by name, a record's text a
key of its `strings` table, its speakers named by `cast`, its glyphs from the
`faces` (`ui/dialogue/{glyphs,bold,speakers}`, `pe2.fonts` faces whose glyphs are
the atlases' pictures) - and makes the file (`native`) and the atlas page its rule
lays out (`atlas`). `tools/editor_project/dialogue.py declare` writes them from the
bundles: each dialogue once at `<room>/dialogue_<i>`, a shared one where its rooms
are (`stages/shared` across stages), a string table a stage
(`stages/<stage>/strings/dialogue`, keys `<room>/<dialogue>/<command>/<record>`).
One planned operation reads every dialogue (`extract_dialogues`), recognizing glyphs
by `tools/peassets/cap_glyphs.tsv` and `cap_plates.tsv`; a dialogue not read exactly
says why (`failed`), and an I/l guess is listed under `unsettled`. A string table is
a plain key-value store in the dialogues' markup; there may be any number of them.
A stage's table is translated: its stack lays in `ui/dialogue/translated`, and a
dialogue reads its `strings` output (`uid://...#strings`), the table of the build's
`language` (its `translations`, by language) laid over it key by key, what it does
not translate its own; with none for that language, the table as it is.

**A config says more than its release and target: properties** (`[property.<name>]`
in `project.toml`, its `type` - `text`, `texts`, `integer`, `boolean`, `table`, `any`
- or `values`, and its `default`; `[config.<name>.properties]` sets them). A value not
as declared is a problem by config and property, never a load error, and the default
holds; `pe2-cli project config declare|set|unset|list` and Settings ▸ Configurations
edit them. An extension reads them in `BuildConfig::properties`; what reads the
config is kept by the hash of the part it read (`exec::config_hash`, `ConfigPart`):
all of it - with what every config says, `known` - through `Call::config`, what it
says - release, target, properties - through the `config` operation, so two configs
saying the same share it, or its release alone (`Call::release`, a file the releases
have apart). A game project declares
`language` (`tools/pe2project.py`). `pe2-cli project output` makes an output of
assets (or of every asset of a `--type`) for a `--config`, saying each file's SHA-1.

**The work graph has choice, and is pulled all the same.** Any step may take
`active` (a `bool`; a stack layer `active = { from = "id" }`, a graph operation its
`from.active`): where it is false the step makes nothing, nor does what takes from
it, but `merge` - the first of what it takes that is made. A step is made where it
is asked for, its `active` first and only then what it takes (`exec::Run`), so a
branch not taken is never made and only the taken one's reads are traced; the key
stays the whole plan's. A step about to take a value others take tells those others
whether they run first - where that is made, or is logic over the config alone - so a
value one of them will not take is still moved, not copied. `active = false` (or
`true`) written as a value is a constant. Beside `config`: `equals` (at a dotted `field`, `to` a
value), `all`/`any`/`invert` over booleans (named apart from the event gates, which
take signals), `select` (another node's output, by the key it is given, among its
`of`; only that one read, nothing where it names none) and `overlay` (a table over
another, key by key, at a `field`).

**A project defines operations and templates of its own** (`pe2.graph`): a node of
`pe2.graph@operation` holds a graph, made and edited in the graph view as a graph
layer is, whose boxes say what it is from outside - each `input` it takes (`name`,
`type`, `many`, `index`), each `setting` it is set to (a property's shape: `type` or
`values`, `default`, `label`; its value its box's output, and read where a value is
`{ setting = "<name>" }`) and each `output` it makes (the first its result, the others
wired `id.<name>`). A stack names one by its node's uid (`op = "uid://..."`), as a
layer or a graph box, and it is laid in there (`graph::Definition`,
`Lowering::inline`), each output its own step; it may not use itself. The inspector
shows its settings as fields. A `for_each` box runs one (`body`) for each item of a list
- or of a table, as `{ key, value }` - with what `with` takes, each item kept by what it
is given and the body, at the same time; what the body makes nothing of is left out
(`exec::Evaluation::for_each`, the body laid out by `Plan::of_definition`). The host
takes them from the index as imports are attached (`Extensions::take_definitions`),
what nodes are read again where one changed, and what uses one made again with it. A
`pe2.graph@template` (`label`, `node_type`, `files`, its stack as `makes`) is offered
beside the extensions' templates - for a file it takes on import too - by its uid. A
declaration lays layers after its template's stack (`layers`).

**Game flags are named in registries** (`pe2.flags`, `declarations/flags.toml`): a
`flag_registry` holds flags of the 504-nibble bank by name - `id`, `kind`
(`marker`, `state`, `counter`, `bits`, `packed`, `reserved`), `values` or `bits`,
`count` for a run, `description`, `verified` - written by hand from what the code
confirms (`packages/engine/include/event_flags.h`, a stage's `event_flags.h`), never
by a pass over `gameflag_ids.h`. An ID has one owner across every registry: a
registry's `ids` output fails naming both. A dialogue's command names its flag by
registry (`game_flag = { flags = "uid://...", flag = "story_dialogue_index" }`)
where one holds the ID. Packages do not read registries yet: the headers stay until
every package can.

What an animation's cues play is the code's (a cue is only bits on a clip's
key; gameplay's reload state picks the sound by weapon and phase), so an
animation bank *describes* it, for the editor: `sounds` names the sound bank,
and `cues.<clip slot>` the events in turn - a slot of that bank at the start
(`delay` ticks after) or at the next cue of `cue`'s bits, the last cue's again
at every cue after. The weapons' reloads are described so far; keep a
description in step with the code it was read from.

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
