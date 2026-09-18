#ifndef ACTOR_161500_H
#define ACTOR_161500_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block `ActorsShared80131e24Sub0` hangs off its task's `Task::work`
/// slot (0x1C); it allocates it with `Mem_Calloc(0x4FC, 0)`, and the size below
/// is that allocation. `light` / `color` go to the sub-model's `field_1C` /
/// `field_20`, and `anim`, `slots` and `field_374` are what `func_800B3F84`
/// fills in.
///
/// The routine optionally spawns a paired enemy, parks that spawn's task in
/// `field_4F4` and reparents its own task under it, so the two share one
/// sub-model chain; `enemy` is the enemy its own task belongs to. `animId` is
/// the animation id the pair starts on, 2 with the pair and 1 without it.
typedef struct Actor161500Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       field_374;
    /* 0x375 */ byte       pad_375[0x13F];
    /* 0x4B4 */ s16        state;
    /* 0x4B6 */ byte       pad_4B6[0x2];
    /* 0x4B8 */ u16        animId;
    /* 0x4BA */ byte       pad_4BA[0x30];
    /* 0x4EA */ s16        travel;
    /* 0x4EC */ s16        animArg;
    /* 0x4EE */ s16        field_4EE;
    /* 0x4F0 */ s16        field_4F0;
    /* 0x4F2 */ byte       pad_4F2[0x2];
    /* 0x4F4 */ Task*      field_4F4;
    /* 0x4F8 */ GpEnemy*   enemy;
} Actor161500Work;
STATIC_ASSERT_SIZEOF(Actor161500Work, 0x4FC);

void func_actor_161500_8013252C(Task* task);

#endif
