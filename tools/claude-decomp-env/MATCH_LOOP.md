# Matching loop (GCC 2.8.1)

Score with `./build.sh`. 100% is a match. Read the **Penalties:** line (`stack` / `branch` / `regs` / `reorder` / `insert` / `delete`).

1. First `./build.sh base.c` is a **baseline**. Minimal edits so it compiles. Do not rewrite m2c or a give-up seed from the asm before that score.
2. Prefer already-matched siblings in the same TU (BRIEF.md) over m2c gotos. Write each attempt as `base_N.c`.
3. Plan the next edit from the **penalty mix + dumps**, not from the object-dump / asm-differ alone.
4. Search the learnings corpus with `python3 tools/learn.py <terms>` — it ranks whole
   sections, where a raw grep returns hundreds of context-free lines ("delay slot"
   appears in 286 sections but only 16 titles). `--show N` prints the top N in full.
   Never read `DECOMPILATION_LEARNINGS.md` end-to-end; it is 1.3 MB.
   If `CODEGEN_MODEL.md` is present, read it first — it is the general model the
   corpus entries are instances of, and it is short.

`build.sh` keeps the `.s` with RTL insn uids (`# 31 movsi_internal2/5`). At ≥90% it runs `./dump.sh` and prints a **NEXT:** line naming dump files. The printed summary is not enough — **open those files** (`base_N.i.lreg`, …) before the next C edit. You can also run `./dump.sh base_N.c` by hand.

| leftover | file | what to do |
|---|---|---|
| `insert` / `delete` / `branch` ≠ 0 | `.jump` `.jump2` | Control flow is still wrong. Fix C shape. Pins and the permuter will not finish this. |
| `regs` | `.lreg` `.greg` | Shorten the loser's live range, **split a reused local**, or **unpin**. |
| `reorder` | `.sched` `.sched2` `.dbr` | Statement order and delay slots (store vs `mflo`/`lbu`/`jal`). |
| `stack` | extra locals / frame | Split or shrink locals. |
| fused const, `lb` vs `lh`, dropped `andi` | `.cse` `.cse2` `.combine` | |
| loop IV / one walking pointer | `.loop` then `.cse2` | |
| dead store / `REG_DEAD` | `.flow` | |

A 93% score with `branch`/`insert`/`delete` still non-zero is a **control-flow** miss, not a register-coloring miss.

## Pins

`register T x asm("s4")` is function-scope in GCC 2.8.1: it reserves that hard register for the **whole function**. Do not add pins because `$s4` is wrong in the object dump.

- Do not pin until dumps say a live range is the leftover **and** an unpinned attempt exists.
- If the seed already has pins, **unpin and rescore** as its own `base_N.c`. Unpinning is often the 100% move.
- Never treat a pinned ≥90% as the best seed. Leave an unpinned `base_N.c` in the scratch dir.

## Empty asm

Prefer the named helpers in `include/decomp/common.h` over raw empty `asm` / `asm volatile`. `TOUCH_REG(x)` is `"+r"` (blocks CSE / copy-prop). `TOUCH_REG_USE(x, y)` is `"+r"(x)` plus a keep-live `"r"(y)`. `USE_REG(x)` is input-only (keeps live); `SOFT_USE_REG` is the non-volatile form. `SCHED_BARRIER()` vs `SOFT_BARRIER()` is volatile vs not — that is a matching difference. Do not wrap them in `do { } while (0)` or extra braces. Instruction-emitting `lui`/`lo`/`sll` stays written out. `register T x asm("v0")` is still a pin, not these macros.

## Permuter

Only if best ≥ 95% **and** leftovers are `regs` / `reorder` / `stack` (`branch`=`insert`=`delete`=0). From the project root, **before** pins:

```
./permute.sh --run --timeout 360 -j4 $functionName <asm-path-from-BRIEF> base_N.c
```

Vacuum also permutes after you exit if you leave a ≥95% unpinned `base_N.c`. Give up after ~10 attempts with no gain, or ~40 total. On stall, `build.sh` tells you to stop.

If the kept `.s` matches and the `.o` does not, the bug is maspsx (`--expand-div`), not GCC.

## Shared bodies

Overlays in a family share a lot of code — 44% of the room functions are still
copies of another room's. Before matching one, check:

```
python3 tools/overlay_dup_index.py find <function>
```

If it reports copies, the body is worth matching **once**: put it in
`src/<family>/lib/<unit>.c`, add the span to `configs/USA/overlays.toml` and the
shared symbol to each sharer's `configs/USA/sym/<family>/<overlay>.txt`, and one
object is linked into every overlay that uses it. `=` in that listing means the
copies are byte-identical, `~` means the same body at a different link offset.

This is not hypothetical: of the first 158 room functions matched, only 29 were
distinct bodies — one two-instruction stub was matched 112 times.

## Verify

Worktrees copy `build/` binaries that still checksum from `INCLUDE_ASM`. `sha256sum --check` on `build/USA/out/SLUS_010.42` is **not** a match.

Verify in two steps, both from the **repo / worktree root**:

```
./tools/build-and-verify.sh --only <scope>    # inner loop, seconds
./tools/build-and-verify.sh                   # the finish line
```

`<scope>` is the `Build scope:` line in BRIEF.md — the overlay basename (`gameplay`, `m93r`), or a family (`core`, `weapons`). A scoped run splits, builds and checksums only those units and leaves every other overlay's `asm/` and `linkers/` alone, so it is safe to run repeatedly while iterating: 1.6s for one weapon overlay and 4s for gameplay, against 18s for the project.

A scoped `✅ SCOPED BUILD SUCCEEDED` says **nothing** about the overlays it skipped, and it says so in the message. Only the bare `./tools/build-and-verify.sh` printing `✅ BUILD SUCCEEDED` counts as a match — run it before committing or reporting one. Struct changes are the case that bites: they reach every overlay that shares the type, and the scoped run cannot see it.

If `python3 ninja_config.py` fails on splat/spimdisasm, use `venv/bin/python3`.
