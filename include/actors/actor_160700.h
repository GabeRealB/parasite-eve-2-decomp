#ifndef ACTOR_160700_H
#define ACTOR_160700_H

#include "common.h"

#include <psyq/libgte.h>

#include "actors/actors_shared_80132614.h"
#include "actors/actors_shared_801326ac.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block the actor's spawn handler allocates (0x4F8 bytes, cleared) and
/// hangs off `Task::work`, which is not a `TaskIdMap` here.
///
/// The head holds the light and colour matrices the actor's model and its
/// attached sub-model draw with, and the animation context with its twenty
/// slots. `state` selects what the step body does next: 1 reseeds the slots
/// from `animId` with `animArg`, 2 resets them to `animId`, and both then
/// advance to 3, which ticks the slots. `appliedAnimId` records the clip the
/// slots were last seeded with. The placement opcodes cache the root yaw in
/// `yaw`; the "walk to" opcode leaves the remaining distance, in steps of 12,
/// in `travel`, which the step body counts down while clip 4 plays.
/// `field_4F0` is the task of the enemy spawned alongside this one, whose
/// model the visibility opcode drives together with the actor's own, and
/// `enemy` is the enemy this actor's own task belongs to.
typedef struct Actor160700Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       field_374;
    /* 0x375 */ byte       pad_375[0x13F];
    /* 0x4B4 */ s16        state;
    /* 0x4B6 */ s16        appliedAnimId;
    /* 0x4B8 */ s16        animId;
    /* 0x4BA */ s16        field_4BA;
    /* 0x4BC */ byte       pad_4BC[0x2A];
    /* 0x4E6 */ u16        yaw;
    /* 0x4E8 */ byte       pad_4E8[0x2];
    /* 0x4EA */ s16        travel;
    /* 0x4EC */ s16        animArg;
    /* 0x4EE */ byte       pad_4EE[0x2];
    /* 0x4F0 */ Task*      field_4F0;
    /* 0x4F4 */ GpEnemy*   enemy;
} Actor160700Work;
STATIC_ASSERT_SIZEOF(Actor160700Work, 0x4F8);

/// Argument block of the play-animation script opcode: which clip to play,
/// and whether to reseed the slots with `animArg` or just reset them.
typedef struct Actor160700AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
    /* 0x8 */ s32  withArg;
    /* 0xC */ u16  animArg;
} Actor160700AnimArgs;

void func_actor_160700_80131F70(GpEnemy* enemy, Task* task);
void func_actor_160700_80132184(Task* task);
void func_actor_160700_80132390(GpEnemy* enemy, Task* task);
void func_actor_160700_80132414(Task* task);
void func_actor_160700_8013243C(Task* task);
void func_actor_160700_801324C8(Task* task);
void func_actor_160700_80132514(Task* task);
void func_actor_160700_8013258C(Task* task);

#endif
