---
name: match-function
description: Match one Parasite Eve 2 function so GCC 2.8.1 -O2 -mips1 output is byte-identical. Use when asked to decompile or match a function, when vacuum hands off a function, when replacing INCLUDE_ASM, or when the user runs /match-function.
---

# Match one function

Vacuum or `./tools/claude --bootstrap-only` may already have created `nonmatchings/<func>/` with `base.c`, `target.o`, and `BRIEF.md`. If that directory exists, **do not** re-run `./tools/claude`.

If it does not exist, from the repo root:

```
./tools/claude --bootstrap-only <func>
```

That command resolves **any** overlay (`asm/<ver>/<overlay>/…`, including nested future units like `stage1/101`). Never search only `asm/USA/main/`.

## Loop

1. Read `BRIEF.md` in the scratch dir.
2. `cd` there and run `./build.sh base.c`. Only then rewrite.
3. Put each attempt in `base_N.c`. Prefer already-matched siblings in the same TU over m2c control flow.
4. Grep `DECOMPILATION_LEARNINGS.md` for the mismatch; do not read the whole file.
5. **Permuter:** if the best score is ≥ 95% and leftovers are registers / scheduling / stack (not control flow), run `./permute.sh --run --timeout 360 -j4 <func> <asm> <scratch>/base_N.c` **before** adding `register … asm("")` pins. Pins shrink the search. Vacuum also runs the permuter after you exit if you leave a ≥95% `base_N.c` in the scratch dir.
6. Stop after ~10 attempts with no gain (or ~40 total). `build.sh` will also flag a stall.

## Finish

- **100%:** replace `INCLUDE_ASM` in the host C file. New types go in **this overlay's** `include/` (`include/main/`, `include/gameplay/`, `include/<overlay>/`). Not `include/main/unknown_syms.h`. Run `./tools/build-and-verify.sh`. Commit `matched <func> <attempts>`.
- **Give up:** append `tools/difficult_functions` as `<func> <attempts> <best%>`. Revert host C.
- If vacuum launched you, **leave the scratch dir** (best unpinned `base_N.c` included). Otherwise delete it after commit/give-up.

No pointer arithmetic with manual offsets. No `void*` when a struct is known. C89: declarations first, no comments in the matched function.
