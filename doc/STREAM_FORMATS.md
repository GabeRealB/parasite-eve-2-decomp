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
| Descriptor struct | `include/main/stream_types.h` (`StreamSlot`), `tools/peassets/format.py` |
| Runtime load of descriptors | `src/main/fs.c` (`fsBuildFolderTables`, `_fsStage0HeaderReadyCallback`) |
| Movie play | `src/main/stream.c` (`Stream_*`, `Mdec_*`), `cdcmd.c` (cmd `0x61`) |
| Audio play | `src/main/cdaudio.c`, `cdstream.c` (`CdStream_*`, `_MtsHeader`) |
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

Each entry is **`0x28` bytes** (`StreamSlot` / peassets `STREAMING_LIST_ENTRY_SIZE`).

Empty slots are all zeros. STAGE0 HED mixes streams with the file table using
the high bit of the first word when scanned as a flat sector list (see
`_fsStage0HeaderReadyCallback`: after the `0xFFFFFFFF` terminator test,
`(s32)fileId < 0` → stream).

### 1.2 Entry type

| `u16` @ `0x00` | Meaning |
|----------------|---------|
| `1` | **Movie** (`STREAM_KIND_MOVIE`) |
| `2` | **Scene/audio** (`STREAM_KIND_SCENE_AUDIO`) |

The named `control`, `source` and `data` unions in `StreamSlot` distinguish movie presentation from scene/audio resource and timing data.

### 1.3 Common header (both types)

| Off | Size | C / peassets | Notes |
|-----|------|--------------|-------|
| `0x00` | `s16` | `kind` / `stream_type` | `1` movie, `2` scene/audio; `0` in empty entries |
| `0x02` | `u16` | `control.headerBits` / `unknown1` | STAGE0 bit 15 (`0x8000`) marks a stream in the mixed HED walk; other movie bits are unproven. Scene/audio splits this into two bytes |
| `0x04` | `s32` | `startSector` / offset fields | CD sector offset on disc, absolute sector in a live slot; `0` marks an unloaded sector |
| `0x0C` | `u16` | `key.parts.group` | Movie room selector (`0` wildcard); scene/audio exact group |
| `0x0E` | `u16` | `key.parts.id` / `stream_id` | Primary stream ID; movie lookup matches the location/view byte |
| `0x10` | `u16` | `subId` / low half of `stream_sub_id` | First exact selection qualifier |

Runtime **absolutizes** `startSector` when loading:

- STAGE0: `+= Fs_StageCdfSectors[0]`
- Folder: `+= folder.sectorOffset + stage_cdf_base`

Both movie and scene/audio offsets in a folder are relative to that folder's
base. Peassets offset fields are measured in **bytes**, whereas the C descriptor
stores **2048-byte sectors**. A zero complete `key.word` terminates the serialized
folder run. The loader relies on that run fitting the fifteen-entry live table;
the eighteen-entry serialized capacity does not enlarge the runtime table.

### 1.4 Runtime table

`fsBuildFolderTables` copies non-empty entries into **`Stream_Slots[15]`**
(BSS). Title may bulk-copy `Fs_Streams` → `Stream_Slots`. Lookup:
`streamFindMovieSlot` selects a loaded movie using the location key's view byte
as stream ID, its room byte (or descriptor group 0 as a wildcard), and a 16-bit
sub-ID. Its optional view-stream filter stops at a rejected room-specific match,
but continues past a rejected wildcard. `streamFindViewMovieSlot` selects the
first view-enabled movie with the same ID/room rules, without checking sub-ID
or loaded sector. Both return a slot index 0..14 or `STREAM_SLOT_NOT_FOUND`.
Playback uses the CD command queue / CdAudio.

---

## 2. Audio streams (MTS)

### 2.1 Descriptor (type = 2)

These entries can load scene images/resources and a frame-timing prefix alongside
CD audio. Their common key is matched by `streamSelectScene` without a wildcard;
group zero selects the stage-zero table, and a nonzero group selects the folder
table. `data.scene` occupies the same bytes as `data.movie`.

| Off | Size | C | peassets | Role |
|-----|------|---|----------|------|
| `0x00` | `s16` | `kind` | `stream_type` | **`2`** (`STREAM_KIND_SCENE_AUDIO`) |
| `0x02` | `u8` | `control.scene.volumeIndex` | low byte of `unknown1` | CD-audio volume-table index |
| `0x03` | `u8` | `control.scene.timingBufferKind` | high byte of `unknown1` | `0` none, `1` auxiliary allocation, `2/3/4` actor buffers `0/1/2`; HED marker is cleared before use |
| `0x04` | `s32` | `startSector` | `offset_stage` | Absolute start sector after loading |
| `0x08` | `s32` | `source.decodeBufferBytes` | `unknown2` | Initial auxiliary decode-buffer allocation size in bytes |
| `0x0C` | `u16` | `key.parts.group` | `stage_number` | Exact stream-selection group; the external schema name does not establish a stage index |
| `0x0E` | `u16` | `key.parts.id` | `stream_id` | Exact stream ID |
| `0x10` | `u16` | `subId` | low half of `stream_sub_id` | First exact qualifier |
| `0x12` | `u16` | `data.scene.subId2` | high half of `stream_sub_id` | Second exact qualifier |
| `0x14` | `u16` | `data.scene.resumeSectorOffset` | `unknown3` | Resume CD audio at `startSector + resumeSectorOffset`; `0` skips this seek |
| `0x16` | `u16` | `data.scene.soundBankMask` | `unknown4` | Sound-bank selection mask applied around scene playback |
| `0x18` | `u16` | `data.scene.vlcTableMode` | low half of `unknown5` | `0` reserves/builds a VLC table, `1` builds it in the image buffer during decode; other modes are unproven |
| `0x1A` | `u16` | `data.scene.vlcBufferKind` | high half of `unknown5` | Reserved VLC table: `0` allocate, `1/2/3` actor buffers `0/1/2` |
| `0x1C` | `u16` | `data.scene.timingBytes` | low half of `unknown6` | Timing-prefix bytes rounded to sectors before audio starts |
| `0x1E` | `u16` | `data.scene.timingBufferBytes` | high half of `unknown6` | Timing-buffer allocation/reservation size in bytes |
| `0x20` | 8 | `data.scene.unknown_20` | `unknown7` | Role unproven; always zero in retail samples |

The timing buffer contains the `u32` entries used to pace scene frames. When it
shares an actor buffer with the reserved VLC table, it starts after the table's
`STREAM_VLC_TABLE_BYTES` (`0x11000`) bytes. Scene payloads in that same actor buffer start after the timing
buffer's reserved byte extent. The source allocation and timing-prefix byte
counts have separate roles and are not sector addresses.

### 2.2 Payload: MTS sector stream

Payload is a contiguous run of **2048-byte** ISO sectors in the stage CDF
(folder tail after file chunks, or absolute stage offset). Not XA Form 2.

Every **period** sectors an **MTS header sector** appears:

```text
0x00  s32   chunkIndex    chunk number, shared by every channel of the chunk
0x04  s32   chunkCount    total chunks in the stream (read from the first header)
0x08  u32   magic         LE 0x4D5453cc → bytes: cc 'S' 'T' 'M'
                          cc = channel count (retail streams are 2)
0x0C  s8    channelIndex  which channel this header's audio belongs to (0, 1)
0x0D  u8    period        sectors until the next header (retail: 5 or 10)
0x0E  u8    gapSectors    disc sectors skipped after this chunk
0x0F  s8    flags         bit7 apply gapSectors; bit6 stop halfway through the
                          final chunk; bit5 stop at the final chunk.
                          Bit6 wins when both are set. Neither plays it through.
                          Retail headers are 0xC0 or 0xA0 (bit7 plus bit6 or bit5)
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

Headers are **not** always a perfect grid. `gapSectors` on a chunk's channel-0
header is the number of disc sectors between the end of that chunk and the next
chunk's first header; intra-chunk channel headers stay `period` apart. Demux
must **scan** for MTS magics, not assume stride = period.

Some streams have a short **preamble** (TOC-like table) before the first MTS
header; skip until magic.

Nominal body length: `chunk_count × channels × period` sectors (from first
header). The descriptor's `data.scene.resumeSectorOffset` supplies a subsequent
audio seek; derive the body length from the MTS headers.

### 2.3 `_cdStreamSectorReadyCallback` write sizes (critical for decode)

Per period window (`sectorsLeft % mtsPeriod` in `_cdStreamSectorReadyCallback`):

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

### 2.6 Field contracts and remaining questions

| Field | Confidence |
|-------|------------|
| `kind`, `startSector`, `key.parts.group`, `key.parts.id` | High |
| `control.headerBits` bit15 on STAGE0 | High (HED stream marker) |
| `data.scene.resumeSectorOffset` as a subsequent audio seek | High |
| `source.decodeBufferBytes` as an initial auxiliary allocation | High |
| `data.scene.soundBankMask`, usually `0x20` | High (sound-bank mask consumers) |
| `data.scene.vlcTableMode` values `0` / `1` | High; other modes remain unproven |
| `control.scene.timingBufferKind`, `data.scene.vlcBufferKind` | High (allocation/reuse selectors) |
| `data.scene.timingBytes`, `data.scene.timingBufferBytes` | High (prefix and reservation byte counts) |
| `data.scene.unknown_20` | Role unproven |
| `_MtsHeader.chunkIndex`, `chunkCount`, `magic`, `channelIndex`, `period` | High |
| `_MtsHeader.gapSectors` | High: sector gap after the chunk, applied when `flags` bit 7 is set |
| `_MtsHeader.flags` bits 7, 6 and 5 | High: gap enable and final-chunk end point. Other bits are unread |
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

Practical mapping when playing **INTER** movies (`data.movie.volumeTableIndex != 0`):

| Stage | Lives on | INTER used |
|-------|----------|------------|
| 0 (title, …) | Both discs | That disc’s INTER (sector 0 title matches) |
| 1, 2 | Disk1 only | **INTER0 only** |
| 3 | **Both** discs | **INTER0 or INTER1** depending on inserted disc |
| 4, 5 | Disk2 only | **INTER1 only** |

The engine does **not** map stage number → INTER index. It always seeks the
**first `.STR` on the inserted disc** (`D_8006AC30` from ISO root scan). Which
stages exist on that disc is how the player reaches disk1 vs disk2 content.

Some movies use `data.movie.volumeTableIndex == 0` and live in the **STAGE*.CDF** folder (not
INTER). The table above is only for INTER payloads.

### 3.2 Descriptor (type = 1) — field map

Layout matches `StreamSlot` (`0x28` bytes). USA retail survey
(~65 movie rows on both discs). Peassets retains its external serialized field
names; the C movie interpretation is `data.movie`.

| Off | Size | C | peassets | Role | Conf. |
|-----|------|---|----------|------|-------|
| `0x00` | `s16` | `kind` | `stream_type` | **`1`** = movie | High |
| `0x02` | `u16` | `control.headerBits` | `unknown1` | STAGE0 HED stream marker bit15 (`0x8000`); other movie bits unproven | High (bit15) |
| `0x04` | `s32` | `startSector` | `offset_folder` | Start **sector**, absolutized at table load; seek base when `volumeTableIndex == 0` | High |
| `0x08` | `s32` | `source.interSectorOffset` | `offset_inter` | Sector offset **within** `INTER*.STR`; seek base when `volumeTableIndex != 0` | High |
| `0x0C` | `u16` | `key.parts.group` | `unknown2` | Movie room selector: `0` wildcard, otherwise matches key byte 1. USA: almost always `0` | High |
| `0x0E` | `u16` | `key.parts.id` | `stream_id` | Primary lookup ID matched against key byte 0 | High |
| `0x10` | `u16` | `subId` | `stream_sub_id` | Exact selection qualifier (often `0`; small room sub-indices) | High |
| `0x12` | `u16` | `data.movie.width` | `picture_width` | Decoded width in pixels (e.g. 320) → `D_8006AC5A` | High |
| `0x14` | `u16` | `data.movie.height` | `picture_height` | Decoded height in rows (e.g. 240, 192) → `D_8006AC6C` | High |
| `0x16` | `u16` | `data.movie.vramX` | `unknown3` | Upload origin X in VRAM words → `D_8006AC0E` | High |
| `0x18` | `u16` | `data.movie.vramY` | `unknown4` | Upload origin Y in VRAM rows → `D_8006AC10`; often **24** (`0x18`) | High |
| `0x1A` | `u16` | `data.movie.frameLimit` | `unknown5` | One-based playback stop frame → `D_8006AC0C`; can precede the physical STR end | High |
| `0x1C` | `u16` | `data.movie.loopMode` | low half of `unknown6` | `1` restarts playback after pausing at completion; other values stop | High |
| `0x1E` | `u16` | `data.movie.viewStream` | middle half of `unknown6` | Nonzero permits selection for view-stream playback | High |
| `0x20` | 2 | `data.movie.unknown_20` | high half of `unknown6` | Role unproven | Low |
| `0x22` | `u16` | `data.movie.displayMode` | `unknown7` | `0` texture stream, `1` full-screen RGB24, `2` full-screen RGB16 → `D_8006AC14` | High |
| `0x24` | `u16` | `data.movie.volumeTableIndex` | `movie_number` | Low byte selects CD volume; any nonzero value selects INTER seeking and enables streaming audio → `D_8006AC58` | High |
| `0x26` | `u16` | `data.movie.uploadMode` | `unknown8` | `1` uses fixed VRAM coordinates; other values add the current draw-buffer Y offset → `D_8006AC18` | High |

`_streamLoadMovieSlotState` wiring (slot field → BSS):

```text
startSector                → D_8006AC08   (seek sector; may be rewritten)
data.movie.frameLimit      → D_8006AC0C
data.movie.width           → D_8006AC5A
data.movie.height          → D_8006AC6C
data.movie.vramX           → D_8006AC0E
data.movie.vramY           → D_8006AC10
data.movie.loopMode        → D_8006AC16
data.movie.displayMode     → D_8006AC14
data.movie.volumeTableIndex→ D_8006AC58
data.movie.uploadMode      → D_8006AC18
```

The two bytes in `data.movie.unknown_20` have no field-level consumer.
`startSector` is still absolutized at load when `volumeTableIndex != 0`, but
INTER playback replaces that seek base with the root STR LBA plus
`source.interSectorOffset`. The extracted schema's `movie_number` therefore
selects an audio-volume entry and INTER playback, rather than identifying a
particular movie.

### 3.3 How the game picks which descriptor

The engine does **not** scan INTER for a valid frame. It always:

```text
caller supplies stream id (+ optional sub keys)
    → streamFindMovieSlot (exact match on key.parts.id / subId / key.parts.group rules)
    → _streamLoadMovieSlotState(slot)
    → if data.movie.volumeTableIndex ≠ 0: seek INTER_LBA + source.interSectorOffset
      else:                seek absolutized offset (stage CDF)
    → CdCmd 0x61 play
```

#### Title (fully known)

ISO root scan sets the disc number (`Wip_SysFlags.discNumber`):

- stage1/2 present → `1` (disk1-like)
- stage4/5 present → `2` (disk2-like)

```c
// title.c
key = gGameSession->location;
if (Wip_SysFlags.discNumber == GAME_MAIN_DISC_2)
    key.loc.view = 0x65;  // 101
else
    key.loc.view = 0x64;  // 100
slotParam[0] = streamFindMovieSlot(&key.loc, 0, 0);
cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
```

Both title rows share `source.interSectorOffset = 0` (same video). Disc → id is still how
the key is chosen. `data.movie.displayMode == STREAM_MOVIE_DISPLAY_RGB24`: 24-bit MDEC
(`displayConfigureFramebuffers` with `DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_RGB24 |
DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW`, packed value 0xF010). 320×240.

#### In-game

Same `streamFindMovieSlot` path. The id comes from session/stage keys (e.g.
`gGameSession` field block filled from room/event data such as
`Stage_Ctx->pendingView`). We have **not** fully decompiled every script path that
chooses id 100 vs 101 for stage‑3 duals; the engine side is only **id → slot**.

### 3.4 Dual descriptors (stage 3) — not dual-valid

Stage 3 appears on **both** discs. Its streaming list often contains **two rows
for the same cutscene** (same length / same `data.movie.volumeTableIndex` pair, different
`stream_id` and `source.interSectorOffset`):

```text
Same cutscene
  ├─ id A  +  higher source.interSectorOffset  →  real clip head on INTER0 (disk1)
  └─ id B  +  lower source.interSectorOffset   →  real clip head on INTER1 (disk2)
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

Runtime absolutization of `startSector` (table load):

- STAGE0: `startSector += Fs_StageCdfSectors[0]` (STAGE0.CDF LBA)
- Folder: `startSector += folder.sectorOffset + Fs_StageCdfSectors[stage]`

Play init (`_streamLoadMovieSlotState` + `streamPrepareMoviePlayback`):

1. `D_8006AC08 = startSector` (absolute LBA into stage CDF space)
2. If **`data.movie.volumeTableIndex != 0`**: **overwrite**
   `D_8006AC08 = source.interSectorOffset + D_8006AC30.startSector`
   (`D_8006AC30.startSector` = ISO-root LBA of the disc’s `INTER*.STR`)
3. Seek with `CdIntToPos(D_8006AC08)` / `CdRead2`

| `data.movie.volumeTableIndex` | Container | Sector form | Start |
|----------------|-----------|-------------|--------|
| **`0`** | `STAGE*.CDF` folder | 2048 B ISO user | Absolutized `startSector` |
| **≠ 0** | `INTER0` / `INTER1` on current disc | 2336 B Mode 2 Form 1 | **`source.interSectorOffset` only** (`0` valid = file start). Never fall back to `startSector` |

STAGE0 title: `source.interSectorOffset = 0`, serialized `startSector = 0x41B` (unused for seek). Playing from
`startSector` as an INTER index wrongly starts mid-clip (~frame 106).

INTER sector layout:

```text
0x000  8 B    XA subheader
0x008  2048 B user data (STR header + payload)
0x808  280 B  ECC/EDC
──────── 2336 B total
```

ISO-extracted CDF sectors are already 2048 B user data (no 2336 wrapper).

### 3.6 Length: `data.movie.frameLimit` / `unknown5` is a playback frame limit

| Interpretation | Result |
|----------------|--------|
| As **sector** count (wrong) | ~1/10 of real duration |
| Gap to next movie start on same container | Physical frame count ≈ `data.movie.frameLimit`; playback may stop earlier |

~**10 user-sectors per frame** (video + XA pad). Extract: gap to next validated
start, else `data.movie.frameLimit × 11 + pad`, then frame-align window ends.

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

**CDF / ISO movies** (`data.movie.volumeTableIndex == 0`) are stored as 2048-byte Form 1 user
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
streamFindMovieSlot(&key.loc, subId, 0) → slot
CdCmd 0x61 + _streamLoadMovieSlotState
  data.movie.volumeTableIndex≠0 → seek INTER + source.interSectorOffset
  data.movie.volumeTableIndex==0 → seek stage CDF + startSector
STR → demux → MDEC → VRAM
```

### 3.12 Confidence / open items

| Topic | Status |
|-------|--------|
| Disc STAGE / INTER inventory | High |
| INTER seek = `source.interSectorOffset + INTER_LBA` (incl. 0) | High (`streamPrepareMoviePlayback`) |
| `data.movie.volumeTableIndex` volume selection and CDF vs INTER | High |
| Stream id lookup (`streamFindMovieSlot`) | High |
| Title disc flag → id 100/101 | High (`title.c` + ISO scan) |
| Stage‑3 dual rows = per-disc packing (not dual-valid) | High (hashes + frame heads) |
| In-game script choice of id 100 vs 101 | Medium (path clear; not all callers decompiled) |
| `data.movie.frameLimit` ≈ frame count | High (empirical) |
| `data.movie.displayMode` (texture / RGB24 / RGB16) | High (`streamPrepareMoviePlayback`) |
| `key.parts.group` movie room selector | High |
| `data.movie.vramX`, `vramY`, `loopMode`, `uploadMode` | High (upload and playback consumers) |
| `data.movie.unknown_20` | Role unproven |
| `startSector` when `data.movie.volumeTableIndex != 0` | Absolutized at load, then replaced for INTER seeking |
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
kind=1  key.parts.id=100/101  data.movie.volumeTableIndex=1  320×240
source.interSectorOffset = 0  → sector 0 of that disc’s INTER (frame 1)
serialized startSector = 0x41B (ignored for INTER seek)
data.movie.frameLimit = 0x73A (~1850 frames)
span to next INTER clip (~18620 sectors) → full title (~2 min @ 15 fps)
title pick: disk1 → id 100, disk2 → id 101 (same video)
```

Wrong: treat `startSector` as INTER start when `source.interSectorOffset == 0` → mid-clip frame 106.
Wrong: treat `0x73A` as sector count → ~186 frames only.

### Movie (stage 3 dual packing)

```text
Same cutscene, two STAGE3 rows (example folder 901):
  id=100  source.interSectorOffset=75977  → valid frame-1 start on INTER0 only
  id=101  source.interSectorOffset=18620  → valid frame-1 start on INTER1 only
Content at INTER0@75977 == INTER1@18620 (hash match)
```

---

## 6. Related docs

- [`ASSET_FORMATS.md`](ASSET_FORMATS.md) — stage chunks, BS stills, SPK, stages.json
- jPSXdec *PlayStation1_STR_format* — general STR/MDEC reference
- `src/main/cdstream.c` — `_MtsHeader`, `CdStreamState`
- `include/main/stream_types.h` — `StreamSlot`; `include/main/stream.h` — MDEC helpers
