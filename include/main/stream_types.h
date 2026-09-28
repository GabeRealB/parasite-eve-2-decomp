#ifndef MAIN_STREAM_TYPES_H
#define MAIN_STREAM_TYPES_H

#include "common.h"

/// One 0x28-byte HED stream descriptor, shared by the stage-zero Fs_Streams
/// table and the per-folder Stream_Slots table. Type 1 describes STR playback;
/// type 2 is used by the scene/CD-audio loader. Remaining fields depend on type.
/// The stage-zero loader clears the marker bit (bit 31 of the first word) and
/// makes the sector offset absolute before publishing the descriptor.
typedef struct _StreamSlot {
    /* 0x00 */ s16 field_0;
    /* 0x02 */ u8  volumeIndex; // Type 2: selects the CdAudio_StartTrack volume-table entry
    /* 0x03 */ u8  bufferKind;  // Type 2: secondary buffer selection (0..4)
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    // A zero key word terminates the folder's descriptor run.
    /* 0x0C */ union {
        s32 word;
        struct {
            u16 group;
            u16 id;
        } parts;
    } key;
    /* 0x10 */ u16  field_10;
    /* 0x12 */ u16  field_12;
    /* 0x14 */ u16  field_14;
    /* 0x16 */ u16  field_16;
    /* 0x18 */ u16  field_18;
    /* 0x1A */ u16  field_1A;
    /* 0x1C */ u16  field_1C;
    /* 0x1E */ u16  field_1E;
    /* 0x20 */ byte unknown_20[0x2];
    /* 0x22 */ u16  field_22;
    /* 0x24 */ u16  field_24;
    /* 0x26 */ u16  field_26;
} StreamSlot;
STATIC_ASSERT_SIZEOF(StreamSlot, 0x28);

/// ISO root scan: LBA of the first `.STR` (stream) file, plus a sibling word.
typedef struct {
    s32 sector;
    s32 field_4;
} FsStrInfo;

#endif // MAIN_STREAM_TYPES_H
