#ifndef ACTORS_SHARED_80132610_H
#define ACTORS_SHARED_80132610_H

#include "common.h"

#include "gameplay/1BC.h"

/// The part of the `ActorsShared80131f9c` work block this body touches. Each
/// carrier's block is its own type (see `include/actors/actor_151000.h` for
/// the full 0x4C0 layout); the shared unit names the animation context with
/// its slots, the two animation-set words at the tail and the cached record
/// pointer this body clears.
typedef struct ActorsShared80132610Work {
    /* 0x000 */ byte       pad_0[0x40];
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x13];
    /* 0x34C */ byte       pad_34C[0x132];
    /* 0x47E */ u16        field_47E;
    /* 0x480 */ u16        field_480;
    /* 0x482 */ byte       pad_482[0x36];
    /* 0x4B8 */ GpAnimRec* field_4B8;
    /* 0x4BC */ byte       pad_4BC[0x4];
} ActorsShared80132610Work;
STATIC_ASSERT_SIZEOF(ActorsShared80132610Work, 0x4C0);

extern ActorsShared80132610Work* ActorsShared80131f9cWork;

/// Restarts animation slots 1..0x12 from `field_480`, replaying each from the
/// top, and latches that set into `field_47E`.
void ActorsShared80132610(void);

#endif
