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

/// 4-byte table entry in `D_8010D2F8` (8 entries). field_1 is an item id
/// used to index `D_80072330`; field_0 selects which (id, count) pair;
/// field_2 is the mapped item id (`func_800B6CF0`).
typedef struct _GpItemMap {
    /* 0x00 */ u8 field_0;
    /* 0x01 */ u8 field_1;
    /* 0x02 */ u8 field_2;
    /* 0x03 */ u8 field_3;
} GpItemMap;
STATIC_ASSERT_SIZEOF(GpItemMap, 0x4);

extern GpItemSlot D_80072330[];
extern GpItemMap  D_8010D2F8[];

GpItemSlot* func_800BAFE0(s32 arg0);
GpItemMap*  func_800BBDC8(s32 arg0);

#endif // GAMEPLAY_268_H
