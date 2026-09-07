#ifndef ACTOR_143900_H
#define ACTOR_143900_H

#include "common.h"
#include "gameplay/1BC.h"

/// Per-actor work block for the `actor_143900` overlay.
///
/// `func_actor_143900_80131E70` allocates it with `Mem_Calloc(0x4F0, 0)` and
/// stores the pointer both in `D_actor_143900_801496B8` and in the task's
/// 0x1C slot, so the size below is the allocation and not a guess. Every
/// other function in the overlay reaches the block through the global.
///
/// `anim` is the animation context `Gp_AnimTickIndex` and friends walk.
typedef struct Actor143900Work {
    /* 0x000 */ byte      pad_0[0x40];
    /* 0x040 */ GpAnimCtx anim;
    /* 0x054 */ byte      pad_54[0x49C];
} Actor143900Work;
STATIC_ASSERT_SIZEOF(Actor143900Work, 0x4F0);

extern Actor143900Work* D_actor_143900_801496B8;

void func_actor_143900_801324C8(void);

#endif
