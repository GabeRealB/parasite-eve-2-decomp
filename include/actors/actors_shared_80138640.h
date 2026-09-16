#ifndef ACTORS_SHARED_80138640_H
#define ACTORS_SHARED_80138640_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct ActorShared80138640Obj2C {
    /* 0x0 */ byte           pad_0[0x8];
    /* 0x8 */ GsCOORDINATE2* field_8;
} ActorShared80138640Obj2C;

typedef struct ActorShared80138640Work {
    /* 0x000 */ byte    pad_0[0x33C];
    /* 0x33C */ VECTOR3 field_33C; ///< previous frame's coord translation
    /* 0x348 */ byte    pad_348[0x30];
    /* 0x378 */ s16     field_378; ///< forward speed, 4096 = 1.0
    /* 0x37A */ byte    pad_37A[0x1E];
    /* 0x398 */ s16     field_398; ///< vertical speed, whole units
} ActorShared80138640Work;

typedef struct ActorShared80138640 {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ ActorShared80138640Work*  field_1C;
    /* 0x20 */ byte                      pad_20[0xC];
    /* 0x2C */ ActorShared80138640Obj2C* field_2C;
} ActorShared80138640;

/// Steps the actor's coordinate forward, saving the previous position first.
void ActorsShared80138640(ActorShared80138640* arg0);

#endif
