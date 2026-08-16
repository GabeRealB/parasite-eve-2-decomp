#ifndef GAMEPLAY_1BC_H
#define GAMEPLAY_1BC_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3A34.h"
#include "main/task.h"
#include "main/tmd.h"

/// 0x60-byte enemy work object allocated by `func_800B0494`
/// ("new_enemy ---> NULL"). Stored in `Task::spawnArg2`; `task` back-points
/// at the owner. `node` is the `func_800DAB38` list entry at +0x10.
typedef struct _GpEnemy {
    /* 0x00 */ Task*      task;
    /* 0x04 */ byte       pad_4[0x8];
    /* 0x0C */ s32        field_C;
    /* 0x10 */ GpLinkNode node;
    /* 0x18 */ void*      field_18;
    /* 0x1C */ byte       pad_1C[0x44];
} GpEnemy;
STATIC_ASSERT_SIZEOF(GpEnemy, 0x60);

/// Callback for GpEnemy + Task state handlers (entries in `D_80093A10`).
typedef void (*GpEnemyTaskFunc)(GpEnemy* enemy, Task* task);

/// Fixed-size table of `GpEnemyTaskFunc` callbacks. Copied onto the stack by
/// `func_800B058C` so the call uses a local jump table.
typedef struct {
    GpEnemyTaskFunc funcs[3];
} GpEnemyTaskFuncTable3;

/// Three-entry dispatcher table: `func_800B0544`, `func_800B0560`, `func_800B01F0`.
extern GpEnemyTaskFuncTable3 D_80093A10;

/// Source object for `func_800B3CCC` / `func_800B3F60`. Word at 0x30 is
/// copied into the dest context; the address of 0x34 is stored as dest
/// `field_4` (base of 0x50-byte records in `func_800B3448`).
typedef struct _GpAnimObj {
    /* 0x00 */ byte  pad_0[0x30];
    /* 0x30 */ void* field_30;
    /* 0x34 */ byte  field_34;
} GpAnimObj;

/// 4-byte animation record. `func_800B3E74` / `func_800B3EE8` index this by
/// `GpAnimSlot::field_6` and copy `field_2 << 4` into the slot's
/// `field_C` / `field_E`. `field_3` is the opcode-like byte tested by
/// `func_800B3AA4` / stored into `field_B` by `func_800B3FA8`.
typedef struct _GpAnimRec {
    /* 0x00 */ u16 field_0;
    /* 0x02 */ u8  field_2;
    /* 0x03 */ u8  field_3;
} GpAnimRec;
STATIC_ASSERT_SIZEOF(GpAnimRec, 4);

/// Object behind each pointer in `GpAnimSlot::field_20` (same table as
/// `GpAnimCtx::field_0`). `field_0` is the base of 4-byte records.
/// `field_4` is a u16 table indexed by `GpAnimSlot::field_15`.
typedef struct _GpAnimSet {
    /* 0x00 */ GpAnimRec* field_0;
    /* 0x04 */ u16*       field_4;
} GpAnimSet;

/// 0x28-byte animation slot. `field_15` is this slot's index in the
/// `GpAnimCtx::field_C` array; `func_800B3DB4` / `func_800B3DF4` /
/// `func_800B3E34` recover the base as `slot - slot->field_15`.
/// `field_0 == 0x7FFF` marks the slot inactive. `field_10` is a flags
/// word (`func_800B4754` sets bit 0 when clamping `field_2`).
typedef struct _GpAnimSlot {
    /* 0x00 */ u16         field_0;
    /* 0x02 */ u16         field_2;
    /* 0x04 */ byte        pad_4[2];
    /* 0x06 */ u16         field_6;
    /* 0x08 */ byte        pad_8[4];
    /* 0x0C */ u16         field_C;
    /* 0x0E */ u16         field_E;
    /* 0x10 */ u16         field_10;
    /* 0x12 */ byte        pad_12[3];
    /* 0x15 */ u8          field_15;
    /* 0x16 */ byte        pad_16[0xA];
    /* 0x20 */ GpAnimSet** field_20;
    /* 0x24 */ byte        pad_24[4];
} GpAnimSlot;
STATIC_ASSERT_SIZEOF(GpAnimSlot, 0x28);

/// 0x14-byte context filled by `func_800B3CCC` (no `field_C`) and
/// `func_800B3F60` (also writes `field_C`). Nearby helpers index
/// `field_C` as a 0x28-byte slot array and `field_8` at a 0x10 stride.
typedef struct _GpAnimCtx {
    /* 0x00 */ void*       field_0;
    /* 0x04 */ void*       field_4;
    /* 0x08 */ void*       field_8;
    /* 0x0C */ GpAnimSlot* field_C;
    /* 0x10 */ void*       field_10;
} GpAnimCtx;
STATIC_ASSERT_SIZEOF(GpAnimCtx, 0x14);

/// Object returned by `func_800B5A08` (`GpAreaRec.field_4`). `field_0` is a
/// signed id compared with `GpAreaKey.field_5`; `field_1` is a flags byte
/// (bits 0/1/2/4 in nearby 1BC / 1A8 helpers; `func_800B59A8` returns bit 1).
/// Full size unknown.
typedef struct _GpAreaObj {
    /* 0x00 */ s8 field_0;
    /* 0x01 */ u8 field_1;
} GpAreaObj;

/// 8-byte record in tables pointed to by `D_8010CBCC`. Indexed by
/// `GpAreaKey.field_2`. `field_0` is a nested table (`func_800B5CE8`
/// returns the entry at `field_5`; `func_800B5C88` returns that entry's
/// `field_4`). `field_4` is the object `func_800B5A08` returns.
typedef struct _GpAreaRec {
    /* 0x00 */ struct _GpAreaRec* field_0;
    /* 0x04 */ GpAreaObj*         field_4;
} GpAreaRec;
STATIC_ASSERT_SIZEOF(GpAreaRec, 8);

/// Location key used to index `D_8010CBCC`. Bytes 2/3 select the table and
/// record; byte 5 indexes a nested table (`func_800B5CE8` / `func_800B5C88`)
/// and is written from `GpAreaObj.field_0` by `func_800B601C`. Same 4-byte
/// prefix as `GameSessionFrom4` / `Mc_SaveData.field_4`. Full size unknown.
typedef struct _GpAreaKey {
    /* 0x00 */ u8 field_0;
    /* 0x01 */ u8 field_1;
    /* 0x02 */ u8 field_2;
    /* 0x03 */ u8 field_3;
    /* 0x04 */ u8 field_4;
    /* 0x05 */ u8 field_5;
} GpAreaKey;

/// Overlay of `Task::spawnArg2` for sibling walkers. `field_A` high byte is
/// the work type (`func_800B5E08` / `func_800B5EE8` / `func_800B5F5C` match 9;
/// `func_800B5E78` skips 9). `field_8` is the id compared against the search
/// key (`as_u16` / `as_u8`; `func_800B584C` matches `as_u16` on slot 4's
/// children). Full size unknown.
typedef struct _GpWorkObj {
    /* 0x00 */ byte pad_0[8];
    /* 0x08 */ union {
        u16 as_u16;
        u8  as_u8;
    } field_8;
    /* 0x0A */ u16 field_A;
} GpWorkObj;

/// 8-byte mask/flag record. `D_8010D1C4` is a 0-terminated table of these.
/// `func_800AFF90` / `func_800B0034` walk it: if `arg0 & mask`, apply `flags`
/// to `SndEvt_EnqueueType7` / `SndBank_SetEnableFlags`.
typedef struct _GpSndMaskRec {
    /* 0x0 */ s32 mask;
    /* 0x4 */ s32 flags;
} GpSndMaskRec;
STATIC_ASSERT_SIZEOF(GpSndMaskRec, 8);

/// Per-area pointer table. Index is `GpAreaKey.field_3` (also
/// `GameSession.field_7` via `&Game_Session->field_4`).
extern GpAreaRec* D_8010CBCC[];

/// 0-terminated `GpSndMaskRec` table walked by `func_800AFF90` / `func_800B0034`.
extern GpSndMaskRec D_8010D1C4[];

void     func_800AFF90(u16 arg0);
void     func_800B0034(u16 arg0);
void     func_800B00C4(void);
s32      func_800B0118(s32 arg0, s32 arg1);
GpEnemy* func_800B0168(s32 bank, s32 type, s32 arg2, GpEnemy* parent);
GpEnemy* func_800B01AC(TaskDesc* table, s32 idx, s32 arg2, GpEnemy* parent);
void     func_800B01F0(GpEnemy* enemy, Task* task);
void     func_800B0234(Task* task);
GpEnemy* func_800B0494(Task* task, GpEnemy* parent);
void     func_800B0544(GpEnemy* enemy, Task* task);
void     func_800B0560(GpEnemy* enemy, Task* task);
void     func_800B058C(Task* arg0);
s32      func_800B05E8(s32 arg0);
void     func_800B065C(u8 arg0);
void     func_800B27C4(u16* arg0, u16* arg1, s32 arg2, u16* arg3);
void     func_800B3CCC(GpAnimCtx* arg0, void* arg1, GpAnimObj* arg2, void* arg3);
void     func_800B3DB4(GpAnimCtx* arg0, GpAnimSlot* arg1);
void     func_800B3DF4(GpAnimCtx* arg0, GpAnimSlot* arg1);
void     func_800B3E34(GpAnimCtx* arg0, GpAnimSlot* arg1);
void     func_800B3E74(GpAnimCtx* arg0, GpAnimSlot* arg1, s32 arg2, s32 arg3);
void     func_800B3EE8(GpAnimCtx* arg0, GpAnimSlot* arg1, s32 arg2, s32 arg3, s32 arg4);
void     func_800B3F60(GpAnimCtx* arg0, void* arg1, GpAnimObj* arg2, void* arg3, GpAnimSlot* arg4);
void     func_800B3F84(GpAnimCtx* arg0, void* arg1, GpAnimObj* arg2, void* arg3, GpAnimSlot* arg4);
void     func_800B3FA8(GpAnimCtx* arg0, s32 arg1, s32 arg2);
void     func_800B4514(GpAnimCtx* arg0, s32 arg1);
GpAnimRec* func_800B4668(GpAnimCtx* arg0, GpAnimSlot* arg1);
void     func_800B4754(GpAnimCtx* arg0, GpAnimSlot* arg1, u16 arg2, u16 arg3);
void     func_800B47A8(GpAnimCtx* arg0, s32 arg1, s32 arg2, u16 arg3, s32 arg4, s32 arg5, s32 arg6,
                       void* arg7);
void       func_800B56AC(void);
void       func_800B57EC(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1);
GpWorkObj* func_800B584C(u16 arg0);
void       func_800B58D4(TmdObject* arg0, s32 arg1, s32 arg2);
void       func_800B5914(s32 arg0);
s32        func_800B59A8(GpAreaKey* arg0);
GpAreaObj* func_800B5A08(GpAreaKey* arg0);
void       func_800B5A48(GpAreaKey* arg0, GpAreaObj* arg1);
void       func_800B5B30(GpAreaKey* arg0, s32 arg1, s32 arg2);
void       func_800B5BFC(s32 arg0, GpAreaKey* arg1);
GpAreaObj* func_800B5C88(GpAreaKey* arg0);
GpAreaRec* func_800B5CE8(GpAreaKey* arg0);
void       func_800B5D44(GpAreaKey* arg0);
void       func_800B601C(GpAreaKey* arg0);
s32        func_800B5E08(Task* arg0, Task* arg1, s32 arg2, Task** arg3);
s32        func_800B5E78(Task* arg0, Task* arg1, s32 arg2, Task** arg3);
s32        func_800B5EE8(Task* arg0);
s32        func_800B5F5C(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
void       func_800B62D4(void);
void       func_800B6CF0(void);
s32        func_800B6DA4(s32 arg0, s32 arg1);

#endif // GAMEPLAY_1BC_H
