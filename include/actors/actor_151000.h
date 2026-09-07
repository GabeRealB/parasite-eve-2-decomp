#ifndef ACTOR_151000_H
#define ACTOR_151000_H

#include "common.h"
#include "gameplay/1BC.h"

/// Per-actor work block for the `actor_151000` overlay.
///
/// `func_actor_151000_80131F1C` allocates it with `Mem_Calloc(0x4C0, 0)` and
/// stores the pointer both in `D_actor_151000_8013D37C` and in the task's
/// 0x1C slot, so the size below is the allocation and not a guess. Every
/// other function in the overlay reaches the block through the global.
///
/// `anim` is the animation context `Gp_AnimTickIndex` and friends walk.
typedef struct Actor151000Work {
    /* 0x000 */ byte      pad_0[0x40];
    /* 0x040 */ GpAnimCtx anim;
    /* 0x054 */ byte      pad_54[0x46C];
} Actor151000Work;
STATIC_ASSERT_SIZEOF(Actor151000Work, 0x4C0);

extern Actor151000Work* D_actor_151000_8013D37C;

void func_actor_151000_801325C4(void);

#endif
