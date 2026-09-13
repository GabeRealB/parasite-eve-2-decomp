#ifndef ACTOR_402200_H
#define ACTOR_402200_H

#include "common.h"

/// Per-instance work block the overlay's setup `func_actor_402200_80137444`
/// allocates with `Mem_Calloc(0x71C)` and parks in the 0x1C slot below (the
/// task's `Task::idMap`, which is not a `TaskIdMap` here).
///
/// `field_6F4` is the actor's phase, written and read as a signed halfword:
/// the frame handler clears it on entry, `ActorsShared801381e0` raises it to 1
/// while the remaining-enemy count is positive, and the handler branches on
/// 0 / 1 thereafter.
typedef struct Actor402200Work {
    /* 0x000 */ byte pad_0[0x6F4];
    /* 0x6F4 */ s16  field_6F4;
    /* 0x6F6 */ byte pad_6F6[0x26];
} Actor402200Work;
STATIC_ASSERT_SIZEOF(Actor402200Work, 0x71C);

/// Actor context handed to this overlay's callbacks: `field_1C` is the work
/// block above. Same shape as the other actor overlays' contexts.
typedef struct Actor402200 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor402200Work* field_1C;
} Actor402200;

#endif
