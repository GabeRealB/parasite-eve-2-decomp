#ifndef ACTOR_160600_H
#define ACTOR_160600_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block the actor's spawn routine allocates with `memCalloc(0x4F8, 0)`
/// and hangs off its task's `Task::work` slot. `light` / `color` are the
/// matrix pair the model is lit with, and `anim`, `slots` and `field_374` are
/// what `func_800B3F84` fills in.
///
/// `state` selects what the step body does next: 1 and 2 reseed the animation
/// slots from `animId` (with and without `animArg`) and advance to 3, which
/// ticks them. `appliedAnimId` records the id the slots were last seeded with.
/// `animId` 4 is the walk clip, which `travel` counts down. `yaw` caches the
/// heading the placement and walk-to opcodes last gave the root coordinate.
/// `field_4EE` nonzero enables the per-frame effect spawns. `enemy` is the
/// enemy the actor's task belongs to.
typedef struct Actor160600Work {
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
    /* 0x4EE */ s16        field_4EE;
    /* 0x4F0 */ byte       pad_4F0[0x4];
    /* 0x4F4 */ GpEnemy*   enemy;
} Actor160600Work;
STATIC_ASSERT_SIZEOF(Actor160600Work, 0x4F8);

/// Payload of the "play animation" script opcode: which clip to play, and
/// whether to seed the slots with `animArg`.
typedef struct Actor160600AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
    /* 0x8 */ s32  withArg;
    /* 0xC */ s16  animArg;
} Actor160600AnimArgs;

/// Payload of the script opcode that sets the work block's `field_4EE`.
typedef struct Actor160600FlagArgs {
    /* 0x0 */ byte pad_0[2];
    /* 0x2 */ u16  value;
} Actor160600FlagArgs;

/// Payload of the placement opcode: a world translation followed by Euler
/// angles, of which only the yaw (`rot.vy`) is used.
typedef struct Actor160600Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor160600Placement;

/// Payload of the walk-to opcode: the world position to walk to. Only the
/// horizontal components are read.
typedef struct Actor160600WalkTarget {
    /* 0x00 */ VECTOR pos;
} Actor160600WalkTarget;

extern u32 Gp_LcgState;

void func_actor_160600_80131E68(GpEnemy* enemy, Task* task);
void func_actor_160600_80131FFC(Task* task);
void func_actor_160600_80132208(GpEnemy* enemy, Task* task);
void func_actor_160600_80132350(Task* task);
void func_actor_160600_80132378(Task* task);
void func_actor_160600_80132404(Task* task);
void func_actor_160600_80132450(Task* task);
void func_actor_160600_801324C8(Task* task);

#endif
