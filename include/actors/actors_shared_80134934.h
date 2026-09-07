#ifndef ACTORS_SHARED_80134934_H
#define ACTORS_SHARED_80134934_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct ActorShared80134934Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} ActorShared80134934Obj2C;

typedef struct ActorShared80134934 {
    /* 0x00 */ byte                      pad_0[0x20];
    /* 0x20 */ void*                     field_20;
    /* 0x24 */ byte                      pad_24[8];
    /* 0x2C */ ActorShared80134934Obj2C* field_2C;
} ActorShared80134934;

void ActorsShared80134934(ActorShared80134934* arg0);

#endif
