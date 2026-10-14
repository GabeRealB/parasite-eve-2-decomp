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
  - Handwritten assembly in `src/main/hasm/`: `fsDecompressStream`, `fsDecompressImagePayload`, `tmdDrawModelStream`, `tmdSkipStreamRecord`, `Tmd_DispatchStream`, `Tmd_StreamHandler_Prim32`, `Tmd_StreamHandler_Prim3A`, `Tmd_StreamHandlers_Ops`.
- **Patches to maspsx** ([`tools/patches/README.md`](tools/patches/README.md)), not upstream:
  - `maspsx-bss-layout` reaches beyond the assembler: it also reproduces where the linker, PSYLINK, places non-`static` variables, and depends on `align_main_bss` in `ninja_config.py` and on those variables being declared in address order.
  - `maspsx-lo-load-nop` and `maspsx-bss-local` change assembler behaviour only.
- **Order of global variables.** PSYLINK placed each file's non-`static` uninitialised variables by a hash of their names; the original names are unknown, so the sources declare them in address order instead.
- **Follow-ups from an AI review**, by category: semantics, rematching, runtime, unrelated.
- **Reverse engineering:** names, types, comments, documentation of the game's formats and systems.
- **Cleanup** of the sources and the tooling.

### Other regions

Only the USA release is set up. The Japanese version (`SLPS-02480` / `SLPS-02481`) is not being worked on.

## Prerequisites

Linux is the platform the project is developed and checked on. Windows builds natively. A macOS toolchain is bundled under `tools/macos/`.

### Linux

- Python 3.11+
- Ninja
- `binutils-mips-linux-gnu` (assembler, linker, objcopy, cpp) — **binutils ≥ 2.45** is required; older packages often fail to assemble or link correctly
- `clang-format` (used by `tools/build-and-verify.sh`)
- Git (with submodules)

On Ubuntu/Debian-style systems this is roughly:

```bash
sudo apt-get install -y \
  build-essential ninja-build python3 python3-pip python3-venv \
  binutils-mips-linux-gnu clang-format git
```

Check the installed version with `mips-linux-gnu-as --version`. If your distro ships something older than 2.45, install a newer binutils from a backports/PPA source or build from upstream.

A `Dockerfile` is provided for a containerized environment similar to CI (`ghcr.io/gaberealb/decomp-images-parasite-eve-2`).

### Windows

- Python 3.11+
- Ninja
- Git

The compiler, binutils and preprocessor are bundled under `tools/windows/`. The repository's `.gitattributes` checks text files out with LF endings whatever Git's `core.autocrlf` is set to; the build needs that. `tools/build-and-verify.sh` is a shell script and is not used on Windows: run `ninja_config.py` and `ninja` directly (see [Building](#building)).

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

This pulls in:

- [maspsx](https://github.com/mkst/maspsx) — ASPSX-compatible assembler front-end
- [m2c](https://github.com/matt-kempster/m2c) — assembly → C decompiler aid
- [asm-differ](https://github.com/simonlindholm/asm-differ) — instruction-level diffs
- [decomp-permuter](https://github.com/simonlindholm/decomp-permuter) — match search via code mutation

[splat](https://github.com/ethteck/splat), which splits the binaries, comes through pip.

Two of the submodules are patched in place by the build. [`tools/patches/README.md`](tools/patches/README.md) says what each patch does, why, and how its behaviour was checked against the real Psy-Q assembler and linker. Run `git submodule update` again after a pull that moves a submodule.

The original PSX GCC 2.8.1 compiler ships under `tools/linux/` (and equivalents for Windows/macOS).

## Obtaining game files

1. Dump **both** USA discs to BIN/CUE (or equivalent).
2. Place the images under `rom/image/` with these names:

   | File | Description |
   |---|---|
   | `rom/image/SLUS-01042.bin` + `.cue` | USA Disk 1 |
   | `rom/image/SLUS-01055.bin` + `.cue` | USA Disk 2 |

   Each `.cue` file must reference the matching `.bin` by name (e.g. `FILE "SLUS-01042.bin" BINARY`). Dump tools often leave the original dump filename in the cue sheet; if extraction fails, open the `.cue` in a text editor and fix the `FILE` line so it matches the renamed `.bin`.

3. Extract disc contents:

   ```bash
   python3 ninja_config.py --iso_extract
   ```

   This uses `dumpsxiso` and writes files under `rom/USA/disk1/` and `rom/USA/disk2/`, and the executable, packages and other assets under `assets/USA/`. `--iso_extract_minimal` extracts only what the build needs and is much quicker.

## Building

From the repository root (with the venv active):

```bash
# Configure (runs splat, generates build.ninja, link scripts, etc.)
python3 ninja_config.py

# Build and verify the checksums against the retail binaries
ninja
```

On Linux the all-in-one helper formats the sources, reconfigures, builds and checks:

```bash
./tools/build-and-verify.sh
```

Only its `✅ BUILD SUCCEEDED` means the project matches. It re-splits only what changed and rebuilds incrementally, so a run with nothing changed takes seconds; the first run, or one after a change to `ninja_config.py`, splits everything and takes minutes.

It also takes a scope, for iterating on one overlay:

```bash
./tools/build-and-verify.sh --only m93r                 # one overlay
./tools/build-and-verify.sh --only rooms                # a whole family
./tools/build-and-verify.sh --only gameplay,acropolis_patio
```

A scoped run splits, builds and checksums only those units, and says so in its success message. It tells you nothing about the overlays it skipped, so finish with an unscoped run.

### Configure flags

| Flag | Meaning |
|---|---|
| `-iso_e` / `--iso_extract` | Extract everything from the disc images |
| `-iso_min` / `--iso_extract_minimal` | Extract only what the build needs (what CI uses) |
| `-iso_raw` / `--iso_extract_raw` | Extract the raw files only, without inflating or converting |
| `-o` / `--only` | Split and build only the named families or overlays |
| `--fresh` | Ignore the split cache and split everything selected |
| `-c` / `--clean` | Clean build and permuter outputs |
| `-j` / `--jobs` | Parallel split workers (default: `$PE2_JOBS`, else the CPU count) |
| `-sc` / `--skip_checksum` | Build without the checksum step |
| `-obj` / `--objdiff_config` | Generate the objdiff setup for per-object diffs |

## Working on the source

Any change has to keep the build matching, so the loop is: edit, `./tools/build-and-verify.sh`, and look at the difference when it fails.

```bash
python3 tools/asm-differ/diff.py --no-pager <function_name>
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

## Repository layout

```text
asm/                   # Disassembly, generated by the split (not in git)
assets/                # Extracted game files (not in git)
build/                 # Build output (not in git)
configs/               # splat configs, the overlay manifest, symbol maps, checksums
doc/                   # The game's file formats and systems
include/               # Headers: decomp types, per-module game headers, Psy-Q
lib/                   # Prebuilt Psy-Q .o files linked into the executable
linkers/               # Generated linker scripts (not in git)
rom/                   # Disc images and extracted disc trees (not in git)
src/                   # C source: src/main, src/gameplay, src/title,
                       #   src/<family>/<overlay>, and src/shared
tools/                 # Build steps, checks, refactoring, tracer, extractor, patches
ninja_config.py        # Configure / extract / generate the ninja build
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
- [doc/CAP_FORMAT.md](doc/CAP_FORMAT.md) — caption and dialogue bundles
- [doc/LZSS_ENCODER.md](doc/LZSS_ENCODER.md) — the compressor
- [doc/TASKS.md](doc/TASKS.md) — the cooperative task and actor model
- [tools/patches/README.md](tools/patches/README.md) — the patches to maspsx and the permuter

## Legal

- Project source, scripts, and documentation are released under **CC0 1.0** (see [LICENSE](LICENSE)).
- *Parasite Eve II*, related trademarks, and all original game assets remain property of their respective rights holders (Square / Square Enix, et al.).
- No game binaries or assets are redistributed by this project. Contributors must use dumps of discs they own.

## Acknowledgments

Build system and project structure are adapted from other PSX matching decomp efforts (notably the Silent Hill decomp-style `ninja_config` layout). Tooling includes splat, maspsx, m2c, asm-differ, decomp-permuter, and mkpsxiso/dumpsxiso.
