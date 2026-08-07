#ifndef STREAM_H
#define STREAM_H

#include "common.h"

// Types — stream slots (src/main/stream.c)

/// Element of BSS array Stream_Slots (15 entries, total 0x258).
/// Fields inferred from F344.c accessors (Stream_InitFromSlot, Stream_FindSlotByKey, etc.).
typedef struct _StreamSlot {
    /* 0x00 */ s16  field_0;
    /* 0x02 */ byte unknown_2[0x2];
    /* 0x04 */ s32  field_4;
    /* 0x08 */ byte unknown_8[0x4];
    /* 0x0C */ u16  field_C;
    /* 0x0E */ u16  field_E;
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

#endif // STREAM_H
