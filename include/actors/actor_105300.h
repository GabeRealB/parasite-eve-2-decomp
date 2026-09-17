#ifndef ACTOR_105300_H
#define ACTOR_105300_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"
#include "main/session.h"
#include "main/task.h"

#include "gameplay/3FB8.h"

/// The model object in `Task::extra`, seen through this overlay: `field_8` is
/// the object's trailing `GsCOORDINATE2` array. The spawn seeds one of those
/// coordinates from `D_actor_105300_80133A20` and hands it to the enemy as
/// `GpEnemy::field_18`.
typedef struct Actor05300Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} Actor05300Obj2C;

/// Work block of the task this enemy hangs off -- the spawn reaches it as
/// `task->parent->idMap`, the same slot `ActorsShared80133838` (the shared
/// teardown of `actor_105300` / `actor_105400`) reads as the sound id
/// `field_31C`. `field_334` is the enemy's sub-state index: it selects the
/// spawn position in `D_actor_105300_80133A20` and also which of the two
/// per-enemy death flags the spawn raises.
///
/// The pose half is what the per-frame handlers drive: `field_32C` is the
/// animation sub-state `func_actor_105300_8013222C` dispatches on, `field_328`
/// the row of the clip table that state walks, `field_32A` the countdown the
/// LCG reseeds and `field_320` / `field_324` the pose the shared animation unit
/// `ActorsShared80133610` ticks and the frames it has counted for it.
/// `field_2FC` is the local coordinate matrix that handler copies into the
/// model's own coordinate each frame. `field_320` / `field_324` are unsigned
/// here: every signed read of them casts at the use.
typedef struct Actor05300Work {
    /* 0x000 */ byte   pad_0[0x29C];
    /* 0x29C */ s32    field_29C;
    /* 0x2A0 */ byte   pad_2A0[0x5C];
    /* 0x2FC */ MATRIX field_2FC;
    /* 0x31C */ s32    field_31C;
    /* 0x320 */ u16    field_320;
    /* 0x322 */ s16    field_322;
    /* 0x324 */ u16    field_324;
    /* 0x326 */ u16    field_326;
    /* 0x328 */ u16    field_328;
    /* 0x32A */ u16    field_32A;
    /* 0x32C */ u16    field_32C;
    /* 0x32E */ u16    field_32E;
    /* 0x330 */ u16    field_330;
    /* 0x332 */ byte   pad_332[2];
    /* 0x334 */ s16    field_334;
    /* 0x336 */ s16    field_336;
    /* 0x338 */ s16    field_338;
} Actor05300Work;

/// 0x48-byte part object the spawn allocates with `Mem_Calloc` and parks in
/// `Task::idMap`. It leads with the `GpObj` list node linked into
/// `Gp_ObjLists[2]` -- and the one the shared teardown hands back to
/// `Gp_UnlinkObj` -- so `obj.field_C` is the single-entry `GpRec18` collision
/// table at 0x20. `field_38` is the same coordinate `obj.field_8` points at,
/// and `field_46` is the `ActorsShared80133838Work::field_46` the teardown
/// reads back to pick its death flag.
typedef struct Actor05300Part {
    /* 0x00 */ GpObj          obj;
    /* 0x20 */ GpRec18        rec18[1];
    /* 0x38 */ GsCOORDINATE2* field_38;
    /* 0x3C */ s16            field_3C;
    /* 0x3E */ s16            field_3E;
    /* 0x40 */ s16            field_40;
    /* 0x42 */ u16            field_42;
    /* 0x44 */ s16            field_44;
    /* 0x46 */ u16            field_46;
} Actor05300Part;
STATIC_ASSERT_SIZEOF(Actor05300Part, 0x48);

/// The task whose work block is `Actor05300Work`, reached as `task->field_1C`
/// (the `Task::idMap` slot). `field_20` is the `Task::spawnArg2` slot holding
/// the enemy: the sound events this enemy plays carry its actor id in the
/// high nibble of `GpEnemy::field_8`. `field_2C` is the `Task::extra` model
/// object.
typedef struct Actor05300 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor05300Work*  field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ Actor05300Obj2C* field_2C;
} Actor05300;

/// Spawn position copied into a coordinate's translation, one entry per
/// `Actor05300Work::field_334` sub-state.
typedef struct Actor05300SpawnPos {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
} Actor05300SpawnPos;
STATIC_ASSERT_SIZEOF(Actor05300SpawnPos, 0x6);

/// One row of the two clip/scale tables (`D_actor_105300_8013D3E0` for sub-
/// state 0, `D_actor_105300_8013D3EC` for 1) the animation schedule walks by
/// `Actor05300Work::field_328`. A zero `field_0` advances the row; a non-zero
/// one ends the clip and reseeds the countdown, so each table's last row is
/// its terminator. `field_2` is the scale that row hands
/// `ActorsShared80136574`.
typedef struct Actor05300Clip {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ u16 field_2;
} Actor05300Clip;
STATIC_ASSERT_SIZEOF(Actor05300Clip, 0x4);

/// One row of the per-area sound table `D_actor_105300_8013D3C4`, indexed by
/// `GameSession::field_4`. `field_0` and `field_2` are the two s8 parameters
/// `SndEvt_EnqueueTypeA` is handed with the work block's sound id.
typedef struct Actor05300SndRow {
    /* 0x0 */ s8 field_0;
    /* 0x1 */ s8 pad_1;
    /* 0x2 */ s8 field_2;
    /* 0x3 */ s8 pad_3;
} Actor05300SndRow;
STATIC_ASSERT_SIZEOF(Actor05300SndRow, 0x4);

/// The gameplay LCG the clip schedules reseed their countdowns from,
/// `state = state * 5 + 0x71357911`. Unsigned here for the same reason as
/// `Gp_LcgState` elsewhere: the draws are logical shifts of the high half
/// (`srl`), which a signed declaration would turn into an arithmetic one.
extern u32 Gp_LcgState;

s16 Actor05300_Fn01B70(Actor05300* arg0);

#endif
