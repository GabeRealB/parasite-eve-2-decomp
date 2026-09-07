#ifndef ACTORS_SHARED_801324C8_H
#define ACTORS_SHARED_801324C8_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `Task::idMap`
/// slot (0x1C), which is not a `TaskIdMap` here. The carriers allocate blocks
/// of different sizes, so only the prefix this body reaches is described.
///
/// `anim` is the animation context `func_800B4114` reseeds. `field_4B8` is the
/// animation id the slots are seeded with and `field_4B6` the copy of it kept
/// for change detection; `field_4EC` is passed through as the reset argument.
typedef struct ActorsShared801324c8Work {
    /* 0x000 */ byte      pad_0[0x40];
    /* 0x040 */ GpAnimCtx anim;
    /* 0x054 */ byte      pad_54[0x462];
    /* 0x4B6 */ s16       field_4B6;
    /* 0x4B8 */ s16       field_4B8;
    /* 0x4BA */ byte      pad_4BA[0x32];
    /* 0x4EC */ s16       field_4EC;
} ActorsShared801324c8Work;
STATIC_ASSERT_SIZEOF(ActorsShared801324c8Work, 0x4F0);

/// Reseeds animation slots 1..0x13 from the current animation id and records
/// that id as the one now playing.
void ActorsShared801324c8(Task* task);

#endif
