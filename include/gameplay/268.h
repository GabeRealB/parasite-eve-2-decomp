#ifndef GAMEPLAY_268_H
#define GAMEPLAY_268_H

#include "common.h"

#include "main/session.h"

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

/// 4-byte scan descriptor for `func_800BB500` / `func_800BB6FC` /
/// `func_800BB5BC` / `func_800BB540` / `func_800BB610`. field_0 is
/// the start index into a 4-byte item table; field_1 is the entry count;
/// field_2 selects the table (1 / 2 / default). Also the object at
/// `Mc_SaveData.field_5BC` (`D_80072724`). `D_8010D520` is a ROM default
/// copied there by `func_800BC490`.
typedef struct _GpItemScan {
    /* 0x00 */ u8 field_0;
    /* 0x01 */ u8 field_1;
    /* 0x02 */ u8 field_2;
    /* 0x03 */ u8 field_3;
} GpItemScan;
STATIC_ASSERT_SIZEOF(GpItemScan, 0x4);

/// Overlay of `D_80072330` that places `D_80072724` at +0x3F4. Callers that
/// already have the inventory-slot base in a register pass `&scan` as
/// `base + 0x3F4` (`func_800BB3C0` / `func_800BB418`).
typedef struct _GpItemBlock {
    /* 0x000 */ byte       pad_0[0x3F4];
    /* 0x3F4 */ GpItemScan scan;
} GpItemBlock;
STATIC_ASSERT_SIZEOF(GpItemBlock, 0x3F8);

/// 4-byte row in the item tables selected by `GpItemScan` (`D_80072314` /
/// `D_80114C20` / `*D_80114D70`). field_0 is the item id looked up by
/// `func_800D6910` / returned by `func_800BB610`; field_1 is a count
/// compared as signed by `func_800CF448` / `func_800B91C8`; field_2 is a
/// u16 quantity added by `func_800BB6FC`.
typedef struct _GpItemRec {
    /* 0x00 */ u8  field_0;
    /* 0x01 */ u8  field_1;
    /* 0x02 */ u16 field_2;
} GpItemRec;
STATIC_ASSERT_SIZEOF(GpItemRec, 0x4);

/// 4-byte entry in `D_8010D278` / `D_8010E238` (32 entries, item ids
/// 0x80–0x9F). field_0 is a count (`func_800BB938` / `func_800B6CF0`);
/// field_1–3 are related item ids (`func_800C942C` / `func_800B904C`).
/// `D_8010E238` is the first `GpItemSlot` pair (arg1 == 0);
/// `D_8010D278` is the second.
typedef struct _GpItemQty {
    /* 0x00 */ u8 field_0;
    /* 0x01 */ u8 field_1;
    /* 0x02 */ u8 field_2;
    /* 0x03 */ u8 field_3;
} GpItemQty;
STATIC_ASSERT_SIZEOF(GpItemQty, 0x4);

/// 8-byte entry in `D_8010D230`, indexed by session field_7 /
/// `GameSessionFrom4.field_3`. field_4 is packed 2-bit flags
/// (`func_800BB974` / `func_800BB8E8` / `func_800BB470`).
typedef struct _GpBit2Bank {
    /* 0x00 */ void* field_0;
    /* 0x04 */ u32*  field_4;
} GpBit2Bank;
STATIC_ASSERT_SIZEOF(GpBit2Bank, 0x8);

/// 8-byte item attribute row. `D_8010DFB8` is indexed by raw item id
/// (`func_800BC324`); ids 0x60–0x7F land in the `D_8010E2B8` slice.
/// field_5 is the unsigned base added to `Mc_SaveData.field_908[id-0x60]`
/// and clamped to 10.
typedef struct _GpItemAttr {
    /* 0x00 */ u8 field_0;
    /* 0x01 */ u8 field_1;
    /* 0x02 */ u8 field_2;
    /* 0x03 */ u8 field_3;
    /* 0x04 */ u8 field_4;
    /* 0x05 */ u8 field_5;
    /* 0x06 */ u8 field_6;
    /* 0x07 */ u8 field_7;
} GpItemAttr;
STATIC_ASSERT_SIZEOF(GpItemAttr, 0x8);

/// 4-word (128-bit) flag array at `Mc_SaveData+0x5AC`. Indexed with
/// `id & 0x7F` (`func_800BAE5C` / `func_800BAEC0` / `func_800BB4BC`).
/// `func_800BAF08` returns the number of bits set across all four words.
extern s32        D_80072714[4];
extern GpItemRec  D_80072314[];
extern GpItemSlot D_80072330[];
extern GpItemScan D_80072724;
extern GpBit2Bank D_8010D230[];
extern GpItemQty  D_8010D278[];
extern GpItemMap  D_8010D2F8[];
extern GpItemScan D_8010D520;
extern GpItemAttr D_8010DFB8[];
extern GpItemQty  D_8010E238[];
/// Byte remap of an item id used as a sort/order key (`func_800BC18C` /
/// `func_800B8588`). Split by item class: 0x01–0x5F → `D_80114A40[id]`,
/// 0x60–0x7F → `D_80114A88[id-0x60]`, 0x80–0x9F → `D_80114A98[id-0x80]`,
/// 0xA0–0xBF → `D_80114ABC[id-0xA0]`. A 0 entry (or an id outside those
/// ranges) falls back to `id + 0x100`; id 0 returns 0x1000.
extern u8         D_80114A40[];
extern u8         D_80114A88[];
extern u8         D_80114A98[];
extern u8         D_80114ABC[];
extern GpItemRec  D_80114C20[];
extern GpItemRec* D_80114D70;

void        func_800BAE38(void);
s32         func_800BAF08(void);
void        func_800B91C8(GpItemRec* arg0);
GpItemSlot* func_800BAFE0(s32 arg0);
void        func_800BB190(s32 arg0, s32 arg1);
s32         func_800BB26C(GpItemScan* arg0, s32 arg1);
s32         func_800BB3C0(s32 arg0, s32 arg1);
s32         func_800BB418(s32 arg0, s32 arg1);
s32         func_800BB470(s32 arg0);
s32         func_800BB4BC(s32 arg0);
GpItemRec*  func_800BB500(GpItemScan* arg0);
s32         func_800BB540(GpItemScan* arg0, GpItemRec* arg1);
GpItemRec*  func_800BB5BC(GpItemScan* arg0, s32 arg1);
s32         func_800BB610(GpItemScan* arg0, s32 arg1);
s32         func_800BB6FC(GpItemScan* arg0, s32 arg1);
void        func_800BB7C0(s32 arg0, s32 arg1);
void        func_800BB8E8(s32 arg0, u8 arg1, s32 arg2);
s32         func_800BB974(GameSessionFrom4* arg0, s32 arg1);
GpItemMap*  func_800BBDC8(s32 arg0);
s32         func_800BBEC0(s32 arg0);
s32         func_800BC18C(s32 arg0);
s32         func_800BC324(s32 arg0);

#endif // GAMEPLAY_268_H
