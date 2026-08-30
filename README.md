# Parasite Eve 2 Decompilation
![coverage usa](https://decomp.dev/GabeRealB/parasite-eve-2-decomp.svg?mode=shield&label=Parasite+Eve+II+%28USA%29)

A work-in-progress **matching decompilation** of *Parasite Eve II* for the Sony PlayStation.

The goal is to recover readable C that, when compiled with a period-correct toolchain, produces a binary **byte-identical** to the original executable.

| Item | Value |
|---|---|
| Platform | PlayStation (PSX / PS1) |
| Target | USA main executable `SLUS_010.42` |
| Disks (USA) | `SLUS-01042` (Disk 1), `SLUS-01055` (Disk 2) |
| Compiler | GCC 2.8.1 (PSX `cc1`) + maspsx |
| Libraries | Psy-Q 4.5 object files (see `lib/`) |
| License (project code) | [CC0 1.0](LICENSE) |

> **You must own the game.** This repository does not include ROMs, disc images, or copyrighted assets. Obtain a legal dump of your own discs.

## Status

### USA

| Component | Status |
|---|---|
| **Main executable** (`SLUS_010.42`) | ![coverage SLUS_010.42](https://decomp.dev/GabeRealB/parasite-eve-2-decomp/SLUS-01042.svg?mode=shield&category=main&label=SLUS_010.42) |
| **Gameplay overlay** — resident in-game code (actors, combat, inventory) | ![coverage gameplay](https://decomp.dev/GabeRealB/parasite-eve-2-decomp/SLUS-01042.svg?mode=shield&category=gameplay&label=gameplay) |
| **Title overlay** — title / demo / main menu | ![coverage title](https://decomp.dev/GabeRealB/parasite-eve-2-decomp/SLUS-01042.svg?mode=shield&category=title&label=title) |

<details>
<summary><strong>Overlays</strong> (448 packages, 237 split)</summary>

Everything but the actor overlays and the `501xx` helpers is split and matching.
An overlay is an extracted `.pe2pkg` package named after what it is; the names,
load addresses and per-overlay notes live in `configs/USA/overlays.toml`, and the
RAM slots and package layout in [`doc/OVERLAYS.md`](doc/OVERLAYS.md).

| Family | Overlays | What it is | Progress |
|---|---:|---|---|
| `weapons` | 32 | Equipped-weapon overlays | ![coverage weapons](https://decomp.dev/GabeRealB/parasite-eve-2-decomp/SLUS-01042.svg?mode=shield&category=weapons&label=weapons) |
| `rooms` | 168 | Room overlays (STAGE1-5, one per folder) | ![coverage rooms](https://decomp.dev/GabeRealB/parasite-eve-2-decomp/SLUS-01042.svg?mode=shield&category=rooms&label=rooms) |
| `mapui` | 5 | Area maps and their room-name pools | ![coverage mapui](https://decomp.dev/GabeRealB/parasite-eve-2-decomp/SLUS-01042.svg?mode=shield&category=mapui&label=mapui) |
| `aya` | 6 | Aya costume meshes and the replay bonus | ![coverage aya](https://decomp.dev/GabeRealB/parasite-eve-2-decomp/SLUS-01042.svg?mode=shield&category=aya&label=aya) |
| `humans` | 4 | Named human characters | ![coverage humans](https://decomp.dev/GabeRealB/parasite-eve-2-decomp/SLUS-01042.svg?mode=shield&category=humans&label=humans) |
| `options` | 2 | Options menu | ![coverage options](https://decomp.dev/GabeRealB/parasite-eve-2-decomp/SLUS-01042.svg?mode=shield&category=options&label=options) |
| `mappic` | 17 | Map pictures, by stage | no code in these packages |
| `debug` | 1 | Leftover NMC name table | no code in these packages |

Rooms are most of the overlay code, so they are also tracked per area:

| Area | Rooms | Progress |
|---|---:|---|
| Akropolis Tower | 17 | ![coverage rooms_acropolis](https://decomp.dev/GabeRealB/parasite-eve-2-decomp/SLUS-01042.svg?mode=shield&category=rooms_acropolis&label=Akropolis+Tower) |
| MIST HQ | 4 | ![coverage rooms_mist](https://decomp.dev/GabeRealB/parasite-eve-2-decomp/SLUS-01042.svg?mode=shield&category=rooms_mist&label=MIST+HQ) |
| Dryfield | 65 | ![coverage rooms_dryfield](https://decomp.dev/GabeRealB/parasite-eve-2-decomp/SLUS-01042.svg?mode=shield&category=rooms_dryfield&label=Dryfield) |
| Mine | 8 | ![coverage rooms_mine](https://decomp.dev/GabeRealB/parasite-eve-2-decomp/SLUS-01042.svg?mode=shield&category=rooms_mine&label=Mine) |
| Shelter | 52 | ![coverage rooms_shelter](https://decomp.dev/GabeRealB/parasite-eve-2-decomp/SLUS-01042.svg?mode=shield&category=rooms_shelter&label=Shelter) |
| Neo Ark | 22 | ![coverage rooms_neo_ark](https://decomp.dev/GabeRealB/parasite-eve-2-decomp/SLUS-01042.svg?mode=shield&category=rooms_neo_ark&label=Neo+Ark) |

Bodies that several rooms share are split into their own unit so they are matched
once and linked into each room that uses them (![coverage rooms_shared](https://decomp.dev/GabeRealB/parasite-eve-2-decomp/SLUS-01042.svg?mode=shield&category=rooms_shared&label=shared)).

**Not yet split:** the 196 actor packages (`1xxxxx` / `2xxxxx` / `3xxxxx`) and the
40 `501xx` helpers, whose role is still unknown.
</details>

### Other regions

The Japanese version (`SLPS-02480` / `SLPS-02481`) is currently not being worked on.

## Prerequisites

### System packages (Linux)

- Python 3.10+
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

Check the installed version with `mips-linux-gnu-as --version` (or `ld` / `objcopy`). If your distro ships something older than 2.45, install a newer binutils from a backports/PPA source or build from upstream.

A `Dockerfile` is provided for a containerized environment similar to CI (`ghcr.io/gaberealb/decomp-images-parasite-eve-2`).

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

- [splat](https://github.com/ethteck/splat) (via pip) — binary splitting
- [maspsx](https://github.com/mkst/maspsx) — ASPSX-compatible assembler front-end
- [m2c](https://github.com/matt-kempster/m2c) — assembly → C decompiler aid
- [asm-differ](https://github.com/simonlindholm/asm-differ) — instruction-level diffs
- [decomp-permuter](https://github.com/simonlindholm/decomp-permuter) — match search via code mutation

The original PSX GCC 2.8.1 / 2.7.2 compilers ship under `tools/linux/` (and equivalents for Windows/macOS).

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

   This uses `dumpsxiso` and writes files under `rom/USA/disk1/` and `rom/USA/disk2/`, and prepares the executable/assets used by the build.

## Building

From the repository root (with the venv active):

```bash
# Configure (runs splat, generates build.ninja, link scripts, etc.)
python3 ninja_config.py

# Build and verify checksum against the retail executable
ninja
```

Or use the all-in-one helper (formats sources, reconfigures, builds, and checks):

```bash
./tools/build-and-verify.sh
```

With 238 build targets a full verify takes about a minute, which is fine as a
gate and too slow as an inner loop, so it also takes a scope:

```bash
./tools/build-and-verify.sh --only m93r                 # one overlay
./tools/build-and-verify.sh --only rooms                # a whole family
./tools/build-and-verify.sh --only gameplay,acropolis_patio
```

A scoped run splits, builds and checksums only those units and leaves every
other overlay's `asm/` and `linkers/` alone. It says so in its success message:
only the bare `./tools/build-and-verify.sh` printing `✅ BUILD SUCCEEDED` means
the project matches.

### Useful configure flags

| Flag | Meaning |
|---|---|
| `-c` / `--clean` | Clean build and permuter outputs |
| `-iso_e` / `--iso_extract` | Extract files from disc images (full inflate) |
| `-iso_min` / `--iso_extract_minimal` | CI extract: raw + the overlays the build needs (237 packages, ~12s) |
| `-o` / `--only` | Split and build only the named families or overlays |
| `-iso_raw` / `--iso_extract_raw` | Extract raw/{type}/ only (no inflate) |
| `-sc` / `--skip_checksum` | Build without the matching checksum step |
| `-nm` / `--non_matching` | Compile with `-DNON_MATCHING` |
| `-obj` / `--objdiff_config` | Generate objdiff setup for progress / per-object diffs |
| `-ver USA` / `-ver JAP` | Select game version (default: USA) |

### Diffing a function

```bash
python3 tools/asm-differ/diff.py --no-pager <function_name>
```

### Matching decompilation workflow (high level)

1. Pick a function (e.g. via `python3 tools/score_functions.py asm/USA/main/nonmatchings/`).
2. Decompile / rewrite it in C under the appropriate `src/**/*.c` unit.
3. Replace the corresponding `INCLUDE_ASM(...)` with the C implementation.
4. Rebuild and confirm the full executable still matches — scoped first, then
   unscoped (see above).
5. Prefer real structs over raw pointer arithmetic — see project conventions below.

In an overlay family, check first whether the body is shared: over half the room
functions are copies of another room's, and `python3 tools/overlay_dup_index.py
find <fn>` lists every overlay carrying the same one. A shared body is matched
once, in `src/<family>/lib/`, and linked into each overlay that uses it.

Scratch helpers for AI-assisted matching live under `tools/claude` / `tools/claude-decomp-env/`.

## Repository layout

```text
asm/                   # Disassembled code (matchings + nonmatchings)
assets/                # Extracted assets (main.exe, stages, …)
configs/               # splat config, symbols, relocs, checksum
expected/              # Expected objects for objdiff (when generated)
include/               # Shared headers (decomp types, game, Psy-Q)
lib/                   # Prebuilt Psy-Q .o files linked into the binary
linkers/               # Linker scripts and auto undefineds
rom/                   # Disc images + extracted disc trees (local only)
src/                   # Decompiled C: src/main, src/gameplay, src/title, and
                       #   src/<family>/<overlay> for the generated families
tools/                 # Build helpers, peassets, m2c, differ, permuter, …
ninja_config.py        # Configure / extract / generate ninja build
```

Related docs:

- [NAMING.md](NAMING.md) — symbol and module naming conventions
- [DECOMPILATION_LEARNINGS.md](DECOMPILATION_LEARNINGS.md) — GCC 2.8.1 / matching notes
- [CLAUDE.md](CLAUDE.md) — contributor / agent workflow notes
- [doc/ASSET_FORMATS.md](doc/ASSET_FORMATS.md) — STAGE*.CDF chunks, images, LZSS
- [doc/OVERLAYS.md](doc/OVERLAYS.md) — pe2pkg RAM slots, rooms, models / anim
- [doc/STREAM_FORMATS.md](doc/STREAM_FORMATS.md) — MTS audio + STR movie
- [doc/TMD_FORMAT.md](doc/TMD_FORMAT.md) — model streams and their opcodes
- [doc/TASKS.md](doc/TASKS.md) — cooperative task / actor model, `Task_DescBanks`

## Legal

- Project source, scripts, and documentation are released under **CC0 1.0** (see [LICENSE](LICENSE)).
- *Parasite Eve II*, related trademarks, and all original game assets remain property of their respective rights holders (Square / Square Enix, et al.).
- No game binaries or assets are redistributed by this project. Contributors must use dumps of discs they own.

## Acknowledgments

Build system and project structure are adapted from other PSX matching decomp efforts (notably the Silent Hill decomp-style `ninja_config` layout). Tooling includes splat, maspsx, m2c, asm-differ, decomp-permuter, and mkpsxiso/dumpsxiso.
