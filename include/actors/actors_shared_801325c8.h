#ifndef ACTORS_SHARED_801325C8_H
#define ACTORS_SHARED_801325C8_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `Task::idMap`
/// slot (0x1C), which is not a `TaskIdMap` here. It is the same block
/// `ActorsShared8013257c` walks - `anim` at 0x40 with its nineteen 0x28-byte
/// slots at 0x54 - carried far enough to reach the two animation-set words at
/// the tail. The overlays allocate different total sizes, so only the prefix
/// this body reaches is described rather than a whole-block size that would be
/// wrong for some of them.
///
/// `field_480` is the animation set the overlay wants played; this body copies
/// it into `field_47E` after resetting every slot to it, so `field_47E` is the
/// set the slots were last reset to.
typedef struct ActorsShared801325c8Work {
    /* 0x000 */ byte       pad_0[0x40];
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[19];
    /* 0x34C */ byte       pad_34C[0x132];
    /* 0x47E */ u16        field_47E;
    /* 0x480 */ s16        field_480;
    /* 0x482 */ byte       pad_482[2];
} ActorsShared801325c8Work;
STATIC_ASSERT_SIZEOF(ActorsShared801325c8Work, 0x484);

/// Resets animation slots 1..0x12 to the requested set and records it.
void ActorsShared801325c8(Task* task);

#endif
