---
name: match-function
description: Match one Parasite Eve 2 function so GCC 2.8.1 -O2 -mips1 output is byte-identical. Use when asked to decompile or match a function, when vacuum hands off a function, when replacing INCLUDE_ASM, or when the user runs /match-function.
---

# Match one function

The matching loop (dumps, pins, permuter, verify) lives in `tools/claude-decomp-env/MATCH_LOOP.md` and is loaded as a Grok project rule (`.grok/rules/match-loop.md`). **Follow it.** Do not plan edits from asm-differ alone. Do not add `register … asm("")` because a saved register is wrong in the object dump.

Vacuum or `./tools/claude --bootstrap-only` may already have created `nonmatchings/<func>/` with `base.c`, `target.o`, and `BRIEF.md`. If that directory exists, **do not** re-run `./tools/claude`.

If it does not exist, from the repo root:

```
./tools/claude --bootstrap-only <func>
```

That command resolves **any** overlay (`asm/<ver>/<overlay>/…`, including nested future units like `stage1/101`). Never search only `asm/USA/main/`. If `tools/giveups/<func>/base.c` exists, bootstrap copies that seed into `base.c` instead of m2c — start from it.

## This session

1. Read `BRIEF.md` in the scratch dir.
2. `cd` there and run `./build.sh base.c` with **minimal** edits so it compiles. Do **not** rewrite m2c/give-up C from the asm before the first score. If the seed is pinned, unpin as `base_1.c` and rescore.
3. Put each attempt in `base_N.c`. Prefer already-matched siblings in the same TU over m2c control flow.
4. After each ≥90% score, `build.sh` prints a **NEXT:** line. The next tool call is **reading those dump files**, then one C edit. Control-flow leftovers (`branch`/`insert`/`delete` ≠ 0) are not a pin/permuter job.
5. Grep `DECOMPILATION_LEARNINGS.md` for the mismatch; do not read the whole file.
6. Permuter only at ≥ 95% with `branch`=`insert`=`delete`=0, **before** pins. Vacuum also permutes after you exit if you leave a ≥95% unpinned `base_N.c`.
7. Stop after ~10 attempts with no gain (or ~40 total). `build.sh` will also flag a stall.

## Finish

- **100%:** replace `INCLUDE_ASM` in the host C file. New types go in **this overlay's** `include/` (`include/main/`, `include/gameplay/`, `include/<overlay>/`). Not `include/main/unknown_syms.h`. Run `./tools/build-and-verify.sh` and wait for `✅ BUILD SUCCEEDED`. Commit `matched <func> <attempts>`.
- **Give up:** append `tools/difficult_functions` as `<func> <attempts> <best%>`. Revert host C.
- If vacuum launched you, **leave the scratch dir** (best unpinned `base_N.c` included). Vacuum archives it to `tools/giveups/<func>/` then cleans the scratch. Otherwise delete the scratch after commit/give-up.
- If the vacuum prompt says this is a disposable worktree (`pe2-wt-<func>`), commit **only on this worktree**. Do not `git merge` onto trunk and do not append `tools/difficult_functions` there; a later port agent on the original tree lands the result.
- If the vacuum prompt says you are the **port** agent: rewrite the function onto current trunk. Never `git merge` / rebase / cherry-pick the worktree. Adapt to structs and TUs other agents already landed, then verify and commit on trunk.
- **Stale `build/`:** worktrees copy existing binaries that still checksum from the INCLUDE_ASM version. `sha256sum --check` on `build/USA/out/SLUS_010.42` is not a match. Only `./tools/build-and-verify.sh` → `✅ BUILD SUCCEEDED` counts. If `python3` lacks splat/spimdisasm, use `venv/bin/python3`.

No pointer arithmetic with manual offsets. No `void*` when a struct is known. C89: declarations first, no comments in the matched function.
