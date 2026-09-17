#ifndef ACTORS_SHARED_801330AC_H
#define ACTORS_SHARED_801330AC_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `Task::idMap`
/// slot (0x1C), which is not a `TaskIdMap` here. Each overlay's spawn routine
/// allocates it with `Mem_Calloc` and stores it into that field; the carriers
/// ask for different sizes, so only the prefix this body reaches is described
/// here rather than a whole-block size that would be wrong for some of them.
///
/// `state` drives the machine: 1 starts the animation through
/// `ActorsShared80132640` and 2 reseeds the slots through `ActorsShared801325c8`,
/// both then advancing it to 3. `animId` is the clip now playing - 4 is the
/// walk clip, which `travel` counts down one step per frame; `animArg` is the
/// reset argument stored alongside the clip reset when the walk ends.
///
/// `Actor450800SpawnWork` describes the same block for its carrier, naming the
/// clip `animId` and the pair `travel` / `animArg` as well.
typedef struct ActorsShared801330acWork {
    /* 0x000 */ byte pad_0[0x47C];
    /* 0x47C */ s16  state;
    /* 0x47E */ byte pad_47E[0x2];
    /* 0x480 */ u16  animId;
    /* 0x482 */ s16  field_482;
    /* 0x484 */ byte pad_484[0x2E];
    /* 0x4B2 */ s16  travel;
    /* 0x4B4 */ s16  animArg;
    /* 0x4B6 */ byte pad_4B6[0x2];
} ActorsShared801330acWork;
STATIC_ASSERT_SIZEOF(ActorsShared801330acWork, 0x4B8);

/// The enemy model task's step body, shared by the overlays that carry it.
void ActorsShared801330ac(Task* task);

#endif
