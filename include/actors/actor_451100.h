#ifndef ACTOR_451100_H
#define ACTOR_451100_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block this overlay hangs off its task's `Task::work` slot (0x1C),
/// which is not a `TaskIdMap` here. This overlay's own
/// `ActorsShared80131e24Sub0` allocates it with `memCalloc(0x4C0, 0)` and
/// stores the result straight into that field, so the size below is the
/// allocation and not a guess; the same routine parks the block's first matrix
/// in the object's `field_1C` and the second in `field_20`, which is why the
/// two leading matrices of `Actor150400Work` - the work block of this actor's
/// twin - sit at 0x0 and 0x20 here too.
///
/// `animId` is the clip to play, `animArg` the extra argument only the
/// `withArg` start path forwards. `state` is what the shared starter
/// `ActorsShared80132a1c` dispatches on: 1 runs the animation through
/// `ActorsShared80132640`, 2 through `ActorsShared801325c8`, and both then
/// advance it to 3. Those are the same two shared steps, on the same offsets,
/// as this actor's twin `Actor150400Work` block takes.
///
/// `travel` counts the walk clip down: while `animId` is 4 and `travel` is
/// non-zero the step body moves the actor 0x11 units along its coordinate and
/// drops back to clip 1 when the count runs out.
///
/// `field_47E` is `animId` as it was when the slots were last seeded: both
/// walks over the slot array - this overlay's `func_actor_451100_801324B8` and
/// the `ActorsShared801325c8` the starter dispatches to - latch it there once
/// they have re-aimed every slot, so it records the set now playing. `anim` is
/// the animation context those walks reseed.
typedef struct Actor451100Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       pad_374[0x108];
    /* 0x47C */ s16        state;
    /* 0x47E */ u16        field_47E;
    /* 0x480 */ u16        animId;
    /* 0x482 */ s16        field_482;
    /* 0x484 */ byte       pad_484[0x2A];
    /* 0x4AE */ s16        yaw; // model yaw the walk turns by 0x33 a frame
    /* 0x4B0 */ byte       pad_4B0[0x2];
    /* 0x4B2 */ s16        travel;
    /* 0x4B4 */ u16        animArg;
    /* 0x4B6 */ byte       pad_4B6[0x2];
    /* 0x4B8 */ Task*      pairTask;
    /* 0x4BC */ GpEnemy*   enemy;
} Actor451100Work;
STATIC_ASSERT_SIZEOF(Actor451100Work, 0x4C0);

extern Actor451100Work* ActorsShared80131f9cWork;

/// Reset argument this overlay forwards to every reseeded slot.
extern s16 D_actor_451100_8013F700;

/// Picks the distance `func_actor_451100_80131F84` walks the model each frame:
/// 0 steps 0x3C forward, 1 steps 0xF back, 2 steps 0x19 forward.
extern s16 D_actor_451100_8014E74C;

/// Argument block of the script opcode `func_actor_451100_80132E98`
/// implements: which animation to play, and how. Same shape as the
/// `Actor150400AnimArgs` and `Actor450800AnimArgs` that opcode's twins take.
typedef struct Actor451100AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
    /* 0x8 */ s32  withArg;
    /* 0xC */ u16  animArg;
} Actor451100AnimArgs;

void ActorsShared80132a1c(Task* task);

/// Task exit callback this overlay installs on spawn.
void func_actor_451100_80132CAC(Task* task);

/// Ticks the animation once the runner has moved and turned the model.
void func_actor_451100_801323DC(void);

/// Restarts the animation with the reset argument in
/// `D_actor_451100_8013F700`; the runner's state 1.
void func_actor_451100_801324B8(void);

/// Per-frame update: states 1 and 2 run their one-shot animation reseed and
/// leave the work block in state 3; state 3 walks the model while `travel`
/// counts down (distance picked by `D_actor_451100_8014E74C`), turns it while
/// `animArg` counts down in animation 3, then ticks the animation.
void func_actor_451100_80131F84(Task* task);

/// Second script opcode taking `Actor451100AnimArgs`: like
/// `func_actor_451100_80132E98`, but it accepts a wider id range and enters the
/// run through `func_actor_451100_80131F84`.
s32 func_actor_451100_80132538(Task* task, s32 arg1, Actor451100AnimArgs* args);

s32 func_actor_451100_80132E98(Task* task, s32 arg1, Actor451100AnimArgs* args);

#endif
