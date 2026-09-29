#ifndef INCLUDE_ACTORS_ACTORS_SHARED_80135C4C_H
#define INCLUDE_ACTORS_ACTORS_SHARED_80135C4C_H

#include "common.h"

#include "gameplay/actor.h"

#include "main/session_types.h"

/// Collision object and its single record, allocated by the shared setup body.
typedef struct ActorsShared80135c4cObjWork {
    /* 0x00 */ GpObj                 obj;
    /* 0x20 */ WorldCollisionContact rec;
    /* 0x38 */ s16                   field_38; // frame counter used by the projectile tick
    /* 0x3A */ s16                   field_3A;
    /* 0x3C */ u16                   field_3C;
    /* 0x3E */ byte                  pad_3E[2];
} ActorsShared80135c4cObjWork;
STATIC_ASSERT_SIZEOF(ActorsShared80135c4cObjWork, 0x40);

#endif // INCLUDE_ACTORS_ACTORS_SHARED_80135C4C_H
