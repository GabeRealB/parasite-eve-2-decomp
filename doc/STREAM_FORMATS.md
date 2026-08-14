# Parasite Eve 2 — CD stream formats (audio + movie)

What we know about **streaming list** entries and their on-disc payloads:
**MTS** CD→SPU audio and **STR** MDEC video. Derived from the USA discs, the
matching decomp (`fs.c`, `stream.c`, `cdstream.c`, `cdaudio.c`), and the
extract tools under `tools/peassets/`.

These are **not** normal stage file chunks (`.pe2pkg`, `.bs`, `.spk`, …). The
game keeps only a small descriptor table in RAM (`Stream_Slots` / `Fs_Streams`)
and **seeks the CD** to feed hardware (SPU or MDEC) in real time.

| Area | Code / tools |
|------|----------------|
| Descriptor struct | `include/main/fs.h` (`FsCdfStream`), `tools/peassets/format.py` |
| Runtime load of descriptors | `src/main/fs.c` (`Fs_BuildFolderTables`, `Fs_InitStage0TablesCb`) |
| Movie play | `src/main/stream.c` (`Stream_*`, `Mdec_*`), `cdcmd.c` (cmd `0x61`) |
| Audio play | `src/main/cdaudio.c`, `cdstream.c` (`CdStream_*`, `MtsSector`) |
| MTS codec / extract | `mts_codec.py`, `extract.py` (also `extract_streams.py`) |
| STR codec / extract | `str_codec.py`, `extract.py` (also `extract_movies.py`) |
| BS/MDEC frame decode | `bs_codec.py` (v2 + v3 DC) |
| Viewer | `viewer.py` (type dirs `audio`, `movie`) |

Related: stage chunk formats in [`ASSET_FORMATS.md`](ASSET_FORMATS.md);
SPK one-shot banks (§9.1 there) are separate from MTS streams.

---

## 1. Streaming list (common)

### 1.1 Where it lives

| Container | Location | Capacity |
|-----------|----------|----------|
| **STAGE0.HED** | Bytes `0x00`… — first entries of the HED | **3** entries (`0x3 × 0x28`) |
| **STAGE1–5 folder** | Folder base + **`0x514`** (after `0xA2` file slots + pad) | **18** entries (`0x12 × 0x28`) |

Each entry is **`0x28` bytes** (`FsCdfStream` / peassets `STREAMING_LIST_ENTRY_SIZE`).

Empty slots are all zeros. STAGE0 HED mixes streams with the file table using
the high bit of the first word when scanned as a flat sector list (see
`Fs_InitStage0TablesCb`: `fileId < 0` → stream).

### 1.2 Entry type

| `u16` @ `0x00` | Meaning |
|----------------|---------|
| `1` | **Movie** (`FS_CDF_STREAM_TYPE_MOVIE`) |
| `2` | **Audio** (`FS_CDF_STREAM_TYPE_AUDIO`) |

Layout after the common header differs by type (union in `FsCdfStream`).

### 1.3 Common header (both types)

| Off | Size | C / peassets | Notes |
|-----|------|--------------|--------|
| `0x00` | `u16` | `type` / `stream_type` | `1` movie, `2` audio |
| `0x02` | `u16` | `field_2` / `unknown1` | STAGE0: bit 15 (`0x8000`) marks stream in mixed HED walk; low bits vary |
| `0x04` | `u32` | `offset` | **Sector count** on disc (× `0x800` → bytes in tools) |

Runtime **absolutizes** `offset` when loading:

- STAGE0: `+= Fs_StageCdfSectors[0]`
- Folder: `+= folder.offset + stage_cdf_base`  
  On-disc folder movie/audio offsets are **folder-relative sectors** for the
  common `offset` field (audio: stage/CDF-relative per comments; see §2 / §3).

### 1.4 Runtime table

`Fs_BuildFolderTables` copies non-empty entries into **`Stream_Slots[15]`**
(BSS). Title may bulk-copy `Fs_Streams` → `Stream_Slots`. Lookup:
`Stream_FindSlot` / `Stream_FindSlotByKey` by stream id fields; playback via
CD command queue / CdAudio.

---

## 2. Audio streams (MTS)

### 2.1 Descriptor (type = 2)

| Off | Size | C (`data.audio`) | peassets | Role |
|-----|------|------------------|----------|------|
| `0x00` | `u16` | `type` | `stream_type` | **`2`** |
| `0x02` | `u16` | `field_2` | `unknown1` | Flags / tag; STAGE0 often has `0x8000` |
| `0x04` | `u32` | `offset` | `offset_stage` | Start **sector** (folder: relative to folder; STAGE0: relative to stage CDF) |
| `0x08` | `u32` | `field_8` | `unknown2` | Often `0` or SPU-range (`0x5800`…`0x19000`) — likely SPU base |
| `0x0C` | `u16` | `stageIdx` | `stage_number` | Stage index `0`…`5` |
| `0x0E` | `u16` | `id` | `stream_id` | Lookup id |
| `0x10` | `u32` | `subId` | `stream_sub_id` | Low half almost always `0xB`; high half `0` |
| `0x14` | `u16` | `field_14` | `unknown3` | Often **length in sectors** when non-zero (rough; not always exact gap) |
| `0x16` | `u16` | `field_16` | `unknown4` | Almost always **`0x20`** (matches volume-ramp constant) |
| `0x18` | `u32` | `field_18` | `unknown5` | Mode/flags: `0`, `1`, `2`, or `0x20000` |
| `0x1C` | `u16` | `field_1c` | low of `unknown6` | Often `0x800` / `0` / `0x1000` — buffer size-ish |
| `0x1E` | `u16` | `field_1e` | high of `unknown6` | Often `0x800` or larger |
| `0x20` | 8 | `field_20` | `unknown7` | Always zero in retail samples |

### 2.2 Payload: MTS sector stream

Payload is a contiguous run of **2048-byte** ISO sectors in the stage CDF
(folder tail after file chunks, or absolute stage offset). Not XA Form 2.

Every **period** sectors an **MTS header sector** appears:

```text
0x00  s32   field_0     chunk index (steps every stereo pair)
0x04  s32   field_4     total chunk count for the stream
0x08  u32   magic       LE 0x4D5453cc → bytes: cc 'S' 'T' 'M'
                        cc = channel count (almost always 2)
0x0C  s8    field_C     channel index (0 / 1 for L/R)
0x0D  u8    period      sectors between successive MTS headers (often 5 or 10)
0x0E  u8    field_E
0x0F  s8    field_F
0x10  …     SPU-ADPCM (see write sizes below)
```

Intervening sectors are ADPCM continuation (no MTS magic).

**Stereo pattern** (2 ch, period *P*):

```text
sec 0:       MTS ch0 + ADPCM
sec 1..P-1:  ADPCM ch0
sec P:       MTS ch1 + ADPCM
sec P+1..:   ADPCM ch1
sec 2P:      MTS ch0 next chunk
…
```

Headers are **not** always a perfect grid: padding gaps between chunk pairs
are common. Demux must **scan** for MTS magics, not assume stride = period.

Some streams have a short **preamble** (TOC-like table) before the first MTS
header; skip until magic.

Nominal body length: `chunk_count × channels × period` sectors (from first
header). Descriptor `field_14` often matches total span from the list offset
when set; else derive from headers.

### 2.3 `CdStream_ReadyMts` write sizes (critical for decode)

Per period window (`remaining % period` in `CdStream_ReadyMts`):

| Sector in window | What the game feeds SPU |
|------------------|-------------------------|
| Header (`rem % P == 0`) | `SpuWrite(sec+0x10, 0x800)` but ring advances **`0x7F0`** → use `sec[0x10:0x800]` |
| Middle | Full **`0x800`** |
| Last (`rem % P == 1`) | **`0x780` only** (`ringHalf` math). Trailing **`0x80`** is pad/zeros |

Including the last-sector pad as ADPCM causes a **periodic click** (~10 ms of
digital zero every period). Offline demux must drop it.

### 2.4 End-flag and silence pad

Odd period windows often end with an ADPCM frame whose **end flag** is set
(`flags & 1`, e.g. `0x03`). Hardware hits that flag and loops/stops; trailing
zero frames are **not** heard. Offline decode should:

1. Keep samples through the end-flag frame  
2. Drop trailing all-zero 16-byte frames  
3. Drop leading zero frames on later windows (optional keep on first window)

### 2.5 Sample rate and extract layout

SPU-native rate ≈ **22050 Hz** (same as SPK samples). Stereo WAV is L/R demuxed
ADPCM decoded independently then interleaved.

```text
raw/audio/{stem}.mts      on-disc sector payload
audio/{stem}.wav          decoded stereo PCM
audio/{stem}.json         geometry + descriptor
audio/streams.json        catalog
```

```bash
# included in a full extract.py / --iso_extract run
python3 tools/peassets/extract.py ... -o assets/USA
# audio only
python3 tools/peassets/extract_streams.py --rom rom/USA --out assets/USA
```

### 2.6 Open / low-confidence audio fields

| Field | Confidence |
|-------|------------|
| type, offset, stageIdx, id | High |
| field_2 bit15 on STAGE0 | High (HED stream marker) |
| field_14 as sector length | Medium |
| field_8 as SPU base | Medium |
| field_16 = `0x20` | Medium (volume/ramp) |
| field_18, field_1c/1e | Low (mode / buffer sizes) |
| Interleaved **XA** speech on movie STR | Separate; MTS path is SPU-ADPCM only |
| Pack / re-encode MTS | Not implemented (raw preferred) |

---

## 3. Movie streams (STR / MDEC)

### 3.1 Discs, stages, and INTER files (USA)

| Disc | Stage CDFs present | INTER file | Size (approx) |
|------|--------------------|------------|---------------|
| **Disk1** | 0, 1, 2, **3** | `INTER0.STR` | ~252 MB / 107 950 × 2336 B |
| **Disk2** | 0, **3**, 4, 5 | `INTER1.STR` | ~301 MB / 128 780 × 2336 B |

`INTER0` and `INTER1` are **different files** (different size/hash). Early content
matches for a long prefix (title at sector 0 is the same); later they diverge.
There is no “one INTER shared by both discs.”

Practical mapping when playing **INTER** movies (`movie_number ≠ 0`):

| Stage | Lives on | INTER used |
|-------|----------|------------|
| 0 (title, …) | Both discs | That disc’s INTER (sector 0 title matches) |
| 1, 2 | Disk1 only | **INTER0 only** |
| 3 | **Both** discs | **INTER0 or INTER1** depending on inserted disc |
| 4, 5 | Disk2 only | **INTER1 only** |

The engine does **not** map stage number → INTER index. It always seeks the
**first `.STR` on the inserted disc** (`D_8006AC30` from ISO root scan). Which
stages exist on that disc is how the player reaches disk1 vs disk2 content.

Some movies use `movie_number == 0` and live in the **STAGE*.CDF** folder (not
INTER). The table above is only for INTER payloads.

### 3.2 Descriptor (type = 1) — field map

Layout matches `FsCdfStream` / `StreamSlot` (`0x28` bytes). USA retail survey
(~65 movie rows on both discs).

| Off | Size | C (`data.movie`) | peassets | Role | Conf. |
|-----|------|------------------|----------|------|-------|
| `0x00` | `u16` | `type` | `stream_type` | **`1`** = movie | High |
| `0x02` | `u16` | `field_2` | `unknown1` | STAGE0 HED stream marker bit15 (`0x8000`); else usually `0` | High (bit15); low bits unused? |
| `0x04` | `u32` | `offset` | `offset_folder` | Start **sector** (folder-relative STAGE1–5; STAGE0-relative on HED). Absolutized at table load. **Seek LBA only if `movie_number == 0`** | High |
| `0x08` | `u32` | `interOffset` | `offset_inter` | Sector index **within** the disc’s `INTER*.STR` (`0` = file start). **Seek base when `movie_number ≠ 0`** | High |
| `0x0C` | `u16` | `field_c` | `unknown2` | Secondary match key in `Stream_FindSlot`: if ≠0, must equal `key[1]`; if `0`, any secondary. USA: almost always `0` | Medium |
| `0x0E` | `u16` | `id` | `stream_id` | **Primary lookup key** (`Stream_FindSlot` matches `key[0]`) | High |
| `0x10` | `u16` | `subId` | `stream_sub_id` | Matched against `arg1` in `Stream_FindSlot` (often `0`; small room sub-indices) | High |
| `0x12` | `u16` | `width` | `picture_width` | MDEC width (e.g. 320) → `D_8006AC5A` | High |
| `0x14` | `u16` | `height` | `picture_height` | MDEC height (e.g. 240, 192) → `D_8006AC6C` | High |
| `0x16` | `u16` | `field_16` | `unknown3` | Copied to `D_8006AC0E`. Usually `0`; occasional non-zero (display/crop-related?) | Low |
| `0x18` | `u16` | `field_18` | `unknown4` | Copied to `D_8006AC10`. Often **`24`** (`0x18`); else `0` / small values | Low–med |
| `0x1A` | `u16` | `field_1a` | `unknown5` | **≈ frame count** (stop / length hint), **not** sector count → `D_8006AC0C` | High (empirical) |
| `0x1C` | 6 | `field_1c` | `unknown6` | Mostly zeros. Low `u16` at `0x1C` → `D_8006AC16`. Sparse non-zero patterns | Low |
| `0x22` | `u16` | `field_22` | `unknown7` | **Display / buffer mode** → `D_8006AC14`. USA: **`0` or `1`**. Non-zero takes the title-style clear/setup path in `func_8001F180` (`Display_SetMode` 0xD010 vs 0xF010 when `== 1`) | Medium–high |
| `0x24` | `u16` | `field_24` | `movie_number` | **`0`** → payload in STAGE CDF; **≠0** → payload in INTER + rewrite seek. Also → `D_8006AC58` (volume fade path). Same value often shared by dual-disc pair rows | High |
| `0x26` | `u16` | `field_26` | `unknown8` | → `D_8006AC18`. Almost always `0` on USA | Low |

`Stream_InitFromSlot` wiring (slot field → BSS):

```text
offset      → D_8006AC08   (seek sector; may be rewritten)
field_1a    → D_8006AC0C   (frame-count / stop hint)
width       → D_8006AC5A
height      → D_8006AC6C
field_16    → D_8006AC0E
field_18    → D_8006AC10
field_1c    → D_8006AC16   (first halfword of the 6-byte block)
field_22    → D_8006AC14   (display mode branch)
movie_number→ D_8006AC58
field_26    → D_8006AC18
```

**Still unknown in detail:** semantic meaning of `field_16`, `field_18`, most of
`field_1c[6]`, and `field_26` beyond “copied into globals.” `offset` when
`movie_number ≠ 0` is still absolutized at load but **not used for INTER seek**
(legacy / unused for play head).

### 3.3 How the game picks which descriptor

The engine does **not** scan INTER for a valid frame. It always:

```text
caller supplies stream id (+ optional sub keys)
    → Stream_FindSlot (exact match on id / subId / field_c rules)
    → Stream_InitFromSlot(slot)
    → if movie_number ≠ 0: seek INTER_LBA + interOffset
      else:                seek absolutized offset (stage CDF)
    → CdCmd 0x61 play
```

#### Title (fully known)

ISO root scan sets disc class (`Wip_SysFlags.unknown_0[0]`):

- stage1/2 present → `1` (disk1-like)
- stage4/5 present → `2` (disk2-like)

```c
// title.c
if (Wip_SysFlags.unknown_0[0] == 2)
    key_id = 0x65;  // 101
else
    key_id = 0x64;  // 100
slot = Stream_FindSlot(key, 0, 0);
CdCmd_Enqueue(0x61, slot);
```

Both title rows share `interOffset = 0` (same video). Disc → id is still how
the key is chosen.

#### In-game

Same `Stream_FindSlot` path. The id comes from session/stage keys (e.g.
`Game_Session` field block filled from room/event data such as
`Stage_Ctx->field_20`). We have **not** fully decompiled every script path that
chooses id 100 vs 101 for stage‑3 duals; the engine side is only **id → slot**.

### 3.4 Dual descriptors (stage 3) — not dual-valid

Stage 3 appears on **both** discs. Its streaming list often contains **two rows
for the same cutscene** (same length / same `movie_number` pair, different
`stream_id` and `interOffset`):

```text
Same cutscene
  ├─ id A  +  interOffset_high  →  real clip head on INTER0 (disk1)
  └─ id B  +  interOffset_low   →  real clip head on INTER1 (disk2)
```

Both rows are listed in STAGE3 on **both** discs. Only one is a real frame‑1
start on the INTER that is actually inserted; the other lands mid-stream or on
non-STR data for that file. Content hashes match across discs at the paired
offsets (same video, different packing).

This is **not** “one descriptor valid on both INTER files,” and it is **not**
English vs Japanese video on USA. It is **per-disc INTER packing** exposed as
two table rows.

Extract policy (`extract.py` / `extract_movies.py`):

1. Keep INTER starts only if STR magic and **frame ≤ 1** on that disc’s file  
2. One owner per `(disk, INTER, sector)` (earlier stage wins shared starts)  
3. One extract per stem across discs  
4. Length = gap to next **validated** start, or `unknown5 × 11 + pad` if the gap
   is far shorter than the frame hint (avoids false boundaries)

### 3.5 Payload locations and seek start

Runtime absolutization of `offset` (table load):

- STAGE0: `offset += Fs_StageCdfSectors[0]` (STAGE0.CDF LBA)
- Folder: `offset += folder.offset + Fs_StageCdfSectors[stage]`

Play init (`Stream_InitFromSlot` + `func_8001F180`):

1. `D_8006AC08 = offset` (absolute LBA into stage CDF space)
2. If **`movie_number ≠ 0`**: **overwrite**  
   `D_8006AC08 = interOffset + D_8006AC30.sector`  
   (`D_8006AC30.sector` = ISO-root LBA of the disc’s `INTER*.STR`)
3. Seek with `CdIntToPos(D_8006AC08)` / `CdRead2`

| `movie_number` | Container | Sector form | Start |
|----------------|-----------|-------------|--------|
| **`0`** | `STAGE*.CDF` folder | 2048 B ISO user | Folder base + `offset` |
| **≠ 0** | `INTER0` / `INTER1` on current disc | 2336 B Mode 2 Form 1 | **`interOffset` only** (`0` valid = file start). Never fall back to `offset` |

STAGE0 title: `interOffset = 0`, `offset = 0x41B` (unused for seek). Playing from
`offset` as an INTER index wrongly starts mid-clip (~frame 106).

INTER sector layout:

```text
0x000  8 B    XA subheader
0x008  2048 B user data (STR header + payload)
0x808  280 B  ECC/EDC
──────── 2336 B total
```

ISO-extracted CDF sectors are already 2048 B user data (no 2336 wrapper).

### 3.6 Length: `field_1a` / `unknown5` is frame count

| Interpretation | Result |
|----------------|--------|
| As **sector** count (wrong) | ~1/10 of real duration |
| Gap to next movie start on same container | Frame count ≈ `field_1a` |

~**10 user-sectors per frame** (video + XA pad). Extract: gap to next validated
start, else `field_1a × 11 + pad`, then frame-align window ends.

### 3.7 STR sector header (2048 B user data)

Classic PlayStation STR (jPSXdec / libpress-compatible):

```text
0x00  u32  magic 0x80010160
0x04  u16  chunk index (0 .. total-1)
0x06  u16  number of chunks in this frame
0x08  u32  frame number (monotone within a clip)
0x0C  u32  demuxed frame size in bytes (multiple of 4)
0x10  u16  width
0x12  u16  height
0x14  u16  MDEC code count (÷2, rounded)
0x16  u16  0x3800
0x18  u16  quant scale
0x1A  u16  version (2 or 3)
0x1C  u32  0
0x20  …    2016 bytes chunk payload
```

Non-magic sectors are skipped (XA audio / pad). Demux: concatenate chunks
`0 .. total-1`, truncate to `frame_size`.

### 3.8 Demuxed frame = BS MDEC bitstream

Same layout as room backgrounds (`.bs`):

```text
0x00  u16  mdec_code_count_div2
0x02  u16  0x3800
0x04  u16  quant scale
0x06  u16  version (2 or 3)
0x08  …    VLC bitstream (16-bit LE words)
```

- **v2:** DC signed 10-bit absolute per block  
- **v3:** DC differential Huffman (separate Cr/Cb predictors; shared luma)  

Macroblocks **column-major**. Decode: `bs_codec.decode_bs_frame`.  
Typical sizes: **320×240** (title), **320×192** (many in-game), plus smaller clips.

### 3.9 XA audio (INTER movies)

INTER streams interleave **Form 2 XA-ADPCM** sectors with STR video (typical
**7 video + 1 audio**). Subheader on USA title FMV::

```text
file=1  channel=1  submode=0x64  codinginfo=0x01
  → stereo, 37800 Hz, 4-bit ADPCM
```

Payload after the 8-byte subheader is 18 × 128-byte sound groups (2304 B) plus
pad/EDC inside the 2336-byte raw sector. Decode matches FFmpeg ``xa_decode``
(filter headers at bytes 4–11, samples in column-major nibbles at
``16+i+j*4``): `tools/peassets/xa_codec.py`.

**CDF / ISO movies** (`movie_number == 0`) are stored as 2048-byte Form 1 user
sectors only in the extracted dump — **no XA track** there (often silent video,
or audio comes from a separate type-2 MTS stream).

Extract writes ``movie/{stem}.wav`` when XA sectors are present; meta gets an
``xa`` object (sector counts, rate, duration).

### 3.10 Extract layout

```text
raw/movie/{stem}.str     normalized 2048-byte STR video blob
movie/{stem}.mp4         **lossless** H.264 (crf0, yuv444p) + ALAC when XA
movie/{stem}.json        geometry, descriptor, XA/encode meta
movie/movies.json        catalog
```

Encode uses ``ffmpeg`` for **true lossless** (bit-exact vs decoded RGB + PCM):

* **Video:** ``libx264 -crf 0 -pix_fmt yuv444p`` (no chroma subsampling)
* **Audio:** **ALAC** at native XA rate (usually **37800 Hz** stereo)
* CDF movies are video-only MP4s (no XA in ISO dumps)

**Playback:** Windows Media Player **cannot** open this (no H.264 4:4:4, poor
ALAC). Use **VLC**, **mpv**, **MPC-HC**, or ``ffplay``.

```bash
# included in a full extract.py / --iso_extract run
python3 tools/peassets/extract.py ... -o assets/USA
# movies only
python3 tools/peassets/extract_movies.py --rom rom/USA --out assets/USA -j 16
# --no-audio  → video-only MP4 for INTER
# --no-mp4    → skip encode (json/raw only)
```

### 3.11 Runtime play path (summary)

```text
Disc insert → ISO scan → INTER LBA + disc class flag
Folder/HED load → Stream_Slots (all descriptors, including duals)
Caller sets stream id (title: disc flag; in-game: session/event)
Stream_FindSlot(id) → slot
CdCmd 0x61 + Stream_InitFromSlot
  movie_number≠0 → seek INTER + interOffset
  movie_number==0 → seek stage CDF + offset
STR → demux → MDEC → VRAM
```

### 3.12 Confidence / open items

| Topic | Status |
|-------|--------|
| Disc STAGE / INTER inventory | High |
| INTER seek = `interOffset + INTER_LBA` (incl. 0) | High (`func_8001F180`) |
| `movie_number` CDF vs INTER | High |
| Stream id lookup (`Stream_FindSlot`) | High |
| Title disc flag → id 100/101 | High (`title.c` + ISO scan) |
| Stage‑3 dual rows = per-disc packing (not dual-valid) | High (hashes + frame heads) |
| In-game script choice of id 100 vs 101 | Medium (path clear; not all callers decompiled) |
| `field_1a` ≈ frame count | High (empirical) |
| `field_22` display mode 0/1 | Medium–high (`func_8001F180`) |
| `field_c` secondary FindSlot key | Medium |
| `field_16` / `field_18` / `field_1c` / `field_26` | Low |
| `offset` meaning when `movie_number ≠ 0` | Low (loaded, unused for seek) |
| Sector length without next-start gap | Medium (`×11` estimate) |
| Frame rate (extract WebP @ 15 fps) | Medium |
| **XA audio** demux (INTER, codinginfo 0x01) | High (title ~122 s matches video) |
| XA on CDF movies | N/A in ISO 2048 dumps |
| Pack / re-encode STR / XA | Not implemented |

---

## 4. Type-store summary

| Type | `raw/{type}/` | inflated `{type}/` |
|------|---------------|---------------------|
| **audio** | `.mts` sector payload | `.wav` + `.json` |
| **movie** | `.str` (2048 B/sec video) | lossless `.mp4` (H.264 4:4:4 crf0 + ALAC) + `.json` |

Viewer: **By type → audio / movie**, with waveform + Play (WAV / WebP via
`ffplay` when available).

---

## 5. Quick examples (USA retail)

### Audio (STAGE0)

```text
type=2  id=1  stage=0  offset → 0x87C000 in STAGE0.CDF
MTS stereo, period 10, ~285 chunks → long BGM-style stream
```

### Movie (STAGE0 title FMV)

```text
type=1  id=100/101  movie_number=1  320×240
interOffset = 0  → sector 0 of that disc’s INTER (frame 1)
offset = 0x41B (ignored for INTER seek)
field_1a = 0x73A (~1850 frames)
span to next INTER clip (~18620 sectors) → full title (~2 min @ 15 fps)
title pick: disk1 → id 100, disk2 → id 101 (same video)
```

Wrong: treat `offset` as INTER start when `interOffset == 0` → mid-clip frame 106.  
Wrong: treat `0x73A` as sector count → ~186 frames only.

### Movie (stage 3 dual packing)

```text
Same cutscene, two STAGE3 rows (example folder 901):
  id=100  interOffset=75977  → valid frame-1 start on INTER0 only
  id=101  interOffset=18620  → valid frame-1 start on INTER1 only
Content at INTER0@75977 == INTER1@18620 (hash match)
```

---

## 6. Related docs

- [`ASSET_FORMATS.md`](ASSET_FORMATS.md) — stage chunks, BS stills, SPK, stages.json  
- jPSXdec *PlayStation1_STR_format* — general STR/MDEC reference  
- `include/main/cdstream.h` — `MtsSector`, `CdStreamState`  
- `include/main/stream.h` — `StreamSlot`, MDEC helpers  
