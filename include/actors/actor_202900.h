#ifndef ACTOR_202900_H
#define ACTOR_202900_H

#include "common.h"
#include "gameplay/1BC.h"

/// Per-actor work block for the `actor_202900` overlay, reached through the
/// global `ActorsShared80131f9c` publishes.
///
/// Only the prefix the overlay's matched functions need is reconstructed so
/// far. `anim` sits at 0x40 and the slot array at 0x54, the same layout as
/// `Actor143900Work` and `Actor151000Work`; the nineteen slots are the ones
/// `func_actor_202900_8014A208` ticks.
typedef struct Actor202900Work {
    /* 0x000 */ byte       pad_0[0x40];
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x13];
} Actor202900Work;

extern Actor202900Work* ActorsShared80131f9cWork;

void func_actor_202900_8014A208(void);

#endif
