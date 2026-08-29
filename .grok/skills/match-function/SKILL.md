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

That command resolves **any** overlay (`asm/<ver>/<overlay>/…`, including nested future units like `stage1/101`). Never search only `asm/USA/main/`. If `tools/giveups/<func>/base.c` exists, bootstrap copies that seed into `base.c` instead of m2c — start from it.

## Loop

1. Read `BRIEF.md` in the scratch dir.
2. `cd` there and run `./build.sh base.c` with **minimal** edits so it compiles. Do **not** rewrite m2c/give-up C from the asm before the first score.
3. Put each attempt in `base_N.c`. Prefer already-matched siblings in the same TU over m2c control flow.
4. Read `build.sh`'s **Penalties:** line. At ≥90% `build.sh` runs `./dump.sh` itself and prints a `.lreg`/`.greg`/`.dbr` summary plus a dump-delta vs the previous `base_N`. Pick the next edit from that mix:
   - `insert` / `delete` / `branch` → control flow still wrong. Fix C shape. Read `.jump` / `.jump2`.
   - `regs` → read `.lreg` / `.greg`. Shorten the loser's live range, **split a reused local**, or **unpin**. Do not add `register … asm("")` yet.
   - `reorder` → `.sched` / `.sched2` / `.dbr`. Statement order and delay slots.
   - `stack` → extra locals / frame; split or shrink locals.
5. Grep `DECOMPILATION_LEARNINGS.md` for the mismatch; do not read the whole file.
6. **Permuter:** if the best score is ≥ 95% and leftovers are registers / scheduling / stack (not control flow), run `./permute.sh --run --timeout 360 -j4 <func> <asm> <scratch>/base_N.c` **before** adding `register … asm("")` pins. Pins shrink the search. Vacuum also runs the permuter after you exit if you leave a ≥95% `base_N.c` in the scratch dir.
7. Pins are last resort. Function-scope `register T x asm("s4")` reserves that hard register for the **whole function** in GCC 2.8.1. Unpinning is often the 100% move.
8. Stop after ~10 attempts with no gain (or ~40 total). `build.sh` will also flag a stall.

## Compiler dumps

`./dump.sh base_N.c` reruns cc1 with `-da -dp` and prints a short summary (`DUMP.txt`). `build.sh` does this automatically at ≥90%. The kept `.s` comments like `# 31 movsi_internal2/5` are RTL insn uids.

| leftover | file |
|---|---|
| wrong `$sN` / swapped regs | `.lreg` `.greg` |
| fused const, `lb` vs `lh`, dropped `andi` | `.cse` `.cse2` `.combine` |
| loop IV / one walking pointer | `.loop` then `.cse2` |
| insn order, load-delay `nop` | `.sched` `.sched2` |
| empty delay slot after `jal`/`beq` | `.dbr` |
| merged tails, extra `j` | `.jump` `.jump2` |
| dead store / `REG_DEAD` | `.flow` |

Diff the dump that changed between attempts. Same `.i` is deterministic. If the kept `.s` matches and the `.o` does not, the bug is maspsx (`--expand-div`), not GCC.

## Finish

- **100%:** replace `INCLUDE_ASM` in the host C file. New types go in **this overlay's** `include/` (`include/main/`, `include/gameplay/`, `include/<overlay>/`). Not `include/main/unknown_syms.h`. Run `./tools/build-and-verify.sh` and wait for `✅ BUILD SUCCEEDED`. Commit `matched <func> <attempts>`.
- **Give up:** append `tools/difficult_functions` as `<func> <attempts> <best%>`. Revert host C.
- If vacuum launched you, **leave the scratch dir** (best unpinned `base_N.c` included). Vacuum archives it to `tools/giveups/<func>/` then cleans the scratch. Otherwise delete the scratch after commit/give-up.
- If the vacuum prompt says this is a disposable worktree (`pe2-wt-<func>`), commit **only on this worktree**. Do not `git merge` onto trunk and do not append `tools/difficult_functions` there; a later port agent on the original tree lands the result.
- If the vacuum prompt says you are the **port** agent: rewrite the function onto current trunk. Never `git merge` / rebase / cherry-pick the worktree. Adapt to structs and TUs other agents already landed, then verify and commit on trunk.
- **Stale `build/`:** worktrees copy existing binaries that still checksum from the INCLUDE_ASM version. `sha256sum --check` on `build/USA/out/SLUS_010.42` is not a match. Only `./tools/build-and-verify.sh` → `✅ BUILD SUCCEEDED` counts. If `python3` lacks splat/spimdisasm, use `venv/bin/python3`.

No pointer arithmetic with manual offsets. No `void*` when a struct is known. C89: declarations first, no comments in the matched function.
