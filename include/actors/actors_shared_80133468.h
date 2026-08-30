#ifndef ACTORS_SHARED_80133468_H
#define ACTORS_SHARED_80133468_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct ActorShared80133468Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
} ActorShared80133468Obj2C;

typedef struct ActorShared80133468 {
    /* 0x00 */ byte                      pad_0[0x2C];
    /* 0x2C */ ActorShared80133468Obj2C* field_2C;
} ActorShared80133468;

typedef struct ActorShared80133468Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
} ActorShared80133468Ctx;

void ActorsShared80133468(ActorShared80133468Ctx* arg0, ActorShared80133468* arg1);

#endif
