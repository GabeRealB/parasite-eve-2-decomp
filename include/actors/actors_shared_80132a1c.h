#ifndef ACTORS_SHARED_80132A1C_H
#define ACTORS_SHARED_80132A1C_H

#include "common.h"

#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `Task::work`
/// slot (0x1C), which is not a `TaskIdMap` here. The two carriers allocate it
/// with `memCalloc` and describe it as their own `Actor451100Work` /
/// `Actor150400Work`; those differ in the animation slots above `state` and in
/// what follows `animArg`, so only the prefix this body reaches is described
/// here rather than a whole-block size that would be wrong for one of them.
///
/// `state` is what this body dispatches on: 1 runs the animation through
/// `ActorsShared80132640`, 2 through `ActorsShared801325c8`, and both then
/// advance it to 3, the step state. `animId` is the clip to play, `travel` the
/// walk clip's countdown and `animArg` the extra argument the reseed forwards
/// to `func_800B4114` - the same two fields `ActorsShared80132640Work` names
/// `field_480` / `field_4B4`.
typedef struct ActorsShared80132a1cWork {
    /* 0x000 */ byte pad_0[0x47C];
    /* 0x47C */ s16  state;
    /* 0x47E */ byte pad_47E[0x2];
    /* 0x480 */ u16  animId;
    /* 0x482 */ byte pad_482[0x30];
    /* 0x4B2 */ s16  travel;
    /* 0x4B4 */ u16  animArg;
} ActorsShared80132a1cWork;
STATIC_ASSERT_SIZEOF(ActorsShared80132a1cWork, 0x4B6);

/// The actors' step body, run once per frame. States 1 and 2 run the two
/// shared reseed routines and advance the state to 3, which walks the actor
/// 0x11 units along its coordinate while the walk clip has `travel` left,
/// dropping back to clip 1 when the count runs out, then ticks the slots.
void ActorsShared80132a1c(Task* task);

#endif
