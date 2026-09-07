#ifndef ACTORS_SHARED_80132640_H
#define ACTORS_SHARED_80132640_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `Task::idMap`
/// slot (0x1C), which is not a `TaskIdMap` here. Each overlay's spawn routine
/// allocates it with `Mem_Calloc` and stores it into that field; the carriers
/// ask for different sizes, so only the prefix this body reaches is described
/// here rather than a whole-block size that would be wrong for some of them.
///
/// `anim` is the animation context `func_800B4114` walks. `field_480` is the
/// current animation id; after the slots are started it is latched into
/// `field_47E`. `field_4B4` is the extra argument forwarded to `func_800B4114`.
typedef struct ActorsShared80132640Work {
    /* 0x000 */ byte      pad_0[0x40];
    /* 0x040 */ GpAnimCtx anim;
    /* 0x054 */ byte      pad_54[0x42A];
    /* 0x47E */ u16       field_47E;
    /* 0x480 */ u16       field_480;
    /* 0x482 */ byte      pad_482[0x32];
    /* 0x4B4 */ s16       field_4B4;
    /* 0x4B6 */ byte      pad_4B6[2];
} ActorsShared80132640Work;
STATIC_ASSERT_SIZEOF(ActorsShared80132640Work, 0x4B8);

/// Starts animation slots 1..0x12 of the actor's animation context.
void ActorsShared80132640(Task* task);

#endif
