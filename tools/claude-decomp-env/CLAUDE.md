# CLAUDE.md

## Your Job

You are decompiling Playstation 1 assembly from Parasite Eve 2. Generate C for `$functionName` that, when compiled, 100% matches the target assembly.

The compiler is GCC 2.8.1 with flags `-O2 -mips1`. C89.

This scratch directory is already bootstrapped. **Do not** run `./tools/claude`. Project root is the parent of `nonmatchings/` (one level up). Read `BRIEF.md` before exploring.

### Laying the Foundation

If `BRIEF.md` exists, use it instead of a context-gathering subagent. Then make the **minimal** edits needed so `base.c` compiles. `base.c` should only `#include "common.h"`; any other types go inline. An accurate baseline score depends on not rewriting m2c output yet.

If `BRIEF.md` is missing:

<subagent-instructions>
1. Locate `$functionName` via `python3 ../tools/decomp_overlay.py find $functionName --json` (works for any overlay, including nested future ones). Write a short summary to `LEARNINGS.md`.
2. Make `base.c` compile. Do not stop until `./build.sh base.c` runs.
3. Report back.
</subagent-instructions>

### Build Loop

1. `./build.sh base.c` — 100% is a perfect match.
2. Plan a small change from the diff. Prefer nearby matched C in the same TU (see BRIEF.md) over m2c control flow.
3. Write `base_N.c` (one attempt per file).
4. Repeat. Grep `DECOMPILATION_LEARNINGS.md` for the mismatch pattern; do not read it end-to-end.

**Permuter:** if the best score is ≥ 95% and the remaining diffs are register allocation, instruction scheduling, or stack slots (not control flow), stop hand-pinning and from the project root run:

```
./permute.sh --run --timeout 360 -j4 $functionName <asm-path-from-BRIEF> base_N.c
```

Do this **before** `register … asm("")` pins. Vacuum also permutes after you exit if you leave a ≥95% `base_N.c` here. Give up after ~10 attempts with no improvement, or ~40 attempts total. On stall, `build.sh` itself will tell you to stop.

### After a 100% match (or giving up)

1. Integrate into the host C file (replace `INCLUDE_ASM`). Types for this overlay live in **that overlay's** `include/` tree (`include/main/`, `include/gameplay/`, `include/<overlay>/`, …). Do not add named types to `include/main/unknown_syms.h`.
2. From the project root run `./tools/build-and-verify.sh` until `build/USA/out/SLUS_010.42: OK` (and the overlay checksum if this unit has one).
3. Commit `matched $functionName <attempts>`.
4. On give-up: append `tools/difficult_functions` as `$functionName <attempts> <best%>`, revert any host C / header edits, do not leave a partial `INCLUDE_ASM` replacement.
5. Append generalizable findings to project-root `DECOMPILATION_LEARNINGS.md`.
6. If vacuum launched you, leave this scratch directory (vacuum archives the best seed to `tools/giveups/<func>/`). Otherwise delete it.

## Tools

- `./build.sh <file>.c` — compile and score against `target.o`
- `./objdump.py <file>.o`
- `./diff.sh <file>.o`
- `./map_asm_to_c.py <file>.o <line>`
- `../tools/decomp_overlay.py find|pack <func>` — overlay-agnostic paths / brief

## Coding Guidelines

### Types and Structs
- Reuse existing structs. Search `src/` and `include/` (including this overlay's headers) before adding a type.
- Extending a shared struct must keep offsets compatible with every TU that uses it.

### Struct Field Access

Always prefer struct field or array accesses over pointer arithmetic.

Common mistakes:
- `return *(u8 *)(arg0 + 0xC1);`
- `*(s32*)((u8*)arg0 + 0x34) = value;`
- `*((s16*)ptr + 2);`

If the field is missing, add it to the struct with a correct `/* 0xNN */` offset and padding.

### Style
- `for` loops over `do`/`while`.
- Temps are fine; do not re-assign the same value across extra variables.
- Declarations at the start of the function.
- No comments in the matched C.

## Decompilation Strategy

Learnings: `DECOMPILATION_LEARNINGS.md` (symlinked here). Grep it.

### General Approach
- Figure out what the function does in-game, then write that C. That is the surest 100% path.
- Fix control flow before registers/stack.
- Use callers/callees from BRIEF.md.

### Cleaning Up Decompilation Artefacts

<artefact name="for-loops">
m2c turns `for (i = 0; i < 10; i++)` into `i = 0; if (i <= 10) { do { ... i++; } while (i <= 10); }` (comparison may flip).
</artefact>

<artefact name="gotos">
GOTOs are usually decomp artefacts. Restructure.
</artefact>

<artefact name="duplicated-variables">
One reused variable is more likely than a pile of temps.
</artefact>

<artefact name="shifts-instead-of-arithmetic">
`x >> 2` → `x / 4`; `x << 2` → `x * 4`.
</artefact>

<artefact name="false-returns">
An explicit `return` in m2c is often fall-through.
</artefact>

<artefact name="gcc-281-codegen">
- Signed `/` by a power of two: write `x / 4`, do not match the shift-and-bias by hand.
- Simple conditionals may be branchless (`slti` + `addu`); a ternary often matches.
- `do{}while(0)` is a last-resort scheduler hammer.
- Dead stores: an init (`result = 0`) before a branch can restore a missing write.
</artefact>
