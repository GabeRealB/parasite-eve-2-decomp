#ifndef ACTOR_460200_H
#define ACTOR_460200_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/1BC.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"

extern u8 D_80072729;

/// Steps `coord` `amount` units along its local Z axis, unless movement is
/// frozen. The direction is staged on the scratchpad stack.
static __inline__ void Actor460200_MoveForward(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 == 1) {
        return;
    }
    head                       = *(SVECTOR**)G_SCRATCH_HEAD;
    vec                        = head - 1;
    *(SVECTOR**)G_SCRATCH_HEAD = vec;
    Gfx_MatrixCol2(&coord->coord, vec);
    VectorNormalSS(vec, vec);
    gte_lddp(amount);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    coord->coord.t[0]          += head[-1].vx;
    coord->coord.t[1]          += vec->vy;
    coord->coord.t[2]          += vec->vz;
    coord->flg                  = 0;
    *(SVECTOR**)G_SCRATCH_HEAD += 1;
}

/// Work block this overlay's actors hang off their task's `Task::work` slot
/// (0x1C), which is not a `TaskIdMap` here. `ActorsShared80132514Work`
/// describes the same block from the animation-reset path's side.
///
/// `state` drives `func_actor_460200_801325FC`: 1 starts the animation through
/// `func_actor_460200_80132AC8` and 2 through `func_actor_460200_80132A50`,
/// both of which then advance it to 3. `animId` is the clip those slots are
/// seeded with and `animArg` the extra argument the first path carries; they
/// are the same two fields `ActorsShared80132514Work` names `field_4B8` /
/// `field_4EC`, which is also where `func_actor_460200_80132A50` latches
/// `animId` onto `field_4B6`.
///
/// `slots` is the twenty-element `GpAnimSlot` array at +0x54 those resets walk,
/// and the `0x374` pose buffer the spawn routine hands `func_800B3F84` sits
/// directly past it. `enemy` is the `GpEnemy` the spawn routine is passed in
/// its first argument (`Task::spawnArg2`) and parks for the exit path. `yaw`
/// caches the heading the placement opcode last gave the root coordinate.
typedef struct Actor460200Work {
    /* 0x000 */ byte       pad_0[0x40];
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       pad_374[0x140];
    /* 0x4B4 */ s16        state;
    /* 0x4B6 */ s16        field_4B6;
    /* 0x4B8 */ u16        animId;
    /* 0x4BA */ s16        field_4BA;
    /* 0x4BC */ byte       pad_4BC[0x2A];
    /* 0x4E6 */ u16        yaw;
    /* 0x4E8 */ byte       pad_4E8[0x4];
    /* 0x4EC */ u16        animArg;
    /* 0x4EE */ s16        field_4EE;
    /* 0x4F0 */ u16        field_4F0;
    /* 0x4F2 */ byte       pad_4F2[2];
    /* 0x4F4 */ GpEnemy*   enemy;
} Actor460200Work;
STATIC_ASSERT_SIZEOF(Actor460200Work, 0x4F8);

/// Work block of the actors whose spawn routine allocates it with
/// `memCalloc(0x4F8, 0)` and hangs it off `Task::work` (0x1C); the same
/// `Actor460200Work` prefix through `animArg`. Past that it diverges from the
/// 0x4FC-sized carriers `Actor460200Work` describes, so only the one field
/// this body reaches is described rather than a whole-block size that would
/// be wrong for them.
///
/// `field_4F0` is the task of the enemy the actor was paired with when its
/// `Task::spawnArg1` was set; the visibility opcode below drives that task's
/// model alongside its own, and falls back to its own model when no enemy was
/// spawned. It is the same field `ActorsShared80132eccWork` reaches.
typedef struct Actor460200PairedWork {
    /* 0x000 */ byte  pad_0[0x4F0];
    /* 0x4F0 */ Task* field_4F0;
} Actor460200PairedWork;

/// Argument block of the script opcode `func_actor_460200_80132B2C`
/// implements: which animation to play, and how. Same layout as the
/// `Actor150400AnimArgs` that opcode's twin takes.
typedef struct Actor460200AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
    /* 0x8 */ s32  withArg;
    /* 0xC */ u16  animArg;
} Actor460200AnimArgs;

/// Work block of the paired variant whose spawn routine allocates it with
/// `memCalloc(0x4FC, 0)`: `light` / `color` go to the sub-model's
/// `TmdObject::lightMtx` / `field_20`, and `anim`, `slots` and `pose` are what
/// `func_800B3F84` fills in. When `Task::spawnArg1` is set the routine spawns a
/// partner enemy, reparents its own task under the partner's and parks that task
/// in `field_4F4`; `animId` is then 2 rather than 1. `yaw` is the heading the
/// walk-to opcode aimed the root coordinate at and `travel` the number of
/// steps left on that walk. `enemy` is the actor's own
/// `GpEnemy`, handed back to `Gp_DestroyEnemy` on exit.
typedef struct Actor460200PairWork {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       pose[0x140];
    /* 0x4B4 */ s16        state;
    /* 0x4B6 */ s16        field_4B6;
    /* 0x4B8 */ u16        animId;
    /* 0x4BA */ byte       pad_4BA[0x2C];
    /* 0x4E6 */ u16        yaw;
    /* 0x4E8 */ byte       pad_4E8[0x2];
    /* 0x4EA */ s16        travel;
    /* 0x4EC */ byte       pad_4EC[0x2];
    /* 0x4EE */ s16        field_4EE;
    /* 0x4F0 */ s16        field_4F0;
    /* 0x4F2 */ byte       pad_4F2[0x2];
    /* 0x4F4 */ Task*      field_4F4;
    /* 0x4F8 */ GpEnemy*   enemy;
} Actor460200PairWork;
STATIC_ASSERT_SIZEOF(Actor460200PairWork, 0x4FC);

void func_actor_460200_801325FC(Task* task);

s32 func_actor_460200_80132B2C(Task* task, s32 arg1, Actor460200AnimArgs* args);

s32 func_actor_460200_80133C64(Task* task, s32 arg1, Actor460200AnimArgs* args);

s32 func_actor_460200_80133CD0(Task* task, s32 arg1, s32 flags);

/// Payload of the script opcodes that set the work block's `field_4EE`, the
/// flag that makes the actor turn its head toward the player.
typedef struct Actor460200LookArgs {
    /* 0x0 */ byte pad_0[2];
    /* 0x2 */ u16  look;
} Actor460200LookArgs;

/// Payload of the walk-to opcode: the world position to walk to. Only the
/// horizontal components are read.
typedef struct Actor460200WalkTarget {
    /* 0x00 */ VECTOR pos;
} Actor460200WalkTarget;

/// Payload of the placement opcode: a world translation followed by Euler
/// angles, of which only the yaw (`rot.vy`) is used.
typedef struct Actor460200Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor460200Placement;

/// Spawn argument (`Task::spawnArg2`) of the screen-capture task
/// `func_actor_460200_80131E24`: `duration` seeds the task's kill countdown,
/// and `done` is cleared on start, set when the countdown runs out, and ends
/// the task once non-zero.
typedef struct Actor460200CaptureArgs {
    /* 0x0 */ u16 duration;
    /* 0x2 */ s16 done;
} Actor460200CaptureArgs;

#endif
