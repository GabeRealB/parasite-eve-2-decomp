# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Overview

This is a matching decompilation project for Parasite Eve 2 (PS1). The goal is to create C code that, when compiled, produces the exact same assembly as the original game ROM.

## Project Structure

- `src/<overlay>` decompiled C (`src/main`, `src/gameplay`, `src/title`) and
  `src/<family>/<overlay>` for the generated overlay families (`src/weapons/m93r`, …)
- `include/<overlay>` headers for that unit (`include/main`, `include/gameplay`, …)
- `asm/<ver>/<overlay>/nonmatchings` unmatched functions (one `.s` per function). Overlays may be nested (e.g. `asm/USA/stage1/101/nonmatchings`).
- `asm/<ver>/<overlay>/matchings` already-matched functions
- `lib` library code such as Psy-Q objects we link against
- `assets` binary asset blobs extracted from the rom

### Generated overlay configs

`main` / `gameplay` / `title` have hand-written splat configs; each has quirks a
template cannot express. Every other overlay is a flat `.pe2pkg` with the same
shape, and there are 446 of them, so their configs are **generated**:

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
built from `assets/USA/pe2pkg/<key>.pe2pkg` — file ids appear nowhere in the
decomp. Several stage-0 ids can load one package (the extractor dedups by
SHA-1, so `10500` and `10600` are one file); naming it once in the extractor is
what makes it one thing here. The generator errors if two entries resolve to the
same package.

Packages the decomp builds are marked `"required": True` in `asset_data.py`, so
`python3 ninja_config.py -iso_min` materialises exactly that set — 237 overlays
in ~12s, which is the CI/matching extract. `stages.json` and the ISO manifests
are written in **every** extraction mode: the content tree comes from the
HED/CDF plus the extract-time chunk map, so it does not depend on having
inflated anything, and the viewer and `dump_asset_db.py` both need it.

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

Two maintenance commands, neither run by the build:

- `python3 tools/gen_overlay_configs.py [--family F] [--list]` — regenerate the
  configs. `ninja_config.py` also calls this, so the build is self-consistent.
- `python3 tools/gen_overlay_imports.py [family]` — rebuild a family's imports
  file from the split output, then re-split so the sources pick up the names.
  Run it after adding a family or after a naming pass.

Overlays in a family all load at the same address, so `symbol_name_format`
prefixes generated names with the segment (`func_m4a1_8011D1C4`). Keep that:
the decomp tooling (`decomp_overlay.py find`, `tools/claude`,
`score_functions.py`, the vacuum) assumes a function name identifies one
overlay.

## Tools

- `./tools/build-and-verify.sh [--only SELECTOR[,SELECTOR...]]` build the
  project and verify that it matches the target. `--only` splits, builds and
  checksums just those units — a family (`core`, `weapons`) or a single
  basename (`gameplay`, `m93r`) — leaving every other overlay's `asm/` and
  `linkers/` alone. Use it as the inner loop while matching (2.6s for one
  weapon overlay, 4.8s for gameplay, against 38s for the project), then run it
  **unscoped** before committing: a scoped pass says nothing about the
  overlays it skipped, which is exactly what a struct change breaks.
- `diff.py` you can view the difference between the compiled and target assembly code of a given function by running `python3 tools/asm-differ/diff.py --no-pager <function name>`
- `./tools/claude [--bootstrap-only] [--cli grok|claude] <function>` spin up a scratch matching env. Resolves **any** overlay; always m2c-bootstraps unless `--no-bootstrap`. Matching loop: `tools/claude-decomp-env/MATCH_LOOP.md` (Grok also loads it from `.grok/rules/match-loop.md`).
- `python3 tools/decomp_overlay.py find|pack|list-nonmatchings|list-overlays <function>` overlay-agnostic path lookup and vacuum brief.
- `python3 tools/score_functions.py <directory>` find the easiest function to decompile in a given directory (and its subdirectories).
- `./tools/vacuum.sh [--grok|--claude] [--dry-run] [--orchestrator] [--difficult] [--overlay NAME]` pick the easiest unmatched function across every overlay, bootstrap, pack a brief, run the agent. `--difficult` retries only names in `tools/difficult_functions` (a verified match removes that name from the list); `--overlay gameplay` (or `USA/main`) restricts to that overlay; both flags together are the intersection. Auto-commits a verified match if the agent forgot to. After a ≥95% give-up, runs decomp-permuter (`--stop-on-zero`, 6 min cap) and a short port follow-up on a hit. Best scratch C is kept at `tools/giveups/<func>/` (gitignored) so a later retry does not start from m2c. `--orchestrator` claims a function via `tools/vacuum_orch.py`, matches in a throwaway `pe2-wt-<func>` worktree, independently verifies that worktree (agents often sha256 leftover `build/` artifacts), fast-ports when trunk files have not moved, otherwise runs a port agent. Failed trunk landings are retried (`VACUUM_PORT_TRIES`, default 2) then marked difficult so the same claim cannot loop. Do not run a non-orchestrator vacuum on this checkout at the same time as an orchestrator session.
- `python3 tools/vacuum_orch.py claim|relinquish|finish|merge-acquire|merge-release|status|serve` coordinate multiple vacuums: function leases plus a merge lock on the original tree. State: `$(git rev-parse --git-common-dir)/vacuum-orch.json`.
- `./permute.sh --run --timeout 360 -j4 <func> <asm> <c>` when a match is stuck ≥95% on registers/scheduling. Stops on score 0.
- `python3 tools/overlay_dup_index.py stats|shared|find [--family F]` find code
  that repeats across overlays in a family. 56% of the room functions are copies
  of another room's; `find <fn>` lists every overlay carrying the same body,
  marking which are byte-identical.
- `python3 tools/peassets/tmd_export.py <family> [--out DIR]` export a manifest
  family's model streams to Wavefront OBJ (vertices and faces only). Useful for
  identifying an overlay whose name is still a placeholder.
- `python3 tools/check_pointer_arithmetic.py <file or directory>` detect pointer arithmetic with casts that should be replaced with struct field access. Use `--strict` to fail on violations.

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
   - Is it accessing an array element? (e.g., `arg1 * 36` means 36-byte elements)
   - What field within the element? (e.g., `+ 0xA` means field at offset 10)

2. **Create appropriate structs:**

   - Define the element struct with correct size and field offsets
   - Define the container struct with pointer at correct offset
   - Use meaningful names or `field_[Offset]` naming convention
   - **Define the struct in the matching module header**, not in a kitchen-sink
     header. Nothing auto-moves types for you.
     - **Main executable** (`src/main/`, `include/main/`): see `NAMING.md` for
       the module → header map (e.g. sound in `include/main/sound.h`, UI in
       `ui.h`, FS/CdCmd in `fs.h`). Include the specific module header; do not
       add a kitchen-sink aggregator.
     - **Overlays** (stage/file pe2pkg units, not yet decompiled under their own
       trees): use that overlay’s own `include/` / header layout when it exists;
       do not dump overlay-only types into `include/main/`.
   - Use `include/main/unknown_syms.h` only for residual main-executable symbols
     (`func_800*`, unfiled BSS/data) with no module home yet. Do **not** add new
     named types or Module_ APIs there.

3. **Verify struct sizes:**

   - Calculate total size to ensure it matches the multiplier in pointer arithmetic
   - Example: `arg1 * 36` means struct must be exactly 36 (0x24) bytes

### When Decompiling

If you write code with pointer arithmetic:

- **STOP immediately**
- Create proper struct definitions first
- Then write the function using struct access
- This applies even if the pointer arithmetic "works" - it's always wrong in a decompilation project

## Tasks

### Decompile directory to C code

You may be given a directory containing assembly files either in its own directory or its subdirectories.

1. Use `python3 tools/score_functions.py $(python3 tools/decomp_overlay.py list-nonmatchings)` to find the easiest unmatched function across every overlay. Start with that one.
2. Follow the instructions in the `Decompile assembly to C code` of this document.
3. If you are able to get a perfect matching decompilation, commit the change with the message `matched <function name> <attempts>` and return to step (1). If you cannot get a perfect match after several attempts, add the function name to `tools/difficult_functions` along with the number of attempts and best match percentage (function names should be separated by newlines). This should be in the form `<function name> <number of attempts to match> <best match percentage>\n`. By adding the function name to difficult_functions. You should also revert any changes you've made adding the function to the C file (we do not want to save incomplete matches).
4. You are done. Do not attemp to find the next closest match.

### Decompile assembly to C code

You may be given a function and asked to decompile it to C code.

First we need to spin up a decomp environment for the function, run:

```
./tools/claude --bootstrap-only <function name>
```

The script searches every overlay under `asm/<ver>/` (not just `main`). Move to the directory it prints (`SCRATCH_DIR=…`). Read `BRIEF.md` there instead of re-exploring the repo.

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

Game content lives in files, not in the executable: `assets/USA/pe2img`
(textures), `pe2clut` (palettes), `pe2pkg` (room/actor overlays), `pe2cap2`,
`audio`, `movie`, `bs`. See [`doc/ASSET_FORMATS.md`](doc/ASSET_FORMATS.md).

Some content *is* embedded in the binaries, though, and it must stay in the
generated trees:

| What | Where | Note |
|---|---|---|
| Memory-card save header | `Mc_SaveHeaderMagic` + the block after it (main `.data`) | `"SC"` magic, Shift-JIS title, 16-colour CLUT, three 16x16 4bpp icon frames |
| UI font glyph metrics | `Font_Glyphs0/1/2` (main `.data`) | 224/224/91 x `FontGlyph`; pixels come from a CLUT image, see ASSET_FORMATS 7.6 |
| Item / balance tables | `Gp_ItemDescs`, `Gp_IdParamHi` (gameplay `.data`) | |
| UI and dialogue text | the `.asciz` pools in gameplay `.rodata` | |
| Meshes and animation clips | gameplay `.data` trailing region and room/actor `.pe2pkg` overlays | no separate chunk type; see [`doc/OVERLAYS.md`](doc/OVERLAYS.md) |

What **is** fine to write into C is program structure whose "data" is
references to our own decompiled symbols, or small constants that encode
logic: function-pointer dispatch tables (`Display_TaskStates`,
`Mc_FileSelectStates`, `Gp_ItemMenuStates`), index tables such as
`Gp_FaceEdgePairs`, and the short strings a matched function needs in its own
`.rodata`. That is the existing pattern - data moves into C when matching
forces it into the owning TU's section, not as a cleanup exercise.

Content embedded in a binary is handled twice over, and neither route puts it
in git. For the **build**, a splat `databin` / `rodatabin` segment writes the
bytes to `assets/USA/incbin/` and emits a small `.s` that `.incbin`s them back,
so the build keeps matching. For **inspection**, catalogue it by address in
`asset_db.EMBEDDED_ASSETS` and it flows through the normal extract pipeline
into `raw/{type}/` and the type directory, like any on-disc asset. See
`doc/ASSET_FORMATS.md` 7.7.

There is no `USE_ASSET` macro and no `dmaRequestAndUpdateStateWithSize` in
this project; earlier revisions of this file described a scheme from a
different decomp. Ignore any reference to them.
