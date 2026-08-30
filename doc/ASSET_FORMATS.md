# Parasite Eve 2 — asset & stage formats

What we know about on-disc stage data (`STAGE0.HED` / `STAGE*.CDF`), chunk
types, sector packing, compression, and image load paths. Derived from the
retail USA discs, the matching decomp (`src/main/fs.c`, hasm decompressors),
and the extract/pack tools under `tools/peassets/`.

Implementation references:

| Area | Code |
|---|---|
| Binary layout constants | `tools/peassets/format.py` |
| Extract / pack | `tools/peassets/extract.py`, `pack.py` |
| Shared inflate/decode | `tools/peassets/asset_decode.py` (extract + viewer) |
| LZSS decode / format | `tools/peassets/lzss.py`, `src/main/hasm/Fs_DecompressChunk.s` |
| LZSS encode (policies, identity) | `doc/LZSS_ENCODER.md`, `lzss.py` / `lzss_clut.py` / `lzss_cascading.py` |
| Images / CLUT | `tools/peassets/image_codec.py`, `src/main/fs.c` |
| Asset database | `tools/peassets/asset_db.py`, `asset_data.py` |
| Runtime FS loader | `src/main/fs.c`, `include/main/fs.h` |
| **CD streams (MTS audio + STR movie)** | [`STREAM_FORMATS.md`](STREAM_FORMATS.md), `mts_codec.py`, `str_codec.py` |
| **Overlays (RAM slots, rooms, models)** | [`OVERLAYS.md`](OVERLAYS.md) |
| **Models (TMD)** | [`TMD_FORMAT.md`](TMD_FORMAT.md), `tools/peassets/pkg_model.py`, `src/main/tmd.c` |
| Animation in packages | `tools/peassets/pkg_anim.py`, `src/gameplay/1BC.c` |
| Assets embedded in the executables | `tools/peassets/exe_assets.py` |

---

## 1. Discs and stage containers

USA PE2 is two discs. Stage content lives in ISO files such as:

| File | Role |
|---|---|
| `STAGE0.HED` | Stage 0 file table + streaming list (fixed size `0x1B80`) |
| `STAGE0.CDF` | Stage 0 file payloads (flat file list) |
| `STAGE1.CDF` … `STAGE5.CDF` | Per-stage **folders** (rooms / areas), each with its own file table |

Tooling layout after extract (`assets/USA/`):

```text
raw/pe2pkg/ pe2img/ …   deduplicated **on-disc** clean payloads
                         name = ASSETS id (sha1), else {type}_{n}
pe2pkg/ pe2img/ …        **inflated** edit forms (one per unique raw)
                         pe2pkg = LZSS-decoded; pe2img/pe2clut = PNG + meta
raw/audio/ audio/        MTS CD streams (not file chunks)
raw/movie/ movie/        STR movies from INTER*.STR / STAGE*.CDF
stage0/ stageN/          pack sidecars only (trailer.bin, layout.json, …)
stages.json              pack manifest (paths into inflated type dirs)
```

Duplicate payloads are stored once under `raw/{type}/`. Inflate walks unique
raw files only. Extract’s in-memory chunk map is folded into `stages.json`
(no separate `assets_map.json`).

Pack matching mode prefers `raw/{type}/` for LZSS types; edit inflated dirs
for hybrid/decoded packs.

---

## 2. STAGE0 vs STAGE1–5

### STAGE0 (`STAGE0.HED` + `STAGE0.CDF`)

- Flat list of files in HED order (`file_id`, sector-aligned `file_offset`).
- **Streaming list** (movies / audio): first `3 × 0x28` bytes of the HED —
  see [`STREAM_FORMATS.md`](STREAM_FORMATS.md).
- Also: `INTER0.STR` / `INTER1.STR` on the ISO for FMV (not in the CDF).
- Chunks land in type dirs; pack sidecars under `stage0/<fileName>/`
  (default `file{id}`, or a friendly name from `TREE`).

### STAGE1–5 (`STAGEn.CDF`)

- Leading folder table: up to `0x100` entries of `(folder_id, folder_size)`.
- Each folder is a sector-aligned block:
  - file list (`0xA2` slots)
  - **streaming list** at folder + `0x514` (`18 × 0x28`)
  - then file payloads (and often stream data after the last file)
- Chunks land in type dirs; pack sidecars under `stageN/<folderName>/file…/`.

Folder and file **keys** in `stages.json` use friendly names when set in
`TREE`; otherwise `file12` / decimal folder id. Order is always on-disc
order so pack recovers numeric ids.

---

## 3. Chunk layout (on-disc)

Each stage **file** is a sequence of **chunks**. Chunks are the units that
become individual assets (`.pe2pkg`, `.pe2img`, `.bs`, …). Implementation:
`tools/peassets/format.py` (`FileChunkHeader`, `unpack_chunk_payload`,
`pack_chunk_payload`).

### 3.1 Hierarchy

```text
STAGE*.CDF / stage0 file table
  └── file  (sector-aligned offset in container)
        └── chunk 0   [header @ sector0][payload sectors…]
        └── chunk 1   …
        └── chunk N   (end_flag = 0xFF)
```

- A **file** may contain one or many chunks back-to-back (on-disc `end_flag`
  chains them; pack sets last chunk → End, others → Continue).
- Chunk **byte size** on disc is always a multiple of the CD sector size
  **`0x800`**. On-disc size is **inferred** from clean payload length and
  `sector_len` at pack time (not stored in `stages.json`).

### 3.2 Chunk header (`0x10` bytes)

Little-endian. Present only once, at the start of the **first CD sector** of
that chunk (not repeated on continuation sectors):

| Offset | Size | Field | Notes |
|---|---|---|---|
| `0x0` | `u8` | `type` | Chunk type (see §3.5) |
| `0x1` | `u8` | `end_flag` | `0x01` = another chunk follows this one in the file; `0xFF` = last chunk |
| `0x2` | `u16` | `sector_len` | Exclusive end of **valid** data in each sector buffer |
| `0x4` | `u32` | `chunk_size` | Total on-disc size in **bytes** (multiple of `0x800`) |
| `0x8` | `u32` | `load_addr` | RAM load address for room packages / cap2; else `0` |
| `0xC` | `u32` | pad | Always `0` |

On disc, `chunk_size` is a **byte** length (e.g. `0x3800` = 7 sectors);
sector count is `chunk_size / 0x800`. Pack computes it from the clean asset
size and `sector_len` via `format.pack_chunk_payload` — it is **not** a
`stages.json` field.

### 3.3 Multi-sector payload layout and `sector_len`

CD sector size is **`0x800`**. The loader maps each sector into `Fs_CdSector`
and only treats bytes **below `sector_len`** as valid payload.

`sector_len` is an **exclusive end offset** inside the sector buffer. It must
cover the header on sector 0:

- Valid range: **`(0x10, 0x800]`** (`format.parse_sector_len` /
  `validate_sector_len`).
- Default when omitted from `stages.json` is **`0x800`**; extract only writes
  the field when retail differs from that default.

```text
Sector 0 of a chunk (first sector only carries the header):
  [0x00 .. 0x10)           16-byte chunk header
  [0x10 .. sector_len)     valid payload  (“first_cap” bytes)
  [sector_len .. 0x800)    padding (ignored by the game)

Sector 1, 2, … of the same chunk:
  [0x00 .. sector_len)     valid payload  (“cont_cap” bytes)
  [sector_len .. 0x800)    padding
```

Common `sector_len` values:

| `sector_len` | Valid bytes / first sector (`first_cap`) | Valid bytes / later sector (`cont_cap`) |
|---|---|---|
| `0x800` (most chunks) | `0x7F0` | `0x800` |
| `0x600` | `0x5F0` | `0x600` |
| `0x500` | `0x4F0` | `0x500` |

```text
first_cap = sector_len - 0x10
cont_cap  = sector_len
```

**Clean payload** (sector strip before inflate/store; what pack re-pads) is the
concatenation of every valid range **with the header stripped**:

```text
clean_len = first_cap + (n_sectors - 1) * cont_cap
          = (sector_len - 0x10) + (n_sectors - 1) * sector_len
```

where `n_sectors` is derived from the on-disc blob (extract) or from the
minimum sectors needed for the clean payload (pack).

Runtime: `D_8006C4D4 = &Fs_CdSector + sector_len` bounds decompress input
(`src/main/fs.c`).

### 3.4 Extract vs pack

| Direction | Function | Behaviour |
|-----------|----------|-----------|
| **Extract** | `unpack_chunk_payload` | Read full multi-sector blob → strip header + pad → inflate → **type store** |
| **Pack** | `pack_chunk_payload` | Clean payload + `sector_len` → re-stride into sectors with pad; set header `chunk_size` |

How many sectors a clean payload needs (pack):

```text
if len(payload) <= first_cap:
    n_sectors = 1
else:
    n_sectors = 1 + ceil( (len(payload) - first_cap) / cont_cap )
chunk_size = n_sectors * 0x800
```

If re-encode grows the payload, pack uses **more** sectors than retail as needed.

**Legacy naive extract** stored `sec0[0x10:0x800] + sec1[0:0x800] + …`, which
embeds pad when `sector_len < 0x800`. Prefer re-extract, or
`format.ensure_clean_payload` / `strip_naive_payload_padding`.

Measure production re-encode vs retail sector counts:

```text
python3 tools/peassets/lzss_roundtrip_report.py --log layout_diff.log
```

### 3.5 Chunk types

| `type` | Name | Extension | Loader (main) | Notes |
|---|---|---|---|---|
| `0x0` | Room package | `.pe2pkg` | `Fs_DecompressChunk` | LZSS body; overlays load at `load_addr` |
| `0x1` | Image | `.pe2img` | `Fs_CopyWorkEntries` + `Fs_LoadImageStrip` | Sequential LZSS strips → VRAM |
| `0x2` | Color lookup table | `.pe2clut` | `Fs_LoadImageChunk` + `Fs_DecompressImage` | 16-byte image header + LZSS → ABGR1555 |
| `0x4` | Dialogue / cap2 | `.pe2cap2` | (package-like) | Often has load address |
| `0x5` | Room background | `.bs` | MDEC path | PSX BS v2 → PNG (320×240) |
| `0x6` | Music | `.spk` | SndLoad / SPU | `hSPK` bank → WAV samples |
| `0x7` | Ascii | `.txt` | | |

There is no type `0x3` in the tables we use.

### 3.6 What the clean payload is (by type)

After sector unpack, the clean bytes are:

| Type | Clean payload contents |
|------|------------------------|
| Room package | One LZSS stream (entire body) |
| Image | Work-entry table + sequential independent LZSS strips |
| CLUT | 16-byte image-chunk header + one LZSS colour stream |
| Others | Opaque bytes (music, text, `.bs`, …) as stored |

LZSS bitstream details: §4. Encode policies: [`LZSS_ENCODER.md`](LZSS_ENCODER.md).

---

## 4. LZSS compression

Used by:

- `.pe2pkg` room packages (entire body after sector strip)
- `.pe2img` strips (each strip is its own stream)
- `.pe2clut` colour data after the 16-byte image-chunk header

Algorithm family (same as `Fs_DecompressChunk` / md_hyena-style):

- 256-byte ring dictionary
- Flag bit: `1` = literal byte, `0` = back-reference
- Back-ref: 8-bit offset, 4-bit length; copy `length + 2` bytes
- Offset `0` = end of stream
- Dictionary correction: stored offset is decremented by 1 before use

Implementation: `tools/peassets/lzss.py` (decode / trim / pack / production
encode). Runtime: `Fs_DecompressChunk.s` (resumable, CD-fed) and
`Fs_DecompressImage.s` (non-resumable, in-memory). Same bitstream for both.

**Encode policies, identity scores, multi_max research, and tool map:** see
[`LZSS_ENCODER.md`](LZSS_ENCODER.md).

## 5. Room packages (`.pe2pkg`)

- Chunk type `0x0`.
- Body is one LZSS stream (after sector_len strip).
- `load_addr` is the RAM load address when non-zero (e.g. title / gameplay
  overlays at `0x80093800`).
- Unique inflated bodies live under `pe2pkg/` (stem from `ASSETS` or
  `pe2pkg_N`); decomp overlays use those files directly (e.g.
  `pe2pkg/title.pe2pkg`).

“Room package” is the on-disc type name. Only stages 1–5 actually use these
as **per-room** overlays (`0x8017D5C0`). Stage 0 uses the same type for
gameplay, title, Aya, weapons, actors, maps, and menus — see
[`OVERLAYS.md`](OVERLAYS.md).

Examples:

| Friendly name | Canonical id | Role |
|---|---|---|
| `gameplay` | `stage0/file0/1.pe2pkg` | Resident in-game overlay |
| `title` | `stage0/file1/5.pe2pkg` | Title / demo / menu overlay |

---

## 6. Images (`.pe2img`) — type 1

### 6.1 On-disk structure

```text
Work-entry table (8 bytes each), until x == 0xFFFF:
  u16 x          VRAM X (column origin, in halfwords)
  u16 y          VRAM Y
  u32 offset     only the *first* entry’s offset is used as stream start;
                 later offsets are not seeks in the load path we reverse

Terminator:
  u16 0xFFFF
  u16 term_y     may override column height when this is the *second* slot
  u32 0

Then: sequential LZSS strips (see below)
```

### 6.2 Load path (`fs.c`)

1. `Fs_CopyWorkEntries` copies the table into `Fs_WorkEntries`.
2. Sets `Fs_ImageRect = { x: entry0.x, y: entry0.y, w: 0x40, h: 0x20 }`.
3. Sets remaining height `D5B498_8006ACD4 = 0x100` (unless single-column
   terminator special-cases `0x40` or `term_y & 0x7FFF` when `term_y & 0x8000`).
4. Points the decompress cursor at `base + entry0.offset`.
5. `Fs_LoadImageStrip` loop:
   - Decompress one strip into a scratch buffer (`0x1000` bytes).
   - `LoadImage2` that strip to VRAM.
   - `y += 0x20`, `ACD4 -= 0x20`.
   - Re-init decompress state (`ADE1`) for the next strip.
   - When a column is finished, advance to the next work entry for a new
     VRAM `x` / `y`, **without** seeking; the compressed stream continues.

So: **columns are sequential in the compressed stream**, not stored as
independent blobs at each entry’s `offset`.

### 6.3 Geometry

| Quantity | Value |
|---|---|
| Strip width | `0x40` halfwords |
| Strip height | `0x20` lines |
| Strip payload | `0x1000` bytes |
| Default column height | `0x100` lines → **8 strips** |
| Pixel width of one column | halfwords × (16 / bpp) |

### 6.4 BPP and colour

VRAM transfer is always halfword-oriented. Texture depth is separate:

| BPP | Pixels per halfword | Typical use |
|---|---|---|
| 16 | 1 (ABGR1555) | Direct colour |
| 8 | 2 (indices) | Textures / UI with 256-colour CLUT |
| 4 | 4 (indices) | Fonts / small UI with 16-colour CLUT |

BPP is **not stored** on disc. Extract and the viewer default to `guess_bpp()`
(halfword chroma + unique-count). Override on the unique blob in `ASSETS`
(one depth, or one per work-entry column):

```python
ASSETS["pe2img_2"]["bpp"] = 8
ASSETS["pe2img_455"]["bpp"] = [4, 8]  # font column + 8-bit bitmap column
```

The viewer Image tab has a **BPP** combo per column (`Auto` / `4` / `8` /
`16`). Auto uses `ASSETS[id].bpp` when set, else the guess. The chosen
depth is written to `pe2img/<stem>.pe2img.json` (`bpp`, `bpp_source`).

When a neighbouring `.pe2clut` exists (`N±1` by chunk index), the PNG exporter
applies it for 4/8 bpp (see §7.4 for multi-row selection). Examples:

- `stage0/title/1.pe2img` (`pe2img_0`) → 4 bpp font page at `(960, 256)`
- `stage0/file2/2.pe2img` + `1.pe2clut` → title chrome (`pe2img_2` is 8 bpp,
  two columns at x=768; clut at `(0, 255)`)
- `stage0/file30500/1.pe2img` + `0.pe2clut` → NEO ARK map (needs CLUT **row 1**,
  not row 0 — row 0 is a green monochrome palette)

Decoded viewable form: `pe2img/<stem>.png` plus
`pe2img/<stem>.pe2img.json` (bpp, height, entries, sector_len, …).

---

## 7. Colour lookup tables (`.pe2clut`) — type 2

### 7.1 On-disk structure

```text
FsImageChunk (0x10 bytes) — same layout as runtime struct in fs.h:
  u16 x, y     VRAM destination of the CLUT
  u16 h, w     height, width in halfwords (note: h then w in the struct)
  u8  pad[8]

Then: LZSS-compressed ABGR1555 colour data
  decompressed size = w * h * 2 bytes
```

Example: `w=256, h=6` → 1536 colours (six 256-colour palettes), common for
8 bpp textures. `w=16` / small heights → 4 bpp palettes.

### 7.2 Load path

`Fs_LoadImageChunk`:

1. Build `RECT` from `x,y,w,h` (with minor Y adjustments in some modes).
2. `Fs_DecompressImage` from bytes after the header.
3. `LoadImage2` the palette into VRAM.

Do **not** treat the bytes after the header as raw colours without LZSS.

### 7.3 How the game uses CLUTs (draw time)

Image and CLUT chunks are **independent**. There is no on-disk “this texture
uses that palette” link. The runtime:

1. Loads the texture into a VRAM region (from the pe2img work-entry `x,y`
   columns / strip path).
2. Loads the CLUT into another VRAM region (`FsImageChunk.x/y`, often
   `(0, 255)` for UI).
3. When building a primitive (`SPRT`, `POLY_FT4`, …), sets:
   - `tpage` → which texture page (where the pixels live)
   - `clut` → which **palette row**, via `getClut(x, y)` /
     `GetClut(x, y)` from `libgpu.h`:

   ```c
   #define getClut(x, y)  (((y)<<6)|(((x)>>4)&0x3f))
   ```

   That encodes the VRAM coordinates of the **start of one CLUT row**.
   For a multi-row CLUT loaded at `(cx, cy)` with height `h`:
   - row 0 → `getClut(cx, cy)`
   - row 1 → `getClut(cx, cy + 1)`
   - …
   - row `h-1` → `getClut(cx, cy + h - 1)`

Different draw sites can pick different rows of the **same** CLUT block
(e.g. a full-colour map palette vs a monochrome overlay). Example in main:
`Prim_DrawLoadingSprt` uses `GetClut(0, 0xFF)` — VRAM `(0, 255)`, which is
exactly where several stage0 CLUTs load.

### 7.4 Offline PNG colourisation

The exporter does **not** know which `getClut` Y a given texture will use.
Heuristic:

- Prefer a neighbouring `.pe2clut` (`N±1` by chunk index in the same file).
- If the CLUT has multiple rows (`h > 1`), score each row and pick the most
  **varied** one (avoid monochrome / single-hue rows that make maps look
  “all green”).

Concrete case — `stage0/file30500`:

| Chunk | Role |
|---|---|
| `0.pe2clut` | VRAM `(0, 255)`, `w=256`, `h=6` (six 256-colour rows) |
| `1.pe2img` | 8 bpp NEO ARK map texture (two columns at x=896, 960) |

- **Row 0**: ~27 unique greens → monochrome green map if used alone.
- **Row 1**: 255 varied colours → correct full-colour map (labels, water, UI).
- Rows 2–5: sparse / empty.

An earlier exporter that always used row 0 produced the all-green PNG; picking
the highest-scoring row recovers the real map. Meta JSON notes
`has_clut` / `clut_note` when a sibling palette was applied.

### 7.5 Title STAGE0 occupancy

HED file order (`stages.json` TREE names): **gameplay** (file 0), **title**
(file 1), **file2** (still + chrome). Boot brings in the title overlay from
file 1; `Title_InitTask` then `CdCmd_EnqueueLoadFile(1, …)` (same file) and
`Text_LoadClutImages`. Chrome/still are file 2.

| Path | Payload | VRAM (halfwords) | BPP |
|---|---|---|---|
| `title/1.pe2img` (`pe2img_0`) | UI font page | `(960, 256)` 64×256 | **4** |
| `title/2.pe2clut` (`pe2clut_0`) | Font TIM clut | `(256, 243)` 64×6 | — |
| `title/3.pe2img` (`pe2img_1`) | Extra title sheet | `(896, 256)` 64×256 | **8** |
| `title/4.pe2clut` (`pe2clut_1`) | Img1 clut | `(0, 240)` 256×3 | — |
| `title/title.pe2pkg` | Title overlay | RAM `0x80093800` | — |
| `file2/0.bs` (`bs_0`) | Title still | draw band 320×240 | 16-bit BS v2 |
| `file2/1.pe2clut` (`pe2clut_2`) | Chrome clut | `(0, 255)` 256×6 | — |
| `file2/2.pe2img` (`pe2img_2`) | Menu chrome | `(768, 256)` 128×256 | **8** (two columns) |

`pe2clut_0` **row 0 is all zero**. 4bpp font draws nothing until
`Text_LoadClutImages` overwrites `(256, 243)` (and the outline palettes at
`(0x3D0, 0x1FF)`). See §7.6.

Chrome clut `(0, 255)` `h=6` sits on the last draw-band line and the 32-line
CLUT gutter (`y=240…271`). Do not `isbg` a 256-high env at `y=240`.

**Chrome is a sprite atlas**, not C-string labels. `Title_DrawSpriteRow`:

| | |
|---|---|
| SPRT | `x0=-0x80`, `w=0x100`, `h=0x10`, `u0=0`, `v0=v`, code `0x66` (shaded + ABR) |
| clut | `0x3FC0` = `GetClut(0, 255)` |
| tpage | `0xE10002BC` — 8bpp, ABR 1, **dtd=1**, page origin `(768, 256)` |

Atlas `v` (texel Y in the chrome page):

| `v` | Row |
|---|---|
| `0x00` | Logo |
| `0x10` | Footer |
| `0x20` | Cursor |
| `0x30`… | Menu (New Game / Load Game / Option), 16px each |

### 7.6 UI font (main EXE) and `Text_LoadClutImages`

Glyph metrics are **not** a CDF chunk. They live in `SLUS_010.42`:

| Table | VA | Count | `glyphTable` | `vBias` (`func_8002E53C`) |
|---|---|---|---|---|
| `Font_Glyphs0` | `0x8005EFB0` | 224 (`0x20`…`0xFF`) | 0 | `0x26` |
| `Font_Glyphs1` | `0x8005FA30` | 224 | 1–4 | `0x80` |
| `Font_Glyphs2` | `0x800604B0` | 91 (`0x20`…`0x7A`) | 5 | `0` |

`Ui_DrawTextUnderline` sets `glyphTable = 5` (slot 2). Slot 1 letters live
128 lines down the 256-tall font page; drawing them with `vBias=0` samples
padding, not glyphs.

`FontGlyph` `u`/`v`/`w`/`h` are **page-local texels** in the 4bpp page at
`(960, 256)`. SPRT `w`/`h` are `glyph.w+1` / `glyph.h+1`. Pair-shrink is 2
except table 5 (1). `off_x` / `off_y` are stored bytes used as signed.

`Text_LoadClutImages` (`src/main/textutil.c`):

| Source | Size | `LoadImage` dest | GPU clut |
|---|---|---|---|
| `D_80060910` | 64 entries (4×16) | `(0x100, 0xF3)` = `(256, 243)`, `w=0x40`, `h=1` | fill rows at that origin |
| `D_800609B0` | 48 entries (3×16) | `(0x3D0, 0x1FF)`, `w=0x30`, `h=1` | `0x7FFD` / `0x7FFE` / `0x7FFF` |

UI strings (`Text_DrawGlyphImmediate` / `Queued`) use **`0x7FFD` only**.
Indices 0–10 are transparent; 11–15 are the letter. Dual SPRT (`0x7FFD` +
`0x7FFF`) is a second layer (`0x64` + `0x67`). Index 5 in the 4bpp page is
cell padding: the TIM/fill clut at `(256, 243)` maps it to mid-grey (one
box per glyph); `0x7FFD` keys it out.

---

### 7.7 Assets embedded in the executables

A few things are baked into `SLUS_010.42` rather than shipped as CDF chunks.
Nothing on disc references them, so the chunk walker cannot reach them - they
are catalogued by **address** in `asset_db.EMBEDDED_ASSETS` (source binary,
load-time VA, exact size, extension) and then go through the normal store like
any other asset:

```text
raw/mcsave/mc_save_header.mcsave        512 bytes, byte-identical to the
                                        main.exe slice, SHA-1 deduped
mcsave/mc_save_header/meta.json         inflated form in the type dir
                     /clut.png
                     /icon0.png … icon2.png
```

`extract.py` slices them right after `main.exe` is copied, so every mode gets
them, minimal included; the decoded form is produced by the usual
`materialize_inflated` pass, so a `--raw-only` run stores the bytes and stops.
`--skip-embedded` turns it off and a missing Pillow degrades to a warning.
`exe_assets.py` holds the slicing and the decoder.

Separately, the *build* gets the same bytes through a splat `databin` segment
writing to `assets/USA/incbin/` — a build input regenerated by every split, not
a catalogue asset. Both trees are gitignored, so game data stays out of git
either way.

#### Memory-card save header (`main.exe` `0x0516FC`, VA `0x80060EFC`)

One 512-byte PlayStation save header, the entry the game writes for a card that
failed to validate. Split across two symbols in the disassembly:
`Mc_DefaultChecksumSrc` is the first 4 bytes (and what `Mc_BufferSlots[0]`
points at), `Mc_SaveHeaderBody` the rest.

| Off | Size | Field |
|---|---|---|
| 0x00 | 2 | `"SC"` magic |
| 0x02 | 1 | icon flag `0x11`-`0x13`; low nibble is the frame count (3 here) |
| 0x03 | 1 | block count (1) |
| 0x04 | 0x3C | Shift-JIS title, NUL-terminated - `ＰＥ２　Ｄａｔａ　Ｃｏｒｒｕｐｔｅｄ．` |
| 0x60 | 0x20 | 16-entry 5-5-5 CLUT; entry 0 transparent |
| 0x80 | 0x80 x3 | icon frames, 16x16 4bpp, low nibble = left pixel |

The three frames animate a portrait dissolving into the teal PE energy effect.

Adding another embedded asset means one `EMBEDDED_ASSETS` entry plus, if it is
a new format, an extension in `TYPE_DIR_BY_EXT` and a branch in the materialize
dispatch. `exe_assets.scan_for_save_headers` is the signature scan that found
this block and can find it again if a version moves it; extraction itself
trusts the catalogue rather than scanning.

---

## 8. Room backgrounds (`.bs`)

- Chunk type `0x5`.
- PSX **BS version 2** MDEC bitstream (same demuxed frame format as STR v2):

  | Offset | Size | Field |
  |--------|------|--------|
  | 0 | u16 | `mdec_code_count_div2` |
  | 2 | u16 | magic `0x3800` |
  | 4 | u16 | frame quantisation scale |
  | 6 | u16 | version (`2`) |
  | 8 | … | VLC bitstream (16-bit LE words, MSB-first after byte-swap) |

- Macroblocks are 16×16, ordered **column-major** (top→bottom, then next
  column). Each MB is Cr, Cb, Y1, Y2, Y3, Y4 (8×8 blocks). PE2 retail
  backgrounds are **320×240** (20×15 MBs).
- Extract: `raw/bs/*.bs` (on-disc) → `bs/*.png` + `*.bs.json` via
  `tools/peassets/bs_codec.py` / `asset_decode.materialize_bs_asset`.
- Pack **matching** prefers `raw/bs/` for bit-identity. Hybrid/decoded (or
  matching with missing raw) re-encodes PNG→BS v2 via `encode_bs_v2` (lossy
  DCT; uses meta `quant_scale` when present).
- Often appear with a CLUT and/or image overlay in the same stage file.

---

## 9. Models and animation

There is **no separate model or animation chunk type**. Both live inside the
`.pe2pkg` overlay packages, alongside the room/actor code that uses them — see
[`OVERLAYS.md`](OVERLAYS.md) for which package is which.

### 9.1 Where they live

The index is in one binary and the content is in another. `Gp_PlayerAnimBlkTbl`
and `Gp_AnimBlkTbl` sit in the **gameplay** overlay's `.data`, but the addresses
they hold point into *other* RAM slots — the packages that get loaded on top:

| Table | VA | Entries | Targets | Slot |
|---|---|---|---|---|
| `Gp_PlayerAnimBlkTbl` | `0x80112D6C` | 34 | `0x8012A0D8`–`0x8012EDD0` | weapons, `0x8011D1C0` |
| `Gp_AnimBlkTbl` | `0x80113368` | 8 | `0x8016CB98`–`0x801772E8` | named humans, `0x80167A70` |

`Gp_PlayerAnimBlkTbl` is indexed by `GameActor.field_93A`
(`Gp_WeaponIdBase[...] + Wip_SysConfig.field_21`), so an entry is only
meaningful while *that weapon's* package is loaded. Two consequences:

- The same VA means different things depending on which package is resident.
- An entry whose pointers all resolve inside a given package is that package's
  entry. Validating in-range identifies the owner with no signature guessing,
  which is how `pkg_anim.py` attributes blocks to packages.

### 9.2 Animation blocks

Layout, from `Gp_AnimInitCtx` / `Gp_AnimResetSlot` / `Gp_AnimResetSlotEx` in
`src/gameplay/1BC.c`:

```text
table entry ─→ GpAnimSet*[]        slot 0 unused; NULL entries are holes

GpAnimSet (types in include/gameplay/1BC.h):
  0x00  GpAnimRec*     field_0   base of the 4-byte clip records
  0x04  u16*           field_4   clip index table (values are record indices)
  0x08  void*          field_8[] pose banks, indexed by a record's field_3 & 0xF

GpAnimRec (4 bytes):
  0x00  u16 field_0
  0x02  u8  field_2
  0x03  u8  field_3    opcode; low nibble picks the pose bank,
                       >= 0xC0 ends the clip, signed < 0 continues a chain
```

Pose bank formats, dispatched by `func_800B3448` on `GpAnimSlot.field_B`:

| `field_3 & 0xF` | Bank type | Layout |
|---|---|---|
| 1 | `GpPackedPose` | packed translation + rotation |
| 4 | `GpPackedSvec` | `s32 vx:11, vy:10, vz:11` |

**Clip walk.** Entry *n* of the index table gives the first record of clip *n*;
records run on until one has `field_3 >= 0xC0`. The index table has no explicit
terminator, and stopping at the first non-ascending value is *not* enough — past
the real end it keeps finding plausible ascending `u16`s and over-runs. The
record array is the bound: it runs from `field_0` up to `field_4`, so no index
may be `>= (field_4 - field_0) / 4`.

Worked example — `pe2pkg_2` (a weapon), set at `0x8011DAD8`:

```text
records  0x8011D980
index    0x8011DAB0   ->  gap 0x130 = 304 bytes = 76 records
index table   0 4 8 12 ... 72, then 0  ->  19 clips of 4 records
last clip starts at 72, 72 + 4 = 76    ->  exactly the record count
```

That "last clip ends exactly where the record array does" property holds for
**1767 of 1767** sets across all packages, and is emitted as `walk_consistent`
in the tool output so a regression shows up rather than passing silently.

Totals: 45 blocks, 1767 sets, 33 723 clips. Weapon packages are uniform at
44 sets / 836 clips each, consistent with a shared player skeleton carrying
per-weapon clip data.

### 9.3 Model streams (TMD)

**→ [`TMD_FORMAT.md`](TMD_FORMAT.md)** — the container (`TmdSource`), the
packet stream, element layouts, the opcode bit structure and a per-opcode
reference grouped by draw family.

In brief: a model is `[vertices][normals][face stream][TmdSource]` laid out
contiguously inside a package, with the record's `+0x14`/`+0x18`/`+0x20`
pointing at the three parts. The stream is packets of
`[id][handler slot][dims][payload]` terminated by `0xFFFFFFFF`. Geometry is
solved for the `0x38` (triangle) and `0x78` (quad) families; texture
coordinates and the remaining 21 families are open.

Streams are carved to `raw/model/*.tmd` and stored raw only, since nothing
decodes them yet.

### 9.3.1 Animation tracks bind to model parts one-for-one

An animation set's index table has exactly one entry per **model part**
([`TMD_FORMAT.md` §2.2](TMD_FORMAT.md)), so what `pkg_anim.py` reports as
"clips" are per-bone tracks. Measured across every package holding an
animation block:

| Package | Parts | Tracks per set | Sets |
|---|---:|---:|---:|
| `actor_800200`, `actor_800300` | 19 | 19 | 34 |
| `human_800101`–`human_800104` | 20 | 20 | 34 |
| every weapon | 1 | 19 | 44 |

Actors and named humans match their own part count exactly, 7 blocks out of 7.
The weapons look like an exception and are the clearest confirmation: a weapon
is a single part, its blocks come from `Gp_PlayerAnimBlkTbl`, and 19 is **Aya's**
part count — the weapon package carries the *player's* animation for that
weapon, driving Aya's skeleton, not its own geometry.

That fixes the model ↔ animation binding. The rest pose itself does not come
from here — it ships in the `TmdSource` ([`TMD_FORMAT.md` §2.2](TMD_FORMAT.md)),
so a static model can be posed without touching the animation data at all.
Animation then replaces each bone's local matrix and reuses the same parent
links.

### 9.4 What is still open

- **Model opcodes.** Arity and element layout are settled for two of the 23
  draw families; the rest are open, tracked per family in
  [`TMD_FORMAT.md` §5](TMD_FORMAT.md#5-opcode-reference) with what is still
  missing in [§6](TMD_FORMAT.md#6-what-is-still-open).
- **Pose banks.** The per-model bone count is now available — it is the number
  of `0xFFFFFFFE`-delimited parts in the model stream
  ([`TMD_FORMAT.md` §2.2](TMD_FORMAT.md)), and §9.3.1 binds tracks to parts —
  but the per-part block that `Tmd_SetupGteMatrices` consumes is a
  `GsCOORDINATE2` (0x50 bytes, `workm` at `+0x24`) whose parent links
  (`.sub`) are built at runtime, and how the animation fills its local
  `coord` is not yet traced. `0xC8` turned out
  to be a vertex transform pass rather than bone data
  ([`TMD_FORMAT.md` §3.0](TMD_FORMAT.md#35-the-transform-pre-pass--0xc0--0xc4--0xc8)), so the count has
  to come from somewhere else — the animation side, or one of the remaining
  transform opcodes.
- **Import.** Round-tripping OBJ back to a stream needs byte-exact re-encoding
  of the tail words, so export has to come first.
- **False positives.** The stream validator checks structure, not semantics; a
  short run in a package that is otherwise ~0% covered is the shape to
  distrust. `MIN_PACKETS` in `pkg_model.py` is the knob.

### 9.5 Where they end up

| Kind | Store | Why |
|---|---|---|
| Model streams | `raw/model/{pkg}_model_{off}.tmd` | contiguous byte ranges, so each is a self-contained asset; the map entry carries the `TmdSource` vertex/normal arrays where one is found |
| Animation blocks | *not stored* | not contiguous — see below |

`.tmd` is in `RAW_ONLY_EXTS`: located but not decoded, so there is no type-store
form and nothing implies a decoder that does not exist. Drop the extension from
that set once a materialize branch lands.

572 streams are located across 212 packages; SHA-1 dedup in the store collapses
them to **298 unique** files, so 274 are meshes shared between packages. (Both
figures dropped once the scanner stopped accepting zero padding as a stream —
see [`TMD_FORMAT.md` §2.1](TMD_FORMAT.md#21-rejecting-false-streams).)

**Animation blocks cannot be carved the same way.** A block's pieces are spread
across the package rather than sitting in one range: the `GpAnimSet*[]` table is
near the end, the sets and their record arrays near the start. Measured spans
are 91–99% of the whole package (e.g. `pe2pkg_11`: pieces from `0x00004` to
`0x0CF18` of a 53 332-byte file), so a byte range holding "the animation" would
be a near-copy of the package that already exists in the store. The clip
structure is readable — `pkg_anim.py` walks it — but there is no blob to store
until the individual pose banks can be delimited, which needs the per-model
bone count from the model stream.

| Tool | Role |
|---|---|
| `tools/peassets/pkg_model.py` | locates streams, carves them into `raw/model/` |
| `tools/peassets/pkg_anim.py` | walks clip structure; reports, stores nothing |
| `tools/peassets/pkg_overlay.py` | family → overlay → embedded asset, for the viewer's Overlays tree |
| `tools/peassets/mesh_view.py` | software-rendered mesh viewport (Tk canvas) |

The first two run from `extract.py`, need the full package set, and no-op after
a minimal extract, which materialises only the two overlays the build splits.

### 9.6 Browsing them

The viewer's **Overlays** tab is the second tree, next to the CDF one. The CDF
tree browses the disc — stage → file → chunk; an overlay *is* one of those
chunks, and the assets inside it sit at raw offsets that no chunk boundary
describes. So the Overlays tree goes family → overlay → embedded asset, read
straight out of the package:

- selecting the **overlay** node gives the usual `.pe2pkg` hex/strings preview;
- a **model** node draws the mesh in the **Model** tab — drag to rotate, wheel
  to zoom, right-drag to pan, with solid / wireframe / points shading and
  backface culling on by default — and
  writes the `TmdSource` arrays, the undecoded opcodes and the Euler
  characteristic `V - E + F` into Txt/hex as a sanity check on the face decode;
- an **anim** node lists the block's sets and clips.

Scanning is on demand and cached per package: `find_streams` walks every 4-byte
offset and re-walks each candidate, so scanning all 448 packages up front would
stall the window. A package is scanned when its node is first expanded.

The mesh comes from the same decoder as `tmd_export.py`, Y-flip included, so a
model that looks upright in the viewport looks upright in Blender. Only faces
are drawn — no textures and no UVs.

**Characters are posed with their rest skeleton.** A model stream is split by
`0xFFFFFFFE` into parts, one per bone, and each part's vertices are in that
bone's local frame; the skeleton that places them ships in the `TmdSource`
([`TMD_FORMAT.md` §2.2](TMD_FORMAT.md)). The viewer composes it, so characters
stand up. The **Part** selector isolates one bone's geometry when you want to
look at a single limb. The last part holds the pre-transformed primitives and
is positioned by the earlier parts, so it does not stand alone.

Visibility and shading come from two different places, as they do on the
hardware (see [`TMD_FORMAT.md` §7](TMD_FORMAT.md)). Backface culling is
`NCLIP` on the projected points, exactly what `Tmd_StreamHandler_Op38` does;
the stored normals feed `NCCS`, which is lighting only, and culling on them
removes real surface — that is what made heads and legs vanish. Depth sorts
descending, because `SZ3` grows with distance and negating Y does not touch Z.

---

## 10. Music (`.spk`) and other types

### 10.1 SPK sound banks (`.spk`)

- Chunk type `0x6`. Magic **`hSPK`**. Fed to `SndLoad_*` then `SpuWritePartly`.
- Clean payload layout (see `tools/peassets/spk_codec.py`):

  | Offset | Field |
  |--------|--------|
  | 0 | `char[4]` magic `hSPK` |
  | 4 | `u16` bank_id (high nibble = bank type for `Snd_AllocBank`) |
  | 6 | `u8` field_22 |
  | 7 | `u8` group_count |
  | 8 | `u8` note_count |
  | 9 | `u8` field_25 |
  | 0xA | `u16` field_26 (SPU page; usually 0) |
  | 0xC | `u8` field_28 / field_29 |
  | 0xE | `u16` prog_size (bytes of program / `hONE` stream) |
  | 0x10 | `s32` spu_size (SPU-ADPCM pool size) |
  | 0x14 | group table (`4 × group_count`) + note table (`0x14 × note_count`) |
  | 0x800 | program (`hONE` / `oneV` / `oneC` / `endC` …) |
  | `align16(0x800+align4(prog_size))` | SPU-ADPCM sample pool |

- Each `SndNote.waveAddr` is a byte offset into the sample pool (16-byte ADPCM
  frames, ~22050 Hz mono).
- Extract: `raw/spk/*.spk` → `spk/{stem}/meta.json` + `sample_XX.wav`.
- Pack always uses `raw/spk/` (no WAV→SPK encoder).

#### What works today

| Piece | Status |
|--------|--------|
| `hSPK` header + group/note tables | Parsed (`spk_codec.parse_spk`) |
| SPU-ADPCM sample pool → WAV | Working (~22 050 Hz mono) |
| Extract materialize + viewer meta/waveform | Working |
| Pack bit-identity | Via `raw/spk/` only |

#### What is still missing (sequence / full audio)

All retail SPK program regions use Square’s **`hONE` SndScript**, not SMF
MIDI (`MThd` never appears in SPK blobs). The game’s `Midi_InitSequence` path
expects SMF; SPK playback goes through **`SndScript_Exec`** (`one*` opcodes).

| Gap | Notes |
|-----|--------|
| **`hONE` header** | Counts/sizes after magic not fully pinned |
| **Tagged script stream** | `oneC` / `oneV` / `oneE` / `oneA` / `endC` / `Loop` / `Wait` / `endL` — structs partially in `include/main/sound.h`; need a stream walker |
| **`SndScript_Exec`** | Still `INCLUDE_ASM` in `src/main/sndscript.c` — authoritative interpreter for timing and opcodes |
| **Timed event list** | Needs Wait/Loop stack + timebase (script ticks vs frame rate) |
| **Audio mix / “play the song”** | Needs timed events + pitch (root/fine/`oneV`) + vol/pan + ADSR/`oneE` + polyphony; samples alone are not enough |
| **WAV → SPK encoder** | Not planned; matching packs use raw |
| **SMF export** | Optional convenience only — not native format |

Suggested roadmap: (1) structural `hONE` → JSON events, (2) timed events after
`SndScript_Exec` reverse, (3) naive mixer to a single WAV, (4) closer SPU model.

### 10.2 Other types

- **`.pe2cap2`**: dialogue-related; may carry a RAM load address in `load_addr`.
- **`.txt`**: ASCII (type `0x7`). On-disc clean payload is CRLF text
  (usually ending in `\Z` or `\Z\r\n`) zero-padded to chunk capacity.
  Inflated `txt/` is the text only (no trailing NUL / zero pad).

### 10.3–10.4 CD streams (MTS audio + STR movie)

Not file chunks — **streaming list** descriptors (`STAGE0.HED` or folder
`+0x514`) plus CD payloads. Full field tables (what each movie header
byte means / confidence), disc layout (`INTER0` vs `INTER1`, stage‑3 dual
descriptors), seek/play path, extract pitfalls:

**→ [`STREAM_FORMATS.md`](STREAM_FORMATS.md)** (§3 movies, §2 audio)

Full `extract.py` (and `ninja_config.py --iso_extract`) writes these after
stage chunks. `--raw-only` keeps `raw/audio` + `raw/movie` without WAV/MP4;
`--minimal-inflate` skips streams (CI). Standalone tools still work for a
re-extract:

```bash
python3 tools/peassets/extract.py ... -o assets/USA          # includes streams
python3 tools/peassets/extract_streams.py --rom rom/USA --out assets/USA   # audio only
python3 tools/peassets/extract_movies.py  --rom rom/USA --out assets/USA -j 16  # movie only
```

```text
raw/audio/*.mts   audio/*.wav
raw/movie/*.str   movie/{stem}.mp4  movie/{stem}.json   # lossless H.264 4:4:4 + ALAC (use VLC)
```

---

## 11. Asset database (`asset_db.py` / `asset_data.py`)

Two tables:

* **`ASSETS`** — unique raw blobs. Key is a stable id (type-store stem).
  Value: `sha1` of the on-disc payload, `type`, and optional attributes
  (`bpp`, `required`, …).
* **`TREE`** — STAGE*.CDF file tree keyed by integer disc ids. Chunk
  leaves are asset ids. Folder/file `name` is an optional path component.

```text
ASSETS["gameplay"] = {sha1, type: pe2pkg, required: True}
ASSETS["pe2img_2"] = {sha1, type: pe2img, bpp: 8}

TREE[0]["files"][0] = {name: "gameplay", chunks: {1: "gameplay"}}
TREE[1]["folders"][101]["files"][0]["chunks"][2] = "pe2img_12"
```

Regenerate hashes and placement after extract (preserves extra fields and
names, matched by sha1):

```bash
python3 tools/peassets/dump_asset_db.py
```

Rules of thumb:

- Canonical extract-map keys always use numeric ids (`stage0/file0/1.pe2pkg`).
- `stages.json` file/folder keys use TREE `name` when set (else `file0` /
  `101`). Chunk keys stay disc-index based unless the asset id is a
  friendly (non-`type_N`) name. Dict order is still disc order.
- Type-store stems are asset ids: sha1 lookup in `ASSETS`, else `{type}_{n}`.
  Duplicates share the same store path in `stages.json`.
- pe2img `bpp` lives on the unique blob (`ASSETS[id]["bpp"]`): an int, or a
  list of 4/8/16 — one per work-entry column (VRAM page). Not the CDF slot.

---

## 12. `stages.json` schema

Pack manifest under the assets root. Validated by
`format.validate_stages_manifest` at pack time: **only known keys** are
allowed at each level; unknown keys (e.g. `chunk_size`, `end_flag`) fail.

### 12.1 Top level

```text
{
  "stage0": { "files": { … } },
  "stage1": { "folders": { … } },
  …
}
```

| Rule | Detail |
|------|--------|
| Keys | Only `"stageN"` (`stage0` …) |
| Stage body | Exactly one of `"files"` or `"folders"` (not both, not empty) |

### 12.2 Files stages (`stage0`)

```text
"stage0": {
  "files": {
    "<file_key>": {
      "<chunk_key>": { …content… },
      …
    },
    …
  }
}
```

- **file_key**: `names.stages_file_key` — friendly name or `file{id}`.
- **chunk_key**: `names.stages_chunk_key` — friendly stem + ext or `{idx}{ext}`.
- Object order of files and of chunks = on-disc order.

### 12.3 Folder stages (`stage1`…)

```text
"stage1": {
  "folders": {
    "<folder_key>": {
      "<file_key>": {
        "<chunk_key>": { …content… }
      }
    }
  }
}
```

- **folder_key**: friendly name or decimal folder id.

### 12.4 Chunk content object

| Key | Required | Allowed on | Notes |
|-----|----------|------------|--------|
| `path` | **yes** | all | Relative to assets root (type store) |
| `type` | **yes** | all | See type table below |
| `sector_len` | no | all | Omit if `0x800`. Range `(0x10, 0x800]` |
| `load_addr` | **yes** (non-zero) | **`room_pkg`, `cap2` only** | RAM load address; retail never uses `0` |

**Never in `stages.json`** (inferred or on-disc only):

| Field | How pack handles it |
|-------|---------------------|
| `end_flag` | Last chunk in a file → End (`0xFF`); earlier → Continue (`0x01`) |
| `chunk_size` | `n_sectors × 0x800` from clean payload + `sector_len` |

**`type` values** (and allowed keys):

| `type` | Extension | Keys |
|--------|-----------|------|
| `room_pkg` | `.pe2pkg` | `path`, `type`, **`load_addr`**, `sector_len?` |
| `image` | `.pe2img` / `.png` | `path`, `type`, `sector_len?` |
| `clut` | `.pe2clut` / `.png` | `path`, `type`, `sector_len?` |
| `cap2` | `.pe2cap2` | `path`, `type`, **`load_addr`**, `sector_len?` |
| `room_background` | `.bs` / `.png` | `path`, `type`, `sector_len?` |
| `music` | `.spk` | `path`, `type`, `sector_len?` |
| `ascii` | `.txt` | `path`, `type`, `sector_len?` |
| `raw` | `raw.bin` | `path`, `type` only (non-chunked file) |

Example:

```json
{
  "stage0": {
    "files": {
      "gameplay": {
        "0.spk": {
          "path": "spk/spk_0/meta.json",
          "type": "music"
        },
        "gameplay.pe2pkg": {
          "path": "pe2pkg/gameplay.pe2pkg",
          "type": "room_pkg",
          "load_addr": "0x80093800"
        }
      }
    }
  },
  "stage1": {
    "folders": {
      "101": {
        "file0": {
          "0.bs": {
            "path": "bs/bs_0.png",
            "type": "room_background",
            "sector_len": "0x500"
          }
        }
      }
    }
  }
}
```

---

## 13. Type store + overlays

### Type directories

Every chunk type has directories under `raw/` (on-disc) and at the assets
root (inflated). Dedup is by SHA-1 of the **raw** clean payload:

| Type | `raw/{type}/` | inflated `{type}/` |
|---|---|---|
| pe2pkg | trim_lzss on-disc stream | LZSS-decoded package |
| pe2img / pe2clut | clean pe2 blob | PNG + `*.pe2img.json` / `*.pe2clut.json` |
| audio | MTS sector payload (`.mts`) | stereo WAV + JSON (§10.3) |
| movie | STR 2048-byte sector blob (`.str`) | PNG frames + WebP + meta (§10.4) |
| other | clean on-disc payload | hardlink/copy of raw |

Pack sees them via `stages.json` content entries (same path for every
duplicate reference). See §12 for the full schema.

### Overlays (pe2pkg store)

Room packages used by the decomp build live under `pe2pkg/` and are listed
in `stages.json` (`type: room_pkg`, `path`, `load_addr`).

- Title and gameplay share `0x80093800` (mutually exclusive).
- Stage 1–5 **room** overlays load at `0x8017D5C0`. Other high addresses
  (`0x80115770`, `0x8011D1C0`, `0x80131E20`, …) are Aya / weapon / actor
  slots, not rooms — see [`OVERLAYS.md`](OVERLAYS.md).
- Splat targets point at the store file (e.g.
  `target_path: assets/USA/pe2pkg/title.pe2pkg`).

---

## 14. Tooling workflow

```text
Extract:
  STAGE*.CDF  →  raw/{type}/ (unique on-disc payloads)
              →  pe2pkg|pe2img|…/ (inflate unique raw only)
              →  raw/audio + audio/  (MTS from STAGE*.CDF)
              →  raw/movie + movie/  (STR from INTER*.STR / CDF)
              →  stage*/ (pack sidecars) + stages.json
  extract.py --raw-only          →  raw/{type}/ + raw/audio + raw/movie
  extract.py --minimal-inflate   →  raw/ + pe2pkg/{title,gameplay} only (CI; no streams)
  extract.py --skip-streams      →  stage chunks only (no MTS/STR)

Pack:
  matching  raw/{type}/ LZSS + inflated non-LZSS  →  STAGE*
  raw       raw/{type}/ only                        →  STAGE*
  hybrid/decoded  inflated type dirs (re-encode)    →  STAGE*
```

Commands (from repo root):

```bash
# extract (needs disc paths; see extract.py --help)
python3 tools/peassets/extract.py ... -o assets/USA
python3 tools/peassets/extract.py ... -o assets/USA --raw-only
python3 tools/peassets/extract.py ... -o assets/USA --minimal-inflate

# browse extracted assets (by type, CDF stage tree, or overlay tree with the
# assets embedded in each package; audio/movie playable, models in the Model tab)
python3 tools/peassets/viewer.py assets/USA

# CD streams only (already included in a full extract.py run; see STREAM_FORMATS.md)
python3 tools/peassets/extract_streams.py --rom rom/USA --out assets/USA
python3 tools/peassets/extract_movies.py  --rom rom/USA --out assets/USA

# pack (default: matching)
python3 tools/peassets/pack.py \
  --stages assets/USA/stages.json \
  --assets assets/USA \
  --output /tmp/pe2-out

# pack from edited type-store assets
python3 tools/peassets/pack.py \
  --stages assets/USA/stages.json \
  --assets assets/USA \
  --source hybrid \
  --output /tmp/pe2-out
```

Dependencies: see `requirements.txt` (includes **Pillow** for PNG).

---

## 15. Known gaps / open questions

- **`.bs` BS v2 MDEC** backgrounds: decoded to PNG on extract (see §8).
- **Exact bpp** is not stored in the image chunk; exporters **guess** unless
  `ASSETS[id]["bpp"]` or the viewer BPP combo overrides. Meta JSON records
  the choice (`bpp` + `bpp_source`) for re-encode.
- **Which CLUT row** an 8 bpp texture uses when `h > 1` is not known offline
  (game picks via `getClut(x,y)`). Exporter scores rows and picks the most
  varied; wrong for textures that intentionally use a monochrome row.
  Title chrome is `GetClut(0, 255)`. UI text is `0x7FFD` (EXE outline
  palettes), not the font TIM clut — see §7.5 / §7.6.
- **Cap2 / dialogue** internals undocumented.
- **Streaming movies (STR/MDEC)** and **audio (MTS)**: documented in
  [`STREAM_FORMATS.md`](STREAM_FORMATS.md). A full `extract.py` run writes
  them (or re-run `extract_movies.py` / `extract_streams.py`). INTER movies
  mux XA as ALAC in the MP4.
- Runtime image path has mode-dependent Y adjustments (`Fs_ChunkMode`,
  `D5B498_8006C233/C234`) that do not affect off-line PNG export.

---

## 16. Quick reference — endianness and colour

- All multi-byte integers: **little-endian**.
- VRAM halfword colours: **ABGR1555**
  - bits `0–4` R, `5–9` G, `10–14` B, bit `15` STP/semi-trans.
- Sizes in headers that are “sector counts” are multiplied by `0x800` for
  byte sizes in tools and docs.
