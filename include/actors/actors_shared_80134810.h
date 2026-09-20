#ifndef ACTORS_SHARED_80134810_H
#define ACTORS_SHARED_80134810_H

#include "common.h"
#include "main/task.h"
#include "main/tmd.h"

/// Scratchpad stack head at G_SCRATCH_HEAD. A member access preserves the
/// dependency between publishing the vector and reading the actor scale.
typedef struct ActorScaleScratchHead {
    /* 0x00 */ void* head;
} ActorScaleScratchHead;
STATIC_ASSERT_SIZEOF(ActorScaleScratchHead, 0x4);

void ActorsShared80134810(Task* arg0, GsCOORDINATE2* arg1);

#endif
