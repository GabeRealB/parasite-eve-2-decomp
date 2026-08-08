# Handwritten early-image helpers

These routines live in the **early PSX executable image**, linked as `.text`
(and companion `.rodata`) ordered with early rodata
(`linker_section_order: .rodata` in `configs/USA/main.yaml`). Permanent
handwritten assembly (splat `type: hasm`, `hasm_in_src_path: True`).

| File | Symbol(s) | VRAM | Role |
|------|-----------|------|------|
| `Fs_DecompressChunk.s` | `jtbl_Fs_DecompressChunk` + `Fs_DecompressChunk` | `0x80010008` / `0x80010024` | Resume jump table + resumable LZ for FS CD chunks |
| `Fs_DecompressImage.s` | `Fs_DecompressImage` | `0x80010398` | Non-resumable LZ for image strips → VRAM |
| `Tmd_StreamHandler_Default.s` | `Tmd_StreamHandler_Default` | `0x800105AC` | Default stream handler (index/stride) |
| `Tmd_StreamHandler_Prim32.s` | `Prim32` + alabel `Prim30` | `0x800105CC` / `0x800105F4` | GTE prim stream (codes 0x32 / 0x30) |
| `Tmd_StreamHandler_Prim3A.s` | `Prim3A` + alabel `Prim38` | `0x800106F0` / `0x80010718` | GTE prim stream (codes 0x3A / 0x38) |
| `Tmd_SetupGteMatrices.s` | `Tmd_SetupGteMatrices` | `0x80010848` | TMD draw: GTE light matrices + transforms |
| `Tmd_DispatchStream.s` | `Tmd_DispatchStream` | `0x80010A20` | Stream walk + `jalr` handlers (callee of Setup) |

## `Fs_DecompressChunk` (jtbl + code in one file)

```yaml
- [0x808, .rodata, hasm/Fs_DecompressChunk]   # sibling → same .s
- { start: 0x824, type: hasm, name: hasm/Fs_DecompressChunk,
    linker_section_order: .rodata }
```

Source layout (same pattern as matched TUs with embedded jtbls):

```asm
.section .rodata, "a"
  dlabel jtbl_Fs_DecompressChunk
    .word .L800100F4, .L80010148, ...   /* 7 resume labels */
.section .text, "ax"
  glabel Fs_DecompressChunk
  ...
  lw a0, %lo(jtbl_Fs_DecompressChunk + 4*i)(a0)
```

Linker pulls **one object** twice into the early-image run:

1. `Fs_DecompressChunk.s.o(.rodata)` — jump table  
2. `Fs_DecompressChunk.s.o(.text)` — decompressor  

### Resume jump table

Not a GCC switch table: one `dlabel jtbl_Fs_DecompressChunk` with relocatable
mid-function labels for cooperative suspend when the CD sector buffer ends
(`D_8006C4D4`). Loads use `%lo(jtbl + 4*i)`; next call `jr`s to the entry.

| Index | Offset | Label | Meaning |
|-------|--------|-------|---------|
| 0 | +0x00 | `.L800100F4` | After flag-bit refill |
| 1 | +0x04 | `.L80010148` | After literal path refill |
| 2 | +0x08 | `.L80010180` | After literal 2nd-byte load |
| 3 | +0x0C | `.L80010200` | After match-offset refill |
| 4 | +0x10 | `.L80010238` | After match-offset 2nd byte |
| 5 | +0x14 | `.L800102A0` | After match-length refill |
| 6 | +0x18 | `.L800102E0` | After match-length 2nd byte |

## Why handwritten (not C)

1. **Opcodes** — signed `sub` / `addi` (GCC only emits `subu` / `addiu`).
2. **Mid-function resume** — fixed PCs in the jump table; C cannot invent them.
3. **Early-image layout** — not normal main `.text`.
4. **Hand schedule** — zero-frame `$t*` machine / hand GTE ops (`Tmd_SetupGteMatrices`).

## Build notes

- `hasm_in_src_path`: sources under `src/main/hasm/` (survive `asm/` wipe).
- Reconfigure does **not** overwrite existing hasm `.s` files.
- `ninja_config.py` assembles `hasm` with the same `as` rule as `asm`.

## Do not

- Split the jtbl into a separate TU
- Convert to `type: c` / `INCLUDE_ASM` without fixing layout + resume PCs
- Expect a 100% pure-C match from decomp-permuter
- Leave splat’s `nonmatching` macro on these files — that emits
  `Symbol.NON_MATCHING` markers meaning “not decompiled yet”. These units
  are finished as hasm; only `glabel` / `dlabel` / `endlabel` belong here.
