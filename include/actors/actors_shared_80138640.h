#ifndef ACTORS_SHARED_80138640_H
#define ACTORS_SHARED_80138640_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

typedef struct ActorShared80138640Work {
    /* 0x000 */ byte    pad_0[0x33C];
    /* 0x33C */ VECTOR3 field_33C; ///< previous frame's coord translation
    /* 0x348 */ byte    pad_348[0x30];
    /* 0x378 */ s16     field_378; ///< forward speed, 4096 = 1.0
    /* 0x37A */ byte    pad_37A[0x1E];
    /* 0x398 */ s16     field_398; ///< vertical speed, whole units
} ActorShared80138640Work;

/// Steps the actor's coordinate forward, saving the previous position first.
void ActorsShared80138640(Task* task);

#endif
