#ifndef ACTORS_SHARED_80136938_H
#define ACTORS_SHARED_80136938_H

#include "common.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/task.h"

/// Ground-burst work allocated by the specimen's setup handler and advanced
/// by ActorsShared80136c80. The collision node points at the capsule, whose
/// contact table occupies the final 0x18 bytes.
typedef struct ActorsShared80136938Work {
    /* 0x00 */ SVECTOR      vec;
    /* 0x08 */ GpObj        obj;
    /* 0x28 */ GpActorD4Rec rec;
    /* 0x40 */ GpRec18      rec2;
} ActorsShared80136938Work;
STATIC_ASSERT_SIZEOF(ActorsShared80136938Work, 0x58);

/// Each carrier supplies its own contact identity pair at this symbol.
extern GpU16Pair ActorsShared80136938Pair;

extern u32 Gp_LcgState;

void ActorsShared80136938(Task* arg0);

#endif
