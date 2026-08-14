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

/// 4-byte scan descriptor for `func_800BB6FC`. field_0 is the start index
/// into a 4-byte item table; field_1 is the entry count; field_2 selects
/// the table (1 / 2 / default). Also the object at `Mc_SaveData.field_5BC`
/// (`D_80072724`).
typedef struct _GpItemScan {
    /* 0x00 */ u8 field_0;
    /* 0x01 */ u8 field_1;
    /* 0x02 */ u8 field_2;
    /* 0x03 */ u8 field_3;
} GpItemScan;
STATIC_ASSERT_SIZEOF(GpItemScan, 0x4);

/// 4-word (128-bit) flag array at `Mc_SaveData+0x5AC`. Indexed with
/// `id & 0x7F` (`func_800BAE5C` / `func_800BAEC0` / `func_800BB4BC`).
extern s32        D_80072714[4];
extern GpItemSlot D_80072330[];
extern GpItemMap  D_8010D2F8[];

void        func_800BAE38(void);
GpItemSlot* func_800BAFE0(s32 arg0);
s32         func_800BB6FC(GpItemScan* arg0, s32 arg1);
GpItemMap*  func_800BBDC8(s32 arg0);
s32         func_800BBEC0(s32 arg0);

#endif // GAMEPLAY_268_H
