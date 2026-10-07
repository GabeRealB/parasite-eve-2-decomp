# Handwritten early-image helpers

These routines live in the **early PSX executable image** (ROM `0x800`–
`0x2F50`). Linked as `.text` (and companion `.rodata`) ordered with early
rodata (`linker_section_order: .rodata` in `configs/USA/main.yaml`).
Permanent handwritten assembly (splat `type: hasm`, `hasm_in_src_path: True`).

After the last hasm unit, `boot` `.rodata` begins at `0x2F50`
(`Boot_BuildStamp` + `Boot_LoadInitialFile` jtbl from `src/main/boot.c`).
That is the first normal module `.rodata`; PsyQ `.rdata` follows.

| File | Symbol(s) | VRAM | Role |
|------|-----------|------|------|
| `fsDecompressStream.s` | `jtbl_Fs_DecompressChunk` + `fsDecompressStream` | `0x80010008` / `0x80010024` | Resume jump table + resumable LZSS into RAM for package payloads and image strips |
| `fsDecompressImagePayload.s` | `fsDecompressImagePayload` | `0x80010398` | Non-resumable LZSS for a complete image/CLUT payload into RAM |
| `tmdSkipStreamRecord.s` | `tmdSkipStreamRecord` | `0x800105AC` | Fallback record handler: steps over elements the current pass does not consume |
| `Tmd_StreamHandler_Prim32.s` | `Prim32` + alabel `tmdDrawStreamPrimG3PreXform` | `0x800105CC` / `0x800105F4` | Pre-transformed untextured gouraud triangles, one entry per prim code (0x32 blended / 0x30 opaque) |
| `Tmd_StreamHandler_Prim3A.s` | `Prim3A` + alabel `tmdDrawStreamPrimG4PreXform` | `0x800106F0` / `0x80010718` | Pre-transformed untextured gouraud quads, one entry per prim code (0x3A blended / 0x38 opaque) |
| `tmdDrawModelStream.s` | `tmdDrawModelStream` | `0x80010848` | Draw command groups with per-part GTE transforms and light matrices |
| `Tmd_DispatchStream.s` | `Tmd_DispatchStream` | `0x80010A20` | Stream walk + `jalr` handlers (callee of Setup) |
| `Tmd_StreamHandlers_Ops.s` | 20 handlers, one per record family | `0x80010A90`–`0x80012750` | TMD draw: completes and links each record's packet, each named for the command it serves |

## `fsDecompressStream` (jtbl + code in one file)

```yaml
- [0x808, .rodata, hasm/fsDecompressStream]   # sibling → same .s
- { start: 0x824, type: hasm, name: hasm/fsDecompressStream,
    linker_section_order: .rodata }
```

Source layout (same pattern as matched TUs with embedded jtbls):

```asm
.section .rodata, "a"
  dlabel jtbl_Fs_DecompressChunk
    .word .LfsStreamResumeTokenFlag, .LfsStreamResumeLiteral, ...   /* 7 resume labels */
.section .text, "ax"
  glabel fsDecompressStream
  ...
  lw a0, %lo(jtbl_Fs_DecompressChunk + 4*i)(a0)
```

Linker pulls **one object** twice into the early-image run:

1. `fsDecompressStream.s.o(.rodata)` — jump table
2. `fsDecompressStream.s.o(.text)` — decompressor

### Resume jump table

Not a GCC switch table: one `dlabel jtbl_Fs_DecompressChunk` with relocatable
mid-function labels for cooperative suspend when the CD sector buffer ends
(`D_8006C4D4`). Loads use `%lo(jtbl + 4*i)`; next call `jr`s to the entry.

| Index | Offset | Label | Meaning |
|-------|--------|-------|---------|
| 0 | +0x00 | `.LfsStreamResumeTokenFlag` | After flag-bit refill |
| 1 | +0x04 | `.LfsStreamResumeLiteral` | After literal path refill |
| 2 | +0x08 | `.LfsStreamFinishLiteral` | After literal 2nd-byte load |
| 3 | +0x0C | `.LfsStreamResumeMatchIndex` | After match-offset refill |
| 4 | +0x10 | `.LfsStreamFinishMatchIndex` | After match-offset 2nd byte |
| 5 | +0x14 | `.LfsStreamResumeMatchLength` | After match-length refill |
| 6 | +0x18 | `.LfsStreamFinishMatchLength` | After match-length 2nd byte |

## Why handwritten (not C)

1. **Opcodes** — signed `sub` / `addi` (GCC only emits `subu` / `addiu`).
2. **Mid-function resume** — fixed PCs in the jump table; C cannot invent them.
3. **Early-image layout** — not normal main `.text`.
4. **Hand schedule** — packed matrix composition and hand GTE ops (`tmdDrawModelStream`).

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
