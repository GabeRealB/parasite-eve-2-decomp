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
| Friendly names | `tools/peassets/names.py` |
| Runtime FS loader | `src/main/fs.c`, `include/main/fs.h` |

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
                         name = names.NAMES stem, else {type}_{n}
pe2pkg/ pe2img/ …        **inflated** edit forms (one per unique raw)
                         pe2pkg = LZSS-decoded; pe2img/pe2clut = PNG + meta
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
- Streaming list (movies / audio) lives in the HED as well.
- Chunks land in type dirs; pack sidecars under `stage0/<fileName>/`
  (default `file{id}`, or a friendly name from `names.NAMES`).

### STAGE1–5 (`STAGEn.CDF`)

- Leading folder table: up to `0x100` entries of `(folder_id, folder_size)`.
- Each folder is a sector-aligned block:
  - file list (`0xA2` slots)
  - streaming list
  - then file payloads
- Chunks land in type dirs; pack sidecars under `stageN/<folderName>/file…/`.

Folder and file **keys** in `stages.json` use friendly names when set in
`names.py`; otherwise `file12` / decimal folder id. Order is always on-disc
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
- Unique inflated bodies live under `pe2pkg/` (stem from `NAMES` or
  `pe2pkg_N`); decomp overlays use those files directly (e.g.
  `pe2pkg/title.pe2pkg`).

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

1. `Fs_CopyWorkEntries` copies the table into `D5B498_8006ACE8`.
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

When a neighbouring `.pe2clut` exists (`N±1` by chunk index), the PNG exporter
applies it for 4/8 bpp (see §7.4 for multi-row selection). Examples:

- `stage0/file2/2.pe2img` + `1.pe2clut` → title menu art
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

## 9. Music (`.spk`) and other types

### 9.1 SPK sound banks (`.spk`)

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

### 9.2 Other types

- **`.pe2cap2`**: dialogue-related; may carry a RAM load address in `load_addr`.
- **`.txt`**: ASCII (type `0x7`). On-disc clean payload is CRLF text
  (usually ending in `\Z` or `\Z\r\n`) zero-padded to chunk capacity.
  Inflated `txt/` is the text only (no trailing NUL / zero pad).

### 9.3 CD audio streams (MTS)

Not file chunks — listed in the **streaming list** (`STAGE0.HED` or folder
`+0x514`, type `2`). Runtime: `CdStream_*` → `SpuWrite` (`cdstream.h`
`MtsSector`).

| Piece | Notes |
|-------|--------|
| Descriptor | `0x28` `FsCdfStream` audio arm; `offset` = start sector (STAGE0: stage-abs; folders: folder-rel); `field_14` often total length in sectors |
| Payload | Sector stream; every *period* sectors an **MTS** header (`bytes @+8` = `cc 'S''T''M'`, `cc` = channel count) |
| Channels | Almost always stereo; headers alternate `field_C` 0/1; gaps between chunk pairs are allowed |
| Samples | SPU-ADPCM from header `+0x10` + continuation sectors; ~22050 Hz |

Extract into the type-store layout (same root as pe2pkg/spk)::

```text
raw/audio/{stem}.mts     on-disc MTS sector payload
audio/{stem}.wav         decoded stereo PCM (~22050 Hz)
audio/{stem}.json        geometry + descriptor
audio/streams.json       catalog
```

```bash
python3 tools/peassets/extract_streams.py --rom rom/USA --out assets/USA
```

Codec: `tools/peassets/mts_codec.py`.

---

## 10. Friendly naming (`names.py`)

Optional map from **canonical disc ids** to path components:

```text
stage0/file0          →  gameplay          (file directory)
stage0/file0/1        →  gameplay          (chunk stem / pe2pkg store name)
stage1/101            →  <room name>       (folder)
```

Rules of thumb:

- Canonical extract-map keys always use numeric ids (`stage0/file0/1.pe2pkg`).
- `stages.json` file/folder/**chunk** keys use friendly names when set in
  `NAMES` (else `file0` / `101` / `1.pe2pkg`). Dict order is still disc order.
- Type-store stems: chunk `NAMES` when set, else `{type}_{n}` for the
  n-th **unique** asset of that type. Duplicates share the same store path
  in `stages.json`.

---

## 11. `stages.json` schema

Pack manifest under the assets root. Validated by
`format.validate_stages_manifest` at pack time: **only known keys** are
allowed at each level; unknown keys (e.g. `chunk_size`, `end_flag`) fail.

### 11.1 Top level

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

### 11.2 Files stages (`stage0`)

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

### 11.3 Folder stages (`stage1`…)

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

### 11.4 Chunk content object

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

## 12. Type store + overlays

### Type directories

Every chunk type has directories under `raw/` (on-disc) and at the assets
root (inflated). Dedup is by SHA-1 of the **raw** clean payload:

| Type | `raw/{type}/` | inflated `{type}/` |
|---|---|---|
| pe2pkg | trim_lzss on-disc stream | LZSS-decoded package |
| pe2img / pe2clut | clean pe2 blob | PNG + `*.pe2img.json` / `*.pe2clut.json` |
| audio | MTS sector payload (`.mts`) | stereo WAV + JSON (§9.3) |
| other | clean on-disc payload | hardlink/copy of raw |

Pack sees them via `stages.json` content entries (same path for every
duplicate reference). See §11 for the full schema.

### Overlays (pe2pkg store)

Room packages used by the decomp build live under `pe2pkg/` and are listed
in `stages.json` (`type: room_pkg`, `path`, `load_addr`).

- Load address is shared for title/gameplay-style packages (`0x80093800`);
  room overlays typically load higher (e.g. `0x80131E20`, `0x8017D5C0`).
- Splat targets point at the store file (e.g.
  `target_path: assets/USA/pe2pkg/title.pe2pkg`).

---

## 13. Tooling workflow

```text
Extract:
  STAGE*.CDF  →  raw/{type}/ (unique on-disc payloads)
              →  pe2pkg|pe2img|…/ (inflate unique raw only)
              →  stage*/ (pack sidecars) + stages.json
  extract.py --raw-only          →  raw/{type}/ only
  extract.py --minimal-inflate   →  raw/ + pe2pkg/{title,gameplay} only (CI)

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

# browse extracted assets (by type or stage tree + preview)
python3 tools/peassets/viewer.py assets/USA

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

## 14. Known gaps / open questions

- **`.bs` BS v2 MDEC** backgrounds: decoded to PNG on extract (see §8).
- **Exact bpp** is not stored in the image chunk; exporters **guess** (and use a
  sibling CLUT when present). Meta JSON records the choice for re-encode.
- **Which CLUT row** an 8 bpp texture uses when `h > 1` is not known offline
  (game picks via `getClut(x,y)`). Exporter scores rows and picks the most
  varied; wrong for textures that intentionally use a monochrome row.
- **Cap2 / dialogue** internals undocumented.
- **Streaming movies** (STR/MDEC) still need full payload extract/decode.
- **Streaming audio (MTS)** is extractable: see §9.3 and
  `tools/peassets/extract_streams.py` / `mts_codec.py`.
- Runtime image path has mode-dependent Y adjustments (`Fs_ChunkMode`,
  `D5B498_8006C233/C234`) that do not affect off-line PNG export.

---

## 15. Quick reference — endianness and colour

- All multi-byte integers: **little-endian**.
- VRAM halfword colours: **ABGR1555**
  - bits `0–4` R, `5–9` G, `10–14` B, bit `15` STP/semi-trans.
- Sizes in headers that are “sector counts” are multiplied by `0x800` for
  byte sizes in tools and docs.
