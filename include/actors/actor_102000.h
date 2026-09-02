#ifndef ACTOR_102000_H
#define ACTOR_102000_H

#include "common.h"
#include "main/tmd.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// 0x18-byte slot record; `Gp_InitRec18Table` zeroes `count` of them.
typedef struct Actor02000Rec18 {
    /* 0x00 */ byte pad_0[0x18];
} Actor02000Rec18;
STATIC_ASSERT_SIZEOF(Actor02000Rec18, 0x18);

/// Collision/proximity list node this overlay embeds four times in
/// `Actor02000Work`. `Gp_LinkObj` appends it to one of the global object
/// lists and `Gp_UnlinkObj` takes it back off; `field_C` points either
/// straight at an `Actor02000Rec18` table or (list 3, `flags & 7 == 3`) at
/// an `Actor02000ObjRec` whose `field_14` holds the table.
typedef struct Actor02000Obj {
    /* 0x00 */ struct Actor02000Obj* next;
    /* 0x04 */ struct Actor02000Obj* prev;
    /* 0x08 */ GsCOORDINATE2*        field_8;
    /* 0x0C */ void*                 field_C;
    /* 0x10 */ s16                   field_10;
    /* 0x12 */ s16                   field_12;
    /* 0x14 */ s16                   field_14;
    /* 0x16 */ byte                  pad_16[2];
    /* 0x18 */ s32                   field_18;
    /* 0x1C */ s16                   field_1C;
    /* 0x1E */ u16                   flags;
} Actor02000Obj;
STATIC_ASSERT_SIZEOF(Actor02000Obj, 0x20);

/// Indirection record an `Actor02000Obj.field_C` can point at (same shape as
/// the gameplay `GpActorD4Rec`): a bounding box plus the `Actor02000Rec18`
/// table proper at `field_14`.
typedef struct Actor02000ObjRec {
    /* 0x00 */ s16              field_0;
    /* 0x02 */ s16              field_2;
    /* 0x04 */ s16              field_4;
    /* 0x06 */ byte             pad_6[2];
    /* 0x08 */ s16              field_8;
    /* 0x0A */ s16              field_A;
    /* 0x0C */ s16              field_C;
    /* 0x0E */ byte             pad_E[2];
    /* 0x10 */ s16              field_10;
    /* 0x12 */ s16              field_12;
    /* 0x14 */ Actor02000Rec18* field_14;
} Actor02000ObjRec;
STATIC_ASSERT_SIZEOF(Actor02000ObjRec, 0x18);

/// The nineteen 0x28-byte animation slots at `Actor02000Work + 0x14`; the
/// work block opens with the 0x14-byte animation context `func_800B3F84`
/// fills in, so `Gp_AnimResetSlot` reaches these through it.
typedef struct Actor02000AnimSlots {
    /* 0x00 */ byte pad_0[0x2F8];
} Actor02000AnimSlots;
STATIC_ASSERT_SIZEOF(Actor02000AnimSlots, 0x2F8);

/// 0x6E4-byte work block `Actor02000_Fn0251C` allocates and hangs off
/// `Actor02000.field_1C`. It opens with the animation context and its
/// nineteen slots, carries the light/color matrices the model object points
/// at, then the four list nodes with their `Actor02000Rec18` tables.
typedef struct Actor02000Work {
    /* 0x000 */ byte                pad_0[0x14];
    /* 0x014 */ Actor02000AnimSlots field_14;
    /* 0x30C */ byte                field_30C[0x130];
    /* 0x43C */ MATRIX              field_43C;
    /* 0x45C */ MATRIX              field_45C;
    /* 0x47C */ Actor02000Obj       field_47C;
    /* 0x49C */ Actor02000ObjRec    field_49C;
    /* 0x4B4 */ Actor02000Rec18     field_4B4[1];
    /* 0x4CC */ Actor02000Obj       field_4CC;
    /* 0x4EC */ Actor02000Rec18     field_4EC[5];
    /* 0x564 */ Actor02000Obj       field_564;
    /* 0x584 */ Actor02000Rec18     field_584[4];
    /* 0x5E4 */ Actor02000Obj       field_5E4;
    /* 0x604 */ Actor02000Rec18     field_604[1];
    /* 0x61C */ byte                pad_61C[0x50];
    /* 0x66C */ s16*                field_66C;
    /* 0x670 */ GsCOORDINATE2*      field_670;
    /* 0x674 */ s16                 field_674;
    /* 0x676 */ s16                 field_676;
    /* 0x678 */ byte                pad_678[0x1C];
    /* 0x694 */ s16                 field_694;
    /* 0x696 */ byte                pad_696[2];
    /* 0x698 */ s16                 field_698;
    /* 0x69A */ byte                pad_69A[2];
    /* 0x69C */ s16                 field_69C;
    /* 0x69E */ s16                 field_69E;
    /* 0x6A0 */ byte                pad_6A0[2];
    /* 0x6A2 */ s16                 field_6A2;
    /* 0x6A4 */ s16                 field_6A4;
    /* 0x6A6 */ s16                 field_6A6;
    /* 0x6A8 */ s16                 field_6A8;
    /* 0x6AA */ s16                 field_6AA;
    /* 0x6AC */ s16                 field_6AC;
    /* 0x6AE */ s16                 field_6AE;
    /* 0x6B0 */ byte                pad_6B0[2];
    /* 0x6B2 */ s16                 field_6B2;
    /* 0x6B4 */ byte                pad_6B4[4];
    /* 0x6B8 */ s16                 field_6B8;
    /* 0x6BA */ byte                pad_6BA[0x10];
    /* 0x6CA */ s16                 field_6CA;
    /* 0x6CC */ byte                pad_6CC[8];
    /* 0x6D4 */ s16                 field_6D4;
    /* 0x6D6 */ s16                 field_6D6;
    /* 0x6D8 */ byte                pad_6D8[2];
    /* 0x6DA */ s16                 field_6DA;
    /* 0x6DC */ byte                pad_6DC[2];
    /* 0x6DE */ s16                 field_6DE;
    /* 0x6E0 */ s16                 field_6E0;
    /* 0x6E2 */ byte                pad_6E2[2];
} Actor02000Work;
STATIC_ASSERT_SIZEOF(Actor02000Work, 0x6E4);

/// Overlay-local view of the spawn parameter block (`Actor02000Ctx.field_3C`).
/// Byte 1 scales the state-1 dwell timer; bit 0 of `field_2` picks the awake
/// variant this enemy starts in.
typedef struct Actor02000Params {
    /* 0x0 */ byte pad_0[1];
    /* 0x1 */ u8   field_1;
    /* 0x2 */ u16  field_2;
} Actor02000Params;

/// `Gp_LinkNode` list entry at +0x10 of `Actor02000Ctx`.
typedef struct Actor02000Node {
    /* 0x0 */ struct Actor02000Node* next;
    /* 0x4 */ u8                     field_4;
    /* 0x5 */ byte                   pad_5[3];
} Actor02000Node;
STATIC_ASSERT_SIZEOF(Actor02000Node, 0x8);

/// Descriptor `Actor02000_Fn0251C` parks at `Actor02000Ctx.field_50`, taking
/// `field_4` from it as the context's `field_40`.
typedef struct Actor02000Desc {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ u16  field_4;
} Actor02000Desc;

/// Task context (`Task::spawnArg2`). `field_4B` selects how
/// `Actor02000_Fn0251C` starts the enemy: 0 builds the full object set, 1 and
/// 2 only prime the animation state.
typedef struct Actor02000Ctx {
    /* 0x00 */ byte              pad_0[4];
    /* 0x04 */ MATRIX*           field_4;
    /* 0x08 */ u16               field_8;
    /* 0x0A */ byte              pad_A[6];
    /* 0x10 */ Actor02000Node    node;
    /* 0x18 */ GsCOORDINATE2*    field_18;
    /* 0x1C */ s32               field_1C;
    /* 0x20 */ s32               field_20;
    /* 0x24 */ s32               field_24;
    /* 0x28 */ byte              pad_28[0x14];
    /* 0x3C */ Actor02000Params* field_3C;
    /* 0x40 */ s16               field_40;
    /* 0x42 */ byte              pad_42[6];
    /* 0x48 */ u8                field_48;
    /* 0x49 */ byte              pad_49[2];
    /* 0x4B */ u8                field_4B;
    /* 0x4C */ u8                field_4C;
    /* 0x4D */ byte              pad_4D[3];
    /* 0x50 */ Actor02000Desc*   field_50;
    /* 0x54 */ Actor02000Rec18*  field_54;
} Actor02000Ctx;
STATIC_ASSERT_SIZEOF(Actor02000Ctx, 0x58);

typedef struct Actor02000 {
    /* 0x00 */ byte            pad_0[0x1C];
    /* 0x1C */ Actor02000Work* field_1C;
    /* 0x20 */ Actor02000Ctx*  field_20;
    /* 0x24 */ byte            pad_24[8];
    /* 0x2C */ TmdObject*      field_2C;
    /* 0x30 */ s32             field_30;
} Actor02000;

/// Location key `Actor02000_Fn0251C` copies out of the session block before
/// looking up the spawned model's texture page and CLUT row (the gameplay
/// `GpAreaKey`; `Gp_SyncAreaKeyIndex` fills in `field_5`).
typedef struct Actor02000AreaKey {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
    /* 0x2 */ u8 field_2;
    /* 0x3 */ u8 field_3;
    /* 0x4 */ u8 field_4;
    /* 0x5 */ u8 field_5;
} Actor02000AreaKey;

/// 0x10-byte per-room record in the list `Gp_GetNestedAreaRec` reaches
/// (the gameplay `GpCdRec10`). `field_D` / `field_E` are the texture page and
/// CLUT row copied into the model object.
typedef struct Actor02000AreaRec {
    /* 0x00 */ byte pad_0[0xD];
    /* 0x0D */ u8   field_D;
    /* 0x0E */ u8   field_E;
    /* 0x0F */ byte pad_F[1];
} Actor02000AreaRec;
STATIC_ASSERT_SIZEOF(Actor02000AreaRec, 0x10);

/// What `Gp_GetNestedAreaRec` returns (the gameplay `GpCdAreaRec`).
typedef struct Actor02000AreaTable {
    /* 0x0 */ Actor02000AreaRec* field_0;
    /* 0x4 */ void*              field_4;
} Actor02000AreaTable;

/// Handle `Gp_SpawnEnemyFromTable` returns, seen here only through its owning
/// task pointer.
typedef struct Actor02000Eff {
    /* 0x0 */ Actor02000* task;
} Actor02000Eff;

#endif
