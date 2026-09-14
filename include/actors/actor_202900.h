#ifndef ACTOR_202900_H
#define ACTOR_202900_H

#include "common.h"
#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"

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
    /* 0x34C */ byte       pad_34C[0x130];
    /* 0x47C */ s16        field_47C; // actor step: 1 and 2 select the body to run, which then advances it to 3
} Actor202900Work;

extern Actor202900Work* ActorsShared80131f9cWork;

/// Runs the body the actor's step selects and then leaves it in step 3, the
/// running state. `arg0` is handed the actor but the body ignores it: it
/// reaches the work block through the global, like every other function in the
/// overlay.
void func_actor_202900_8014A194(GpActorWork* arg0);
void func_actor_202900_8014A208(void);
void func_actor_202900_8014A260(void);
void func_actor_202900_8014A304(void);

#endif
