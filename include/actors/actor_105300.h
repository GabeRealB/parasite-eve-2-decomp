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
/// `GpEnemy::coord`.
typedef struct Actor05300Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ u16            field_C;
} Actor05300Obj2C;

/// Work block of the task this enemy hangs off -- the part spawn reaches it as
/// `task->parent->work`, and the part teardown reads the sound id `field_31C`
/// from it. `field_334` is the enemy's sub-state index: it selects the spawn
/// position in `D_actor_105300_80133A20` and also which of the two per-enemy
/// death flags the spawn raises.
///
/// The pose half is what the per-frame handlers drive: `field_32C` is the
/// animation sub-state `func_actor_105300_8013222C` dispatches on, `field_328`
/// the row of the clip table that state walks, `field_32A` the countdown the
/// LCG reseeds and `field_320` / `field_324` the pose the pose tick
/// `func_actor_105300_80133610` queues and the frames it has counted for it.
/// `field_2FC` is the local coordinate matrix that handler copies into the
/// model's own coordinate each frame. `field_320` / `field_324` are unsigned
/// here: every signed read of them casts at the use.
typedef struct Actor05300Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[10];
    /* 0x1A4 */ GpAnimPose poses[10];
    /* 0x244 */ MATRIX     field_244;
    /* 0x264 */ MATRIX     field_264;
    /* 0x284 */ GpObj      node0;
    /* 0x2A4 */ GpObj      node1;
    /* 0x2C4 */ GpRec18    rec18[2];
    /* 0x2F4 */ GpEffArg   field_2F4;
    /* 0x2FC */ MATRIX     field_2FC;
    /* 0x31C */ s32        field_31C;
    /* 0x320 */ u16        field_320;
    /* 0x322 */ s16        field_322;
    /* 0x324 */ u16        field_324;
    /* 0x326 */ u16        field_326;
    /* 0x328 */ u16        field_328;
    /* 0x32A */ u16        field_32A;
    /* 0x32C */ u16        field_32C;
    /* 0x32E */ u16        field_32E;
    /* 0x330 */ u16        field_330;
    /* 0x332 */ s16        field_332;
    /* 0x334 */ s16        field_334;
    /* 0x336 */ s16        field_336;
    /* 0x338 */ s16        field_338;
    /* 0x33A */ s16        field_33A;
    /* 0x33C */ s16        field_33C;
    /* 0x33E */ s16        field_33E;
} Actor05300Work;
STATIC_ASSERT_SIZEOF(Actor05300Work, 0x340);

/// The task whose work block is `Actor05300Work`, reached as `task->field_1C`
/// (the `Task::work` slot). `field_20` is the `Task::spawnArg2` slot holding
/// the enemy: the sound events this enemy plays carry its actor id in the
/// high nibble of `GpEnemy::placeKey`. `field_2C` is the `Task::extra` model
/// object.
typedef struct Actor05300 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor05300Work*  field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ Actor05300Obj2C* field_2C;
    /* 0x30 */ s32              field_30;
} Actor05300;

/// One row of the two clip/scale tables (`D_actor_105300_8013D3E0` for sub-
/// state 0, `D_actor_105300_8013D3EC` for 1) the animation schedule walks by
/// `Actor05300Work::field_328`. A zero `field_0` advances the row; a non-zero
/// one ends the clip and reseeds the countdown, so each table's last row is
/// its terminator. `field_2` is the scale that row hands
/// `func_actor_105300_801336D4`.
typedef struct Actor05300Clip {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ u16 field_2;
} Actor05300Clip;
STATIC_ASSERT_SIZEOF(Actor05300Clip, 0x4);

/// The gameplay LCG the clip schedules reseed their countdowns from,
/// `state = state * 5 + 0x71357911`. Unsigned here for the same reason as
/// `Gp_LcgState` elsewhere: the draws are logical shifts of the high half
/// (`srl`), which a signed declaration would turn into an arithmetic one.
extern u32 Gp_LcgState;

#endif
