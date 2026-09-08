#ifndef ACTOR_150400_H
#define ACTOR_150400_H

#include "common.h"

#include "main/task.h"

/// Work block this overlay hangs off its task's `Task::idMap` slot (0x1C),
/// which is not a `TaskIdMap` here. Only the prefix the animation opcode and
/// `func_actor_150400_80132228` reach is described.
///
/// `state` drives `func_actor_150400_80132228`: 1 starts the animation through
/// `ActorsShared80132640`, 2 through `ActorsShared801325c8`, and both then
/// advance it to 3. `animId` is the clip to play and `animArg` the extra
/// argument `ActorsShared80132640` forwards to `func_800B4114`; they are the
/// same two fields `ActorsShared80132640Work` names `field_480` / `field_4B4`.
typedef struct Actor150400Work {
    /* 0x000 */ byte pad_0[0x47C];
    /* 0x47C */ s16  state;
    /* 0x47E */ byte pad_47E[0x2];
    /* 0x480 */ u16  animId;
    /* 0x482 */ s16  field_482;
    /* 0x484 */ byte pad_484[0x30];
    /* 0x4B4 */ u16  animArg;
} Actor150400Work;

/// Argument block of the script opcode `func_actor_150400_801326A4`
/// implements: which animation to play, and how.
typedef struct Actor150400AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
    /* 0x8 */ s32  withArg;
    /* 0xC */ u16  animArg;
} Actor150400AnimArgs;

void func_actor_150400_80132228(Task* task);

s32 func_actor_150400_801326A4(Task* task, s32 arg1, Actor150400AnimArgs* args);

#endif
