#ifndef ACTORS_SHARED_80134FF0_H
#define ACTORS_SHARED_80134FF0_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct ActorShared80134ff0Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} ActorShared80134ff0Obj2C;

typedef struct ActorShared80134ff0 {
    /* 0x00 */ byte                      pad_0[0x20];
    /* 0x20 */ void*                     field_20;
    /* 0x24 */ byte                      pad_24[8];
    /* 0x2C */ ActorShared80134ff0Obj2C* field_2C;
} ActorShared80134ff0;

void ActorsShared80134ff0(ActorShared80134ff0* arg0);

#endif
