#ifndef GAMEPLAY_268_H
#define GAMEPLAY_268_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"

struct _GpEnemy;

/// Save-inventory slot (`Mc_SaveData.field_1C8`). field_0/field_2 are item ids;
/// field_1/field_3 are the matching counts (`func_800BAFF4`).
typedef McItemSlot GpItemSlot;

/// 4-byte table entry in `D_8010D2F8` (8 entries). field_1 is an item id
/// used to index `Mc_SaveData.field_1C8`; field_0 selects which (id, count) pair;
/// field_2 is the mapped item id (`func_800B6CF0`).
typedef struct _GpItemMap {
    /* 0x00 */ u8 field_0;
    /* 0x01 */ u8 field_1;
    /* 0x02 */ u8 field_2;
    /* 0x03 */ u8 field_3;
} GpItemMap;
STATIC_ASSERT_SIZEOF(GpItemMap, 0x4);

/// 4-byte scan descriptor for `func_800BB500` / `func_800BB6FC` /
/// `func_800BB5BC` / `func_800BB540` / `func_800BB610` / `func_800BC3F8` /
/// `func_800BAF5C` / `func_800B8988`.
/// field_0 is the start index into a 4-byte item table; field_1 is the
/// entry count; field_2 selects the table (1 / 2 / default). Also
/// `Mc_SaveData.field_5BC`. `D_8010D520` is a ROM default copied there
/// by `func_800BC490`.
typedef McItemScan GpItemScan;

/// 4-byte row in the item tables selected by `GpItemScan`
/// (`Mc_SaveData.field_1AC` / `D_80114C20` / `*D_80114D70`). field_0 is the
/// item id looked up by `func_800D6910` / `func_800D6A24` / returned by
/// `func_800BB610` / matched by `func_800BC3F8`; field_1 is a count
/// compared as signed by `func_800CF448` / `func_800B91C8` /
/// `func_800D6994` (`== arg0 + 1`) /
/// `func_800BC3F8` (`> 0`); field_2 is a u16 quantity added by
/// `func_800BB6FC`.
typedef McItemRec GpItemRec;

/// 4-byte entry in `D_8010D278` / `D_8010E238` (32 entries, item ids
/// 0x80–0x9F). field_0 is a count (`func_800BB938` / `func_800B6CF0`);
/// field_1–3 are related item ids (`func_800C942C` / `func_800B904C` /
/// `func_800CF4EC`).
/// `D_8010E238` is the first `GpItemSlot` pair (arg1 == 0);
/// `D_8010D278` is the second.
typedef struct _GpItemQty {
    /* 0x00 */ u8 field_0;
    /* 0x01 */ u8 field_1;
    /* 0x02 */ u8 field_2;
    /* 0x03 */ u8 field_3;
} GpItemQty;
STATIC_ASSERT_SIZEOF(GpItemQty, 0x4);

/// 16-byte record walked by `func_800BB838` / `func_800BAB64`.
/// field_0 is a packed item id (0xFFFF terminator); field_6 low 2 bits
/// are the value written into the dest 2-bit bank.
typedef struct _GpBit2Rec {
    /* 0x00 */ u16  field_0;
    /* 0x02 */ byte pad_2[4];
    /* 0x06 */ u16  field_6;
    /* 0x08 */ byte pad_8[8];
} GpBit2Rec;
STATIC_ASSERT_SIZEOF(GpBit2Rec, 0x10);

/// 8-byte list node walked by `func_800BB838` / `func_800BAB64`.
/// field_0 is a `GpBit2Rec` list (NULL skips; `(GpBit2Rec*)-1` ends).
/// `D_8010D230[i].field_0` points at a table of these.
typedef struct _GpBit2List {
    /* 0x00 */ GpBit2Rec* field_0;
    /* 0x04 */ byte       pad_4[4];
} GpBit2List;
STATIC_ASSERT_SIZEOF(GpBit2List, 0x8);

/// 8-byte entry in `D_8010D230`, indexed by session field_7 /
/// `GameSessionFrom4.field_3` / `Mc_SaveData.field_7`. field_0 is a
/// `GpBit2List` table applied by `func_800BAB64` / `func_800BB838`.
/// field_4 is packed 2-bit flags (`func_800BB974` / `func_800BB8E8` /
/// `func_800BB470` / `func_800BAC34`).
typedef struct _GpBit2Bank {
    /* 0x00 */ GpBit2List* field_0;
    /* 0x04 */ u32*        field_4;
} GpBit2Bank;
STATIC_ASSERT_SIZEOF(GpBit2Bank, 0x8);

/// Spawn header for `func_800BBA70`. `field_0` is `Task_SpawnFromTable`
/// arg2; `field_4` is the `TaskDesc` table (`func_800B01AC` uses idx 0).
typedef struct _GpEnemyDesc {
    /* 0x0 */ u16      field_0;
    /* 0x2 */ byte     pad_2[2];
    /* 0x4 */ TaskDesc field_4;
} GpEnemyDesc;
STATIC_ASSERT_SIZEOF(GpEnemyDesc, 0x10);

/// Placement record for `func_800BBA70`. `field_0` / `field_4` pack into
/// `GpEnemy.field_8` as `field_0 | (field_4 << 8)`; `field_2` is copied to
/// `GpEnemy.field_A`. `field_8` / `field_A` / `field_C` are world X/Y/Z
/// (`GsCOORDINATE2.coord.t`); `field_E` is the yaw stored at coord +0x46
/// and passed to `Gfx_RotMatrixY` when non-zero.
typedef struct _GpEnemyPlace {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ u16  field_2;
    /* 0x4 */ u16  field_4;
    /* 0x6 */ byte pad_6[2];
    /* 0x8 */ s16  field_8;
    /* 0xA */ s16  field_A;
    /* 0xC */ s16  field_C;
    /* 0xE */ u16  field_E;
} GpEnemyPlace;
STATIC_ASSERT_SIZEOF(GpEnemyPlace, 0x10);

/// Overlay of `GsCOORDINATE2` at `GameActorExt.field_8` used by `func_800BBA70`.
/// `coord` is `GsCOORDINATE2.coord` (`Gfx_RotMatrixY` / `t[0..2]`); `field_46`
/// is the yaw halfword next to `param`.
typedef struct _GpCoordPlace {
    /* 0x00 */ s32    flg;
    /* 0x04 */ MATRIX coord;
    /* 0x24 */ byte   pad_24[0x22];
    /* 0x46 */ s16    field_46;
} GpCoordPlace;
STATIC_ASSERT_SIZEOF(GpCoordPlace, 0x48);

/// Overlay of `GsCOORDINATE2` at `GameActorExt.field_8` used by `func_800BB9B8`.
/// field_8 / field_14 are `coord.m[0][2]` / `coord.m[2][2]` (`lh` into
/// `ratan2`); field_18 / field_1C / field_20 are the low 16 bits of
/// `coord.t[0..2]` (`lhu`).
typedef struct _GpCoordYaw {
    /* 0x00 */ byte pad_0[8];
    /* 0x08 */ s16  field_8;
    /* 0x0A */ byte pad_A[0xA];
    /* 0x14 */ s16  field_14;
    /* 0x16 */ byte pad_16[2];
    /* 0x18 */ u16  field_18;
    /* 0x1A */ byte pad_1A[2];
    /* 0x1C */ u16  field_1C;
    /* 0x1E */ byte pad_1E[2];
    /* 0x20 */ u16  field_20;
} GpCoordYaw;
STATIC_ASSERT_SIZEOF(GpCoordYaw, 0x22);

/// 8-byte item attribute row. `D_8010DFB8` is indexed by raw item id
/// (`func_800BC324`); ids 0x60–0x7F land in the `D_8010E2B8` slice.
/// field_4 is the unsigned bonus added to `Wip_SysConfig.field_1a` by
/// `func_800BC0C0` when `Wip_SysConfig.field_23` (item id − 0x5F) is
/// non-zero. field_5 is the unsigned base added to
/// `Mc_SaveData.field_908[id-0x60]` and clamped to 10. field_6 is the
/// unsigned bonus added to `Wip_SysConfig.field_1e` by `func_800B7930`
/// when `field_23` is non-zero.
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

/// 4-byte row in `D_8010E3B8`, indexed by item id − 0xA0 (ids ≥ 0xA0).
/// field_0 is the default count copied to `D_80114DD0` by `func_800BF624`.
/// field_2 is a max quantity (`func_800B8CAC` / `func_800B63B8`).
typedef struct _GpItemA0 {
    /* 0x00 */ u8  field_0;
    /* 0x01 */ u8  field_1;
    /* 0x02 */ u16 field_2;
} GpItemA0;
STATIC_ASSERT_SIZEOF(GpItemA0, 0x4);

/// 8-byte row in `D_8010D328` (4 entries), indexed by `Mc_SaveData.field_F`.
/// field_0 is the unsigned base written into `Wip_SysConfig.field_1a`
/// (`func_800BC0C0`). field_4 is the word added into `Wip_SysConfig.field_1e`
/// (`func_800B7930`).
typedef struct _GpStatRow {
    /* 0x00 */ u16 field_0;
    /* 0x02 */ byte pad_2[2];
    /* 0x04 */ s32 field_4;
} GpStatRow;
STATIC_ASSERT_SIZEOF(GpStatRow, 0x8);

extern GpBit2Bank D_8010D230[];
extern GpItemQty  D_8010D278[];
extern GpItemMap  D_8010D2F8[];
extern GpItemScan D_8010D520;
/// Source item-table scan (`func_800BC50C` / item-move UI). field_0 is the
/// start index, field_1 the entry count, field_2 the table id.
extern GpItemScan D_8010D628;
/// Dest item-table scan immediately after `D_8010D628` (`func_800BC50C`).
extern GpItemScan D_8010D62C;
/// Array of `GpItemScan*` (`func_800B7D18` clears `[1]` and `[2]`).
extern GpItemScan* D_8010D550[];
/// Scan dest used while `func_800B7D18` copies the current inventory out.
extern GpItemScan* D_8010D55C;
extern GpStatRow  D_8010D328[];
extern GpItemAttr D_8010DFB8[];
extern GpItemQty  D_8010E238[];
extern GpItemAttr D_8010E2B8[];
extern GpItemA0   D_8010E3B8[];
/// Byte remap of an item id used as a sort/order key (`func_800BC18C` /
/// `func_800B8588`). Split by item class: 0x01–0x5F → `D_80114A40[id]`,
/// 0x60–0x7F → `D_80114A88[id-0x60]`, 0x80–0x9F → `D_80114A98[id-0x80]`,
/// 0xA0–0xBF → `D_80114ABC[id-0xA0]`. A 0 entry (or an id outside those
/// ranges) falls back to `id + 0x100`; id 0 returns 0x1000.
extern u8         D_80114A40[];
extern u8         D_80114A88[];
extern u8         D_80114A98[];
extern u8         D_80114ABC[];
/// 0xFFFF-terminated item-id list walked by `func_800BB668`. Each id's
/// low 7 bits index a collected-item bit in `Mc_SaveData.field_5AC`.
extern u16        D_80114AE0[];
extern GpItemRec  D_80114C20[];
extern GpItemRec* D_80114D70;
/// Overlay string table for item ids `>= 0x500`, indexed by `id - 0x500`
/// (`func_800B8EB0`).
extern char*      D_801D6484[];

s32         func_800B7420(s32 arg0);
void        func_800B7930(void);
void        func_800B7A50(s32 arg0);
void        func_800B7D18(void);
GpItemRec*  func_800BAD08(GpItemScan* arg0, s32 arg1, s32 arg2);
s32         func_800BAD28(GpItemScan* arg0, GpItemRec* arg1, s32 arg2);
s32         func_800B9D80(s32 arg0);
void        func_800B8014(void);
void        func_800B8588(GpItemScan* arg0, s32 arg1);
void        func_800BAA58(void);
void        func_800BAB64(s32 arg0);
void        func_800BAC34(s32 arg0, u8 arg1);
void        func_800BAC8C(GpItemScan* arg0);
void        func_800BAE38(void);
s32         func_800BAF08(void);
s32         func_800BAF5C(GpItemScan* arg0);
/// True if `arg1` can be added to the item table selected by `arg0`.
s32         func_800B8988(GpItemScan* arg0, s32 arg1);
/// Writes item `arg2` into scan slot `arg1`. Ids `0xA0..0xBF` are added with
/// `func_800BAD08` first, then an existing stack is moved onto the slot when
/// it is empty. Other ids overwrite the slot (re-adding the previous item).
GpItemRec*  func_800B8B00(GpItemScan* arg0, s32 arg1, s32 arg2, s32 arg3);
/// Returns the `arg1`-th text field of item `arg0` (NUL / `\\n` / `\\N`
/// delimiters). `arg2 == 0` reads `Mc_SaveData.field_6D0` and adds 3 to
/// `arg1` when the bit is clear. Ids `>= 0x500` index `D_801D6484`;
/// `0x300..0x4FF` unpack and recurse.
char*       func_800B8EB0(s32 arg0, s32 arg1, s32 arg2);
/// Returns the `arg1`-th matching item id from the table selected by `arg0`.
/// `0x80..0x9F` ids match when `arg2 == 0`, or when `arg2` is a related id
/// in `D_8010E238` / `D_8010D278` and the row is stocked or selected.
s32         func_800B904C(GpItemScan* arg0, s32 arg1, s32 arg2);
void        func_800B91C8(GpItemRec* arg0);
GpItemSlot* func_800BAFE0(s32 arg0);
s32         func_800BAFF4(GpItemScan* arg0, s32 arg1);
void        func_800BB0CC(s32 arg0);
void        func_800BB190(s32 arg0, s32 arg1);
s32         func_800BB26C(GpItemScan* arg0, s32 arg1);
void        func_800BB2D4(GpItemScan* arg0, s32 arg1, s32 arg2);
s32         func_800BB3C0(s32 arg0, s32 arg1);
s32         func_800BB418(s32 arg0, s32 arg1);
s32         func_800BB470(s32 arg0);
s32         func_800BB4BC(s32 arg0);
GpItemRec*  func_800BB500(GpItemScan* arg0);
s32         func_800BB540(GpItemScan* arg0, GpItemRec* arg1);
/// `arg2` is unused; some callers pass 0 so the `jal` delay slot is `move a2, zero`.
GpItemRec*  func_800BB5BC(GpItemScan* arg0, s32 arg1, s32 arg2);
s32         func_800BB610(GpItemScan* arg0, s32 arg1);
/// `arg1` is unused; some callers pass 0 so the `jal` delay slot is `move a1, zero`.
s32         func_800BB668(s32 arg0, s32 arg1);
s32         func_800BB6FC(GpItemScan* arg0, s32 arg1);
void        func_800BB7C0(s32 arg0, s32 arg1);
void        func_800BB838(GpBit2List* arg0, u32* arg1);
void        func_800BB8E8(s32 arg0, u8 arg1, s32 arg2);
s32         func_800BB974(GameSessionFrom4* arg0, s32 arg1);
void        func_800BB9B8(void);
struct _GpEnemy* func_800BBA70(GpEnemyDesc* arg0, GpEnemyPlace* arg1);
void        func_800BBC10(Task* arg0);
s32         func_800BBD40(s32 arg0);
GpItemMap*  func_800BBDC8(s32 arg0);
s32         func_800BBDDC(void);
void        func_800BBE54(void);
s32         func_800BBEC0(s32 arg0);
void        func_800BBF84(void);
s32         func_800BC06C(s32 arg0);
void        func_800BC0C0(void);
/// Returns `arg0[1]` (e.g. `McItemScan.field_1` capacity).
s32         func_800BC180(u8* arg0);
s32         func_800BC18C(s32 arg0);
s32         func_800BC324(s32 arg0);
void        func_800BC378(Task* arg0);
s32         func_800BC3F8(s32 arg0);
s32         func_800BC50C(void);

#endif // GAMEPLAY_268_H
