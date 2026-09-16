#ifndef ACTORS_SHARED_80132208_H
#define ACTORS_SHARED_80132208_H

#include "common.h"

#include "gameplay/1BC.h"

/// The part of the `ActorsShared80131f9c` work block this body touches. Each
/// carrier's block is its own type (see `include/actors/actor_110300.h` for
/// the fuller layout); the shared unit only names the animation context, which
/// in the carriers of this body is the block's first member, and the pair of
/// animation ids 0x40 below the fields of the same name in `Actor143900Work`.
typedef struct ActorsShared80132208Work {
    /* 0x000 */ GpAnimCtx anim;
    /* 0x014 */ byte      pad_14[0x462];
    /* 0x476 */ s16       field_476; // copy of `animId`, kept for change detection
    /* 0x478 */ u16       animId;    // animation id the slots are seeded with
    /* 0x47A */ byte      pad_47A[2];
} ActorsShared80132208Work;
STATIC_ASSERT_SIZEOF(ActorsShared80132208Work, 0x47C);

extern ActorsShared80132208Work* ActorsShared80131f9cWork;

void ActorsShared80132208(void);

#endif
