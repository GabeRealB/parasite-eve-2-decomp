#ifndef GAMEPLAY_268_H
#define GAMEPLAY_268_H

#include "common.h"

/// 8-byte inventory slot in `D_80072330`. field_0/field_2 are item ids;
/// field_1/field_3 are the matching counts (`func_800BAFF4`).
typedef struct _GpItemSlot {
    /* 0x00 */ u8   field_0;
    /* 0x01 */ u8   field_1;
    /* 0x02 */ u8   field_2;
    /* 0x03 */ u8   field_3;
    /* 0x04 */ byte pad_4[4];
} GpItemSlot;
STATIC_ASSERT_SIZEOF(GpItemSlot, 0x8);

extern GpItemSlot D_80072330[];

GpItemSlot* func_800BAFE0(s32 arg0);

#endif // GAMEPLAY_268_H
