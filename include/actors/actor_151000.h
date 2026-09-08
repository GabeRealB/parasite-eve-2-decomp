#ifndef ACTOR_151000_H
#define ACTOR_151000_H

#include "common.h"
#include "gameplay/1BC.h"
#include "main/task.h"

#include <psyq/libgte.h>

/// Per-actor work block for the `actor_151000` overlay.
///
/// The overlay's state-0 handler (`ActorsShared80131f9cSub0`, here at
/// 0x80131F1C) allocates it with `Mem_Calloc(0x4C0, 0)` and
/// stores the pointer both in `ActorsShared80131f9cWork` and in the task's
/// 0x1C slot, so the size below is the allocation and not a guess. Every
/// other function in the overlay reaches the block through the global.
///
/// `anim` is the animation context `Gp_AnimTickIndex` and friends walk.
/// `field_480` is the animation id the slots are seeded from; once they are
/// started it is latched into `field_47E`.
typedef struct Actor151000Work {
    /* 0x000 */ byte      pad_0[0x40];
    /* 0x040 */ GpAnimCtx anim;
    /* 0x054 */ byte      pad_54[0x42A];
    /* 0x47E */ u16       field_47E; // animation id currently playing
    /* 0x480 */ u16       field_480; // animation id the slots are seeded with
    /* 0x482 */ byte      pad_482[0x2C];
    /* 0x4AE */ u16       yaw;       // last yaw handed to `Gfx_RotMatrixY`
    /* 0x4B0 */ byte      pad_4B0[0x8];
    /* 0x4B8 */ s32       field_4B8;
    /* 0x4BC */ byte      pad_4BC[0x4];
} Actor151000Work;
STATIC_ASSERT_SIZEOF(Actor151000Work, 0x4C0);

extern Actor151000Work* ActorsShared80131f9cWork;

/// Position + Y rotation for `func_actor_151000_80132810`, laid out like
/// `ActorShared8013411cPlacement` but with only the yaw read.
typedef struct Actor151000Placement {
    /* 0x00 */ VECTOR pos;
    /* 0x10 */ byte   pad_10[2];
    /* 0x12 */ u16    yaw;
    /* 0x14 */ byte   pad_14[2];
} Actor151000Placement;
STATIC_ASSERT_SIZEOF(Actor151000Placement, 0x18);

void func_actor_151000_801325C4(void);
void func_actor_151000_801326AC(void);
s32  func_actor_151000_80132810(Task* task, s32 arg1, Actor151000Placement* placement);

#endif
