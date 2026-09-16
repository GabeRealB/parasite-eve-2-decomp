#ifndef ACTOR_451100_H
#define ACTOR_451100_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block this overlay hangs off its task's `Task::idMap` slot (0x1C),
/// which is not a `TaskIdMap` here. This overlay's own
/// `ActorsShared80131e24Sub0` allocates it with `Mem_Calloc(0x4C0, 0)` and
/// stores the result straight into that field, so the size below is the
/// allocation and not a guess; the same routine parks the block's first matrix
/// in the object's `field_1C` and the second in `field_20`, which is why the
/// two leading matrices of `Actor150400Work` - the work block of this actor's
/// twin - sit at 0x0 and 0x20 here too.
///
/// `animId` is the clip to play, `animArg` the extra argument only the
/// `withArg` start path forwards. `state` is what the starter
/// `func_actor_451100_80132A1C` dispatches on: 1 runs the animation through
/// `ActorsShared80132640`, 2 through `ActorsShared801325c8`, and both then
/// advance it to 3. Those are the same two shared steps, on the same offsets,
/// as `func_actor_150400_80132228` takes.
///
/// `field_47E` is `animId` as it was when the slots were last seeded: both
/// walks over the slot array - this overlay's `func_actor_451100_801324B8` and
/// the `ActorsShared801325c8` the starter dispatches to - latch it there once
/// they have re-aimed every slot, so it records the set now playing. `anim` is
/// the animation context those walks reseed.
typedef struct Actor451100Work {
    /* 0x000 */ byte      pad_0[0x40];
    /* 0x040 */ GpAnimCtx anim;
    /* 0x054 */ byte      pad_54[0x428];
    /* 0x47C */ s16       state;
    /* 0x47E */ u16       field_47E;
    /* 0x480 */ u16       animId;
    /* 0x482 */ s16       field_482;
    /* 0x484 */ byte      pad_484[0x30];
    /* 0x4B4 */ u16       animArg;
    /* 0x4B6 */ byte      pad_4B6[0xA];
} Actor451100Work;
STATIC_ASSERT_SIZEOF(Actor451100Work, 0x4C0);

extern Actor451100Work* ActorsShared80131f9cWork;

/// Argument block of the script opcode `func_actor_451100_80132E98`
/// implements: which animation to play, and how. Same shape as the
/// `Actor150400AnimArgs` and `Actor450800AnimArgs` that opcode's twins take.
typedef struct Actor451100AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
    /* 0x8 */ s32  withArg;
    /* 0xC */ u16  animArg;
} Actor451100AnimArgs;

void func_actor_451100_80132A1C(Task* task);

s32 func_actor_451100_80132E98(Task* task, s32 arg1, Actor451100AnimArgs* args);

#endif
