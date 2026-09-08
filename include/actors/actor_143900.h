#ifndef ACTOR_143900_H
#define ACTOR_143900_H

#include "common.h"
#include "actors/actors_shared_8013411c.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// Per-actor work block for the `actor_143900` overlay.
///
/// The overlay's state-0 handler (`ActorsShared80131f9cSub0`, here at
/// 0x80131E70) allocates it with `Mem_Calloc(0x4F0, 0)` and
/// stores the pointer both in `ActorsShared80131f9cWork` and in the task's
/// 0x1C slot, so the size below is the allocation and not a guess. Every
/// other function in the overlay reaches the block through the global.
///
/// `anim` is the animation context `Gp_AnimTickIndex` and friends walk.
typedef struct Actor143900Work {
    /* 0x000 */ byte      pad_0[0x40];
    /* 0x040 */ GpAnimCtx anim;
    /* 0x054 */ byte      pad_54[0x462];
    /* 0x4B6 */ s16       field_4B6; // copy of `field_4B8`, kept for change detection
    /* 0x4B8 */ s16       field_4B8; // animation id the slots are seeded with
    /* 0x4BA */ byte      pad_4BA[0x2C];
    /* 0x4E6 */ u16       yaw;       // last yaw handed to `Gfx_RotMatrixY`
    /* 0x4E8 */ byte      pad_4E8[8];
} Actor143900Work;
STATIC_ASSERT_SIZEOF(Actor143900Work, 0x4F0);

extern Actor143900Work* ActorsShared80131f9cWork;

void func_actor_143900_801324C8(void);
s32  func_actor_143900_801326FC(Task* task, s32 arg1, ActorShared8013411cPlacement* placement);

#endif
