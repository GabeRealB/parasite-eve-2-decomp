#ifndef ACTORS_SHARED_80133C1C_H
#define ACTORS_SHARED_80133C1C_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/tmd.h"

/// Context as this body sees it: the `GpEnemy` whose work id sizes the sound
/// id and the `TmdObject` whose `field_8` is the actor's root coordinate.
typedef struct ActorShared80133c1c {
    /* 0x00 */ byte       pad_0[0x20];
    /* 0x20 */ GpEnemy*   field_20;
    /* 0x24 */ byte       pad_24[8];
    /* 0x2C */ TmdObject* field_2C;
} ActorShared80133c1c;

s32 ActorsShared80133c1c(ActorShared80133c1c* arg0);

#endif
