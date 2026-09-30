#ifndef MAIN_STREAM_TYPES_H
#define MAIN_STREAM_TYPES_H

#include "common.h"

/// Interpretations of a serialized stream descriptor.
enum {
    STREAM_KIND_EMPTY       = 0,
    STREAM_KIND_MOVIE       = 1,
    STREAM_KIND_SCENE_AUDIO = 2,
    STREAM_KEY_TERMINATOR   = 0,
    STREAM_KEY_ANY_GROUP    = 0,
};

/// Movie display/decode format, completion behavior and VRAM destination.
enum {
    STREAM_MOVIE_DISPLAY_TEXTURE    = 0,
    STREAM_MOVIE_DISPLAY_RGB24      = 1,
    STREAM_MOVIE_DISPLAY_RGB16      = 2,
    STREAM_MOVIE_STOP               = 0,
    STREAM_MOVIE_REPEAT             = 1,
    STREAM_MOVIE_UPLOAD_DRAW_BUFFER = 0,
    STREAM_MOVIE_UPLOAD_FIXED_VRAM  = 1,
};

/// VLC-table byte extent and scene buffer-placement selectors.
enum {
    STREAM_VLC_TABLE_BYTES          = 0x11000,
    STREAM_SCENE_VLC_RESERVED_TABLE = 0,
    STREAM_SCENE_VLC_IMAGE_BUFFER   = 1,
    STREAM_VLC_BUFFER_ALLOCATE      = 0,
    STREAM_VLC_BUFFER_ACTOR_0       = 1,
    STREAM_VLC_BUFFER_ACTOR_1       = 2,
    STREAM_VLC_BUFFER_ACTOR_2       = 3,
    STREAM_TIMING_BUFFER_NONE       = 0,
    STREAM_TIMING_BUFFER_ALLOCATE   = 1,
    STREAM_TIMING_BUFFER_ACTOR_0    = 2,
    STREAM_TIMING_BUFFER_ACTOR_1    = 3,
    STREAM_TIMING_BUFFER_ACTOR_2    = 4,
};

/// A serialized HED/CDF descriptor occupying one resident stream-table slot.
///
/// `kind` selects the movie or scene/audio interpretations of `control`,
/// `source` and `data`. All sector values are in 2048-byte CD sectors. Loaders
/// make `startSector` absolute and remove the HED marker (first-word bit 31).
/// A zero `key.word` ends a folder's serialized run; live unused slots have a
/// zero `startSector`. Table pointers borrow storage until it is reloaded.
typedef struct {
    s16 kind;                    // 0 empty, 1 STR movie, 2 scene/audio stream
    union {
        u16 headerBits;          // HED marker in bit 15; other movie bits have unproven meaning
        struct {
            u8 volumeIndex;      // CD-audio volume-table index
            u8 timingBufferKind; // 0 none, 1 allocate, 2/3/4 actor buffers 0/1/2
        } scene;
    } control;
    s32 startSector;           // Absolute CD sector after loading (0 no loaded sector)
    union {
        s32 interSectorOffset; // Movie: sector offset within INTER.STR
        s32 decodeBufferBytes; // Scene/audio: initial auxiliary decode-buffer allocation
    } source;
    union {
        s32 word;      // Zero terminates the serialized folder stream table
        struct {
            u16 group; // Movie room selector (0 wildcard); scene/audio exact group
            u16 id;    // Stream ID; movies match the location/view byte
        } parts;
    } key;
    u16 subId; // First exact 16-bit stream-selection qualifier
    union {
        struct {
            u16  width;            // Decoded image width in pixels
            u16  height;           // Decoded image height in rows
            u16  vramX;            // Upload origin X in VRAM words
            u16  vramY;            // Upload origin Y in VRAM rows
            u16  frameLimit;       // Playback stop frame, numbered from 1
            u16  loopMode;         // 1 repeats playback; other values stop
            u16  viewStream;       // Nonzero permits selection for view-stream playback
            byte unknown_20[2];    // Meaning unproven; included in complete descriptor copies
            u16  displayMode;      // 0 texture, 1 full-screen RGB24, 2 full-screen RGB16
            u16  volumeTableIndex; // Low byte selects CD volume; nonzero also selects INTER.STR
            u16  uploadMode;       // 1 fixed VRAM origin; other values add the draw-buffer Y offset
        } movie;
        struct {
            u16  subId2;             // Second exact 16-bit stream-selection qualifier
            u16  resumeSectorOffset; // Offset from startSector for audio after scene playback (0 none)
            u16  soundBankMask;      // Sound-bank selection bits applied around scene playback
            u16  vlcTableMode;       // 0 reserved table, 1 image-buffer table; other modes unproven
            u16  vlcBufferKind;      // Reserved VLC table: 0 allocate, 1/2/3 actor buffers 0/1/2
            u16  timingBytes;        // Timing-prefix bytes, rounded to sectors before audio starts
            u16  timingBufferBytes;  // Bytes reserved for the timing buffer
            byte unknown_20[8];      // Meaning unproven; no field-level consumer is established
        } scene;
    } data;
} StreamSlot;
STATIC_ASSERT_SIZEOF(StreamSlot, 0x28);

/// Payload destinations indexed by a scene-image header's `bufferKind`.
enum {
    STREAM_SCENE_BUFFER_DECODE   = 0,
    STREAM_SCENE_BUFFER_ACTOR_0  = 1,
    STREAM_SCENE_BUFFER_ACTOR_1  = 2,
    STREAM_SCENE_BUFFER_ACTOR_2  = 3,
    STREAM_SCENE_BUFFER_EXTERNAL = 4,
};

/// A serialized scene-image header cached with its loaded payload for view decoding.
///
/// The header occupies the first 60 bytes of the first sector. All offsets are
/// byte offsets from the payload immediately following it, in the buffer selected
/// by `bufferKind`. Actor-buffer payloads follow any reserved VLC and timing data.
/// The two three-element offset arrays have parallel relocation flags; a zero
/// offset omits that strip list or image chunk. The image offset may be zero.
///
/// A nonempty `sectorCount` includes the header sector and must be at most 32768
/// for the loader's signed remaining-sector counter. Offsets and transfers must
/// fit the payload and the selected buffer's separately supplied capacity.
/// Cached headers and payload pointers remain valid until that buffer is reused
/// or the scene stream is reset. A zero `forceReload` permits reuse while a
/// reusable payload is present; other values require a fresh transfer.
typedef struct {
    s32  imageDataOffset;        // VLC image data within the payload, in bytes
    s32  stripListOffsets[3];    // Image-strip work-list byte offsets (0 absent)
    s32  imageChunkOffsets[3];   // Compressed image-chunk byte offsets (0 absent)
    s32  timingDataOffset;       // Timing-table byte offset within the payload
    s32  nextDecodeBufferBytes;  // Next decode allocation size in bytes, used by buffer kind 0
    s16  relocateStripLists[3];  // Nonzero shifts strip X by -512 words and the first strip's Y by +128 rows
    s16  relocateImageChunks[3]; // Nonzero shifts chunk Y by +1 row, with another -3 for source Y=245..255
    u16  sectorCount;            // Total 2048-byte sectors including the header sector (0 no payload)
    s16  viewId;                 // View selector matched against the current view byte (0..255)
    s16  bufferKind;             // Payload destination (0 decode allocation, 1/2/3 actor buffers 0/1/2, 4 external)
    s16  forceReload;            // Cache policy (0 allow reuse, nonzero always read the payload)
    u16  timingDataBytes;        // Bytes copied from the payload into the stream's timing buffer
    byte unknown_3A[2];          // Copied with the header; role unproven
} StreamSceneImageHeader;
STATIC_ASSERT_SIZEOF(StreamSceneImageHeader, 0x3C);

/// ISO root scan: LBA of the first `.STR` (stream) file, plus a sibling word.
typedef struct {
    s32 sector;
    s32 field_4;
} FsStrInfo;

#endif // MAIN_STREAM_TYPES_H
