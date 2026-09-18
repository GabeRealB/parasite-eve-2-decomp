#ifndef ACTOR_160600_H
#define ACTOR_160600_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block this overlay hangs off its task's `Task::work` slot (0x1C).
/// `ActorsShared80131e24Sub0` allocates it with `Mem_Calloc(0x4F8, 0)`, and
/// the size below is that allocation. `light` / `color` go to the object's
/// `field_1C` / `field_20`; `anim`, `slots` and `field_374` are what
/// `func_800B3F84` fills in.
///
/// `state`, `animId` and `animArg` are the same three the shared step body
/// reads (`ActorsShared8014c874Work`), at the same offsets: the script opcode
/// `func_actor_160600_8013252C` starts an animation by filling them in.
typedef struct Actor160600Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       field_374;
    /* 0x375 */ byte       pad_375[0x13F];
    /* 0x4B4 */ s16        state;
    /* 0x4B6 */ byte       pad_4B6[0x2];
    /* 0x4B8 */ s16        animId;
    /* 0x4BA */ s16        field_4BA;
    /* 0x4BC */ byte       pad_4BC[0x30];
    /* 0x4EC */ s16        animArg;
    /* 0x4EE */ s16        field_4EE; // nonzero enables the effect spawns
    /* 0x4F0 */ byte       pad_4F0[0x4];
    /* 0x4F4 */ GpEnemy*   enemy;
} Actor160600Work;
STATIC_ASSERT_SIZEOF(Actor160600Work, 0x4F8);

/// Argument block of the script opcode `func_actor_160600_8013252C`
/// implements: which animation to play, and how.
typedef struct Actor160600AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
    /* 0x8 */ s32  withArg;
    /* 0xC */ s16  animArg;
} Actor160600AnimArgs;

extern u32 Gp_LcgState;

#endif
