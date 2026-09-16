#ifndef ACTORS_SHARED_80132428_H
#define ACTORS_SHARED_80132428_H

#include "common.h"

#include "gameplay/1BC.h"

/// Work block the actors sharing this body publish as `ActorsShared80131f9cWork`
/// (the same block they hang off the task's `Task::idMap` slot, 0x1C, which is
/// not a `TaskIdMap` here). It is the block `ActorsShared801325c8` walks
/// through `idMap` - `anim` at 0x40 with its nineteen 0x28-byte slots at 0x54 -
/// carried far enough to reach the two animation-set words at the tail. The
/// overlays sharing it allocate different total sizes, so only the prefix this
/// body reaches is described rather than a whole-block size that would be wrong
/// for some of them.
///
/// `animId` is the animation set the overlay wants played; this body copies it
/// into `field_47E` after resetting every slot to it, so `field_47E` is the set
/// the slots were last reset to.
typedef struct ActorsShared80132428Work {
    /* 0x000 */ byte       pad_0[0x40];
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[19];
    /* 0x34C */ byte       pad_34C[0x132];
    /* 0x47E */ u16        field_47E;
    /* 0x480 */ s16        animId;
    /* 0x482 */ byte       pad_482[2];
} ActorsShared80132428Work;
STATIC_ASSERT_SIZEOF(ActorsShared80132428Work, 0x484);

extern ActorsShared80132428Work* ActorsShared80131f9cWork;

void ActorsShared80132428(void);

#endif
