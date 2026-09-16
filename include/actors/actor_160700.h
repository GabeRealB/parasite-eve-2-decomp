#ifndef ACTOR_160700_H
#define ACTOR_160700_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block `ActorsShared80131e24Sub0` hangs off its task's `Task::idMap`
/// slot (0x1C); it allocates it with `Mem_Calloc(0x4F8, 0)`, and the size below
/// is that allocation. `light` / `color` go to the sub-model's `field_1C` /
/// `field_20`, and `anim`, `slots` and `field_374` are what `func_800B3F84`
/// fills in.
///
/// The routine spawns a paired enemy, then parks that spawn's task in
/// `field_4F0` and reparents its own task under it, so the two share one
/// sub-model chain; `enemy` is the enemy its own task belongs to. `field_4B8`
/// is the animation id the pair starts on.
typedef struct Actor160700Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       field_374;
    /* 0x375 */ byte       pad_375[0x13F];
    /* 0x4B4 */ s16        state;
    /* 0x4B6 */ byte       pad_4B6[0x2];
    /* 0x4B8 */ s16        field_4B8;
    /* 0x4BA */ byte       pad_4BA[0x36];
    /* 0x4F0 */ Task*      field_4F0;
    /* 0x4F4 */ GpEnemy*   enemy;
} Actor160700Work;
STATIC_ASSERT_SIZEOF(Actor160700Work, 0x4F8);

void func_actor_160700_80132184(Task* task);

#endif
