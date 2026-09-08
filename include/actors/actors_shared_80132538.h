#ifndef ACTORS_SHARED_80132538_H
#define ACTORS_SHARED_80132538_H

#include "common.h"

#include "gameplay/1BC.h"

/// The part of the `ActorsShared80131f9c` work block this body touches. Each
/// carrier's block is its own type (see `include/actors/actor_143900.h` for
/// the full 0x4F0 layout); the shared unit only names the animation context,
/// the slots behind it and the pair of animation ids.
///
/// This is the same walk `ActorsShared80132514` does, but reached through the
/// published global instead of through `Task::idMap`, which is what makes it a
/// second body rather than a second carrier of that one.
typedef struct ActorsShared80132538Work {
    /* 0x000 */ byte       pad_0[0x40];
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       pad_374[0x142];
    /* 0x4B6 */ s16        field_4B6; // copy of `field_4B8`, kept for change detection
    /* 0x4B8 */ s16        field_4B8; // animation id the slots are seeded with
    /* 0x4BA */ byte       pad_4BA[2];
} ActorsShared80132538Work;
STATIC_ASSERT_SIZEOF(ActorsShared80132538Work, 0x4BC);

extern ActorsShared80132538Work* ActorsShared80131f9cWork;

void ActorsShared80132538(void);

#endif
