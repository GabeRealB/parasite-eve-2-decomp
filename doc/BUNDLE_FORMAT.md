# Resource bundle (`pe2bundle`)

| | |
|---|---|
| Disc | CDF chunk, opcode 4 (`FILE_SYSTEM_CHUNK_BUNDLE`) |
| Asset type | `pe2bundle`, `raw/pe2bundle/<name>.pe2bundle`; no decoded form |
| Loader | `src/main/fs.c` (`_FsCdfResourceEntry`, `gFsResourceSlots`) |
| Tool | `tools/peassets/bundle.py` |
| Contents | up to 50 numbered resources; each extracted as an asset of its own |

## 1. File

| Offset | Size | |
|---|---|---|
| `0x000` | `0x320` | directory: 50 entries, one per slot 0..49 |
| `0x320` | `0x4D0` | zero |
| `0x7F0` | | resources, in slot order |

Offsets are from the end of the 16-byte CDF chunk header, which the extractor
strips. The header's load address is where everything after the first sector
is streamed.

## 2. Directory entry (`_FsCdfResourceEntry`, 16 bytes)

| Offset | Type | Field | |
|---|---|---|---|
| `0x0` | `u8` | `kind` | 0 empty, 2 image, 3 data |
| `0x1` | `u8` | `pad` | 0 |
| `0x2` | `u16` | `sectorsBeforeRedirect` | 0 in retail |
| `0x4` | `u32` | `byteSize` | resource length; not read by the loader |
| `0x8` | `u32` | `destination` | RAM address of the resource |
| `0xC` | `u32` | `redirectDestination` | 0 in retail (stream stays contiguous) |

## 3. Layout rules

Hold in all 172 retail bundles; together they make every directory field derivable.

- The first used slot's `destination` is the chunk's load address.
- Each next resource starts at the previous one's end rounded up to 16 bytes, in RAM and in the file.
- Gap bytes up to the 16-byte boundary: `0xFF`.
- After the last resource: zeros to a sector boundary, less the 16-byte chunk header.
- Exceptions: `shelter_b1_pod_service_gantry_bundle` and `shelter_b6_nursery_bundle` carry one more zero sector.

## 4. Resources

| Kind | Identified by | Asset type | Name when first met | Read by |
|---|---|---|---|---|
| 2 | — | `pe2clut` | `<room>_view<slot+1>` (view palette), else `<room>_slot<slot>` | view change: slot *mapped view − 1* (`view_image.c`, `cd_loading.c`); `actor_560800`: slots 35, 36 |
| 3 | `"CAP2"` | `cap` ([`CAP_FORMAT.md`](CAP_FORMAT.md)) | `<room>_dialogue_<i>`, *i* = ordinal among kind-3 slots | `capSelectLoadedFile` (*i*-th kind-3 slot) |
| 3 | `"STF1"` | `stf` | `<bundle>_credits` | `_endingSelectCreditsResource` (*i*-th kind-3 slot) |
| 3 | `u16` `0x3800` at `+2` | `bs` | `<bundle>_picture_<slot>` | `ending.c`, by slot |
| 3 | only resource, loaded to `0x80161E20` | `demo` | bundle name less `_bundle` | `titleRestoreAttractDemoState` |

Resources are deduplicated by bytes: one already an asset keeps that asset's name.
The loader publishes each entry's `kind` and `destination` to `gFsResourceSlots[50]`;
`fsStartFolderDirectoryRead` clears them.

### 4.1 Image resource (`FsImageChunk`)

| Offset | Type | |
|---|---|---|
| `0x0` | `u16 x, y, h, w` | VRAM rectangle; height before width |
| `0x8` | 8 bytes | unread; 0 |
| `0x10` | LZSS | `h` rows of `w` colours |

Byte for byte a `.pe2clut` chunk. Rows 245..255 are shifted by `gFsImageHeaderYShiftRows` on upload.

| Shape | Count | |
|---|---|---|
| 256 × 6 at (0, 255) | 932 | six 256-colour palettes of one view |
| 256 × 2 at (0, 245) | 2 | `shelter_b1_pod_service_gantry_bundle` slots 35, 36; uploaded by `actor_560800` with row shift 5 and 8 |

### 4.2 Data resources

| Asset type | Format |
|---|---|
| `cap` | [`CAP_FORMAT.md`](CAP_FORMAT.md) |
| `stf` | `EndingStfFile` (`src/aya/ending/ending_private.h`): magic, `field_4`, offsets of params, glyphs, line table, sprites; relocated in place |
| `bs` | BS v2 MDEC bitstream; size and VRAM position from the overlay's picture record |
| `demo` | `0xD4C`-byte game state (live save, player status, five stage banks, flag nibble bank), then pad input as (buttons, duration) `u16` pairs |

## 5. Asset record

```python
'rooms/akropolis/square/bundle': {"sha1": "…", "type": "pe2bundle",
    'resources': [None, None, 'rooms/akropolis/square/view3_palette', …, 'rooms/akropolis/square/view8_palette',
                  None, …, 'rooms/akropolis/square/dialogue_0', 'akropolis_square_dialogue_1']},
```

| Field | |
|---|---|
| `resources` | asset per slot, `None` when empty; ends at the last used slot |
| `extra_sectors` | zero sectors past §3's end; only where nonzero |

Rebuild: the record, the resources' stored bytes and the load address (`stages.json`).
`bundle.py --check` rebuilds every bundle and compares.

## 6. Retail

| Bundles | Where | Resources |
|---|---|---|
| 168, `<room>_bundle` | room folders | 934 palettes (872 distinct), 213 dialogues (166 distinct) |
| `ending_bundle` | disc file 21000 | credits (slot 0), 18 pictures (slots 1..19; 15 empty) |
| `attract_demo_1_bundle`..`3` | stage-zero files 801000, 801100, 801200 | one demo each |
