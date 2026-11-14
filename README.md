# Parasite Eve 2 Decompilation
![coverage usa](https://decomp.dev/GabeRealB/parasite-eve-2-decomp.svg?mode=shield&label=Parasite+Eve+II+%28USA%29)
=============

![progress usa](https://decomp.dev/GabeRealB/parasite-eve-2-decomp.svg?w=512&h=256)

A **matching decompilation** of *Parasite Eve II* for the Sony PlayStation: C source that, compiled with the original toolchain, produces binaries **byte-identical** to the ones on the disc.

| Item | Value |
|---|---|
| Platform | PlayStation (PSX / PS1) |
| Target | USA release: the executable `SLUS_010.42` and every code package on the discs |
| Disks (USA) | `SLUS-01042` (Disk 1), `SLUS-01055` (Disk 2) |
| Compiler | GCC 2.8.1 (PSX `cc1`) + maspsx standing in for the ASPSX assembler |
| Libraries | Psy-Q 4.5 object files (see `lib/`) |
| License (project code) | [CC0 1.0](LICENSE) |

> **You must own the game.** This repository does not include ROMs, disc images, or copyrighted assets. Obtain a legal dump of your own discs.

## Status

The decompilation is complete: the executable `SLUS_010.42`, the resident `gameplay` overlay, the `title` overlay and all 446 packages build from C and match. Open points:

- **Matching hacks.**
  - Register pins: `hp` in `_actor403600ChooseAttack`, `previousRandomState` in `effectHitSparkBurstTask`, `secondBuffer` in `spriteAllocateViewCachedPackets`.
  - Inline assembly outside the GTE macros: `actor143000CaptureStripTask`, `_capDrawTextStream` (two statements), `_actor560800HandleKyleCue`, `_screenWaveGridTask`.
  - Handwritten assembly in `src/engine/hasm/`: `fsDecompressStream`, `fsDecompressImagePayload`, `tmdDrawModelStream`, `tmdSkipStreamRecord`, `Tmd_DispatchStream`, `Tmd_StreamHandler_Prim32`, `Tmd_StreamHandler_Prim3A`, `Tmd_StreamHandlers_Ops`.
- **Patches to maspsx** ([`tools/patches/README.md`](tools/patches/README.md)), not upstream:
  - `maspsx-bss-layout` reaches beyond the assembler: it also reproduces where the linker, PSYLINK, places non-`static` variables, and depends on `align_main_bss` in `tools/config/splat/configure.py` and on those variables being declared in address order.
  - `maspsx-lo-load-nop` and `maspsx-bss-local` change assembler behaviour only.
- **Order of global variables.** PSYLINK placed each file's non-`static` uninitialised variables by a hash of their names; the original names are unknown, so the sources declare them in address order instead.
- **Follow-ups from an AI review**, by category: semantics, rematching, runtime, unrelated.
- **Reverse engineering:** names, types, comments, documentation of the game's formats and systems.
- **Cleanup** of the sources and the tooling.

### Other regions

Only the USA release is set up. The Japanese version (`SLPS-02480` / `SLPS-02481`) is not being worked on.

## Prerequisites

Linux is the platform the project is developed and checked on. Windows builds natively; macOS on Apple silicon has a toolchain too (see [Tools](#tools)).

### Linux

- Python 3.11+
- `clang-format` (used by `tools/build-and-verify.sh`)
- Git (with submodules)
- For matching only: `binutils-mips-linux-gnu`, whose `mips-linux-gnu-objdump` the diff tools read objects with (the build needs no MIPS binutils of the system's)

On Ubuntu/Debian-style systems this is roughly:

```bash
sudo apt-get install -y \
  build-essential python3 python3-pip python3-venv \
  binutils-mips-linux-gnu clang-format git
```

A `Dockerfile` is provided for a containerized environment similar to CI (`ghcr.io/gaberealb/decomp-images-parasite-eve-2`).

### Windows

- Python 3.11+
- Git

The repository's `.gitattributes` checks text files out with LF endings whatever Git's `core.autocrlf` is set to; the build needs that. `tools/build-and-verify.sh` is a shell script and is not used on Windows: run `config.py splat` and `ninja` directly (see [Building](#building)).

### Python dependencies

```bash
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### Submodules

```bash
git submodule update --init --recursive
```

The one submodule is the editor (`editor/`). The ninja build needs nothing of it; `config.py editor` runs it.

### Tools

The build fetches what it builds with, each pinned by its URL and SHA-256, into `local/tools/` - what is built for a
platform under `local/tools/<platform>/` (`linux-x86_64`, `windows-x86_64`, `macos-aarch64`), so that one checkout
keeps each system's. `config.py splat` fetches what is missing; `python3 config.py splat fetch-tools` fetches everything
ahead of time, and `--path <name>` prints a tool's folder. `tools/config/fetched_tools.toml` pins them all.

- The toolchain, the releases the editor's `pe2.psx` downloads too, built from source by the editor's repository: GCC 2.8.1's `cc1` with old-gcc's PlayStation patches (gcc-psx), GNU binutils 2.47 for
  mipsel (binutils-mipsel) and a C preprocessor taking `cc1 -E`'s arguments (aro-cpp).
- [maspsx](https://github.com/mkst/maspsx), the ASPSX-compatible assembler front-end, at the commit `pe2.psx` uses.
- [m2c](https://github.com/matt-kempster/m2c) (assembly to C), [asm-differ](https://github.com/simonlindholm/asm-differ)
  (instruction-level diffs) and [decomp-permuter](https://github.com/simonlindholm/decomp-permuter) (match search by
  code mutation), at the commits `tools/config/fetched_tools.toml` pins; and, for the GCC tracers, old-gcc's unstripped
  `cc1`.
- From their projects' releases, at the versions `tools/config/fetched_tools.toml` pins:
  [ninja](https://ninja-build.org), which runs the builds `config.py` writes,
  [mkpsxiso](https://github.com/Lameguy64/mkpsxiso) (`dumpsxiso` extracts the discs, `mkpsxiso` builds them back),
  [objdiff](https://github.com/encounter/objdiff)'s `objdiff-cli` (the progress report), and on Windows MinGW's GCC
  ([WinLibs](https://github.com/brechtsanders/winlibs_mingw)), with which m2ctx preprocesses m2c's context.

maspsx and decomp-permuter are patched as they are fetched. [`tools/patches/README.md`](tools/patches/README.md) says
what each patch does, why, and how its behaviour was checked against the real Psy-Q assembler and linker.

[splat](https://github.com/ethteck/splat), which splits the binaries, comes through pip.

## Obtaining game files

1. Dump **both** USA discs to BIN/CUE (or equivalent).
2. Place the images under `rom/image/` with these names:

   | File | Description |
   |---|---|
   | `rom/image/SLUS-01042.bin` + `.cue` | USA Disk 1 |
   | `rom/image/SLUS-01055.bin` + `.cue` | USA Disk 2 |

   Each `.cue` file must reference the matching `.bin` by name (e.g. `FILE "SLUS-01042.bin" BINARY`). Dump tools often leave the original dump filename in the cue sheet; if extraction fails, open the `.cue` in a text editor and fix the `FILE` line so it matches the renamed `.bin`.

3. Extract what the build reads:

   ```bash
   python3 config.py splat extract
   ```

   (`--roms DIR` reads the images from elsewhere.)

   This uses `dumpsxiso` to write the disc's files under `rom/USA/disk1/` and `rom/USA/disk2/`, then writes what the build reads - the executables, the packages, the stored chunks and the instances of the data compiled into overlays - under `assets/USA/`.

   For the editor, with every asset in an editable form (PNG, WAV, USD, ...), make a project instead: `python3 config.py editor --roms rom/image` writes one to `project/` from the declarations - every asset's node and the plan of what to read off the discs - and the editor fills it from your discs (`pe2-cli project extract`).

## Building

There are two build paths, both a work in progress: the ninja build described here, written in Python, and the [editor](#the-editor)'s, written in Rust. They compile the same sources with the original toolchain and know nothing of each other. The ninja build covers every binary on the discs and is what `checksum.sha` checks.

From the repository root (with the venv active):

```bash
# Configure: fetch the tools, split what changed, write build.ninja, the link scripts, ...
python3 config.py splat

# Build, and verify the checksums against the retail binaries
ninja
```

`ninja` is any ninja on your `PATH`, or the one the configure fetched, whose path it prints. `ninja objdiff` writes
`objdiff.json` and the objects [objdiff](https://github.com/encounter/objdiff) compares, function by function.

On Linux the all-in-one helper formats the sources, reconfigures, builds and checks:

```bash
./tools/build-and-verify.sh
```

Only its `✅ BUILD SUCCEEDED` means the project matches. It re-splits only what changed and rebuilds incrementally, so a run with nothing changed takes seconds; the first run, or one after a change to `tools/config/splat/configure.py`, splits everything and takes minutes.

It also takes a scope, for iterating on one overlay:

```bash
./tools/build-and-verify.sh --only m93r                 # one overlay
./tools/build-and-verify.sh --only rooms                # a whole family
./tools/build-and-verify.sh --only gameplay,acropolis_patio
```

A scoped run splits, builds and checksums only those units, and says so in its success message. It tells you nothing about the overlays it skipped, so finish with an unscoped run.

### Configuring

`config.py` writes the build `ninja` runs: the splat build, or an editor project's. Each group's tools load only when
asked for; the editor's needs nothing but Python's standard library.

```text
python3 config.py splat [--release R] [--only SELECTOR[,SELECTOR...]]... [--fresh] [--jobs N]
python3 config.py splat fetch-tools [--path NAME]
python3 config.py splat extract [--release R] [--roms DIR]
python3 config.py editor [DIR] [--config NAME] [--release R]... [--roms DIR] [--fresh] [--no-extract] [--no-extensions] [--no-trust]
python3 config.py editor build-tools [--jobs N]
python3 config.py clean [--all]
```

| Command | What it does |
|---|---|
| `splat` | Fetch the toolchain, split what changed, and write `build.ninja`: `ninja` builds every image and checks it, `ninja objdiff` makes `objdiff.json`. `--only` splits and builds only the named families or overlays, and checks only what they build; `--fresh` ignores the split cache; `--jobs` caps the split's workers (default: `$PE2_JOBS`, else the CPU count) |
| `splat fetch-tools` | Fetch every pinned tool into `local/tools`, or one (`--path NAME`), printing its folder |
| `splat extract` | Extract what the build reads from the disc images (`rom/image`, or `--roms`) into `assets/<release>` (what CI uses) |
| `editor` | Make the editor project in DIR (`project/` by default) where there is none, from the discs (`--roms`), of every release declared (`--release` for fewer), with its extensions (`--no-extensions`) and extracted (`--no-extract`); `--fresh` makes the game's part of one there is anew. Then trust it (`--no-trust`), make it the active one (`compile_commands.json` links to its), and write `build.ninja`: `ninja` runs `pe2-cli project build` for the configuration (`--config`, else the first release's) and checks its images against `checksum.sha` |
| `editor build-tools` | Build the editor and lay it out (`cargo xtask dist`); `config.py editor` does it first where none is |
| `clean` | Remove `build/`, `permuter/`, the ninja files and logs and `objdiff.json`; `--all` also what the split writes (`asm/`, `linkers/`, `expected/`) |

## Working on the source

Any change has to keep the build matching, so the loop is: edit, `./tools/build-and-verify.sh`, and look at the difference when it fails.

```bash
python3 local/tools/asm-differ/diff.py --no-pager <function_name>
```

Tools that help, each documented in its own file:

| Tool | What it is for |
|---|---|
| `tools/refactor/find_references.py`, `rename_item.py` | Find and rename C symbols through the compiler's parse, never by text substitution |
| `tools/refactor/flag_refs.py` | Every place the source reads or writes a given game flag |
| `tools/refactor/check_decls.py` | Declarations that disagree with their definition across files |
| `tools/check/` | Checks on symbols, pointer arithmetic and casts, and matched functions |
| `tools/gametrace/` | Record what the running game does, through an emulator's GDB server, with a terminal front end |
| `tools/peassets/` | The asset extractor and its format decoders |
| `tools/match/`, `tools/vacuum/`, `tools/bulk/` | The scratch environment and automation the functions were matched with |

Conventions for names are in [NAMING.md](NAMING.md). The project prefers real structs over pointer arithmetic, and keeps declarations in the smallest scope that shares them; [CLAUDE.md](CLAUDE.md) spells these out, and is also the instruction file for the AI agents much of the matching was done with.

## The editor

`editor/` holds an editor for the game's assets and its own build path, in Rust. It is a work in progress: it works on a project extracted from your discs, and builds packages from the project's assets rather than from the bytes on the discs, so that an edit can change what is built. It cannot write a disc image yet, so nothing edited reaches the game.

It is a project of its own, [pe2-editor](https://github.com/GabeRealB/pe2-editor), here as a git submodule at `editor/`, pinned to the commit this repository works with: how to build it, ship it and add its extensions is in [editor/README.md](editor/README.md), what a user reads in [editor/docs](editor/docs/index.md), and its license in [editor/LICENSE](editor/LICENSE). Clone with `--recurse-submodules`, or fetch it after with `git submodule update --init`.

```bash
cd editor
cargo build --release   # pe2-editor and pe2-cli, in editor/target/release/
cargo xtask dist        # as they ship, with pe2.psx and pe2.ffmpeg beside them, in editor/target/dist/
```

`config.py editor` gives a new project the two extensions every project holds, `pe2.psx` and `pe2.ffmpeg`, from the editor as `cargo xtask dist` lays it out (`--no-extensions` leaves them out).

## Repository layout

```text
asm/                   # Disassembly, generated by the split (not in git)
assets/                # Extracted game files (not in git)
build/                 # Build output (not in git)
configs/               # splat configs, the overlay manifest, symbol maps, checksums
declarations/          # Where each asset is on the discs, and what fills a package's slots
doc/                   # The game's file formats and systems
editor/                # The editor and its build path (Rust)
include/               # Headers: decomp types, per-module game headers, Psy-Q
lib/                   # Prebuilt Psy-Q .o files linked into the executable
linkers/               # Generated linker scripts (not in git)
packages/              # Overlays both build paths compile: each its sources and link script
rom/                   # Disc images and extracted disc trees (not in git)
src/                   # C source: src/engine, src/gameplay,
                       #   src/<family>/<overlay>, src/stages/<stage>/rooms/<room>
                       #   (merged day/night rooms), and src/shared: code several
                       #   overlays include, by module (actor, creatures/<name>,
                       #   effects, room, screen, walk)
tools/                 # Build steps, checks, refactoring, tracer, extractor, patches
config.py              # Configure a build: the splat build, an editor project's (tools/config)
```

Documentation:

- [NAMING.md](NAMING.md) — symbol and module naming conventions
- [DECOMPILATION_LEARNINGS.md](DECOMPILATION_LEARNINGS.md) — what was learnt about GCC 2.8.1, the Psy-Q tools and this game's code
- [CODEGEN_MODEL.md](CODEGEN_MODEL.md), [COMPILER_ANALYSIS.md](COMPILER_ANALYSIS.md) — how the compiler arrives at its output
- [CLAUDE.md](CLAUDE.md) — project conventions and agent workflow
- [doc/ASSET_FORMATS.md](doc/ASSET_FORMATS.md) — the disc's containers, images, LZSS
- [doc/OVERLAYS.md](doc/OVERLAYS.md) — packages, RAM slots, rooms, models and animations
- [doc/STREAM_FORMATS.md](doc/STREAM_FORMATS.md) — MTS audio and STR movies
- [doc/TMD_FORMAT.md](doc/TMD_FORMAT.md) — model streams and their opcodes
- [doc/BUNDLE_FORMAT.md](doc/BUNDLE_FORMAT.md) — resource bundles: view palettes, dialogue, credits, demos
- [doc/CAP_FORMAT.md](doc/CAP_FORMAT.md) — CAP dialogue
- [doc/LZSS_ENCODER.md](doc/LZSS_ENCODER.md) — the compressor
- [doc/TASKS.md](doc/TASKS.md) — the cooperative task and actor model
- [tools/patches/README.md](tools/patches/README.md) — the patches to maspsx and the permuter

## Legal

- Project source, scripts, and documentation are released under **CC0 1.0** (see [LICENSE](LICENSE)).
- *Parasite Eve II*, related trademarks, and all original game assets remain property of their respective rights holders (Square / Square Enix, et al.).
- No game binaries or assets are redistributed by this project. Contributors must use dumps of discs they own.

## Acknowledgments

Build system and project structure are adapted from other PSX matching decomp efforts (notably the Silent Hill decomp-style `ninja_config` layout). Tooling includes splat, maspsx, m2c, asm-differ, decomp-permuter, and mkpsxiso/dumpsxiso.
