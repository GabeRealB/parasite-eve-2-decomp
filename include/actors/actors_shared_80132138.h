#ifndef ACTORS_SHARED_80132138_H
#define ACTORS_SHARED_80132138_H

#include "common.h"

#include "gameplay/1BC.h"

/// The part of the `ActorsShared80131f9c` work block this body touches. Each
/// carrier's block is its own type (see `include/actors/actor_110300.h` for
/// the fuller layout); the shared unit only names the animation context, which
/// in the carriers of this body is the block's first member - unlike
/// `Actor143900Work` and `Actor151000Work`, where it follows a 0x40-byte state
/// prefix and `&work->anim` costs an addend.
typedef struct ActorsShared80132138Work {
    /* 0x000 */ GpAnimCtx anim;
} ActorsShared80132138Work;
STATIC_ASSERT_SIZEOF(ActorsShared80132138Work, 0x14);

extern ActorsShared80132138Work* ActorsShared80131f9cWork;

void ActorsShared80132138(void);

#endif
