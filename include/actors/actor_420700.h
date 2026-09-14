#ifndef ACTOR_420700_H
#define ACTOR_420700_H

#include "common.h"

/// Per-actor work block for the `actor_420700` overlay.
///
/// The overlay's state-0 handler (`ActorsShared80131f9cSub0`, here at
/// 0x80131E24) allocates it with `Mem_Calloc(0x5A0, 0)` and stores the pointer
/// both in `ActorsShared80131f9cWork` and in the task's 0x1C slot, so the size
/// below is the allocation and not a guess. Every other function in the
/// overlay reaches the block through the global.
typedef struct Actor420700Work {
    /* 0x000 */ byte pad_0[0x4B4];
    /* 0x4B4 */ s16  field_4B4; // actor step: 1 and 2 select the body to run, which then advances it to 3
    /* 0x4B6 */ byte pad_4B6[0xEA];
} Actor420700Work;
STATIC_ASSERT_SIZEOF(Actor420700Work, 0x5A0);

extern Actor420700Work* ActorsShared80131f9cWork;

void func_actor_420700_801324EC(void);
void func_actor_420700_801325C8(void);

/// The shared reset body `src/actors/lib/actors_shared_80132538.c`. Declared
/// here rather than through `actors_shared_80132538.h` because that header
/// publishes `ActorsShared80131f9cWork` with its own work type.
void ActorsShared80132538(void);

#endif
