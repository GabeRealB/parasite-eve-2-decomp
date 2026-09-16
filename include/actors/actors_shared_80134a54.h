#ifndef ACTORS_SHARED_80134A54_H
#define ACTORS_SHARED_80134A54_H

#include "common.h"
#include "actors/actors_shared_80135b58.h"

#include <psyq/libgte.h>
#include <psyq/libgs.h>

/// Work block `field_1C` points at, as this body sees it: only the model
/// matrix it copies into the actor's coordinate node and the yaw it writes
/// into the scale vector's Y. All three carrier slots lay the two out at the
/// same offsets, which is why one object serves them.
typedef struct ActorShared80134a54Work {
    /* 000 */ byte   pad_0[0x32C];
    /* 32C */ MATRIX field_32C;
    /* 34C */ byte   pad_34C[0x1C];
    /* 368 */ s16    field_368;
} ActorShared80134a54Work;

typedef struct ActorShared80134a54 {
    /* 00 */ byte                      pad_0[0x1C];
    /* 1C */ ActorShared80134a54Work*  field_1C;
    /* 20 */ void*                     field_20;
    /* 24 */ byte                      pad_24[8];
    /* 2C */ ActorShared80135b58Obj2C* field_2C;
} ActorShared80134a54;

void ActorsShared80134a54(ActorShared80134a54* arg0);

#endif
