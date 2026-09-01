#ifndef ACTOR_503500_H
#define ACTOR_503500_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/session.h"
#include "main/task.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

/// Work block allocated by `func_actor_503500_80132430`
/// (`Mem_Calloc(0x48)`) and parked in that task's `Task::idMap` slot.
/// `func_actor_503500_801324EC` republishes the two matrices onto
/// `GameActorExt::field_1C` / `field_20` -- the colour/light matrix pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`
/// -- so the allocation is exactly two `MATRIX`es plus a small tail.
typedef struct Actor503500ColorMtx {
    /* 0x00 */ MATRIX light;
    /* 0x20 */ MATRIX color;
    /* 0x40 */ byte   pad_40[0x4];
    /* 0x44 */ s8     field_44;
    /* 0x45 */ byte   pad_45[0x3];
} Actor503500ColorMtx;
STATIC_ASSERT_SIZEOF(Actor503500ColorMtx, 0x48);

/// Message payload at `D_actor_503500_8017655C`. `func_actor_503500_80132DEC`
/// fills it from the player actor: the three words are the translation of the
/// `GsCOORDINATE2` at `GameActorExt::field_8` (`MATRIX.t`), the three
/// halfwords the rotation triple at +0x50/+0x52/+0x54 of that task's `idMap`
/// block. `func_actor_503500_80132DD4` clears the position;
/// `func_actor_503500_80132E7C` hands the record to `Gp_DispatchMsg` as
/// message 0x3E9 while any position word is non-zero.
///
/// Size is bounded by the next `.bss` symbol in the overlay
/// (`D_actor_503500_80176574`, 0x18 bytes later).
typedef struct Actor503500MsgPos {
    /* 0x00 */ s32  x;
    /* 0x04 */ s32  y;
    /* 0x08 */ s32  z;
    /* 0x0C */ byte pad_C[0x4];
    /* 0x10 */ s16  rx;
    /* 0x12 */ s16  ry;
    /* 0x14 */ s16  rz;
    /* 0x16 */ byte pad_16[0x2];
} Actor503500MsgPos;
STATIC_ASSERT_SIZEOF(Actor503500MsgPos, 0x18);

/// Shared field view of an `actor_503500` enemy work block.
///
/// The overlay hosts a dozen separate enemies. Each parks its own work block in
/// the `Task::idMap` slot -- that slot is *not* a `TaskIdMap` here -- and each
/// block is zeroed by its state-0 init, so every size below is anchored by the
/// clearing call rather than inferred from the access spread:
///
/// | state-0 init                  | block                                     | anchor              |
/// |-------------------------------|-------------------------------------------|---------------------|
/// | `func_actor_503500_80132F64`  | `D_actor_503500_80176574` (the boss)      | `Mem_Set(_, 0x7E8)` |
/// | `func_actor_503500_801372C8`  | `D_actor_503500_80176D88`                 | `Mem_Set(_, 0x160)` |
/// | `func_actor_503500_8013852C`  | `D_actor_503500_80176EE8[spawnArg1 - 2]`  | `Mem_Set(_, 0x2EC)` |
/// | `func_actor_503500_8013AD64`  | `D_actor_503500_801774C0[spawnArg1 - 4]`  | `Mem_Set(_, 0xF0)`  |
/// | `func_actor_503500_8013BEE4`  | `D_actor_503500_801776A0`                 | `Mem_Set(_, 0xF4)`  |
/// | `func_actor_503500_8013CAE4`  | `D_actor_503500_801770E8`                 | `Mem_Set(_, 0xF4)`  |
/// | `func_actor_503500_8013DD10`  | `D_actor_503500_8017797C`                 | `Mem_Set(_, 0xF0)`  |
/// | `func_actor_503500_8013ECBC`  | `D_actor_503500_80177A6C`                 | `Mem_Set(_, 0xF4)`  |
/// | `func_actor_503500_8013FA74`  | `D_actor_503500_80177B60`                 | `Mem_Set(_, 0x3D8)` |
/// | `func_actor_503500_801423C8`  | `D_actor_503500_80178AC0`                 | `Mem_Set(_, 0x224)` |
/// | `func_actor_503500_80143AC0`  | `D_actor_503500_80178F10`                 | `Mem_Set(_, 0x38)`  |
///
/// One C type is shared across all of them, the way the first decompiled
/// dispatchers here already assumed, and the observed offsets do not collide --
/// the small blocks simply stop early. The size asserted below is therefore the
/// *largest* instance (the boss); a 0xF0 block typed through this pointer is
/// only valid up to its own end. Splitting this into one type per enemy is a
/// follow-up that would have to retype the already-landed dispatchers
/// (`func_actor_503500_801383D0` reads `field_15C` out of the 0x160 block,
/// `func_actor_503500_8013BD88` reads `field_ED` out of a 0xF0 block,
/// `func_actor_503500_8013EB60` reads `field_EC` out of another 0xF0 block).
typedef struct Actor503500Work {
    /* 0x000 */ byte     pad_0[0x1E];
    /* 0x01E */ u16      field_1E; // flag halfword; bit 0x8000 set on release
    /* 0x020 */ byte     pad_20[0x5E];
    /* 0x07E */ u16      field_7E; // same flag halfword, 0x224 block
    /* 0x080 */ byte     pad_80[0x6A];
    /* 0x0EA */ s16      field_EA;
    /* 0x0EC */ s8       field_EC;  // sub-state index
    /* 0x0ED */ s8       field_ED;  // sub-state index
    /* 0x0EE */ byte     pad_EE[0x2];
    /* 0x0F0 */ s8       field_F0;  // sub-state index
    /* 0x0F1 */ s8       field_F1;  // sub-state phase, cleared with field_F0
    /* 0x0F2 */ byte     pad_F2[0x6A];
    /* 0x15C */ s8       field_15C; // sub-state index
    /* 0x15D */ byte     pad_15D[0x21];
    /* 0x17E */ u16      field_17E; // GpObj::flags of obj[11] in the 0x3D8 block
    /* 0x180 */ byte     pad_180[0xA1];
    /* 0x221 */ s8       field_221; // sub-state index
    /* 0x222 */ byte     pad_222[0xC8];
    /* 0x2EA */ s8       field_2EA;
    /* 0x2EB */ s8       field_2EB; // TMD buffer countdown, 0x2EC block
    /* 0x2EC */ byte     pad_2EC[0xC6];
    /* 0x3B2 */ u16      field_3B2; // fade level, stepped by 0x10 up to 0x1000
    /* 0x3B4 */ byte     pad_3B4[0x22];
    /* 0x3D6 */ s8       field_3D6;
    /* 0x3D7 */ s8       field_3D7; // TMD buffer countdown, 0x3D8 block
    /* 0x3D8 */ byte     pad_3D8[0x314];
    /* 0x6EC */ GpEnemy* enemies[0x22];
    /* 0x774 */ u32      field_774; // one "already asked to die" bit per slot
    /* 0x778 */ byte     pad_778[0x38];
    /* 0x7B0 */ s16      field_7B0; // boss state index
    /* 0x7B2 */ u16      field_7B2;
    /* 0x7B4 */ byte     pad_7B4[0x16];
    /* 0x7CA */ s16      field_7CA;
    /* 0x7CC */ s16      field_7CC;
    /* 0x7CE */ byte     pad_7CE[0x4];
    /* 0x7D2 */ s16      field_7D2;
    /* 0x7D4 */ byte     pad_7D4[0x6];
    /* 0x7DA */ u8       field_7DA; // per-state step counter
    /* 0x7DB */ byte     pad_7DB[0x5];
    /* 0x7E0 */ s8       field_7E0;
    /* 0x7E1 */ byte     pad_7E1[0x1];
    /* 0x7E2 */ s8       field_7E2;
    /* 0x7E3 */ byte     pad_7E3[0x5];
} Actor503500Work;
STATIC_ASSERT_SIZEOF(Actor503500Work, 0x7E8);

/// Work block shape of the `actor_503500` effect tasks -- the ones whose
/// state-0 init `Mem_Calloc`s the block instead of pointing `Task::idMap` at a
/// static global: `func_actor_503500_80144300` (0xC0),
/// `func_actor_503500_801448E8` (0xB4), `func_actor_503500_80144E8C` (0xD0),
/// `func_actor_503500_801455A4` (0x44), `func_actor_503500_80145A2C` (0xAC)
/// and `func_actor_503500_8014642C` (0x4CC). Each starts with the `GpObj`
/// display node their exit callback hands to `Gp_UnlinkObj`, and the ones that
/// place geometry follow it with the `GpRec18` collision record
/// (`func_actor_503500_801448E8` derives its `GpRec18` base as `block + 0x20`).
/// Only that head is shared; the payload after it differs per task, which is
/// why this type stops at 0x38.
typedef struct Actor503500ObjWork {
    /* 0x00 */ GpObj   obj;
    /* 0x20 */ GpRec18 rec;
} Actor503500ObjWork;
STATIC_ASSERT_SIZEOF(Actor503500ObjWork, 0x38);

/// `Task` as this overlay's enemies use it. The layout is `Task`'s
/// (`include/main/task.h`); only two slots are retyped: `idMap` holds the
/// actor's own work block rather than a `TaskIdMap`, and `spawnArg2` is the
/// `GpEnemy` that `Gp_AllocEnemy` parked there (the same object the gameplay
/// code also reaches through its sparse `GpObj5D` view).
typedef struct Actor503500 {
    /* 0x00 */ TaskNode         node;
    /* 0x08 */ Task*            parent;
    /* 0x0C */ Task*            firstChild;
    /* 0x10 */ Task*            nextSibling;
    /* 0x14 */ TaskFunc         callback;
    /* 0x18 */ TaskFunc         exitCallback;
    /* 0x1C */ Actor503500Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ void*            field_24;
    /* 0x28 */ u8               spawnType;
    /* 0x29 */ u8               priority;
    /* 0x2A */ s16              killCountdown;
    /* 0x2C */ GameActorExt*    extra;
    /* 0x30 */ s32              state;
    /* 0x34 */ s32              spawnArg1;
    /* 0x38 */ u8               flags;
    /* 0x39 */ byte             unknown_39[3];
    /* 0x3C */ s32              extraState;
    /* 0x40 */ byte             unknown_40[8];
} Actor503500;
STATIC_ASSERT_SIZEOF(Actor503500, 0x48);

void func_actor_503500_8013F8AC(Actor503500* arg0);
void func_actor_503500_801440F0(Actor503500* arg0);

#endif
